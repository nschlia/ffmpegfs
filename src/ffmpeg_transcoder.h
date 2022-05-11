/*
 * Copyright (C) 2017-2022 by Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * On Debian systems, the complete text of the GNU General Public License
 * Version 3 can be found in `/usr/share/common-licenses/GPL-3'.
 */

/**
 * @file ffmpeg_transcoder.h
 * @brief FFmpeg transcoder
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef FFMPEG_TRANSCODER_H
#define FFMPEG_TRANSCODER_H

#pragma once

#include "ffmpeg_base.h"
#include "ffmpeg_frame.h"
#include "ffmpeg_subtitle.h"
#include "id3v1tag.h"
#include "ffmpegfs.h"
#include "fileio.h"
#include "ffmpeg_profiles.h"

#include <queue>
#include <mutex>
#include <variant>
#include <functional>
#include <optional>

class Buffer;
struct SwrContext;
struct SwsContext;
struct AVFilterContext;
struct AVFilterGraph;
struct AVAudioFifo;
struct AVCodecContext;
struct AVSubtitle;

/**
 * @brief The #FFmpeg_Transcoder class
 */
class FFmpeg_Transcoder : public FFmpeg_Base, FFmpeg_Profiles
{
protected:
    /**
      * @brief Buffer structure, used in FFmpeg_Transcoder::read_packet
      */
    typedef struct BUFFER_DATA
    {
        uint8_t *   ptr;    ///< Pointer to buffer
        size_t      size;   ///< Size left in the buffer
    } BUFFER_DATA;

#define MAX_PRORES_FRAMERATE    2                                   /**< @brief Number of selectable fram rates */

    /**
     * @brief Predicted bitrates for Apple Prores, see https://www.apple.com/final-cut-pro/docs/Apple_ProRes_White_Paper.pdf
     */
    typedef struct PRORES_BITRATE                                   /**< @brief List of ProRes bit rates */
    {
        int                     m_width;                            /**< @brief Resolution: width */
        int                     m_height;                           /**< @brief Resolution: height */
        struct PRORES_FRAMERATE                                     /**< @brief List of ProRes frame rates */
        {
            int                 m_framerate;                        /**< @brief Frame rate */
            int                 m_interleaved;                      /**< @brief Format is interleaved */
        }                       m_framerate[MAX_PRORES_FRAMERATE];  /**< @brief Array of frame rates */
        /**
         * Bitrates in MB/s
         * 0: ProRes 422 Proxy
         * 1: ProRes 422 LT
         * 2: ProRes 422 standard
         * 3: ProRes 422 HQ
         * 4: ProRes 4444 (no alpha)
         * 5: ProRes 4444 XQ (no alpha)
         */
        int                     m_bitrate[6];                       /**< @brief Bitrate of this format */
    } PRORES_BITRATE, *LPPRORES_BITRATE;                            /**< @brief Pointer version of PRORES_BITRATE */
    typedef PRORES_BITRATE const * LPCPRORES_BITRATE;               /**< @brief Pointer to const version of PRORES_BITRATE */

    class StreamRef                                                 /**< @brief In/output stream reference data */
    {
    public:
        StreamRef();
        virtual ~StreamRef();

        /**
         * @brief Set the AVCodecContext pointer. Will be shared and deleted after the last consumer freed it.
         * @param[in] codec_ctx - AVCodecContext pointer to store
         */
        void set_codec_ctx(AVCodecContext *codec_ctx);

        /**
         * @brief Close (reset) AVCodecContext pointer
         */
        void close();

    public:
        std::shared_ptr<AVCodecContext> m_codec_ctx;                /**< @brief AVCodecContext for this encoder stream */
        AVStream *                      m_stream;                   /**< @brief AVStream for this encoder stream */
        int                             m_stream_idx;               /**< @brief Stream index in AVFormatContext */
        int64_t                         m_start_time;               /**< @brief Start time of the stream in stream time base units, may be 0 */
    };

    typedef std::map<int, StreamRef> StreamRef_map;                 /**< @brief Map stream index to StreamRef */

    struct INPUTFILE                                                /**< @brief Input file definition */
    {
        INPUTFILE() :
            m_filetype(FILETYPE_UNKNOWN),
            m_filename("unset"),
            m_format_ctx(nullptr),
            m_pix_fmt(AV_PIX_FMT_NONE)
        {}

        FILETYPE                m_filetype;                         /**< @brief File type, MP3, MP4, OPUS etc. */
        std::string             m_filename;                         /**< @brief Output filename */

        AVFormatContext *       m_format_ctx;                       /**< @brief Output format context */

        StreamRef               m_audio;                            /**< @brief Audio stream information */
        StreamRef               m_video;                            /**< @brief Video stream information */
        AVPixelFormat           m_pix_fmt;                          /**< @brief Video stream pixel format */
        StreamRef_map           m_subtitle;                         /**< @brief Subtitle stream information */

        std::vector<StreamRef>  m_album_art;                        /**< @brief Album art stream */
    };

    // Output file
    struct OUTPUTFILE : public INPUTFILE                            /**< @brief Output file definition */
    {
        OUTPUTFILE() :
            m_audio_pts(0),
            m_video_pts(0),
            m_last_mux_dts(AV_NOPTS_VALUE)
        {
            memset(&m_id3v1, 0, sizeof(m_id3v1));
        }

        int64_t                 m_audio_pts;                        /**< @brief Global timestamp for the audio frames in output audio stream time base units  */
        int64_t                 m_video_pts;                        /**< @brief Global timestamp for the video frames in output video stream time base units  */
        int64_t                 m_last_mux_dts;                     /**< @brief Last muxed DTS */

        ID3v1                   m_id3v1;                            /**< @brief mp3 only, can be referenced at any time */
    };

    typedef std::map<AVHWDeviceType, AVPixelFormat> DEVICETYPE_MAP; /**< @brief Map device types to pixel formats */

    typedef enum HWACCELMODE                                        /**< @brief Currently active hardware acceleration mode */
    {
        HWACCELMODE_NONE,                                           /**< @brief Hardware acceleration not active */
        HWACCELMODE_ENABLED,                                        /**< @brief Hardware acceleration is active */
        HWACCELMODE_FALLBACK                                        /**< @brief Hardware acceleration selected, but fell back to software */

    } HWACCELMODE;

    typedef std::variant<FFmpeg_Frame, FFmpeg_Subtitle> MULTIFRAME; /**< @brief Combined audio/videoframe and subtitle */
    /**
     * @brief MULTIFRAME_NODE node_type for use with std::map<...>::extract.
     * To be honest I was incredibly close to using "auto" as in all examples,
     * although abhoring this keyword.
     * This syntax is really credible for the ugliest piece of C++ code of
     * all times (grin).
     */
    typedef std::multimap<long, MULTIFRAME, std::less<long>, std::allocator<std::pair<const long, MULTIFRAME>>>::node_type MULTIFRAME_NODE;

