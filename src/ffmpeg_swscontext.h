/*
 * This file is part of FFmpegfs.
 *
 * FFmpegfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * @file ffmpeg_swscontext.h
 * @brief FFmpeg SwsContext RAII wrapper.
 *
 * @ingroup ffmpegfs
 */

#ifndef FFMPEG_SWSCONTEXT_H
#define FFMPEG_SWSCONTEXT_H

struct SwsContext;

/**
 * @brief RAII wrapper for SwsContext.
 *
 * Owns a libswscale context and releases it with sws_freeContext(). The wrapper
 * is movable but not copyable.
 */
class FFmpeg_SwsContext
{
public:
    /**
     * @brief Construct an empty scaler-context wrapper.
     */
    FFmpeg_SwsContext();

    /**
     * @brief Construct a wrapper taking ownership of an existing context.
     * @param[in] ctx Context pointer to take ownership of, or nullptr.
     */
    explicit FFmpeg_SwsContext(SwsContext *ctx);

    /**
     * @brief Release the owned scaler context, if any.
     */
    ~FFmpeg_SwsContext();

    FFmpeg_SwsContext(const FFmpeg_SwsContext&) = delete;
    FFmpeg_SwsContext& operator=(const FFmpeg_SwsContext&) = delete;

    /**
     * @brief Move-construct a scaler-context wrapper.
     * @param[in,out] ctx Source wrapper whose context ownership is transferred.
     */
    FFmpeg_SwsContext(FFmpeg_SwsContext&& ctx) noexcept;

    /**
     * @brief Move-assign a scaler-context wrapper.
     * @param[in,out] ctx Source wrapper whose context ownership is transferred.
     * @return Reference to this wrapper.
     */
    FFmpeg_SwsContext& operator=(FFmpeg_SwsContext&& ctx) noexcept;

    /**
     * @brief Get the owned FFmpeg scaler context pointer.
     * @return SwsContext pointer, or nullptr if empty.
     */
    SwsContext *get() const;

    /**
     * @brief Release ownership without freeing the scaler context.
     * @return Previously owned SwsContext pointer, or nullptr if empty.
     */
    SwsContext *release();

    /**
     * @brief Replace the owned context.
     * @param[in] ctx New context pointer to own, or nullptr to only reset.
     * @return true if a previous context was freed, false otherwise.
     */
    bool reset(SwsContext *ctx = nullptr);

    /**
     * @brief Check whether the wrapper currently owns a scaler context.
     * @return true if no context is owned, false otherwise.
     */
    bool empty() const;

    /**
     * @brief Check whether the wrapper owns a valid context.
     */
    explicit operator bool() const;

    /**
     * @brief Convert to the underlying scaler context pointer.
     */
    operator SwsContext *() const;

    /**
     * @brief Access members of the underlying scaler context.
     * @return SwsContext pointer.
     */
    SwsContext *operator->() const;

private:
    SwsContext *m_ctx;  /**< @brief Pointer to underlying SwsContext. */
};

#endif // FFMPEG_SWSCONTEXT_H
