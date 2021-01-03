/*
 * Copyright (C) 2017-2021 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file
 * @brief FFmpeg encoder profiles
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2021 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef FFMPEG_PROFILES_H
#define FFMPEG_PROFILES_H

#pragma once

#include "ffmpeg_utils.h"

/**
 * @brief The #FFmpeg_Profiles class
 */
class FFmpeg_Profiles
{
public:
#define OPT_ALL         0x00000000                      /**< @brief All files */
#define OPT_AUDIO       0x00000001                      /**< @brief For audio only files */
#define OPT_VIDEO       0x00000002                      /**< @brief For videos (not audio only) */

    typedef struct PROFILE_OPTION                       /**< @brief Profiles options */
    {
        const char *            m_key;                  /**< @brief Key, see av_opt_set() and av_dict_set() FFmpeg API function */
        const char *            m_value;                /**< @brief Value, see av_opt_set() and av_dict_set() FFmpeg API function */
        const int               m_flags;                /**< @brief Flags, see av_opt_set() and av_dict_set() FFmpeg API function */
        const int               m_options;              /**< @brief One of the OPT_* flags */
    } PROFILE_OPTION;                                   /**< @brief Profile option */
    typedef PROFILE_OPTION * LPPROFILE_OPTION;          /**< @brief Pointer version of PROFILE_OPTION */
    typedef PROFILE_OPTION const * LPCPROFILE_OPTION;   /**< @brief Pointer to const version of PROFILE_OPTION */

    typedef struct PROFILE_LIST                         /**< @brief List of profiles */
    {
        FILETYPE                m_filetype;             /**< @brief File type this option set is for */
        PROFILE                 m_profile;              /**< @brief One of PROFILE_ */
        LPCPROFILE_OPTION       m_option_codec;         /**< @brief av_opt_set() options */
        LPCPROFILE_OPTION       m_option_format;        /**< @brief av_dict_set() options */
    } PROFILE_LIST;                                     /**< @brief Profile list */
    typedef PROFILE_LIST * LPPROFILE_LIST;              /**< @brief Pointer version of PROFILE_LIST */
    typedef PROFILE_LIST const * LPCPROFILE_LIST;       /**< @brief Pointer to const version of PROFILE_LIST */

protected:
    /**
     * @brief Construct a FFmpeg_Profiles object.
     */
    explicit FFmpeg_Profiles();
    /**
     * @brief Destruct a FFmpeg_Profiles object.
     */
    virtual ~FFmpeg_Profiles();

protected:
    static const PROFILE_LIST   m_profile[];            /**< @brief List of profile options */
};

#endif // FFMPEG_PROFILES_H
