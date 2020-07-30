/*
 * Copyright (C) 2017-2020 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file
 * @brief FFmpeg transcoder
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2020 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef FFMPEG_TRANSCODER_H
#define FFMPEG_TRANSCODER_H

#pragma once

#include "ffmpeg_base.h"
#include "id3v1tag.h"
#include "ffmpegfs.h"
#include "fileio.h"
#include "ffmpeg_profiles.h"

#include <queue>
#include <mutex>

class Buffer;
#if LAVR_DEPRECATE
struct SwrContext;
#else
struct AVAudioResampleContext;
#endif
struct SwsContext;
struct AVFilterContext;
struct AVFilterGraph;
struct AVAudioFifo;

/**
 * @brief The #FFmpeg_Transcoder class
 *
 * @bug Issue #32: After transcoding several files VMEM usage sometimes goes off the edge.\n
 * After playing files for a while, sometimes the VME usage goes up to the machine
 * limit, In this particular case the machine has 32 GB RAM with 13 GB free.
 * Never happens if the files are copied via Samba or directly, but only if played
 * through a web server. Probably has to do with slow connections and time outs.
 *
 * @bug Issue #33: H265 decoding does not work.\n
 * Decoding H265 files fails.
 */
class FFmpeg_Transcoder : public FFmpeg_Base, FFmpeg_Profiles
{
public:
#define MAX_PRORES_FRAMERATE    2                                       /**< @brief Number of selectable fram rates */

    /** @brief Predicted bitrates for Apple Prores, see https://www.apple.com/final-cut-pro/docs/Apple_ProRes_White_Paper.pdf
      *
      */
    typedef struct PRORES_BITRATE                                       /**< @brief List of ProRes bit rates */
    {
        int                     m_width;                                /**< @brief Resolution: width */
        int                     m_height;                               /**< @brief Resolution: height */
        struct PRORES_FRAMERATE                                         /**< @brief List of ProRes frame rates */
        {
            int                 m_framerate;                            /**< @brief Frame rate */
            int                 m_interleaved;                          /**< @brief Format is interleaved */
        }                       m_framerate[MAX_PRORES_FRAMERATE];      /**< @brief Array of frame rates */
        /**
         * Bitrates in MB/s
         * 0: ProRes 422 Proxy
         * 1: ProRes 422 LT
         * 2: ProRes 422 standard
         * 3: ProRes 422 HQ
         * 4: ProRes 4444 (no alpha)
         * 5: ProRes 4444 XQ (no alpha)
         */
        int                     m_bitrate[6];                           /**< @brief Bitrate of this format */
    } PRORES_BITRATE, *LPPRORES_BITRATE;                                /**< @brief Pointer version of PRORES_BITRATE */
    typedef PRORES_BITRATE const * LPCPRORES_BITRATE;                   /**< @brief Pointer to const version of PRORES_BITRATE */

    struct STREAMREF                                    /**< @brief In/output stream reference data */
    {
        STREAMREF() :
            m_codec_ctx(nullptr),
            m_stream(nullptr),
            m_stream_idx(INVALID_STREAM)
        {}

        AVCodecContext *        m_codec_ctx;            /**< @brief AVCodecContext for this encoder stream */
        AVStream *              m_stream;               /**< @brief AVStream for this encoder stream */
        int                     m_stream_idx;           /**< @brief Stream index in AVFormatContext */
    };

    struct INPUTFILE                                    /**< @brief Input file definition */
    {
        INPUTFILE() :
            m_filetype(FILETYPE_UNKNOWN),
            m_filename("unset"),
            m_format_ctx(nullptr)
        {}

        FILETYPE                m_filetype;             /**< @brief File type, MP3, MP4, OPUS etc. */
        std::string             m_filename;             /**< @brief Output filename */

        AVFormatContext *       m_format_ctx;           /**< @brief Output format context */