    typedef std::multimap<int64_t, MULTIFRAME>  MULTIFRAME_MAP;     /**< @brief Audio frame/video frame/subtitle buffer */
    typedef std::map<int, int>                  STREAM_MAP;         /**< @brief Map input subtitle stream to output stream */

public:
    /**
     * Construct FFmpeg_Transcoder object
     */
    explicit FFmpeg_Transcoder();
    /**
     * Destroy FFMPEG_Transcoder object
     * Close and free all internal structures.
     */
    virtual ~FFmpeg_Transcoder();

    /**
     * Check if input file is already open.
     *
     * @return true if open; false if closed
     */
    bool                        is_open() const;
    /**
     * Open the given FFmpeg file and prepare for decoding.
     * Collect information for the file (duration, bitrate, etc.).
     * After this function, the other methods can be used to process the file.
     *
     * @param[in,out] virtualfile - Virtualfile object for desired file. May be a physical file, a DVD, Bluray or video CD
     * @param[in,out] fio - Pass an already open fileio object. Normally the file is opened, but if this parameter is not nullptr the already existing object is used.
     *
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         open_input_file(LPVIRTUALFILE virtualfile, FileIO * fio = nullptr);
    /**
     * @brief Open output file. Data will actually be written to buffer and copied by FUSE when accessed.
     * @param[in] buffer - Cache buffer to be written.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         open_output_file(Buffer* buffer);
    /**
     * Process a single frame of audio data. The encode_pcm_data() method
     * of the Encoder will be used to process the resulting audio data, with the
     * result going into the given Buffer.
     * @param[out] status - On success, returns 0; if at EOF, returns 1; on error, returns -1
     * @return On success returns 0; on error negative AVERROR. 1 if EOF reached
     */
    int                         process_single_fr(int & status);
    /**
     * Encode any remaining PCM data to the given buffer. This should be called
     * after all input data has already been passed to encode_pcm_data().
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         encode_finish();
    /**
     * @brief Close transcoder, free all ressources.
     */
    void                        close();
    /**
     * @brief Get last modification time of file.
     * @return Modification time (seconds since epoch)
     */
    time_t                      mtime() const;
    /**
     * @brief Get the file duration.
     * @return Returns the file in AV_TIME_BASE units.
     */
    int64_t                     duration() const;
    /**
     * @brief Try to predict the recoded file size. This may (better will surely) be inaccurate.
     * @return Predicted file size in bytes.
     */
    size_t                      predicted_filesize() const;
    /**
     * @brief Get the number of video frames in file.
     * @return On success, returns the number of frames; on error, returns 0 (calculation failed or no video source file).
     */
    uint32_t                    video_frame_count() const;
    /**
     * @brief Get the number of HLS segments of file.
     * @return On success, returns the number of segments; on error, returns 0 (calculation failed).
     */
    uint32_t                    segment_count() const;
    /**
     * @brief Assemble an ID3v1 file tag
     * @return Returns an ID3v1 file tag.
     */
    const ID3v1 *               id3v1tag() const;
    /**
     * @brief Return source filename. Must be implemented in child class.
     * @return Returns filename.
     */
    virtual const char *        filename() const override;
    /**
     * @brief Return destination filename. Must be implemented in child class.
     * @return Returns filename.
     */
    virtual const char *        destname() const override;
    /**
     * @brief Predict audio file size. This may (better will surely) be inaccurate.
     * @param[out] filesize - Predicted file size in bytes, including audio stream size.
     * @param[in] codec_id - Target codec ID.
     * @param[in] bit_rate - Target bit rate.
     * @param[in] duration - File duration.
     * @param[in] channels - Number of channels in target file.
     * @param[in] sample_rate - Sample rate of target file.
     * @param[in] sample_format - Selected sample format
     * @return On success, returns true; on failure, returns false.
     */
    static bool                 audio_size(size_t *filesize, AVCodecID codec_id, BITRATE bit_rate, int64_t duration, int channels, int sample_rate, AVSampleFormat sample_format);
    /**
     * @brief Predict video file size. This may (better will surely) be inaccurate.
     * @param[out] filesize - Predicted file size in bytes, including video stream size.
     * @param[in] codec_id - Target codec ID.
     * @param[in] bit_rate - Target bit rate.
     * @param[in] duration - File duration.
     * @param[in] width - Target video width.
     * @param[in] height- Target video height.
     * @param[in] interleaved - 1 if target video is interleaved.
     * @param[in] framerate - Frame rate of target video.
     * @return On success, returns true; on failure, returns false.
     */
    static bool                 video_size(size_t *filesize, AVCodecID codec_id, BITRATE bit_rate, int64_t duration, int width, int height, int interleaved, const AVRational & framerate);
    /**
     * @brief Predict overhead in file size. This may (better will surely) be inaccurate.
     * @param[out] filesize - Predicted file size in bytes, including overhead.
     * @param[in] filetype - File type: MP3, TS etc.
     * @return On success, returns true; on failure, returns false.
     */
    static bool                 total_overhead(size_t *filesize, FILETYPE filetype);
    /**
     * @brief Closes the output file of open and reports lost packets. Can safely be called again after the file was already closed or if the file was never open.
     */
    void                        close_output_file();
    /**
     * @brief Closes the input file of open. Can safely be called again after the file was already closed or if the file was never open.
     */
    void                        close_input_file();

