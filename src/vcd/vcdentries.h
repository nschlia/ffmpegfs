/*
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

#pragma once

#ifndef VCDENTRIES_H
#define VCDENTRIES_H

#include "vcdchapter.h"

#include <vector>

class VcdEntries
{
public:
    typedef enum SEEKRES
    {
        SEEKRES_NOTFOUND,
        SEEKRES_FOUND,
        SEEKRES_ERROR
    } SEEKRES, *LPSEEKRES;
    typedef const SEEKRES * LPCSEEKRES;

public:
    explicit VcdEntries();
    virtual ~VcdEntries();

    void                    clear();
    int                     load_file(const std::string & path);

    time_t                  get_file_date() const;
    const std::string &     get_id() const;
    VCDTYPE                 get_type() const;
    std::string             get_type_str() const;
    VCDPROFILETAG           get_profile_tag() const;
    std::string             get_profile_tag_str() const;
    int                     get_number_of_chapters() const;
    const VcdChapter  *     get_chapter(int chapter_idx) const;
    int64_t                 get_duration() const;
    uint64_t                get_size() const;                   // Get disk size (DAT/MPEG only)

protected:
    int                     scan_chapters();
    SEEKRES                 seek_sync(FILE *fpi, const char * sync, int len) const;

protected:
    // Common data
    time_t                  m_file_date;                        // File date
    std::string             m_id;                               // ID of CD.
    VCDTYPE                 m_type;                             // Type of CD.
    VCDPROFILETAG           m_profile_tag;                      // System profile tag.
    // ENTRIES.XXX data
    std::vector<VcdChapter>  m_chapters;
    int64_t                 m_duration;

    // misc.
    std::string             m_disk_path;
};

#endif // VCDENTRIES_H
