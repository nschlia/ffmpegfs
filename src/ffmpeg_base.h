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

#ifndef FFMPEG_BASE_H
#define FFMPEG_BASE_H

#pragma once

#include "ffmpeg_utils.h"

using namespace std;

#define INVALID_STREAM  -1

class FFMPEG_Base
{
public:
    FFMPEG_Base();
    virtual ~FFMPEG_Base();

protected:
    int open_bestmatch_codec_context(AVCodecContext **avctx, int *stream_idx, AVFormatContext *fmt_ctx, AVMediaType type, const char *filename = NULL) const;
    int open_codec_context(AVCodecContext **avctx, int stream_idx, AVFormatContext *fmt_ctx, AVMediaType type, const char *filename = NULL) const;
    void init_packet(AVPacket *packet) const;
    int init_frame(AVFrame **frame, const char *filename = NULL) const;
    void stream_setup(AVCodecContext *output_codec_ctx, AVStream* output_stream, AVRational frame_rate) const;
    int av_dict_set_with_check(AVDictionary **pm, const char *key, const char *value, int flags, const char *filename = NULL) const;
    int av_opt_set_with_check(void *obj, const char *key, const char *value, int flags, const char *filename = NULL) const;

private:
};

#endif // FFMPEG_BASE_H