    /**
     * @brief Seek to a specific frame. Does not actually perform the seek, this is done asynchronously by the transcoder thread.
     * @param[in] frame_no - Frame number to seek 1...n
     * @return On success returns 0; on error negative AVERROR value and sets errno to EINVAL.
     */
    int                         stack_seek_frame(uint32_t frame_no);
    /**
     * @brief Seek to a specific HLS segment. Does not actually perform the seek, this is done asynchronously by the transcoder thread.
     * @param[in] segment_no - Segment number to seek 1...n
     * @return On success returns 0; on error negative AVERROR value and sets errno to EINVAL.
     */
    int                         stack_seek_segment(uint32_t segment_no);
    /**
     * @brief Check for an export frame format
     * @return Returns true for formats that export all frames as images.
     */
    bool                        is_multiformat() const;
    /**
     * @brief Check for an export frame format
     * @return Returns true for formats that export all frames as images.
     */
    bool                        is_frameset() const;
    /**
     * @brief Check for HLS format
     * @return Returns true for formats that create an HLS set including the m3u file.
     */
    bool                        is_hls() const;
    /**
     * @brief Check if we made a seek operation
     * @return Returns true if a seek was done, false if not.
     */
    bool                        have_seeked() const;
    /**
     * @brief Flush FFmpeg's input buffers
     */
    void                        flush_buffers();
    /**
     * @brief Flush delayed audio packets, if there are any
     */
    int                         flush_delayed_audio();
    /**
     * @brief Flush delayed video packets, if there are any
     */
    int                         flush_delayed_video();
    /**
     * @brief Flush delayed subtitle packets, if there are any
     */
    int                         flush_delayed_subtitles();

protected:
    /**
     * @brief Copy data from audio FIFO to frame buffer.
     * Divides WAV data into proper chunks to be fed into the
     * encoder.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         copy_audio_to_frame_buffer(int *finished);
    /**
     * @brief Find best match stream and open codec context for it.
     * @param[in] format_ctx - Output format context
     * @param[out] codec_ctx, - Newly created codec context
     * @param[in] stream_idx - Stream index of new stream.
     * @param[in] type - Type of media: audio or video.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         open_bestmatch_decoder(AVFormatContext *format_ctx, AVCodecContext **codec_ctx, int *stream_idx, AVMediaType type);
    /**
     * @brief Open the best match video stream, if present in input file.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         open_bestmatch_video();
    /**
     * @brief Open the best match audio stream.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         open_bestmatch_audio();
    /**
     * @brief Open all subtitles streams, if present in input file
     * and if supported by output file. The input and output codec
     * type must also match: Can only transcode bitmap subtitles
     * into bitmap subtitles or text to text.
     * @todo Add text to bitmap conversion.
     * @return On success returns 0; on error negative AVERROR.
     */
    int open_subtitles();
    /**
     * @brief open_albumarts
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         open_albumarts();
    /**
     * @brief Determine the hardware pixel format for the codec, if applicable.
     * @param[in] codec - Input codec used
     * @param[in] dev_type - Hardware device type
     * @param[in] use_device_ctx - If true checks for pix format if using a hardware device context, for a pix format using a hardware frames context otherwise.
     * @return Returns hardware pixel format, or AV_PIX_FMT_NONE if not applicable.
     */
#if IF_DECLARED_CONST
    AVPixelFormat               get_hw_pix_fmt(const AVCodec *codec, AVHWDeviceType dev_type, bool use_device_ctx) const;
#else // !IF_DECLARED_CONST
    AVPixelFormat               get_hw_pix_fmt(AVCodec *codec, AVHWDeviceType dev_type, bool use_device_ctx) const;
#endif // !IF_DECLARED_CONST
    /**
     * @brief Open codec context for stream_idx.
     * @param[in] format_ctx - Output format context
     * @param[out] codec_ctx - Newly created codec context
     * @param[in] stream_idx - Stream index of new stream.
     * @param[in] input_codec - Decoder codec to open, may be nullptr. Will open a matching codec automatically.
     * @param[in] mediatype - Type of media: audio or video.
     * @return On success returns 0; on error negative AVERROR.
     */
#if IF_DECLARED_CONST
    int                         open_decoder(AVFormatContext *format_ctx, AVCodecContext **codec_ctx, int stream_idx, const AVCodec *input_codec, AVMediaType mediatype);
#else // !IF_DECLARED_CONST
    int                         open_decoder(AVFormatContext *format_ctx, AVCodecContext **codec_ctx, int stream_idx, AVCodec *input_codec, AVMediaType mediatype);
#endif // !IF_DECLARED_CONST
    /**
     * @brief Open output frame set. Data will actually be written to buffer and copied by FUSE when accessed.
     * @param[in] buffer - Stream buffer to operate on
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         open_output_frame_set(Buffer *buffer);
    /**
     * @brief Open output file. Data will actually be written to buffer and copied by FUSE when accessed.
     * @param[in] buffer - Stream buffer to operate on
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         open_output(Buffer *buffer);
    /**
     * @brief Process headers of output file
     * Write file header, process meta data and add album arts.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         process_output();
    /**
     * FFmpeg handles cover arts like video streams.
     * Try to find out if we have a video stream or a cover art.
     * @return Return true if file contains a video stream.
     */
    bool                        is_video() const;
    /**
     * @brief Prepare codec options.
     * @param[in] opt - Codec private data.
     * @param[in] profile_option - Selected profile option.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         update_codec(void *opt, LPCPROFILE_OPTION profile_option) const;
    /**
     * @brief Prepare codec options for a file type.
     * @param[in] opt - Codec private data.
     * @param[in] filetype - File type: MP3, MP4 etc.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         prepare_codec(void *opt, FILETYPE filetype) const;
    /**
     * @brief Add new stream to output file.
     * @param[in] codec_id - Codec for this stream.
     * @return On success returns index of new stream [0...n]; on error negative AVERROR.
     */
    int                         add_stream(AVCodecID codec_id);
    /**
     * @brief Add new subtitle stream to output file.
     * @param[in] codec_id - Codec for this stream.
     * @param[in] input_streamref - Streamref of input stream.
     * @param[in] language - (Optional) Language or subtitle file, or std::nullopt if unknown.
     * @return On success returns index of new stream [0...n]; on error negative AVERROR.
     */
    int                         add_subtitle_stream(AVCodecID codec_id, StreamRef & input_streamref, const std::optional<std::string> &language = std::nullopt);
    /**
     * @brief Add new stream copy to output file.
     * @param[in] codec_id - Codec for this stream.
     * @param[in] codec_type - Codec type: audio or video.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         add_stream_copy(AVCodecID codec_id, AVMediaType codec_type);
    /**
     * @brief Add a stream for an album art.
     * @param[in] input_codec_ctx - Input codec context.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         add_albumart_stream(const AVCodecContext *input_codec_ctx);
    /**
     * @brief Add album art to stream.
     * @param[in] output_stream - Output stream.
     * @param[in] pkt_in - Packet with album art.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         add_albumart_frame(AVStream *output_stream, AVPacket *pkt_in);
    /**
     * @brief Open an output file and the required encoder.
     * Also set some basic encoder parameters.
     * Some of these parameters are based on the input file's parameters.
     * @param[in] buffer - Stream buffer to operate on
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         open_output_filestreams(Buffer *buffer);
    /**
     * @brief Safely copy a tag to a target buffer. If the input buffer size
     * is larger than output the data will be truncated to avoid overruns.
     * The function never appends a /0 terminator.
     * @param[out] out - Target buffer
     * @param[in] in - Input buffer
     * @return Constant pointer to target buffer.
     */
    template <size_t size>
    const char *                tagcpy(char (&out) [ size ], const std::string & in) const;
    /**
     * @brief Process the metadata in the FFmpeg file.
     * This should be called at the beginning, before reading audio data.
     * The set_text_tag() and set_picture_tag() methods of the given Encoder will
     * be used to set the metadata, with results going into the given Buffer.
     * This function will also read the actual PCM stream parameters.
     * @param[in] metadata_out - Dictionary of output file. Metadata will be copied into it.
     * @param[in] metadata_in - Dictionary of input file. Metadata will be copied out of it.
     * @param[in] contentstream - True if this is a content stream, i.e, audio or video. False for album arts or sub titles.
     */
    void                        copy_metadata(AVDictionary **metadata_out, const AVDictionary *metadata_in, bool contentstream = true);
    /**
     * @brief Copy metadata from source to target
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         process_metadata();
    /**
     * @brief Copy all album arts from source to target.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         process_albumarts();
    /**
     * @brief Initialize the audio resampler based on the input and output codec settings.
     * If the input and output sample formats differ, a conversion is required
     * libswresample takes care of this, but requires initialization.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         init_resampler();
    /**
     * @brief Initialise a FIFO buffer for the audio samples to be encoded.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         init_audio_fifo();
    /**
     * @brief Update format options
     * @param[in] dict - Dictionary to update.
     * @param[in] option - Profile option to set.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         update_format(AVDictionary** dict, LPCPROFILE_OPTION option) const;
    /**
     * @brief Prepare format optimisations
     * @param[in] dict - Dictionary to update.
     * @param[in] filetype - File type: MP3, MP4 etc.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         prepare_format(AVDictionary **dict, FILETYPE filetype) const;
    /**
     * @brief Write the header of the output file container.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         write_output_file_header();
    /**
     * @brief Store packet in output stream.
     * @param[in] pkt - Packet to store.
     * @param[in] mediatype - Typo of packet: audio, video, image (attachment)
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         store_packet(AVPacket *pkt, AVMediaType mediatype);
    /**
     * @brief Decode one audio frame
     * @param[in] pkt - Packet to decode.
     * @param[in] decoded - 1 if packet was decoded, 0 if it did not contain data.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         decode_audio_frame(AVPacket *pkt, int *decoded);
    /**
     * @brief Decode one video frame
     * @param[in] pkt - Packet to decode.
     * @param[in] decoded - 1 if packet was decoded, 0 if it did not contain data.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         decode_video_frame(AVPacket *pkt, int *decoded);
    /**
     * @brief Decode one subtitle
     * @param[in] pkt - Packet to decode.
     * @param[in] decoded - 1 if packet was decoded, 0 if it did not contain data.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         decode_subtitle(AVPacket *pkt, int *decoded);
    /**
     * @brief Decode one subtitle
     * @param[in] codec_ctx - AVCodecContext object of output codec context.
     * @param[in] pkt - Packet to decode.
     * @param[in] decoded - 1 if packet was decoded, 0 if it did not contain data.
     * @param[in] out_stream_idx - Output stream index.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         decode_subtitle(AVCodecContext *codec_ctx, AVPacket *pkt, int *decoded, int out_stream_idx);
    /**
     * @brief Decode one frame.
     * @param[in] pkt - Packet to decode.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         decode_frame(AVPacket *pkt);
    /**
     * @brief Initialise a temporary storage for the specified number of audio samples.
     * The conversion requires temporary storage due to the different format.
     * The number of audio samples to be allocated is specified in frame_size.
     * @param[out] converted_input_samples - Memory for input samples.
     * @param[in] frame_size - Size of one frame.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         init_converted_samples(uint8_t ***converted_input_samples, int frame_size);
    /**
     * @brief Convert the input audio samples into the output sample format.
     * The conversion happens on a per-frame basis, the size of which is
     * specified by frame_size.
     * @param[in] input_data - Input data.
     * @param[in] in_samples - Number of input samples.
     * @param[out] converted_data - Converted data.
     * @param[out] out_samples - Number of output samples
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         convert_samples(uint8_t **input_data, int in_samples, uint8_t **converted_data, int *out_samples);
    /**
     * @brief Add converted input audio samples to the FIFO buffer for later processing.
     * @param[in] converted_input_samples - Samples to add.
     * @param[in] frame_size - Frame size
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         add_samples_to_fifo(uint8_t **converted_input_samples, int frame_size);
    /**
     * @brief Flush the remaining frames for all streams.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         flush_frames_all(bool use_flush_packet);
    /**
     * @brief Flush the remaining frames
     * @param[in] stream_idx - Stream index to flush.
     * @param[in] use_flush_packet - If true, use flush packet. Otherwise pass nullptr to avcodec_receive_frame.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         flush_frames_single(int stream_idx, bool use_flush_packet);
    /**
     * @brief Read frame from source file, decode and store in FIFO.
     * @param[in] finished - 1 if at EOF.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         read_decode_convert_and_store(int *finished);
    /**
     * @brief Initialise one input frame for writing to the output file.
     * The frame will be exactly frame_size samples large.
     * @param[in] frame - Newly initialised frame.
     * @param[in] frame_size - Size of new frame.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         init_audio_output_frame(AVFrame *frame, int frame_size) const;
    /**
     * @brief Allocate memory for one picture.
     * @param[in] frame - Frame to prepare
     * @param[in] pix_fmt - Pixel format
     * @param[in] width - Picture width
     * @param[in] height - Picture height
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         alloc_picture(AVFrame *frame, AVPixelFormat pix_fmt, int width, int height) const;
    /**
     * @brief Produce audio dts/pts. This is required because the target codec usually has a different
     * frame size than the source, so the number of packets will not match 1:1.
     * @param[in] pkt - Packet to add dts/pts to.
     */
    void                        produce_audio_dts(AVPacket * pkt);
    /**
     * This does not quite work like avcodec_decode_audio4/avcodec_decode_video2.
     * There is the following difference: if you got a frame, you must call
     * it again with pkt=nullptr. pkt==nullptr is treated differently from pkt->size==0
     * (pkt==nullptr means get more output, pkt->size==0 is a flush/drain packet)
     * @param[in] codec_ctx - AVCodecContext of input stream.
     * @param[in] frame - Decoded frame
     * @param[out] got_frame - 1 if a frame was decoded, 0 if not
     * @param[in] pkt - Packet to decode
     * @return On success returns 0. On error, returns a negative AVERROR value.
     */
    int                         decode(AVCodecContext *codec_ctx, AVFrame *frame, int *got_frame, const AVPacket *pkt) const;
    /**
     * @brief Load one audio frame from the FIFO buffer and store in frame buffer.
     * @param[in] frame_size - Size of frame.
     * @return On success returns 0. On error, returns a negative AVERROR value.
     */
    int                         create_audio_frame(int frame_size);
    /**
     * @brief Create one frame worth of audio to the output file.
     * @param[in] frame - Audio frame to encode
     * @param[in] data_present - 1 if frame contained data that could be encoded, 0 if not.
     * @return On success returns 0. On error, returns a negative AVERROR value.
     */
    int                         encode_audio_frame(const AVFrame *frame, int *data_present);
    /**
     * @brief Encode one frame worth of video to the output file.
     * @param[in] frame - Video frame to encode
     * @param[in] data_present - 1 if frame contained data that could be encoded, 0 if not.
     * @return On success returns 0. On error, returns a negative AVERROR value.
     */
    int                         encode_video_frame(const AVFrame *frame, int *data_present);
    /**
     * @brief Encode one subtitle frame to the output file.
     * @param[in] sub - Subtitle frame to encode
     * @param[in] out_stream_idx - Index of stream to encode to.
     * @param[in] data_present - 1 if frame contained data that could be encoded, 0 if not.
     * @return On success returns 0. On error, returns a negative AVERROR value.
     */
    int                         encode_subtitle(const AVSubtitle *sub, int out_stream_idx, int *data_present);
    /**
     * @brief Encode frame to image
     * @param[in] frame - Video frame to encode
     * @param[out] data_present - Set to 1 if data was encoded. 0 if not.
     * @return On success returns 0. On error, returns a negative AVERROR value.
     */
    int                         encode_image_frame(const AVFrame *frame, int *data_present);
    /**
     * @brief Write the trailer of the output file container.
     * @return On success returns 0. On error, returns a negative AVERROR value.
     */
    int                         write_output_file_trailer();
    /**
     * @brief Custom read function for FFmpeg
     *
     * Read from virtual files, may be a physical file but also a DVD, VCD or Bluray chapter.
     *
     * @param[in] opaque - Payload given to FFmpeg, basically the FileIO object
     * @param[in] data - Returned data read from file.
     * @param[in] size - Size of data buffer.
     * @return On success returns bytes read. May be less than size or even 0. On error, returns a negative AVERROR value.
     */
    static int                  input_read(void * opaque, unsigned char * data, int size);
    /**
     * @brief Custom write function for FFmpeg
     * @param[in] opaque - Payload given to FFmpeg, basically the FileIO object
     * @param[in] data - Data to be written
     * @param[in] size - Size of data block.
     * @return On success returns bytes written. On error, returns a negative AVERROR value.
     */
    static int                  output_write(void * opaque, unsigned char * data, int size);
    /**
     * @brief Custom seek function for FFmpeg
     *
     * Write to virtual files, currently only physical files.
     *
     * @param[in] opaque - Payload given to FFmpeg, basically the FileIO object
     * @param[in] offset - Offset to seek to.
     * @param[in] whence - One of the regular seek() constants like SEEK_SET/SEEK_END. Additionally FFmpeg constants like AVSEEK_SIZE are supported.
     * @return On successs returns 0. On error returns -1 and sets errno accordingly.
     */
    static int64_t              seek(void * opaque, int64_t offset, int whence);

