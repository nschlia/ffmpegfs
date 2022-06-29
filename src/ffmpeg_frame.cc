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
 * @file ffmpeg_frame.cc
 * @brief FFmpeg_Frame class implementation
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
#include "ffmpeg_frame.h"

FFmpeg_Frame::FFmpeg_Frame(int stream_index) :
    m_frame(av_frame_alloc()),
    m_res(0),
    m_stream_idx(stream_index)
{
    m_res = (m_frame != nullptr) ? 0 : AVERROR(ENOMEM);
}

FFmpeg_Frame::FFmpeg_Frame(const FFmpeg_Frame& frame) :
    m_frame(nullptr),
    m_res(0),
    m_stream_idx(frame.m_stream_idx)
{
    if (frame.m_frame != nullptr)
    {
        m_frame = av_frame_clone(frame.m_frame);

        m_res = (m_frame != nullptr) ? 0 : AVERROR(ENOMEM);
    }
    else
    {
        m_res = AVERROR(EINVAL);
    }
}

FFmpeg_Frame::FFmpeg_Frame(const AVFrame * frame) :
    m_frame(nullptr),
    m_res(0),
    m_stream_idx(INVALID_STREAM)
{
    if (frame != nullptr)
    {
        m_frame = av_frame_clone(frame);

        m_res = (m_frame != nullptr) ? 0 : AVERROR(ENOMEM);
    }
    else
    {
        m_res = AVERROR(EINVAL);
    }
}

FFmpeg_Frame::~FFmpeg_Frame()
{
    free();
}

AVFrame* FFmpeg_Frame::clone()
{
    return av_frame_clone(m_frame);
}

void FFmpeg_Frame::unref()
{
    if (m_frame != nullptr)
    {
        av_frame_unref(m_frame);
    }
}

void FFmpeg_Frame::free()
{
    if (m_frame != nullptr)
    {
        av_frame_free(&m_frame);
        m_frame = nullptr;
    }
}

int FFmpeg_Frame::res() const
{
    return m_res;
}

AVFrame* FFmpeg_Frame::get()
{
    return m_frame;
}

FFmpeg_Frame::operator AVFrame*()
{
    return m_frame;
}

FFmpeg_Frame::operator const AVFrame*() const
{
    return m_frame;
}

AVFrame* FFmpeg_Frame::operator->()
{
    return m_frame;
}

FFmpeg_Frame& FFmpeg_Frame::operator=(const FFmpeg_Frame & frame) noexcept
{
    // Do self assignment check
    if (this != &frame && m_frame != frame.m_frame)
    {
        AVFrame *new_frame = av_frame_clone(frame.m_frame);

        free();

        m_frame = new_frame;

        m_stream_idx = frame.m_stream_idx;

        m_res = (m_frame != nullptr) ? 0 : AVERROR(ENOMEM);
    }

    return *this;
}

FFmpeg_Frame& FFmpeg_Frame::operator=(const AVFrame * frame) noexcept
{
    if (m_frame != frame)
    {
        AVFrame *new_frame = av_frame_clone(frame);

        free();

        m_frame = new_frame;

        m_stream_idx = INVALID_STREAM;

        m_res = (m_frame != nullptr) ? 0 : AVERROR(ENOMEM);
    }

    return *this;
}
