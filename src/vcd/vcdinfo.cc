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

/**
 * @file
 * @brief S/VCD VcdInfo class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2013-2019 Norbert Schlia (nschlia@oblivion-software.de) @n
 * From BullysPLayer Copyright (C) 1984-2017 by Oblivion Software/Norbert Schlia
 */

#include <string>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "vcdinfo.h"
#include "vcdutils.h"

#include <string.h>
#include <sys/stat.h>

/**
  * @brief VCDINFO structure of INFO.VCD/SVD file
  */
typedef struct VCDINFO
{
#pragma pack(1)

    /**
     * @brief 8 Byte: ID for this CD
     *
     * This ID is CD type dependant: VIDEO_CD, SUPERVCD or HQ-VCD. It is always 8 bytes long.
     */
    char    m_ID[8];
    /**
     * @brief 1 Byte: CD type
     *
     * 1 for VCD 1.0, VCD 1.1, SVCD 1.0 and HQVCD
     * 2 for VCD 2.0
     */
    char    m_type;
    /**
     * @brief 1 Byte: System profile tag
     *
     * 0 fuer VCD 1.0, VCD 2.0, SVCD und HQVCD
     * 1 fuer VCD 1.1
     */
    char    m_profile_tag;
    /**
     * @brief 16 Byte: Album ID
     *
     * The album ID is the name of this album.
     */
    char    m_albumid[16];
    /**
     * @brief 2 Byte: Number of CDs in set
     *
     * Total number of CDs in this set.
     */
    short   m_numberof_cds;
    /**
     * @brief 2 Byte: Number of this CD
     *
     * Defines which number this CD is in this set.
     */
    short   m_cd_number;
    /**
     * @brief 98 Byte: PAL Flags
     *
     * The meaning of these flags is unknown to me.
     */
    char    m_palflags[98];

    // reserved1:
    // restriction:
    // special info:
    // user data cc:
    // start lid #2:
    // start track #2:
    // reserved2:
    // psd size:
    // first segment addr:
    // offset multiplier:
    // maximum lid:
    // maximum segment number:
    //      SEGMENT[1]: audio: video:
    //      volume start time[0]:
    //      ...

} VCDINFO, *LPVCDINFO;                  /**< @brief Pointer version */
typedef const VCDINFO * LPCVCDINFO;     /**< @brief Pointer to const version */

VcdInfo::VcdInfo()
{
    clear();
}

VcdInfo::~VcdInfo()
{
}

void VcdInfo::clear()
{
    m_disk_path.clear();
    m_file_date     = -1;
    m_id.clear();
    m_type          = VCDTYPE_UNKNOWN;
    m_profile_tag   = VCDPROFILETAG_UNKNOWN;
    m_album_id.clear();
    m_number_of_cds = 0;
    m_cd_number     = 0;
}

int VcdInfo::load_file(const std::string & path)
{
    std::string fullname;
    bool is_svcd = false;

    clear();

    if (!VCDUTILS::locate_file(path, "INFO", fullname, is_svcd))
    {
        return errno;
    }

    VCDUTILS::get_directory(path, &m_disk_path);

    FILE *fpi;
    VCDINFO vi;
    bool success = true;

    fpi = fopen(fullname.c_str(), "rb");
    if (fpi == nullptr)
    {
        return errno;
    }

    struct stat st;

    if (fstat(fileno(fpi), &st) != 0)
    {
        return ferror(fpi);
    }

    m_file_date      = st.st_mtime;

    memset(&vi, 0, sizeof(vi));

    if (fread(reinterpret_cast<char *>(&vi), 1, sizeof(vi), fpi) == sizeof(vi))
    {
        m_id            = VCDUTILS::convert_txt2string(vi.m_ID, sizeof(vi.m_ID));
        m_type          = static_cast<VCDTYPE>(vi.m_type);
        m_profile_tag   = static_cast<VCDPROFILETAG>(vi.m_profile_tag);
        m_album_id      = VCDUTILS::convert_txt2string(vi.m_albumid, sizeof(vi.m_albumid));
        m_number_of_cds = htons(static_cast<uint16_t>(vi.m_numberof_cds));
        m_cd_number     = htons(static_cast<uint16_t>(vi.m_cd_number));
    }

    else
    {
        success = false;
    }

    int _errno = 0;

    if (success)
    {
        _errno = ferror(fpi);
    }

    fclose(fpi);

    return _errno;
}

const time_t &VcdInfo::get_file_date() const
{
    return m_file_date;
}

const std::string & VcdInfo::get_id() const
{
    return m_id;
}

VCDTYPE VcdInfo::get_type() const
{
    return m_type;
}

std::string VcdInfo::get_type_str() const
{
    return VCDUTILS::get_type_str(m_type);
}

VCDPROFILETAG VcdInfo::get_profile_tag() const
{
    return m_profile_tag;
}

std::string VcdInfo::get_profile_tag_str() const
{
    return VCDUTILS::get_profile_tag_str(m_profile_tag);
}

const std::string & VcdInfo::get_album_id() const
{
    return m_album_id;
}

int VcdInfo::get_number_of_cds() const
{
    return m_number_of_cds;
}

int VcdInfo::get_cd_number() const
{
    return m_cd_number;
}
