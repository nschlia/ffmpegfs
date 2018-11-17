// -------------------------------------------------------------------------------
//  Project:		Bully's Media Player
//
//  File:           vcdutils.h
//
// (c) 1984-2017 by Oblivion Software/Norbert Schlia
// All rights reserved.
// -------------------------------------------------------------------------------
//
#pragma once

#ifndef VCDUTILS_H
#define VCDUTILS_H

#include <string>

#define     BCD2DEC(hex)    (((hex & 0xF0) >> 4) * 10 + (hex & 0x0F))

#define     MAX_ENTRIES     500

#pragma pack(push)
#pragma pack(1)

typedef struct _tagVCDMSF
{
    // Note: the fields in this structure are BCD encoded
    char    m_min;
    char    m_sec;
    char    m_frame;

} VCDMSF, *LPVCDMSF;
typedef const VCDMSF * LPCVCDMSF;

typedef struct _tagVCDCHAPTER
{
    char    m_track_no;
    VCDMSF  m_msf;

} VCDCHAPTER, *LPVCDCHAPTER;
typedef const VCDCHAPTER * LPCVCDCHAPTER;

typedef struct _tagVCDENTRIES
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
    VCDCHAPTER  m_entry[MAX_ENTRIES];

    uint8_t     reserved[36];           // RESERVED, must be 0x00

} VCDENTRY, *LPVCDENTRIES;
typedef const VCDENTRY * LPCVCDENTRIES;
#pragma pack(pop)

extern const char SYNC[12];

namespace VCDUTILS
{
std::string convert_psz2string(const char * psz, int size, bool trimmed = true);
bool        locate_file(const std::string & path, const std::string & filename, std::string & fullname, bool & is_vcd);
int         locate_video(const std::string & path, int track_no, std::string & fullname);
std::string get_type_str(int type);
std::string get_profile_tag_str(int tag);
void        get_directory(const std::string & fullname, std::string * directory);
}

#endif // VCDUTILS_H
