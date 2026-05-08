/*
 * This file is part of FFmpegfs.
 *
 * FFmpegfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef FFMPEG_SWSCONTEXT_H
#define FFMPEG_SWSCONTEXT_H

struct SwsContext;

/**
 * @brief RAII wrapper for SwsContext.
 */
class FFmpeg_SwsContext
{
public:
    FFmpeg_SwsContext();
    explicit FFmpeg_SwsContext(SwsContext *ctx);
    ~FFmpeg_SwsContext();

    FFmpeg_SwsContext(const FFmpeg_SwsContext&) = delete;
    FFmpeg_SwsContext& operator=(const FFmpeg_SwsContext&) = delete;

    FFmpeg_SwsContext(FFmpeg_SwsContext&& ctx) noexcept;
    FFmpeg_SwsContext& operator=(FFmpeg_SwsContext&& ctx) noexcept;

    SwsContext *get() const;
    SwsContext *release();
    bool reset(SwsContext *ctx = nullptr);

    bool empty() const;

    explicit operator bool() const;
    operator SwsContext *() const;
    SwsContext *operator->() const;

private:
    SwsContext *m_ctx;
};

#endif // FFMPEG_SWSCONTEXT_H
