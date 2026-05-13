/*
 * This file is part of FFmpegfs.
 *
 * FFmpegfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * @file ffmpeg_swrcontext.h
 * @brief FFmpeg SwrContext RAII wrapper.
 *
 * @ingroup ffmpegfs
 */

#ifndef FFMPEG_SWRCONTEXT_H
#define FFMPEG_SWRCONTEXT_H

struct SwrContext;

/**
 * @brief RAII wrapper for SwrContext.
 *
 * Owns a libswresample context and releases it with swr_free(). The wrapper is
 * movable but not copyable.
 */
class FFmpeg_SwrContext
{
public:
    /**
     * @brief Construct an empty resampler-context wrapper.
     */
    FFmpeg_SwrContext();

    /**
     * @brief Construct a wrapper taking ownership of an existing context.
     * @param[in] ctx Context pointer to take ownership of, or nullptr.
     */
    explicit FFmpeg_SwrContext(SwrContext *ctx);

    /**
     * @brief Release the owned resampler context, if any.
     */
    ~FFmpeg_SwrContext();

    FFmpeg_SwrContext(const FFmpeg_SwrContext&) = delete;
    FFmpeg_SwrContext& operator=(const FFmpeg_SwrContext&) = delete;

    /**
     * @brief Move-construct a resampler-context wrapper.
     * @param[in,out] ctx Source wrapper whose context ownership is transferred.
     */
    FFmpeg_SwrContext(FFmpeg_SwrContext&& ctx) noexcept;

    /**
     * @brief Move-assign a resampler-context wrapper.
     * @param[in,out] ctx Source wrapper whose context ownership is transferred.
     * @return Reference to this wrapper.
     */
    FFmpeg_SwrContext& operator=(FFmpeg_SwrContext&& ctx) noexcept;

    /**
     * @brief Get the owned FFmpeg resampler context pointer.
     * @return SwrContext pointer, or nullptr if empty.
     */
    SwrContext *get() const;

    /**
     * @brief Get a writable pointer-to-pointer for FFmpeg allocation APIs.
     *
     * Any currently owned context is freed first so that FFmpeg can write a new
     * context pointer without leaking the previous one.
     *
     * @return Address of the managed context pointer.
     */
    SwrContext **address();

    /**
     * @brief Release ownership without freeing the resampler context.
     * @return Previously owned SwrContext pointer, or nullptr if empty.
     */
    SwrContext *release();

    /**
     * @brief Replace the owned context.
     * @param[in] ctx New context pointer to own, or nullptr to only reset.
     * @return true if a previous context was freed, false otherwise.
     */
    bool reset(SwrContext *ctx = nullptr);

    /**
     * @brief Check whether the wrapper currently owns a resampler context.
     * @return true if no context is owned, false otherwise.
     */
    bool empty() const;

    /**
     * @brief Check whether the wrapper owns a valid context.
     */
    explicit operator bool() const;

    /**
     * @brief Convert to the underlying resampler context pointer.
     */
    operator SwrContext *() const;

    /**
     * @brief Access members of the underlying resampler context.
     * @return SwrContext pointer.
     */
    SwrContext *operator->() const;

private:
    SwrContext *m_ctx;  /**< @brief Pointer to underlying SwrContext. */
};

#endif // FFMPEG_SWRCONTEXT_H
