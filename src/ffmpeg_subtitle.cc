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
 * @file ffmpeg_subtitle.cc
 * @brief FFmpeg_Subtitle class implementation
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
#include <libavcodec/avcodec.h>
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

#include "ffmpeg_utils.h"
#include "ffmpeg_subtitle.h"

FFmpeg_Subtitle::FFmpeg_Subtitle(int stream_index) :
    std::shared_ptr<AVSubtitle>(alloc_subtitle(), &FFmpeg_Subtitle::delete_subtitle),
    m_res(0),
    m_stream_idx(stream_index)
{
  m_res = (get() != nullptr) ? 0 : AVERROR(ENOMEM);
}

FFmpeg_Subtitle::~FFmpeg_Subtitle()
{
}

void FFmpeg_Subtitle::unref() noexcept
{
    reset();
}

int FFmpeg_Subtitle::res() const
{
    return m_res;
}

FFmpeg_Subtitle::operator AVSubtitle*()
{
    return get();
}

FFmpeg_Subtitle::operator const AVSubtitle*() const
{
    return get();
}

AVSubtitle* FFmpeg_Subtitle::operator->()
{
    return get();
}

AVSubtitle* FFmpeg_Subtitle::alloc_subtitle()
{
    return reinterpret_cast<AVSubtitle *>(av_mallocz(sizeof(AVSubtitle)));
}

void FFmpeg_Subtitle::delete_subtitle(AVSubtitle *subtitle)
{
    if (subtitle != nullptr)
    {
        avsubtitle_free(subtitle);
        av_free(subtitle);
    }
}
