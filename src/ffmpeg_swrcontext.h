/*
 * This file is part of FFmpegfs.
 *
 * FFmpegfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef FFMPEG_SWRCONTEXT_H
#define FFMPEG_SWRCONTEXT_H

struct SwrContext;

/**
 * @brief RAII wrapper for SwrContext.
 */
class FFmpeg_SwrContext
{
public:
    FFmpeg_SwrContext();
    explicit FFmpeg_SwrContext(SwrContext *ctx);
    ~FFmpeg_SwrContext();

    FFmpeg_SwrContext(const FFmpeg_SwrContext&) = delete;
    FFmpeg_SwrContext& operator=(const FFmpeg_SwrContext&) = delete;

    FFmpeg_SwrContext(FFmpeg_SwrContext&& ctx) noexcept;
    FFmpeg_SwrContext& operator=(FFmpeg_SwrContext&& ctx) noexcept;

    SwrContext *get() const;
    SwrContext **address();
    SwrContext *release();
    bool reset(SwrContext *ctx = nullptr);

    bool empty() const;

    explicit operator bool() const;
    operator SwrContext *() const;
    SwrContext *operator->() const;

private:
    SwrContext *m_ctx;
};

#endif // FFMPEG_SWRCONTEXT_H