    /**
     * @brief Calculate the appropriate bitrate for a ProRes file given several parameters.
     * @param[in] width - Video width in pixels.
     * @param[in] height - Video height in pixels.
     * @param[in] framerate - Video frame rate.
     * @param[in] interleaved - If 1, video is interleaved; 0 if not.
     * @param[in] profile - Selected ProRes profile.
     * @return Bitrate in bit/s.
     */
    static BITRATE              get_prores_bitrate(int width, int height, const AVRational &framerate, int interleaved, int profile);
    /**
     * @brief Try to predict final file size.
     */
    size_t                      calculate_predicted_filesize() const;
    /**
     * @brief Get the size of the output video based on user selection and apsect ratio.
     * @param[in] output_width - Output video width.
     * @param[in] output_height - Output video height.
     * @return Returns true if video height/width was reduces; false if not.
     */
    bool                        get_video_size(int *output_width, int *output_height) const;
    /**
     * @brief Calculate output sample rate based on user option.
     * @param[in] input_sample_rate - Sample rate from input file.
     * @param[in] max_sample_rate - Max. sample rate if set by user
     * @param[in] output_sample_rate - Selected output sample rate.
     * @return Returns true if sample rate was changed; false if not.
     */
    static bool                 get_output_sample_rate(int input_sample_rate, int max_sample_rate, int * output_sample_rate = nullptr);
    /**
     * @brief Calculate output bit rate based on user option.
     * @param[in] input_bit_rate - Bit rate from input file.
     * @param[in] max_bit_rate - Max. bit rate if set by user.
     * @param[in] output_bit_rate - Selected output bit rate.
     * @return Returns true if bit rate was changed; false if not.
     */
    static bool                 get_output_bit_rate(BITRATE input_bit_rate, BITRATE max_bit_rate, BITRATE * output_bit_rate = nullptr);
    /**
     * @brief Calculate aspect ratio for width/height and sample aspect ratio (sar).
     * @param[in] width - Video width in pixels.
     * @param[in] height - Video height in pixels.
     * @param[in] sar - Aspect ratio of input video.
     * @param[in] ar - Calulcated aspect ratio, if computeable.
     * @return On success returns true; if false is returned ar may not be used.
     */
    bool                        get_aspect_ratio(int width, int height, const AVRational & sar, AVRational * ar) const;

