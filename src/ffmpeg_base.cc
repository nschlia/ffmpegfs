/*
 * Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file ffmpeg_base.cc
 * @brief FFmpeg_Base class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifdef __cplusplus
extern "C" {
#endif
// Disable annoying warnings outside our code
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/pixdesc.h>
#include <libavutil/channel_layout.h>
#include <libavcodec/avcodec.h>
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

#include "ffmpeg_base.h"
#include "transcode.h"
#include "logging.h"

FFmpeg_Base::FFmpeg_Base()
    : m_virtualfile(nullptr)
{
}

FFmpeg_Base::~FFmpeg_Base()
{
}

#if !LAVC_DEP_AV_INIT_PACKET
void FFmpeg_Base::init_packet(AVPacket *pkt) const
{
    av_init_packet(pkt);
    // Set the packet data and size so that it is recognised as being empty.
    pkt->data = nullptr;
    pkt->size = 0;
}
#endif // !LAVC_DEP_AV_INIT_PACKET

int FFmpeg_Base::init_frame(AVFrame **frame, const char *filename) const
{
    *frame = av_frame_alloc();
    if (*frame == nullptr)
    {
        Logging::error(filename, "Could not allocate frame.");
        return AVERROR(ENOMEM);
    }
    return 0;
}

void FFmpeg_Base::video_stream_setup(AVCodecContext *output_codec_ctx, AVStream* output_stream, AVCodecContext *input_codec_ctx, AVRational framerate, AVPixelFormat  enc_hw_pix_fmt) const
{
    AVRational time_base_tbn;
    AVRational time_base_tbc;

    if (!framerate.num || !framerate.den)
    {
        framerate.num = 25;
        framerate.den = 1;
        Logging::warning(nullptr, "No information about the input framerate is available. Falling back to a default value of 25fps for output stream.");
    }

    // timebase: This is the fundamental unit of time (in seconds) in terms
    // of which frame timestamps are represented. For fixed-fps content,
    // timebase should be 1/framerate and timestamp increments should be
    // identical to 1.
    //time_base                                 = m_in.m_pVideo_stream->time_base;

    // tbn: must be set differently for the target format. Otherwise produces strange results.
    switch (output_codec_ctx->codec_id)
    {
    case AV_CODEC_ID_THEORA:        // ogg
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
    {
        time_base_tbn                           = av_inv_q(framerate);
        time_base_tbc                           = time_base_tbn;
        break;
    }
    case AV_CODEC_ID_VP9:           // webm
    {
        time_base_tbn.num                       = 1;
        time_base_tbn.den                       = 1000;
        time_base_tbc                           = time_base_tbn;
        break;
    }
    case AV_CODEC_ID_H264:          // h264
    case AV_CODEC_ID_H265:          // h265
    {
        time_base_tbn.num                       = 1;
        time_base_tbn.den                       = 90000;
        time_base_tbc                           = av_inv_q(framerate);
        break;
    }
    default:                        // mp4 and all others
    {
        time_base_tbn.num                       = 1;
        time_base_tbn.den                       = 90000;
        time_base_tbc                           = time_base_tbn;
        break;
    }
    }

    // tbn
    output_stream->time_base                    = time_base_tbn;
    // tbc
    output_codec_ctx->time_base                 = time_base_tbc;

    // tbr
    // output_stream->r_frame_rate              = m_in.m_pVideo_stream->r_frame_rate;
    output_stream->r_frame_rate                 = framerate;

    // fps
    output_stream->avg_frame_rate               = framerate;
    // output_codec_ctx->framerate                 = framerate;

    if (enc_hw_pix_fmt == AV_PIX_FMT_NONE)
    {
        // Automatic pix_fmt selection
        int loss = 0;

        AVPixelFormat  src_pix_fmt                  = input_codec_ctx->pix_fmt;
        if (output_codec_ctx->codec->pix_fmts != nullptr)
        {
            int alpha = 0;
            enc_hw_pix_fmt = avcodec_find_best_pix_fmt_of_list(output_codec_ctx->codec->pix_fmts, src_pix_fmt, alpha, &loss);
        }

        if (enc_hw_pix_fmt == AV_PIX_FMT_NONE)
        {
            // Fail safe if avcodec_find_best_pix_fmt_of_list has no idea what to use.
            switch (output_codec_ctx->codec_id)
            {
            case AV_CODEC_ID_PRORES:       // mov/prores
            {
                //  yuva444p10le
                // ProRes 4:4:4 if the source is RGB and ProRes 4:2:2 if the source is YUV.
                enc_hw_pix_fmt                         = AV_PIX_FMT_YUV422P10LE;
                break;
            }
            default:                        // all others
            {
                // At this moment the output format must be AV_PIX_FMT_YUV420P;
                enc_hw_pix_fmt                         = AV_PIX_FMT_YUV420P;
                break;
            }
            }
        }
    }

    output_codec_ctx->pix_fmt                   = enc_hw_pix_fmt;
    output_codec_ctx->gop_size                  = 12;   // emit one intra frame every twelve frames at most
}

int FFmpeg_Base::dict_set_with_check(AVDictionary **pm, const char *key, const char *value, int flags, const char * filename, bool nodelete) const
{
    if (nodelete && !*value)
    {
        return 0;
    }

    int ret = av_dict_set(pm, key, value, flags);

    if (ret < 0)
    {
        Logging::error(filename, "Error setting dictionary option key(%1)='%2' (error '%3').", key, value, ffmpeg_geterror(ret).c_str());
    }

    return ret;
}

int FFmpeg_Base::dict_set_with_check(AVDictionary **pm, const char *key, int64_t value, int flags, const char * filename, bool nodelete) const
{
    if (nodelete && !value)
    {
        return 0;
    }

    int ret = av_dict_set_int(pm, key, value, flags);

    if (ret < 0)
    {
        Logging::error(filename, "Error setting dictionary option key(%1)='%2' (error '%3').", key, value, ffmpeg_geterror(ret).c_str());
    }

    return ret;
}

int FFmpeg_Base::opt_set_with_check(void *obj, const char *key, const char *value, int flags, const char * filename) const
{
    int ret = av_opt_set(obj, key, value, flags);

    if (ret < 0)
    {
        Logging::error(filename, "Error setting dictionary option key(%1)='%2' (error '%3').", key, value, ffmpeg_geterror(ret).c_str());
    }

    return ret;
}

void FFmpeg_Base::video_info(bool out_file, const AVFormatContext *format_ctx, const AVStream *stream) const
{
    int64_t duration = AV_NOPTS_VALUE;

    if (stream->duration != AV_NOPTS_VALUE)
    {
        duration = av_rescale_q_rnd(stream->duration, stream->time_base, av_get_time_base_q(), static_cast<AVRounding>(AV_ROUND_UP | AV_ROUND_PASS_MINMAX));
    }

    Logging::debug(out_file ? destname() : filename(), "Video %1: %2@%3 [%4]",
                   out_file ? "out" : "in",
                   get_codec_name(stream->codecpar->codec_id, false),
                   format_bitrate((stream->codecpar->bit_rate != 0) ? stream->codecpar->bit_rate : format_ctx->bit_rate).c_str(),
                   format_duration(duration).c_str());
}

void FFmpeg_Base::audio_info(bool out_file, const AVFormatContext *format_ctx, const AVStream *stream) const
{
    int64_t duration = AV_NOPTS_VALUE;

    if (stream->duration != AV_NOPTS_VALUE)
    {
        duration = av_rescale_q_rnd(stream->duration, stream->time_base, av_get_time_base_q(), static_cast<AVRounding>(AV_ROUND_UP | AV_ROUND_PASS_MINMAX));
    }

    Logging::debug(out_file ? destname() : filename(), "Audio %1: %2@%3 %4 Channels %5 [%6]",
                   out_file ? "out" : "in",
                   get_codec_name(stream->codecpar->codec_id, false),
                   format_bitrate((stream->codecpar->bit_rate != 0) ? stream->codecpar->bit_rate : format_ctx->bit_rate).c_str(),
                   stream->codecpar->channels,
                   format_samplerate(stream->codecpar->sample_rate).c_str(),
                   format_duration(duration).c_str());
}

void FFmpeg_Base::subtitle_info(bool out_file, const AVFormatContext * /*format_ctx*/, const AVStream *stream) const
{
    Logging::debug(out_file ? destname() : filename(), "Subtitle %1: %2",
                   out_file ? "out" : "in",
                   get_codec_name(stream->codecpar->codec_id, false));
}

