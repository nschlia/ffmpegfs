/*
 * This file is part of FFmpegfs.
 *
 * FFmpegfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ffmpeg_swrcontext.h"

extern "C" {
#include <libswresample/swresample.h>
}

FFmpeg_SwrContext::FFmpeg_SwrContext()
    : m_ctx(nullptr)
{
}

FFmpeg_SwrContext::FFmpeg_SwrContext(SwrContext *ctx)
    : m_ctx(ctx)
{
}

FFmpeg_SwrContext::~FFmpeg_SwrContext()
{
    reset();
}

FFmpeg_SwrContext::FFmpeg_SwrContext(FFmpeg_SwrContext&& ctx) noexcept
    : m_ctx(ctx.release())
{
}

FFmpeg_SwrContext& FFmpeg_SwrContext::operator=(FFmpeg_SwrContext&& ctx) noexcept
{
    if (this != &ctx)
    {
        reset(ctx.release());
    }
    return *this;
}

SwrContext *FFmpeg_SwrContext::get() const
{
    return m_ctx;
}

SwrContext **FFmpeg_SwrContext::address()
{
    reset();
    return &m_ctx;
}

SwrContext *FFmpeg_SwrContext::release()
{
    SwrContext *ctx = m_ctx;
    m_ctx = nullptr;
    return ctx;
}

bool FFmpeg_SwrContext::reset(SwrContext *ctx)
{
    bool closed = false;

    if (m_ctx != nullptr)
    {
        swr_free(&m_ctx);
        closed = true;
    }

    m_ctx = ctx;

    return closed;
}

bool FFmpeg_SwrContext::empty() const
{
    return m_ctx == nullptr;
}

FFmpeg_SwrContext::operator bool() const
{
    return m_ctx != nullptr;
}

FFmpeg_SwrContext::operator SwrContext *() const
{
    return m_ctx;
}

SwrContext *FFmpeg_SwrContext::operator->() const
{
    return m_ctx;
}
