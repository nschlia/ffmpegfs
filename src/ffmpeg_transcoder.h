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
#include "wave.h"
#include "transcode.h"

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

    typedef struct _tagMP4_OPTION
    {
        const char *key;
        const char *value;
        const int flags;
        const int options;
    } MP4_OPTION, *LPMP4_OPTION;
    typedef MP4_OPTION const * LPCMP4_OPTION;

    typedef struct _tagMP4_PROFILE
    {
        PROFILE                 m_profile;
        LPCMP4_OPTION           m_opt_codec;
        LPCMP4_OPTION           m_opt_format;

    } MP4_PROFILE, *LPMP4_PROFILE;
    typedef MP4_PROFILE const * LPCMP4_PROFILE;

    struct STREAMREF
    {
        STREAMREF() :
            m_pCodec_ctx(NULL),
            m_pStream(NULL),
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
            m_pFormat_ctx(NULL)
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
            m_pFormat_ctx(NULL),
            m_nAudio_pts(0),
            m_video_start_pts(0),
            m_last_mux_dts((int64_t)AV_NOPTS_VALUE)
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
    ~FFMPEG_Transcoder();
    bool is_open() const;
    int open_input_file(const char* filename);
    int open_output_file(Buffer* buffer);
    int process_single_fr(int & status);
    int encode_finish();
    void close();

    time_t mtime() const;
    size_t calculate_size();

    const ID3v1 * id3v1tag() const;

    const char *filename() const;
    const char *destname() const;

    static const string & get_destname(string *destname, const string & filename);
    
protected:
    bool is_video() const;
    void limit_video_size(AVCodecContext *output_codec_ctx);
    int update_codec(void *opt, LPCMP4_OPTION mp4_opt) const;
    int prepare_mp4_codec(void *opt) const;
    int add_stream(AVCodecID codec_id);    
    int add_albumart_stream(const AVCodecContext *input_codec_ctx);
    int add_albumart_frame(AVStream *output_stream, AVPacket *pkt_in);
    int open_output_filestreams(Buffer *buffer);
    void copy_metadata(AVDictionary **metadata_out, const AVDictionary *metadata_in);
    int process_metadata();
    int process_albumarts();
    int init_resampler();
    int init_fifo();
    int update_format(AVDictionary** dict, LPCMP4_OPTION opt) const;
    int prepare_mp4_format(AVDictionary **dict) const;
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

    static int write_packet(void * pOpaque, unsigned char * pBuffer, int nBufSize);
    static int64_t seek(void * pOpaque, int64_t i4Offset, int nWhence);

    bool get_output_sample_rate(AVStream *stream, int max_sample_rate, int *sample_rate) const;
#if !defined(USING_LIBAV) && (LIBAVUTIL_VERSION_MAJOR > 54)
    bool get_output_bit_rate(AVStream *stream, int64_t max_bit_rate, int64_t * bit_rate) const;
#else // USING_LIBAV
    bool get_output_bit_rate(AVStream *stream, int max_bit_rate, int * bit_rate) const;
#endif
    double get_aspect_ratio(int width, int height, const AVRational & sample_aspect_ratio) const;

    int init_filters(AVCodecContext *pCodecContext, AVStream *pStream);
    AVFrame *send_filters(AVFrame *srcframe, int &ret);
    void free_filters();

private:
    time_t                      m_mtime;
    size_t                      m_nCalculated_size;         // Use this as the size instead of computing it.
    bool                        m_bIsVideo;

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

    static const MP4_PROFILE    m_profile[];
};

#endif // FFMPEG_TRANSCODER_H
