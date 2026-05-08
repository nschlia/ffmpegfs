/*
 * This file is part of FFmpegfs.
 *
 * FFmpegfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ffmpeg_swscontext.h"

extern "C" {
#include <libswscale/swscale.h>
}

FFmpeg_SwsContext::FFmpeg_SwsContext()
    : m_ctx(nullptr)
{
}

FFmpeg_SwsContext::FFmpeg_SwsContext(SwsContext *ctx)
    : m_ctx(ctx)
{
}

FFmpeg_SwsContext::~FFmpeg_SwsContext()
{
    reset();
}

FFmpeg_SwsContext::FFmpeg_SwsContext(FFmpeg_SwsContext&& ctx) noexcept
    : m_ctx(ctx.release())
{
}

FFmpeg_SwsContext& FFmpeg_SwsContext::operator=(FFmpeg_SwsContext&& ctx) noexcept
{
    if (this != &ctx)
    {
        reset(ctx.release());
    }
    return *this;
}

SwsContext *FFmpeg_SwsContext::get() const
{
    return m_ctx;
}

SwsContext *FFmpeg_SwsContext::release()
{
    SwsContext *ctx = m_ctx;
    m_ctx = nullptr;
    return ctx;
}

bool FFmpeg_SwsContext::reset(SwsContext *ctx)
{
    bool closed = false;

    if (m_ctx != nullptr)
    {
        sws_freeContext(m_ctx);
        closed = true;
    }

    m_ctx = ctx;

    return closed;
}

bool FFmpeg_SwsContext::empty() const
{
    return m_ctx == nullptr;
}

FFmpeg_SwsContext::operator bool() const
{
    return m_ctx != nullptr;
}

FFmpeg_SwsContext::operator SwsContext *() const
{
    return m_ctx;
}

SwsContext *FFmpeg_SwsContext::operator->() const
{
    return m_ctx;
}