    /**
     * @brief Initialise video filters
     * @param[in] codec_ctx - AVCodecContext object of output video.
     * @param[in] pix_fmt - Output stream pixel format.
     * @param[in] avg_frame_rate - Average output stream frame rate.
     * @param[in] time_base - Output stream time base.
     * @return Returns 0 if OK, or negative AVERROR value.
     */
    int                         init_deinterlace_filters(AVCodecContext *codec_ctx, AVPixelFormat pix_fmt, const AVRational &avg_frame_rate, const AVRational &time_base);
    /**
     * @brief Send video frame to the filters.
     * @param[inout] srcframe - On input video frame to process, on output video frame that was filtered.
     * @param[in] ret - 0 if OK, or negative AVERROR value.
     * @return Returns 0 if OK, or negative AVERROR value.
     */
    int                         send_filters(FFmpeg_Frame * srcframe, int &ret);
    /**
     * @brief Free filter sinks.
     */
    void                        free_filters();
    /**
     * @brief Check if stream can be copied from input to output (AUTOCOPY option).
     * @param[in] stream - Input stream to check.
     * @return Returns true if stream can be copied; false if not.
     */
    bool                        can_copy_stream(const AVStream *stream) const;
    /**
     * @brief Close and free the resampler context.
     * @return If an open context was closed, returns true; if nothing had been done returns false.
     */
    bool                        close_resample();
    /**
     * @brief Init image size rescaler and pixel format converter.
     * @param[in] in_pix_fmt - Input pixel format
     * @param[in] in_width - Input image width
     * @param[in] in_height - Input image height
     * @param[in] out_pix_fmt - Output pixel format
     * @param[in] out_width - Output image width
     * @param[in] out_height - Output pixel format
     * @return Returns 0 if OK, or negative AVERROR value.
     */
    int 						init_rescaler(AVPixelFormat in_pix_fmt, int in_width, int in_height, AVPixelFormat out_pix_fmt, int out_width, int out_height);
    /**
     * @brief Purge all samples in audio FIFO
     * @return Number of samples that have been purged. Function never fails.
     */
    int                         purge_audio_fifo();
    /**
     * @brief Purge all frames in buffer
     * @return Number of frames that have been purged. Function never fails.
     */
    size_t                      purge_multiframe_map();
    /**
     * @brief Purge all packets in HLS FIFO buffer
     * @return Number of Packets that have been purged. Function never fails.
     */
    size_t                      purge_hls_fifo();
    /**
     * @brief Purge FIFO and map buffers and report lost packets/frames/samples.
     */
    void                        purge();
    /**
     * @brief Actually perform seek for frame.
     * This function ensures that it is positioned at a key frame, so the resulting position may be different from the requested.
     * If e.g. frame no. 24 is a key frame, and frame_no is set to 28, the actual position will be at frame 24.
     * @param[in] frame_no - Frame number 1...n to seek to.
     * @return Returns 0 if OK, or negative AVERROR value.
     */
    int                         do_seek_frame(uint32_t frame_no);
    /**
     * @brief Skip decoded frames or force seek to frame_no.
     * @param[in] frame_no - Frame to seek to.
     * @param[in] forced_seek - Force seek even if np frames skipped.
     * @return Returns 0 if OK, or negative AVERROR value.
     */
    int                         skip_decoded_frames(uint32_t frame_no, bool forced_seek);
    /**
     * @brief Get correct input and output pixel format
     * @param[in] output_codec_ctx - Output codec context.
     * @param[out] in_pix_fmt - Input pixel format.
     * @param[out] out_pix_fmt - Output pixel format.
     */
    void                        get_pix_formats(AVPixelFormat *in_pix_fmt, AVPixelFormat *out_pix_fmt, AVCodecContext* output_codec_ctx = nullptr) const;

