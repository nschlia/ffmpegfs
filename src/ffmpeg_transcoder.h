/*
 * FFmpeg decoder class header for ffmpegfs
 *
 * Copyright (C) 2017-2018 Norbert Schlia (nschlia@oblivion-software.de)
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef FFMPEG_TRANSCODER_H
#define FFMPEG_TRANSCODER_H

#pragma once

#include "ffmpeg_base.h"
#include "id3v1tag.h"
#include "ffmpegfs.h"
#include "fileio.h"

#include <queue>

using namespace std;

class Buffer;
#if LAVR_DEPRECATE
struct SwrContext;
#else
struct AVAudioResampleContext;
#endif
struct SwsContext;
struct AVFilterContext;
struct AVFilterContext;
struct AVFilterGraph;
struct AVAudioFifo;

class FFMPEG_Transcoder : public FFMPEG_Base
{
public:
#define OPT_CODEC       0x80000000

#define OPT_ALL         0x00000000  // All files
#define OPT_AUDIO       0x00000001  // For audio only files
#define OPT_VIDEO       0x00000002  // For videos (not audio only)

    typedef struct _tagPROFILE_OPTION
    {
        const char *            m_key;
        const char *            m_value;
        const int               m_flags;
        const int               m_options;
    } PROFILE_OPTION, *LPPROFILE_OPTION;
    typedef PROFILE_OPTION const * LPCPROFILE_OPTION;

    typedef struct _tagPROFILE_LIST
    {
        FILETYPE                m_filetype;
        PROFILE                 m_profile;
        LPCPROFILE_OPTION       m_option_codec;
        LPCPROFILE_OPTION       m_option_format;
    } PROFILE_LIST, *LPPROFILE_LIST;
    typedef PROFILE_LIST const * LPCPROFILE_LIST;

    struct STREAMREF
    {
        STREAMREF() :
            m_pCodec_ctx(nullptr),
            m_pStream(nullptr),
            m_nStream_idx(INVALID_STREAM)
        {}

        AVCodecContext *        m_pCodec_ctx;
        AVStream *              m_pStream;
        int                     m_nStream_idx;
    };

    // Input file
    struct INPUTFILE
    {
        INPUTFILE() :
            m_file_type(FILETYPE_UNKNOWN),
            m_filename("unset"),
            m_pFormat_ctx(nullptr)
        {}

        FILETYPE                m_file_type;
        string                  m_filename;

        AVFormatContext *       m_pFormat_ctx;

        STREAMREF               m_audio;
        STREAMREF               m_video;

        vector<STREAMREF>       m_aAlbumArt;
    };

    // Output file
    struct OUTPUTFILE
    {
        OUTPUTFILE() :
            m_file_type(FILETYPE_UNKNOWN),
            m_filename("unset"),
            m_pFormat_ctx(nullptr),
            m_nAudio_pts(0),
            m_video_start_pts(0),
            m_last_mux_dts(AV_NOPTS_VALUE)
        {}

        FILETYPE                m_file_type;
        string                  m_filename;

        AVFormatContext *       m_pFormat_ctx;

        STREAMREF               m_audio;
        STREAMREF               m_video;

        vector<STREAMREF>       m_aAlbumArt;

        int64_t                 m_nAudio_pts;           // Global timestamp for the audio frames
        int64_t                 m_video_start_pts;      // Video start PTS
        int64_t                 m_last_mux_dts;         // Last muxed DTS

        ID3v1                   m_id3v1;                // mp3 only, can be referenced at any time
    };

public:
    FFMPEG_Transcoder();
    virtual ~FFMPEG_Transcoder();
    bool is_open() const;
    int open_input_file(LPCVIRTUALFILE virtualfile);
    int open_output_file(Buffer* buffer);
    int process_single_fr(int & status);
    int encode_finish();
    void close();

    time_t mtime() const;
    size_t predict_filesize();

    const ID3v1 * id3v1tag() const;

    virtual const char *filename() const;
    virtual const char *destname() const;
    
protected:
    bool is_video() const;
    void limit_video_size(AVCodecContext *output_codec_ctx);
    int update_codec(void *opt, LPCPROFILE_OPTION mp4_opt) const;
    int prepare_mp4_codec(void *opt) const;
    int prepare_webm_codec(void *opt) const;
    int add_stream(AVCodecID codec_id);    
    int add_albumart_stream(const AVCodecContext *input_codec_ctx);
    int add_albumart_frame(AVStream *output_stream, AVPacket *pkt_in);
    int open_output_filestreams(Buffer *buffer);
    void copy_metadata(AVDictionary **metadata_out, const AVDictionary *metadata_in);
    int process_metadata();
    int process_albumarts();
    int init_resampler();
    int init_fifo();
    int update_format(AVDictionary** dict, LPCPROFILE_OPTION option) const;
    int prepare_mp4_format(AVDictionary **dict) const;
    int prepare_webm_format(AVDictionary **dict) const;
    int write_output_file_header();
    int decode_audio_frame(AVPacket *pkt, int *decoded);
    int decode_video_frame(AVPacket *pkt, int *decoded);
    int decode_frame(AVPacket *pkt);
    int init_converted_samples(uint8_t ***converted_input_samples, int frame_size);
    int convert_samples(uint8_t **input_data, const int in_samples, uint8_t **converted_data, int *out_samples);
    int add_samples_to_fifo(uint8_t **converted_input_samples, const int frame_size);
    int flush_frames(int stream_index);
    int read_decode_convert_and_store(int *finished);
    int init_audio_output_frame(AVFrame **frame, int frame_size);
    AVFrame *alloc_picture(AVPixelFormat pix_fmt, int width, int height);
    void produce_audio_dts(AVPacket * pkt, int64_t *pts);
#if LAVC_NEW_PACKET_INTERFACE
    int decode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt) const;
#endif
    int encode_audio_frame(AVFrame *frame, int *data_present);
    int encode_video_frame(AVFrame *frame, int *data_present);
    int load_encode_and_write(int frame_size);
    int write_output_file_trailer();

    static int input_read(void * opaque, unsigned char * data, int size);
    static int output_write(void * opaque, unsigned char * data, int size);
    static int64_t seek(void * opaque, int64_t offset, int whence);

    size_t predict_filesize(const char * filename, double duration, BITRATE input_audio_bit_rate, int input_sample_rate, BITRATE input_video_bit_rate, bool is_video) const;
    bool get_output_sample_rate(int input_sample_rate, int max_sample_rate, int * output_sample_rate) const;
    bool get_output_bit_rate(BITRATE input_bit_rate, BITRATE max_bit_rate, BITRATE * output_bit_rate) const;
    double get_aspect_ratio(int width, int height, const AVRational & sample_aspect_ratio) const;

#ifndef USING_LIBAV
    int init_filters(AVCodecContext *pCodecContext, AVStream *pStream);
    AVFrame *send_filters(AVFrame *srcframe, int &ret);
    void free_filters();
#endif // !USING_LIBAV

private:
    fileio *                    m_fileio;
    time_t                      m_mtime;
    size_t                      m_predicted_size;         // Use this as the size instead of computing it.
    bool                        m_is_video;

    // Audio conversion and buffering
#if LAVR_DEPRECATE
    SwrContext *                m_pAudio_resample_ctx;
#else
    AVAudioResampleContext *    m_pAudio_resample_ctx;
#endif
    AVAudioFifo *               m_pAudioFifo;

    // Video conversion and buffering
    SwsContext *                m_pSws_ctx;
    AVFilterContext *           m_pBufferSinkContext;
    AVFilterContext *           m_pBufferSourceContext;
    AVFilterGraph *             m_pFilterGraph;
    queue<AVFrame*>             m_VideoFifo;
    int64_t                     m_pts;
    int64_t                     m_pos;

    INPUTFILE                   m_in;
    OUTPUTFILE                  m_out;

    static const PROFILE_LIST   m_profile[];
};

#endif // FFMPEG_TRANSCODER_H
