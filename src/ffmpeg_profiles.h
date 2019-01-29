/*
 * FFmpeg encoder profiles for FFmpegfs
 *
 * Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef FFMPEG_PROFILES_H
#define FFMPEG_PROFILES_H

#pragma once

#include "ffmpeg_utils.h"

class FFMPEG_Profiles
{
public:
#define OPT_CODEC       0x80000000

#define OPT_ALL         0x00000000  // All files
#define OPT_AUDIO       0x00000001  // For audio only files
#define OPT_VIDEO       0x00000002  // For videos (not audio only)

    typedef struct _tagPROFILE_OPTION
    {
        const char *            m_key;
        const char *            m_value;
        const int               m_flags;
        const int               m_options;
    } PROFILE_OPTION, *LPPROFILE_OPTION;
    typedef PROFILE_OPTION const * LPCPROFILE_OPTION;

    typedef struct _tagPROFILE_LIST
    {
        FILETYPE                m_filetype;
        PROFILE                 m_profile;
        LPCPROFILE_OPTION       m_option_codec;
        LPCPROFILE_OPTION       m_option_format;
    } PROFILE_LIST, *LPPROFILE_LIST;
    typedef PROFILE_LIST const * LPCPROFILE_LIST;

protected:
    FFMPEG_Profiles();
    virtual ~FFMPEG_Profiles();

protected:    
    static const PROFILE_LIST   m_profile[];
};

#endif // FFMPEG_PROFILES_H
