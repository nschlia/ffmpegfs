/*
 * Copyright (C) 2017-2024 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file id3v1tag.h
 * @brief %ID3v1 tag structure
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2024 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef ID3V1TAG_H
#define ID3V1TAG_H

#pragma once

#include <array>

/** @brief %ID3 version 1 tag
 */
struct ID3v1
{
    std::array<char, 3>     m_tag;      /**< @brief Contains "TAG" */
    std::array<char, 30>    m_title;    /**< @brief Title of sound track */
    std::array<char, 30>    m_artist;   /**< @brief Artist name */
    std::array<char, 30>    m_album;    /**< @brief Album name */
    std::array<char, 4>     m_year;     /**< @brief Year of publishing */
    std::array<char, 28>    m_comment;  /**< @brief Any user comments */
    char                    m_padding;  /**< @brief Padding byte, must be '\0' */
    char                    m_title_no; /**< @brief Title number */
    char                    m_genre;    /**< @brief Type of music */
};

static_assert(sizeof(ID3v1) == 128);

extern void init_id3v1(ID3v1 *id3v1);   /**< @brief Initialise ID3v1 tag */

#define ID3V1_TAG_LENGTH sizeof(ID3v1)  /**< @brief Fixed 128 bytes */

#endif // ID3V1TAG_H
