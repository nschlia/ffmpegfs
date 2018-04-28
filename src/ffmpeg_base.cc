/*
 * FFmpeg decoder base class source for ffmpegfs
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

#include "ffmpeg_base.h"
#include "transcode.h"

// Disable annoying warnings outside our code
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#ifdef __GNUC__
#  include <features.h>
#  if __GNUC_PREREQ(5,0) || defined(__clang__)
// GCC >= 5.0
#     pragma GCC diagnostic ignored "-Wfloat-conversion"
#  elif __GNUC_PREREQ(4,8)
// GCC >= 4.8
#  else
#     error("GCC < 4.8 not supported");
#  endif
#endif
#ifdef __cplusplus
extern "C" {
#endif
#include <libavutil/opt.h>
#ifdef __cplusplus
}
#endif
#pragma GCC diagnostic pop

FFMPEG_Base::FFMPEG_Base()
{
}

FFMPEG_Base::~FFMPEG_Base()
{
}

// Open codec context for desired media type
int FFMPEG_Base::open_bestmatch_codec_context(AVCodecContext **avctx, int *stream_idx, AVFormatContext *fmt_ctx, AVMediaType type, const char *filename) const
{
    int ret;

    ret = av_find_best_stream(fmt_ctx, type, INVALID_STREAM, INVALID_STREAM, NULL, 0);
    if (ret < 0)
    {
        if (ret != AVERROR_STREAM_NOT_FOUND)    // Not an error
        {
            ffmpegfs_error("%s * Could not find %s stream in input file (error '%s').", filename, get_media_type_string(type), ffmpeg_geterror(ret).c_str());
        }
        return ret;
    }

    *stream_idx = ret;

    return open_codec_context(avctx, *stream_idx, fmt_ctx, type, filename);
}

int FFMPEG_Base::open_codec_context(AVCodecContext **avctx, int stream_idx, AVFormatContext *fmt_ctx, AVMediaType type, const char *filename) const
{
    AVCodecContext *dec_ctx = NULL;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    AVStream *in_stream;
    AVCodecID codec_id = AV_CODEC_ID_NONE;
    int ret;

    in_stream = fmt_ctx->streams[stream_idx];

    // Init the decoders, with or without reference counting
    // av_dict_set_with_check(&opts, "refcounted_frames", refcount ? "1" : "0", 0);

#if LAVF_DEP_AVSTREAM_CODEC
    // allocate a new decoding context
    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx)
    {
        ffmpegfs_error("%s * Could not allocate a decoding context.", filename);
        return AVERROR(ENOMEM);
    }

    // initialise the stream parameters with demuxer information
    ret = avcodec_parameters_to_context(dec_ctx, in_stream->codecpar);
    if (ret < 0)
    {
        return ret;
    }

    codec_id = in_stream->codecpar->codec_id;
#else
    dec_ctx = in_stream->codec;

    codec_id = dec_ctx->codec_id;
#endif

    // Find a decoder for the stream.
    dec = avcodec_find_decoder(codec_id);
    if (!dec)
    {
        ffmpegfs_error("%s * Failed to find %s input codec.", filename, get_media_type_string(type));
        return AVERROR(EINVAL);
    }

    dec_ctx->codec_id = dec->id;

    ret = avcodec_open2(dec_ctx, dec, &opts);

    av_dict_free(&opts);

    if (ret < 0)
    {
        ffmpegfs_error("%s * Failed to open %s input codec (error '%s').", filename, get_media_type_string(type), ffmpeg_geterror(ret).c_str());
        return ret;
    }

    ffmpegfs_debug("%s * Opened %s input codec.", filename, get_codec_name(codec_id));

    *avctx = dec_ctx;

    return 0;
}

// Initialise one data packet for reading or writing.
void FFMPEG_Base::init_packet(AVPacket *packet) const
{
    av_init_packet(packet);
    // Set the packet data and size so that it is recognised as being empty.
    packet->data = NULL;
    packet->size = 0;
}

// Initialise one frame for reading from the input file
int FFMPEG_Base::init_frame(AVFrame **frame, const char *filename) const
{
    if (!(*frame = av_frame_alloc()))
    {
        ffmpegfs_error("%s * Could not allocate frame.", filename);
        return AVERROR(ENOMEM);
    }
    return 0;
}

void FFMPEG_Base::streamSetup(AVCodecContext *output_codec_ctx, AVStream* output_stream, AVRational frame_rate) const
{
    AVRational time_base;

    if (!frame_rate.num || !frame_rate.den)
    {
        frame_rate                              = { .num = 25, .den = 1 };
        ffmpegfs_warning("No information about the input framerate is available. Falling back to a default value of 25fps for output stream.");
    }

    // timebase: This is the fundamental unit of time (in seconds) in terms
    // of which frame timestamps are represented. For fixed-fps content,
    // timebase should be 1/framerate and timestamp increments should be
    // identical to 1.
    //time_base                                 = m_in.m_pVideo_stream->time_base;

    // tbn
    if (output_codec_ctx->codec_id == AV_CODEC_ID_THEORA)
    {
        // Strange, but Theora seems to need it this way...
        time_base                               = av_inv_q(frame_rate);
    }
    else
    {
        time_base                               = { .num = 1, .den = 90000 };
    }

    // tbc
    output_stream->time_base                    = time_base;
    output_codec_ctx->time_base                 = time_base;

    // tbc
#if LAVF_DEP_AVSTREAM_CODEC
    //output_stream->codecpar->time_base        = time_base;
#else
    output_stream->codec->time_base             = time_base;
#endif

#ifndef USING_LIBAV
    // tbr
    // output_stream->r_frame_rate              = m_in.m_pVideo_stream->r_frame_rate;
    // output_stream->r_frame_rate              = { .num = 25, .den = 1 };

    // fps
    output_stream->avg_frame_rate               = frame_rate;
    output_codec_ctx->framerate                 = frame_rate;
#endif

    // At this moment the output format must be AV_PIX_FMT_YUV420P;
    output_codec_ctx->pix_fmt                   = AV_PIX_FMT_YUV420P;

    if (output_codec_ctx->codec_id == AV_CODEC_ID_H264)
    {
        //output_codec_ctx->flags2 |= AV_CODEC_FLAG2_FAST;
        //output_codec_ctx->flags2 = AV_CODEC_FLAG2_FASTPSKIP;
        //output_codec_ctx->profile = FF_PROFILE_H264_HIGH;
        //output_codec_ctx->level = 31;

        // -profile:v baseline -level 3.0
        //av_opt_set(output_codec_ctx->priv_data, "profile", "baseline", 0);
        //av_opt_set(output_codec_ctx->priv_data, "level", "3.0", 0);

        // -profile:v high -level 3.1
        //av_opt_set(output_codec_ctx->priv_data, "profile", "high", 0);
        //av_opt_set(output_codec_ctx->priv_data, "level", "3.1", 0);

        // Set speed (changes profile!)
        av_opt_set(output_codec_ctx->priv_data, "preset", "ultrafast", 0);
        //av_opt_set(output_codec_ctx->priv_data, "preset", "veryfast", 0);
        //av_opt_set(output_codec_ctx->priv_data, "tune", "zerolatency", 0);

        //if (!av_dict_get((AVDictionary*)output_codec_ctx->priv_data, "threads", NULL, 0))
        //{
        //  ffmpegfs_error("%s * Setting threads to auto for codec %s.", destname(), get_codec_name(codec_id));
        //  av_dict_set_with_check((AVDictionary**)&output_codec_ctx->priv_data, "threads", "auto", 0, destname());
        //}
    }
}
