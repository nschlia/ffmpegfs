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

typedef struct VCDINFO
{
#pragma pack(1)

    char    m_ID[8];                // 8 Byte: ID fuer die CD.
    //      Die ID ist vom CD-Typ abhaengig: VIDEO_CD, SUPERVCD oder HQ-VCD. Sie ist immer 8 Byte lang.
    char    m_type;                // 1 Byte: Type der CD.
    //      1 fuer VCD 1.0, VCD 1.1, SVCD 1.0 und HQVCD
    //      2 fuer VCD 2.0
    char    m_profile_tag;         // 1 Byte: System Profile Tag.
    //      0 fuer VCD 1.0, VCD 2.0, SVCD und HQVCD
    //      1 fuer VCD 1.1
    char    m_albumid[16];          // 16 Byte: Album ID
    //      Bei der Album ID handelt es sich um den Namen des Albums.
    short   m_numberof_cds;         // 2 Byte: Anzahl der CDs im Set
    //      Gibt an, wieviele CDs zu dem Film/Set gehoeren.
    short   m_cd_number;            // 2 Byte: Nummer der CD im Set
    //      Gibt an, die wievielte CD des Films/Sets dies ist.
    char    m_palflags[98];         // 98 Byte: PAL Flags

    // Wie und welche Flags gesetzt werden, ist mir nicht bekannt. Die obigen Angaben sind aus einem C-Quelltext

    //        reserved1:
    //        restriction:
    //        special info:
    //        user data cc:
    //        start lid #2:
    //        start track #2:
    //        reserved2:
    //    psd size:
    //    first segment addr:
    //    offset multiplier:
    //    maximum lid:
    //    maximum segment number:
    //        SEGMENT[1]: audio: video:
    //    volume start time[0]:
    //    ...

} VCDINFO, *LPVCDINFO;
typedef const VCDINFO * LPCVCDINFO;

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