        STREAMREF               m_audio;                /**< @brief Audio stream information */
        STREAMREF               m_video;                /**< @brief Video stream information */

        std::vector<STREAMREF>  m_album_art;            /**< @brief Album art stream */
    };

    // Output file
    struct OUTPUTFILE : public INPUTFILE                /**< @brief Output file definition */
    {
        OUTPUTFILE() :
            m_audio_pts(0),
            m_video_pts(0),
            m_last_mux_dts(AV_NOPTS_VALUE)
        {}

        int64_t                 m_audio_pts;            /**< @brief Global timestamp for the audio frames */
        int64_t                 m_video_pts;            /**< @brief Global timestamp for the video frames */
        int64_t                 m_last_mux_dts;         /**< @brief Last muxed DTS */

        ID3v1                   m_id3v1;                /**< @brief mp3 only, can be referenced at any time */
    };

public:
    /**
     * Construct FFmpeg_Transcoder object
     */
    FFmpeg_Transcoder();
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
     * Encode any remaining PCM data to the given Buffer. This should be called
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
    int64_t                     duration();
    /**
     * @brief Try to predict the recoded file size. This may (better will surely) be inaccurate.
     * @return Predicted file size in bytes.
     */
    size_t                      predicted_filesize();
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
    virtual const char *        filename() const;
    /**
     * @brief Return destination filename. Must be implemented in child class.
     * @return Returns filename.
     */
    virtual const char *        destname() const;
    /**
     * @brief Predict audio file size. This may (better will surely) be inaccurate.
     * @param[in] filesize - Predicted file size in bytes.
     * @param[in] codec_id - Target codec ID
     * @param[in] bit_rate - Target bit rate.
     * @param[in] duration - File duration.
     * @param[in] channels - Number of channels in target file.
     * @param[in] sample_rate - Sample rate of target file.
     * @return On success, returns true; on failure, returns false.
     */
    static bool                 audio_size(size_t *filesize, AVCodecID codec_id, BITRATE bit_rate, int64_t duration, int channels, int sample_rate);
    /**
     * @brief Predict video file size. This may (better will surely) be inaccurate.
     * @param[in] filesize - Predicted file size in bytes.
     * @param[in] codec_id - Target codec ID
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
     * @brief Closes the output file of open and reports lost packets. Can safely be called again after the file was already closed or if the file was never open.
     * @return Returns true if the output file was closed, false if it was not upon upon calling this function.
     */
    bool                        close_output_file();
    /**
     * @brief Closes the input file of open. Can safely be called again after the file was already closed or if the file was never open.
     * @return Returns true if the input file was closed, false if it was not upon upon calling this function.
     */
    bool                        close_input_file();

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

protected:
    /**
     * @brief Open output frame set. Data will actually be written to buffer and copied by FUSE when accessed.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         open_output_frame_set(Buffer *buffer);
    /**
     * @brief Open output file. Data will actually be written to buffer and copied by FUSE when accessed.
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
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         add_stream(AVCodecID codec_id);
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
     */
    int                         open_output_filestreams(Buffer *buffer);
    /**
     * @brief copy_metadataBluray I/O class
     *
     * Process the metadata in the FFmpeg file. This should be called at the
     * beginning, before reading audio data. The set_text_tag() and
     * set_picture_tag() methods of the given Encoder will be used to set the
     * metadata, with results going into the given Buffer. This function will also
     * read the actual PCM stream parameters.
     */
    void                        copy_metadata(AVDictionary **metadata_out, const AVDictionary *metadata_in);
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
    int                         init_fifo();
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
     * @param[in] type - Typo of packet: audio, video, image
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         store_packet(AVPacket *pkt, const char *type);
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
     * @param[in] stream_index - Stream index to flush.
     * @param[in] use_flush_packet - If true, use flush packet. Otherwise pass nullptr to avcodec_receive_frame.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         flush_frames_single(int stream_index, bool use_flush_packet);
    /**
     * @brief Read frame from source file, decode and store in FIFO.
     * @param[in] finished - 1 if at EOF.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         read_decode_convert_and_store(int *finished);
    /**
     * @brief Initialise one input frame for writing to the output file.
     * The frame will be exactly frame_size samples large.
     * @param[out] frame - Newly initialised frame.
     * @param[in] frame_size - Size of new frame.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                         init_audio_output_frame(AVFrame **frame, int frame_size);
    /**
     * @brief Allocate memory for one picture.
     * @param[in] pix_fmt - Pixel format
     * @param[in] width - Picture width
     * @param[in] height - Picture height
     * @return On success returns new AVFrame or nullptr on error
     */
    AVFrame *                   alloc_picture(AVPixelFormat pix_fmt, int width, int height);
    /**
     * @brief Produce audio dts/pts. This is required because the target codec usually has a different
     * frame size than the source, so the number of packets will not match 1:1.
     * @param[in] pkt - Packet to add dts/pts to.
     */
    void                        produce_audio_dts(AVPacket * pkt);
#if LAVC_NEW_PACKET_INTERFACE
    /**
     * This does not quite work like avcodec_decode_audio4/avcodec_decode_video2.
     * There is the following difference: if you got a frame, you must call
     * it again with pkt=nullptr. pkt==nullptr is treated differently from pkt->size==0
     * (pkt==nullptr means get more output, pkt->size==0 is a flush/drain packet)
     * @return On success returns 0. On error, returns a negative AVERROR value.
     */
    int                         decode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, const AVPacket *pkt);
