/*
 * Copyright (C) 2017-2026 by Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * @file ffmpeg_formatcontext.h
 * @brief FFmpeg AVFormatContext RAII wrapper
 *
 * @ingroup ffmpegfs
 */

#ifndef FFMPEG_FORMATCONTEXT_H
#define FFMPEG_FORMATCONTEXT_H

#pragma once

#include "ffmpeg_utils.h"

/**
 * @brief RAII wrapper for AVFormatContext.
 *
 * Owns the AVFormatContext and releases it with the correct FFmpeg function.
 * Input contexts are closed with avformat_close_input(), output contexts are
 * released with avformat_free_context(). Custom AVIO contexts allocated with
 * avio_alloc_context() are freed before the format context is released.
 */
class FFmpeg_FormatContext
{
public:
    enum class TYPE
    {
        INPUT,
        OUTPUT
    };

    explicit FFmpeg_FormatContext(TYPE type = TYPE::INPUT);
    ~FFmpeg_FormatContext();

    FFmpeg_FormatContext(const FFmpeg_FormatContext&) = delete;
    FFmpeg_FormatContext& operator=(const FFmpeg_FormatContext&) = delete;

    FFmpeg_FormatContext(FFmpeg_FormatContext&& context) noexcept;
    FFmpeg_FormatContext& operator=(FFmpeg_FormatContext&& context) noexcept;

    int                 alloc_input_context();
    int                 alloc_output_context(const char *format_name, const char *filename = nullptr);

    void                set_type(TYPE type);
    void                set_custom_io(bool custom_io = true);

    bool                reset();
    bool                empty() const;

    AVFormatContext*    get();
    const AVFormatContext* get() const;
    AVFormatContext**   address();

    AVFormatContext*    release();

    operator            AVFormatContext*();
    operator const      AVFormatContext*() const;
    AVFormatContext*    operator->();
    const AVFormatContext* operator->() const;

private:
    void                free_custom_io();

private:
    AVFormatContext *   m_context;      /**< @brief Pointer to underlying AVFormatContext */
    TYPE                m_type;         /**< @brief Input or output context */
    bool                m_custom_io;    /**< @brief true if pb was allocated with avio_alloc_context() */
};

#endif // FFMPEG_FORMATCONTEXT_H
