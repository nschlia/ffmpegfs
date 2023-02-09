/*
 * Copyright (C) 2017-2023 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file ffmpeg_subtitle.h
 * @brief FFmpeg AVSubtitle extension
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2023 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef FFMPEG_SUBTITLE_H
#define FFMPEG_SUBTITLE_H

#pragma once

#include "ffmpeg_utils.h"

#include <memory>

struct AVSubtitle;

/**
 * @brief The #FFmpeg_Subtitle class
 */
class FFmpeg_Subtitle : public std::shared_ptr<AVSubtitle>
{
public:
    /**
     * @brief Construct FFmpeg_Subtitle object.
     * @param[in] stream_index - Index of stream
     */
    explicit FFmpeg_Subtitle(int stream_index = INVALID_STREAM);
    /**
     * @brief Destruct FFmpeg_Subtitle object.
     */
    virtual ~FFmpeg_Subtitle() = default;
    /**
     * @brief Get result of last operation
     * @return Returns 0 if last operation was successful, or negative AVERROR value.
     */
    int             res() const;
    /**
     * @brief Unreference underlying frame. Synonym for shared_ptr::reset().
     */
    void            unref() noexcept;

    /**
     * @brief operator AVSubtitle *: Do as if we were a pointer to AVSubtitle
     */
    operator        AVSubtitle*();
    /**
     * @brief operator const AVSubtitle *: Do as if we were a const pointer to AVSubtitle
     */
    operator const  AVSubtitle*() const;
    /**
     * @brief operator ->: Do as if we were a pointer to AVSubtitle
     * @return Pointer to AVSubtitle struct.
     */
    AVSubtitle* operator->();

protected:
    /**
     * @brief Allocate a subtitle
     * @return Returns a newly allocated AVSubtitle field, or nullptr if out of memory.
     */
    AVSubtitle*         alloc_subtitle();
    /**
      * @brief Delete a subtitle
      * @param[in] subtitle - AVSubtitle structure to delete/free.
      */
    static void         delete_subtitle(AVSubtitle *subtitle);

protected:
    int         m_res;          /**< @brief 0 if last operation was successful, or negative AVERROR value */

public:
    int         m_stream_idx;   /**< @brief Stream index frame belongs to, or -1 (INVALID_STREAM) */
};

#endif // FFMPEG_SUBTITLE_H