    // Hardware de/encoding
    /**
     * Callback to negotiate the pixelFormat
     * @param[in] input_codec_ctx - Input codec context
     * @param[in] pix_fmts is the list of formats which are supported by the codec,
     * it is terminated by -1 as 0 is a valid format, the formats are ordered by quality.
     * The first is always the native one.
     * @note The callback may be called again immediately if initialization for
     * the selected (hardware-accelerated) pixel format failed.
     * @warning Behavior is undefined if the callback returns a value not
     * in the fmt list of formats.
     * @return the chosen format
     * - encoding: unused
     * - decoding: Set by user, if not set the native format will be chosen.
     */
    static enum AVPixelFormat   get_format_static(AVCodecContext *input_codec_ctx, const enum AVPixelFormat *pix_fmts);
    /**
     * Callback to negotiate the pixelFormat
     * @param[in] input_codec_ctx - Input codec context
     * @param[in] pix_fmts is the list of formats which are supported by the codec,
     * it is terminated by -1 as 0 is a valid format, the formats are ordered by quality.
     * The first is always the native one.
     * @note The callback may be called again immediately if initialization for
     * the selected (hardware-accelerated) pixel format failed.
     * @warning Behavior is undefined if the callback returns a value not
     * in the fmt list of formats.
     * @return the chosen format
     * - encoding: unused
     * - decoding: Set by user, if not set the native format will be chosen.
     */
    enum AVPixelFormat          get_format(AVCodecContext *input_codec_ctx, const enum AVPixelFormat *pix_fmts) const;
    /**
     * Open a device of the specified type and create an AVHWDeviceContext for it.
     *
     * This is a convenience function intended to cover the simple cases. Callers
     * who need to fine-tune device creation/management should open the device
     * manually and then wrap it in an AVHWDeviceContext using
     * av_hwdevice_ctx_alloc()/av_hwdevice_ctx_init().
     *
     * The returned context is already initialized and ready for use, the caller
     * should not call av_hwdevice_ctx_init() on it. The user_opaque/free fields of
     * the created AVHWDeviceContext are set by this function and should not be
     * touched by the caller.
     *
     * @param[out] hwaccel_enc_device_ctx - On success, a
     * reference to the newly-created device context will be
     * written here.
     * @param[in] dev_type - The type of the device to create.
     * @param[in] device - A type-specific string identifying the device to open.
     *
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         hwdevice_ctx_create(AVBufferRef **hwaccel_enc_device_ctx, AVHWDeviceType dev_type, const std::string & device) const;
    /**
     * @brief Add reference to hardware device context.
     * @param[in] input_codec_ctx - Input codec context
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         hwdevice_ctx_add_ref(AVCodecContext *input_codec_ctx);
    /**
     * @brief Free (remove reference) to hardware device context
     * @param[inout] hwaccel_device_ctx - Hardware device context to free
     * @return 0 on success, a negative AVERROR code on failure.
     */
    void                        hwdevice_ctx_free(AVBufferRef **hwaccel_device_ctx);
    /**
     * @brief Adds a reference to an existing decoder hardware frame context or
     * allocates a new AVHWFramesContext tied to the given hardware device context
     * if if the decoder runs in software.
     * @param[in] output_codec_ctx - Encoder codexc context
     * @param[in] input_codec_ctx - Decoder codexc context
     * @param[in] hw_device_ctx - Existing hardware device context
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         hwframe_ctx_set(AVCodecContext *output_codec_ctx, AVCodecContext *input_codec_ctx, AVBufferRef *hw_device_ctx) const;
    /**
     * Copy data hardware surface to software.
     * @param[in] output_codec_ctx - Codec context
     * @param[inout] sw_frame - AVFrame to copy data to
     * @param[in] hw_frame - AVFrame to copy data from
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         hwframe_copy_from_hw(AVCodecContext *output_codec_ctx, FFmpeg_Frame *sw_frame, const AVFrame *hw_frame) const;
    /**
     * Copy data software to a hardware surface.
     * @param[in] output_codec_ctx - Codec context
     * @param[inout] hw_frame - AVFrame to copy data to
     * @param[in] sw_frame - AVFrame to copy data from
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         hwframe_copy_to_hw(AVCodecContext *output_codec_ctx, FFmpeg_Frame *hw_frame, const AVFrame *sw_frame) const;
    /**
     * @brief Get the hardware codec name as string. This is required, because e.g.
     * the name for the software codec is libx264, but for hardware it is h264_vaapi
     * under VAAPI.
     * @param[in] codec_id - Id of encoder/decoder codec
     * @param[out] codec_name - Returns the name of the codec, may be nullptr if not requitred.
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         get_hw_decoder_name(AVCodecID codec_id, std::string *codec_name = nullptr) const;
    /**
     * @brief Get the hardware codec name as string. This is required, because e.g.
     * the name for the software codec is libx264, but for hardware it is h264_vaapi
     * under VAAPI.
     * @param[in] codec_id - Id of encoder/decoder codec
     * @param[out] codec_name - Returns the name of the codec, may be nullptr if not requitred.
     * @return 0 on success, AVERROR_DECODER_NOT_FOUND if no codec available.
     */
    int                         get_hw_encoder_name(AVCodecID codec_id, std::string *codec_name = nullptr) const;
    /**
     * @brief Determine VAAPI codec name
     * @param[in] codec_id - Id of encoder/decoder codec
     * @param[out] codec_name - Name of the codec.
     * @return 0 on success, AVERROR_DECODER_NOT_FOUND if no codec available.
     */
    int                         get_hw_vaapi_codec_name(AVCodecID codec_id, std::string *codec_name) const;
    /**
     * @brief Determine MMAL decoder codec name
     * @param[in] codec_id - Id of encoder/decoder codec
     * @param[out] codec_name - Name of the codec.
     * @return 0 on success, AVERROR_DECODER_NOT_FOUND if no codec available.
     */
    int                         get_hw_mmal_decoder_name(AVCodecID codec_id, std::string *codec_name) const;
    /*
     * @brief Determine video for linux decoder codec name
     * @param[in] codec_id - Id of encoder/decoder codec
     * @param[out] codec_name - Name of the codec.
     * @return 0 on success, AVERROR_DECODER_NOT_FOUND if no codec available.
     */
    //int                         get_hw_v4l2m2m_decoder_name(AVCodecID codec_id, std::string *codec_name) const;
    /**
     * @brief Determine OMX encoder codec name
     * @param[in] codec_id - Id of encoder/decoder codec
     * @param[out] codec_name - Name of the codec.
     * @return 0 on success, AVERROR_DECODER_NOT_FOUND if no codec available.
     */
    int                         get_hw_omx_encoder_name(AVCodecID codec_id, std::string *codec_name) const;
    /**
     * @brief Determine video for linux encoder codec name
     * @param[in] codec_id - Id of encoder/decoder codec
     * @param[out] codec_name - Name of the codec.
     * @return 0 on success, AVERROR_DECODER_NOT_FOUND if no codec available.
     */
    int                         get_hw_v4l2m2m_encoder_name(AVCodecID codec_id, std::string *codec_name) const;
    /**
     * @brief Get the software pixel format for the given hardware acceleration.
     * @param[in] type - Selected hardware acceleration.
     * @return 0 on success, a negative AVERROR code on failure.
     */
    static AVPixelFormat        find_sw_fmt_by_hw_type(AVHWDeviceType type);
    /**
     * @brief Calculate next HLS segment from position
     * @param[in] pos - Current transcoder position in AV_TIMEBASE fractional seconds.
     * @return Number of next segment
     */
    uint32_t                    get_next_segment(int64_t pos) const;
    /**
     * @brief Check if segment number is next designated segment.
     * @param[in] next_segment - Number next current segment
     * @return Returns true if next segment should start, false if not.
     */
    bool                        goto_next_segment(uint32_t next_segment) const;

