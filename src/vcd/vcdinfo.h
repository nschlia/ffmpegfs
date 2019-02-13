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

#ifndef VCDINFO_H
#define VCDINFO_H

#include "vcdchapter.h"

class VcdInfo
{
public:
    explicit VcdInfo();
    virtual ~VcdInfo();

    void                    clear();
    int                     load_file(const std::string & path);

    const time_t  &         get_file_date() const;
    const std::string   &   get_id() const;
    VCDTYPE                 get_type() const;
    std::string             get_type_str() const;
    VCDPROFILETAG           get_profile_tag() const;
    std::string             get_profile_tag_str() const;
    const std::string   &   get_album_id() const;
    int                     get_number_of_cds() const;
    int                     get_cd_number() const;

protected:
    // Common data
    std::string             m_disk_path;        // Path to disk
    time_t                  m_file_date;        // File date
    std::string             m_id;               // ID für die CD.
    VCDTYPE                 m_type;             // Type der CD.
    VCDPROFILETAG           m_profile_tag;      // System Profile Tag.
    // INFO.XXX data
    std::string             m_album_id;         // Album ID
    int                     m_number_of_cds;    // Number of CDs in set
    int                     m_cd_number;        // Number of this CD in set
};

#endif // VCDINFO_H
