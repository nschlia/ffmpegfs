/*
 * Copyright (C) 2017-2025 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file ffmpeg_frame.h
 * @brief FFmpeg AVFrame extension
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2025 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef FFMPEG_FRAME_H
#define FFMPEG_FRAME_H

#pragma once

struct AVFrame;

/**
 * @brief The #FFmpeg_Frame class
 */
class FFmpeg_Frame
{
public:
    /**
     * @brief Construct FFmpeg_Frame object.
     * @param[in] stream_index - Index of stream
     */
    explicit FFmpeg_Frame(int stream_index = INVALID_STREAM);
    /**
     * @brief Copy construct from FFmpeg_Frame object.
     * @param[in] frame - Pointer to source FFmpeg_Frame object.
     * @note Do not declare explicit, breaks use in std::variant
     */
    FFmpeg_Frame(const FFmpeg_Frame & frame);
    /**
     * @brief Copy construct from AVFrame struct.
     * @param[in] frame - Pointer to source AVFrame struct.
     * @note Do not declare explicit, breaks use in std::variant
     */
    FFmpeg_Frame(const AVFrame * frame);    // cppcheck-suppress noExplicitConstructor
    /**
     * @brief Destruct FFmpeg_Frame object.
     */
    virtual ~FFmpeg_Frame();
    /**
     * @brief Get result of last operation
     * @return Returns 0 if last operation was successful, or negative AVERROR value.
     */
    int             res() const;
    /**
     * @brief Clone frame to a new AVFrame * struct. Needs to be freed by a av_frame_free() call.
     * @return Returns cloned AVFrame * struct, or nullptr on error.
     */
    AVFrame*        clone();
    /**
     * @brief Unreference underlying frame
     */
    void            unref();
    /**
     * @brief Free underlying frame
     */
    void            free();
    /**
     * @brief Access the underlying frame
     * @return Returns the unerlying AVFrame * struct, or nullptr not valid.
     */
    AVFrame*        get();

    /**
     * @brief operator AVFrame *: Do as if we were a pointer to AVFrame
     */
    operator        AVFrame*();
    /**
     * @brief operator const AVFrame *: Do as if we were a const pointer to AVFrame
     */
    operator const  AVFrame*() const;
    /**
     * @brief operator ->: Do as if we were a pointer to AVFrame
     * @return Pointer to AVFrame struct.
     */
    AVFrame* operator->();
    /**
     * @brief Make copy from other FFmpeg_Frame object.
     * @param[in] frame - Reference to source FFmpeg_Frame object.
     * @return Reference to new FFmpeg_Frame object.
     */
    FFmpeg_Frame& operator=(const FFmpeg_Frame & frame) noexcept;
    /**
     * @brief Make copy from AVFrame structure.
     * @param[in] frame - Pointer to source AVFrame structure.
     * @return Reference to new FFmpeg_Frame object.
     */
    FFmpeg_Frame& operator=(const AVFrame * frame) noexcept;

protected:
    AVFrame *   m_frame;        /**< @brief Pointer to underlying AVFrame struct */
    int         m_res;          /**< @brief 0 if last operation was successful, or negative AVERROR value */

public:
    int         m_stream_idx;   /**< @brief Stream index frame belongs to, or -1 (INVALID_STREAM) */
};

#endif // FFMPEG_FRAME_H