#endif
    /**
     * @brief Encode one frame worth of audio to the output file.
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
     * @brief Encode frame to image
     * @param[in] frame - Video frame to encode
     * @param[out] data_present - Set to 1 if data was encoded. 0 if not.
     * @return On success returns 0. On error, returns a negative AVERROR value.
     */
    int                         encode_image_frame(const AVFrame *frame, int *data_present);
    /**
     * @brief Load one audio frame from the FIFO buffer, encode and write it to the output file.
     * @param[in] frame_size - Size of frame.
     * @return On success returns 0. On error, returns a negative AVERROR value.
     */
    int                         load_encode_and_write(int frame_size);
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
     * @param[in] codec_context - AVCodecContext object of output video.
     * @param[in] pix_fmt - Output stream pixel format.
     * @param[in] avg_frame_rate - Average output stream frame rate.
     * @param[in] time_base - Output stream time base.
     * @return Returns 0 if OK, or negative AVERROR value.
     */
    int                         init_deinterlace_filters(AVCodecContext *codec_context, AVPixelFormat pix_fmt, const AVRational &avg_frame_rate, const AVRational &time_base);
    /**
     * @brief Send video frame to the filters.
     * @param[in] srcframe - Input video frame.
     * @param[in] ret - 0 if OK, or negative AVERROR value.
     * @return Pointer to video frame. May be a simple pointer copy of srcframe.
     */
    AVFrame *                   send_filters(AVFrame *srcframe, int &ret);
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
     * @brief Purge FIFO buffers and report lost packet.
     */
    void                        purge_fifos();
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

    // Hardwar de/encoding
    static enum AVPixelFormat   hwdevice_get_vaapi_format(__attribute__((unused)) AVCodecContext *ctx,
                                                        const enum AVPixelFormat *pix_fmts);
    int                         hwdevice_ctx_create();
    int                         hwdevice_ctx_add_ref(AVCodecContext *input_codec_ctx);
    void                        hwdevice_ctx_free();
    int                         hwframe_ctx_set(AVCodecContext *encoder_ctx, AVCodecContext *decoder_ctx, AVBufferRef *hw_device_ctx);
    int                         hwframe_transfer_data(AVCodecContext *ctx, AVFrame ** hw_frame, const AVFrame *sw_frame);

