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

#ifndef VCDUTILS_H
#define VCDUTILS_H

#include <string>

#define     BCD2DEC(hex)        (((hex & 0xF0) >> 4) * 10 + (hex & 0x0F))

#define     VCD_MAX_CHAPTERS    500

#pragma pack(push)
#pragma pack(1)

typedef struct VCDMSF
{
    // Note: the fields in this structure are BCD encoded
    uint8_t m_min;
    uint8_t m_sec;
    uint8_t m_frame;

} VCDMSF, *LPVCDMSF;
typedef const VCDMSF * LPCVCDMSF;

typedef struct VCDCHAPTER
{
    uint8_t m_track_no;
    VCDMSF  m_msf;

} VCDCHAPTER, *LPVCDCHAPTER;
typedef const VCDCHAPTER * LPCVCDCHAPTER;

typedef struct VCDENTRIES
{
    char        m_ID[8];                // 8 Bytes: ID "ENTRYVCD" or "ENTRYSVD"
    uint8_t     m_type;                 // 1 Byte: CD type
    //          1 for VCD 1.0, VCD 1.1, SVCD 1.0 and HQVCD
    //          2 for VCD 2.0
    //          Identical with value in Info.VCD/SVD
    uint8_t     m_profile_tag;          // 1 Byte: System Profile Tag.
    //          0 for VCD 1.0, VCD 2.0, SVCD und HQVCD
    //          1 for VCD 1.1
    //          Identical with value in Info.VCD/SVD
    uint16_t    m_num_entries;          // 2 Bytes: 1 <= tracks <= 500
    //          There must be at least 1 chapter

    //    Entries (Entsprechend der Anzahl)
    //          1 Byte: Tracknummer
    //          3 Byte: Adresse MSF.
    VCDCHAPTER  m_chapter[VCD_MAX_CHAPTERS];

    uint8_t     reserved[36];           // RESERVED, must be 0x00

} VCDENTRY, *LPVCDENTRIES;
typedef const VCDENTRY * LPCVCDENTRIES;
#pragma pack(pop)

extern const char SYNC[12];

namespace VCDUTILS
{
std::string convert_txt2string(const char * txt, int size, bool trimmed = true);
bool        locate_file(const std::string & path, const std::string & filename, std::string & fullname, bool & is_vcd);
int         locate_video(const std::string & path, int track_no, std::string & fullname);
std::string get_type_str(int type);
std::string get_profile_tag_str(int tag);
void        get_directory(const std::string & fullname, std::string * directory);
}

#endif // VCDUTILS_H
