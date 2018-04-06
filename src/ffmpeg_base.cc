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

#include <assert.h>

FFMPEG_Base::FFMPEG_Base()
{
}

FFMPEG_Base::~FFMPEG_Base()
{
}

// Open codec context for desired media type
int FFMPEG_Base::open_codec_context(int *stream_idx, AVCodecContext **avctx, AVFormatContext *fmt_ctx, AVMediaType type, const char *filename) const
{
    int ret;

    ret = av_find_best_stream(fmt_ctx, type, INVALID_STREAM, INVALID_STREAM, NULL, 0);
    if (ret < 0)
    {
        if (ret != AVERROR_STREAM_NOT_FOUND)    // Not an error
        {
            ffmpegfs_error("Could not find %s stream in input file '%s' (error '%s').", get_media_type_string(type), filename, ffmpeg_geterror(ret).c_str());
        }
        return ret;
    }
    else
    {
        int stream_index;
        AVCodecContext *dec_ctx = NULL;
        AVCodec *dec = NULL;
        AVDictionary *opts = NULL;
        AVStream *in_stream;
        AVCodecID codec_id = AV_CODEC_ID_NONE;

        stream_index = ret;
        in_stream = fmt_ctx->streams[stream_index];

        // Init the decoders, with or without reference counting
        // av_dict_set_with_check(&opts, "refcounted_frames", refcount ? "1" : "0", 0);

#if (LIBAVCODEC_VERSION_MICRO >= 100 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 57, 64, 101 ) )
        // allocate a new decoding context
        dec_ctx = avcodec_alloc_context3(dec);
        if (!dec_ctx)
        {
            ffmpegfs_error("Could not allocate a decoding context.");
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
            ffmpegfs_error("Failed to find %s input codec.", get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        dec_ctx->codec_id = dec->id;

        ret = avcodec_open2(dec_ctx, dec, &opts);

        av_dict_free(&opts);

        if (ret < 0)
        {
            ffmpegfs_error("Failed to open %s input codec (error '%s').", get_media_type_string(type), ffmpeg_geterror(ret).c_str());
            return ret;
        }

        ffmpegfs_debug("Successfully opened %s input codec.", get_codec_name(codec_id));

        *stream_idx = stream_index;

        *avctx = dec_ctx;
    }

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
        ffmpegfs_error("Could not allocate frame for '%s'.", filename);
        return AVERROR(ENOMEM);
    }
    return 0;
}