    /**
     * @brief Create a fake WAV header
     * Create a fake WAV header. Inserts predicted file sizes to allow playback
     * to start directly.
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         create_fake_wav_header() const;
    /**
     * @brief Create a fake AIFF header
     * Create a fake AIFF header. Inserts predicted file sizes to allow playback
     * to start directly.
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         create_fake_aiff_header() const;
    /**
     * @brief Read AIFF chunk
     * @param[in] buffer - Cache buffer to read from
     * @param[inout] buffoffset - Byte offset into buffer. Upon return holds offset to the position of the chunk.
     * @param[in] ID - Chunk ID (fourCC)
     * @param[out] chunk - Buffer for chunk
     * @param[inout] size - Size of chunk. Buffer for chunk must be large enough to hold it. Upon return holds the actual size of the chunk read.
     * @return Returns 0 if successful or -1 on error or end of file. Check buffer->eof().
     */
    int                         read_aiff_chunk(Buffer *buffer, size_t *buffoffset, const char *ID, uint8_t *chunk, size_t *size) const;
    /**
     * @brief Check for audio stream
     * @param[in] stream_idx - ID of stream to check
     * @return Returns true if stream is an audio stream, false if not.
     */
    bool                        is_audio_stream(int stream_idx) const;
    /**
     * @brief Check for video stream
     * @param[in] stream_idx - ID of stream to check
     * @return Returns true if stream is a video stream, false if not.
     */
    bool                        is_video_stream(int stream_idx) const;
    /**
     * @brief Check for subtitle stream
     * @param[in] stream_idx - ID of stream to check
     * @return Returns true if stream is a subtitle stream, false if not.
     */
    bool                        is_subtitle_stream(int stream_idx) const;
    /**
     * @brief Get subtitle stream for the stream index
     * @param[in] stream_idx - Stream index to get subtitle stream for
     * @return Pointer to subbtitle stream or nullptr if not found
     */
    StreamRef *                 get_out_subtitle_stream(int stream_idx);
    /**
     * @brief Check if stream exists
     * @param[in] stream_idx - ID of stream to check
     * @return Returns 0 if stream exists, false if not.
     */
    bool                        stream_exists(int stream_idx) const;

    /**
     * @brief Add entry to input stream to output stream map.
     * @param[in] in_stream_idx - Index of input stream
     * @param[in] out_stream_idx - Index of output stream
     */
    void                        add_stream_map(int in_stream_idx, int out_stream_idx);

    /**
     * @brief Map input stream index to output stream index
     * @param[in] in_stream_idx - Index of input stream
     * @return Returns output stream index or INVALID_STREAM if no match
     */
    int                         map_in_to_out_stream(int in_stream_idx) const;

