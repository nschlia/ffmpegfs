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
 * @brief FFmpeg AVFormatContext RAII wrapper.
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
    /**
     * @brief Managed AVFormatContext direction.
     */
    enum class TYPE
    {
        INPUT,      /**< @brief Input context closed with avformat_close_input(). */
        OUTPUT      /**< @brief Output context freed with avformat_free_context(). */
    };

    /**
     * @brief Construct an empty format-context wrapper.
     * @param[in] type Initial context direction used when resetting/freeing.
     */
    explicit FFmpeg_FormatContext(TYPE type = TYPE::INPUT);

    /**
     * @brief Release the owned format context, if any.
     */
    ~FFmpeg_FormatContext();

    FFmpeg_FormatContext(const FFmpeg_FormatContext&) = delete;
    FFmpeg_FormatContext& operator=(const FFmpeg_FormatContext&) = delete;

    /**
     * @brief Move-construct a format-context wrapper.
     * @param[in,out] context Source wrapper whose context ownership is transferred.
     */
    FFmpeg_FormatContext(FFmpeg_FormatContext&& context) noexcept;

    /**
     * @brief Move-assign a format-context wrapper.
     * @param[in,out] context Source wrapper whose context ownership is transferred.
     * @return Reference to this wrapper.
     */
    FFmpeg_FormatContext& operator=(FFmpeg_FormatContext&& context) noexcept;

    /**
     * @brief Allocate a new input format context.
     * @return 0 on success, or a negative FFmpeg error code.
     */
    int                 alloc_input_context();

    /**
     * @brief Allocate a new output format context.
     * @param[in] format_name Output container format name, or nullptr for automatic selection.
     * @param[in] filename Optional output file name used by FFmpeg for format probing.
     * @return 0 on success, or a negative FFmpeg error code.
     */
    int                 alloc_output_context(const char *format_name, const char *filename = nullptr);

    /**
     * @brief Set the context direction used for later cleanup.
     * @param[in] type Input or output context direction.
     */
    void                set_type(TYPE type);

    /**
     * @brief Mark whether the context owns a custom AVIOContext.
     * @param[in] custom_io true if pb was allocated with avio_alloc_context().
     */
    void                set_custom_io(bool custom_io = true);

    /**
     * @brief Free the owned format context and reset the wrapper to empty.
     * @return true if a context or custom IO object was released, false otherwise.
     */
    bool                reset();

    /**
     * @brief Check whether the wrapper currently owns a format context.
     * @return true if no format context is owned, false otherwise.
     */
    bool                empty() const;

    /**
     * @brief Get the owned FFmpeg format context pointer.
     * @return Mutable AVFormatContext pointer, or nullptr if empty.
     */
    AVFormatContext*    get();

    /**
     * @brief Get the owned FFmpeg format context pointer.
     * @return Const AVFormatContext pointer, or nullptr if empty.
     */
    const AVFormatContext* get() const;

    /**
     * @brief Get a writable pointer-to-pointer for FFmpeg APIs.
     *
     * Any currently owned context is released first so that FFmpeg can write a
     * fresh context pointer without leaking the previous one.
     *
     * @return Address of the managed context pointer.
     */
    AVFormatContext**   address();

    /**
     * @brief Release ownership without freeing the format context.
     * @return Previously owned AVFormatContext pointer, or nullptr if empty.
     */
    AVFormatContext*    release();

    /**
     * @brief Convert to the underlying mutable format context pointer.
     */
    operator            AVFormatContext*();

    /**
     * @brief Convert to the underlying const format context pointer.
     */
    operator const      AVFormatContext*() const;

    /**
     * @brief Access members of the underlying mutable format context.
     * @return Mutable AVFormatContext pointer.
     */
    AVFormatContext*    operator->();

    /**
     * @brief Access members of the underlying const format context.
     * @return Const AVFormatContext pointer.
     */
    const AVFormatContext* operator->() const;

private:
    /**
     * @brief Free the custom AVIOContext attached to the format context.
     */
    void                free_custom_io();

private:
    AVFormatContext *   m_context;      /**< @brief Pointer to underlying AVFormatContext. */
    TYPE                m_type;         /**< @brief Input or output context. */
    bool                m_custom_io;    /**< @brief true if pb was allocated with avio_alloc_context(). */
};

#endif // FFMPEG_FORMATCONTEXT_H
