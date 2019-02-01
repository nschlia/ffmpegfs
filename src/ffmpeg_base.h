/*
 * FFmpeg decoder base class source for FFmpegfs
 *
 * Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
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

#ifndef FFMPEG_BASE_H
#define FFMPEG_BASE_H

#pragma once

#include "ffmpeg_utils.h"

#define INVALID_STREAM  -1

class FFmpeg_Base
{
public:
    FFmpeg_Base();
    virtual ~FFmpeg_Base();

protected:
    int         open_bestmatch_codec_context(AVCodecContext **avctx, int *stream_idx, AVFormatContext *fmt_ctx, AVMediaType type, const char *filename = nullptr) const;
    int         open_codec_context(AVCodecContext **avctx, int stream_idx, AVFormatContext *fmt_ctx, AVMediaType type, const char *filename = nullptr) const;
    void        init_packet(AVPacket *packet) const;
    int         init_frame(AVFrame **frame, const char *filename = nullptr) const;
    void        video_stream_setup(AVCodecContext *output_codec_ctx, AVStream* output_stream, AVCodecContext *input_codec_ctx, AVRational frame_rate) const;
    int         av_dict_set_with_check(AVDictionary **pm, const char *key, const char *value, int flags, const char *filename = nullptr) const;
    int         av_opt_set_with_check(void *obj, const char *key, const char *value, int flags, const char *filename = nullptr) const;
    void        video_info(bool out_file, const AVFormatContext *format_ctx, const AVStream *stream) const;
    void        audio_info(bool out_file, const AVFormatContext *format_ctx, const AVStream *stream) const;
    std::string get_pix_fmt_name(AVPixelFormat pix_fmt) const;
    std::string get_sample_fmt_name(AVSampleFormat sample_fmt) const;
    std::string get_channel_layout_name(int nb_channels, uint64_t channel_layout) const;

    virtual const char *filename() const = 0;
    virtual const char *destname() const = 0;

private:
};

#endif // FFMPEG_BASE_H