std::string FFmpeg_Base::get_pix_fmt_name(enum AVPixelFormat pix_fmt)
{
    const char *fmt_name = av_get_pix_fmt_name(pix_fmt);
    return (fmt_name != nullptr ? fmt_name : "none");
}

std::string FFmpeg_Base::get_sample_fmt_name(AVSampleFormat sample_fmt)
{
    return av_get_sample_fmt_name(sample_fmt);
}

std::string FFmpeg_Base::get_channel_layout_name(int nb_channels, uint64_t channel_layout)
{
    char buffer[1024];
    av_get_channel_layout_string(buffer, sizeof(buffer) - 1, nb_channels, channel_layout);
    return buffer;
}

uint32_t FFmpeg_Base::pts_to_frame(AVStream* stream, int64_t pts) const
{
    if (pts == AV_NOPTS_VALUE)
    {
        return 0;
    }
    int64_t start_time = (stream->start_time != AV_NOPTS_VALUE) ? stream->start_time : 0;
    AVRational factor = av_mul_q(stream->r_frame_rate, stream->time_base);
    return static_cast<uint32_t>(av_rescale(pts - start_time, factor.num, factor.den) + 1);
}

int64_t FFmpeg_Base::frame_to_pts(AVStream* stream, uint32_t frame_no) const
{
    int64_t start_time = (stream->start_time != AV_NOPTS_VALUE) ? stream->start_time : 0;
    AVRational factor = av_mul_q(stream->r_frame_rate, stream->time_base);
    return static_cast<uint32_t>(av_rescale(frame_no - 1, factor.den, factor.num) + start_time);
}