private:
    FileIO *                    m_fileio;                   /**< @brief FileIO object of input file */
    bool                        m_close_fileio;             /**< @brief If we own the FileIO object, we may close it in the end. */
    time_t                      m_mtime;                    /**< @brief Modified time of input file */
    std::recursive_mutex        m_mutex;                    /**< @brief Access mutex */
    std::queue<uint32_t>        m_seek_to_fifo;             /**< @brief Stack of seek requests. Will be processed FIFO */
    volatile uint32_t           m_last_seek_frame_no;       /**< @brief If not 0, this is the last frame that we seeked to. Video sources only. */
    bool                        m_have_seeked;              /**< @brief After seek operations this is set to make sure the trancoding result is marked RESULTCODE_INCOMPLETE to start transcoding over next access to fill the gaps. */
    bool                        m_skip_next_frame;          /**< @brief After seek, skip next video frame */
    bool                        m_is_video;                 /**< @brief true if input is a video file */

    // Audio conversion and buffering
    AVSampleFormat              m_cur_sample_fmt;           /**< @brief Currently selected audio sample format */
    int                         m_cur_sample_rate;          /**< @brief Currently selected audio sample rate */
    uint64_t                    m_cur_channel_layout;       /**< @brief Currently selected audio channel layout */
#if LAVR_DEPRECATE
    SwrContext *                m_audio_resample_ctx;       /**< @brief SwResample context for audio resampling */
#else
    AVAudioResampleContext *    m_audio_resample_ctx;       /**< @brief AVResample context for audio resampling */
#endif
    AVAudioFifo *               m_audio_fifo;               /**< @brief Audio sample FIFO */

    // Video conversion and buffering
    SwsContext *                m_sws_ctx;                  /**< @brief Context for video filtering */
    AVFilterContext *           m_buffer_sink_context;      /**< @brief Video filter sink context */
    AVFilterContext *           m_buffer_source_context;    /**< @brief Video filter source context */
    AVFilterGraph *             m_filter_graph;             /**< @brief Video filter graph */
    std::queue<AVFrame*>        m_video_fifo;               /**< @brief Video frame FIFO */
    int64_t                     m_pts;                      /**< @brief Generated PTS */
    int64_t                     m_pos;                      /**< @brief Generated position */

    INPUTFILE                   m_in;                       /**< @brief Input file information */
    OUTPUTFILE                  m_out;                      /**< @brief Output file information */

    uint32_t                    m_current_segment;          /**< @brief HLS only: Segment file number currently being encoded */

    // If the audio and/or video stream is copied, packets will be stuffed into the packet queue.
    bool                        m_copy_audio;               /**< @brief If true, copy audio stream from source to target (just remux, no recode). */
    bool                        m_copy_video;               /**< @brief If true, copy video stream from source to target (just remux, no recode). */

    FFmpegfs_Format *           m_current_format;           /**< @brief Currently used output format(s) */

    Buffer *                    m_buffer;                   /**< @brief Pointer to cache buffer object */

    bool                        m_reset_pts;                /**< @brief We have to reset audio/video pts to the new position */
    uint32_t                    m_fake_frame_no;            /**< @brief The MJEPG codec requires monotonically growing PTS values so we fake some to avoid them going backwards after seeks */

    static const PRORES_BITRATE m_prores_bitrate[];         /**< @brief ProRes bitrate table. Used for file size prediction. */

    // Hardware accelerarion
    bool						m_hwaccel;                  /**< @brief Enable hardware acceleration */
    bool                        m_hwaccel_decoder;          /**< @brief Enable hardware acceleration for decoder, requires m_hwaccel == true */
    bool                        m_encoder_initialised;      /**< @brief True if encoder has been initilialised */
    bool						m_hwdecode;                 /**< @brief True if decoder runs in hardware */
    AVBufferRef *               m_hw_device_ctx;            /**< @brief Hardware acceleration device context */
};

#endif // FFMPEG_TRANSCODER_H
