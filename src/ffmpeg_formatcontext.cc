/*
 * Copyright (C) 2017-2026 Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * @file ffmpeg_formatcontext.cc
 * @brief FFmpeg_FormatContext class implementation
 *
 * @ingroup ffmpegfs
 */

#ifdef __cplusplus
extern "C" {
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/mem.h>
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

#include "ffmpeg_formatcontext.h"

FFmpeg_FormatContext::FFmpeg_FormatContext(TYPE type) :
    m_context(nullptr),
    m_type(type),
    m_custom_io(false)
{
}

FFmpeg_FormatContext::~FFmpeg_FormatContext()
{
    reset();
}

FFmpeg_FormatContext::FFmpeg_FormatContext(FFmpeg_FormatContext&& context) noexcept :
    m_context(context.m_context),
    m_type(context.m_type),
    m_custom_io(context.m_custom_io)
{
    context.m_context = nullptr;
    context.m_custom_io = false;
}

FFmpeg_FormatContext& FFmpeg_FormatContext::operator=(FFmpeg_FormatContext&& context) noexcept
{
    if (this != &context)
    {
        reset();

        m_context = context.m_context;
        m_type = context.m_type;
        m_custom_io = context.m_custom_io;

        context.m_context = nullptr;
        context.m_custom_io = false;
    }

    return *this;
}

int FFmpeg_FormatContext::alloc_input_context()
{
    reset();

    m_type = TYPE::INPUT;
    m_context = avformat_alloc_context();

    return (m_context != nullptr) ? 0 : AVERROR(ENOMEM);
}

int FFmpeg_FormatContext::alloc_output_context(const char *format_name, const char *filename)
{
    reset();

    m_type = TYPE::OUTPUT;

    int ret = avformat_alloc_output_context2(&m_context, nullptr, format_name, filename);
    if (ret < 0)
    {
        return ret;
    }

    return (m_context != nullptr) ? 0 : AVERROR(ENOMEM);
}

void FFmpeg_FormatContext::set_type(TYPE type)
{
    m_type = type;
}

void FFmpeg_FormatContext::set_custom_io(bool custom_io)
{
    m_custom_io = custom_io;
}

bool FFmpeg_FormatContext::reset()
{
    bool closed = (m_context != nullptr);

    if (m_context != nullptr)
    {
        free_custom_io();

        if (m_type == TYPE::OUTPUT)
        {
            avformat_free_context(m_context);
            m_context = nullptr;
        }
        else
        {
            avformat_close_input(&m_context);
        }
    }

    m_custom_io = false;

    return closed;
}

bool FFmpeg_FormatContext::empty() const
{
    return (m_context == nullptr);
}

AVFormatContext* FFmpeg_FormatContext::get()
{
    return m_context;
}

const AVFormatContext* FFmpeg_FormatContext::get() const
{
    return m_context;
}

AVFormatContext** FFmpeg_FormatContext::address()
{
    return &m_context;
}

AVFormatContext* FFmpeg_FormatContext::release()
{
    AVFormatContext *context = m_context;

    m_context = nullptr;
    m_custom_io = false;

    return context;
}

FFmpeg_FormatContext::operator AVFormatContext*()
{
    return m_context;
}

FFmpeg_FormatContext::operator const AVFormatContext*() const
{
    return m_context;
}

AVFormatContext* FFmpeg_FormatContext::operator->()
{
    return m_context;
}

const AVFormatContext* FFmpeg_FormatContext::operator->() const
{
    return m_context;
}

void FFmpeg_FormatContext::free_custom_io()
{
    if (m_custom_io && m_context != nullptr && m_context->pb != nullptr)
    {
#if (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 80, 0))
        av_freep(&m_context->pb->buffer);
        avio_context_free(&m_context->pb);
#else
        av_freep(m_context->pb);
        m_context->pb = nullptr;
#endif
    }
}