    /**
     * @brief Add all subtitle streams. Already existing streams are not
     * added again.
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         add_subtitle_streams();

    /**
     * @brief Frame sets only: perform seek to a certain frame.
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         seek_frame();

    /**
     * @brief HLS only: start a new HLS segment.
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         start_new_segment();

    /**
     * @brief FFmpeg_Transcoder::read_packet
     * @param[in] opaque
     * @param[in] buf
     * @param[in] buf_size
     * @return
     */
    static int                  read_packet(void *opaque, uint8_t *buf, int buf_size);
    /**
     * @brief Scan for external subtitle files
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         add_external_subtitle_streams();
    /**
     * @brief add_external_subtitle_stream
     * @param[in] subtitle_file - Name of subtitle fule
     * @param[in] language - Language or subtitle file, or std::nullopt if unknown.
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         add_external_subtitle_stream(const std::string & subtitle_file, const std::optional<std::string> & language);
    /**
     * @brief foreach_subititle_file
     * @param[in] search_path - Directory with subtitle files
     * @param[in] regex - Regular expression to select subtitle files
     * @param[in] depth - Recursively scan for subtitles, should be 0.
     * @param[in] f - Funtion to be called for each file found
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int                         foreach_subtitle_file(const std::string& search_path, const std::regex& regex, int depth, std::function<int(const std::string &, const std::optional<std::string> &)> f);

private:
    FileIO *                    m_fileio;                       /**< @brief FileIO object of input file */
    bool                        m_close_fileio;                 /**< @brief If we own the FileIO object, we may close it in the end. */
    time_t                      m_mtime;                        /**< @brief Modified time of input file */
    std::recursive_mutex        m_seek_to_fifo_mutex;           /**< @brief Access mutex for seek FIFO */
    std::queue<uint32_t>        m_seek_to_fifo;                 /**< @brief Stack of seek requests. Will be processed FIFO */
    volatile uint32_t           m_last_seek_frame_no;           /**< @brief If not 0, this is the last frame that we seeked to. Video sources only. */
    bool                        m_have_seeked;                  /**< @brief After seek operations this is set to make sure the trancoding result is marked RESULTCODE_INCOMPLETE to start transcoding over next access to fill the gaps. */
    bool                        m_skip_next_frame;              /**< @brief After seek, skip next video frame */
    bool                        m_is_video;                     /**< @brief true if input is a video file */

    MULTIFRAME_MAP              m_frame_map;                    /**< @brief Audio/video/subtitle frame map */

    // Audio conversion and buffering
    AVSampleFormat              m_cur_sample_fmt;               /**< @brief Currently selected audio sample format */
    int                         m_cur_sample_rate;              /**< @brief Currently selected audio sample rate */
#if LAVU_DEP_OLD_CHANNEL_LAYOUT
    AVChannelLayout             m_cur_ch_layout;                /**< @brief Currently selected audio channel layout */
#else   // !LAVU_DEP_OLD_CHANNEL_LAYOUT
    uint64_t                    m_cur_channel_layout;           /**< @brief Currently selected audio channel layout */
#endif  // !LAVU_DEP_OLD_CHANNEL_LAYOUT
    SwrContext *                m_audio_resample_ctx;           /**< @brief SwResample context for audio resampling */
    AVAudioFifo *               m_audio_fifo;                   /**< @brief Audio sample FIFO */

    // Video conversion and buffering
    SwsContext *                m_sws_ctx;                      /**< @brief Context for video filtering */
    AVFilterContext *           m_buffer_sink_context;          /**< @brief Video filter sink context */
    AVFilterContext *           m_buffer_source_context;        /**< @brief Video filter source context */
    AVFilterGraph *             m_filter_graph;                 /**< @brief Video filter graph */
    int64_t                     m_pts;                          /**< @brief Generated PTS */
    int64_t                     m_pos;                          /**< @brief Generated position */

    // Common things for audio/video/subtitles
    INPUTFILE                   m_in;                           /**< @brief Input file information */
    OUTPUTFILE                  m_out;                          /**< @brief Output file information */
    STREAM_MAP                  m_stream_map;                   /**< @brief Input stream to output stream map */

    uint32_t                    m_current_segment;              /**< @brief HLS only: Segment file number currently being encoded */
    bool                        m_insert_keyframe;              /**< @brief HLS only: Allow insertion of 1 keyframe */

    // If the audio and/or video stream is copied, packets will be stuffed into the packet queue.
    bool                        m_copy_audio;                   /**< @brief If true, copy audio stream from source to target (just remux, no recode). */
    bool                        m_copy_video;                   /**< @brief If true, copy video stream from source to target (just remux, no recode). */

    const FFmpegfs_Format *     m_current_format;               /**< @brief Currently used output format(s) */

    Buffer *                    m_buffer;                       /**< @brief Pointer to cache buffer object */

    uint32_t                    m_reset_pts;                    /**< @brief We have to reset audio/video pts to the new position */
    uint32_t                    m_fake_frame_no;                /**< @brief The MJEPG codec requires monotonically growing PTS values so we fake some to avoid them going backwards after seeks */

    static const PRORES_BITRATE m_prores_bitrate[];             /**< @brief ProRes bitrate table. Used for file size prediction. */

    // Hardware acceleration
    static const DEVICETYPE_MAP m_devicetype_map;               /**< @brief List of AVPixelFormats mapped to hardware acceleration types */
    HWACCELMODE                 m_hwaccel_enc_mode;             /**< @brief Current hardware acceleration mode for encoder */
    HWACCELMODE                 m_hwaccel_dec_mode;             /**< @brief Current hardware acceleration mode for decoder */
    bool						m_hwaccel_enable_enc_buffering; /**< @brief Enable hardware acceleration frame buffers for encoder */
    bool                        m_hwaccel_enable_dec_buffering; /**< @brief Enable hardware acceleration frame buffers for decoder */
    AVBufferRef *               m_hwaccel_enc_device_ctx;       /**< @brief Hardware acceleration device context for encoder */
    AVBufferRef *               m_hwaccel_dec_device_ctx;       /**< @brief Hardware acceleration device context for decoder */
    AVPixelFormat               m_enc_hw_pix_fmt;               /**< @brief Requested encoder hardware pixel format */
    AVPixelFormat               m_dec_hw_pix_fmt;               /**< @brief Requested decoder hardware pixel format */

#define FFMPEGFS_AUDIO          static_cast<uint32_t>(0x0001)   /**< @brief Denote an audio stream */
#define FFMPEGFS_VIDEO          static_cast<uint32_t>(0x0002)   /**< @brief Denote a video stream */
#define FFMPEGFS_SUBTITLE       static_cast<uint32_t>(0x0004)   /**< @brief Denote a subtitle stream */

    uint32_t                    m_active_stream_msk;            /**< @brief HLS: Currently active streams bit mask. Set FFMPEGFS_AUDIO and/or FFMPEGFS_VIDEO */
    uint32_t                    m_inhibit_stream_msk;           /**< @brief HLS: Currently inhibited streams bit mask. Packets temporarly go to m_hls_packet_fifo and will be prepended to next segment. Set FFMPEGFS_AUDIO and/or FFMPEGFS_VIDEO */
    std::queue<AVPacket*>       m_hls_packet_fifo;              /**< @brief HLS packet FIFO */
};

#endif // FFMPEG_TRANSCODER_H
