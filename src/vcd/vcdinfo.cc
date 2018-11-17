// -------------------------------------------------------------------------------
//  Project:		Bully's Media Player
//
//  File:		vcdinfo.cpp
//
// (c) 1984-2017 by Oblivion Software/Norbert Schlia
// All rights reserved.
// -------------------------------------------------------------------------------
//
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

typedef struct _tagVCDINFO
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
    m_type          = 0;
    m_profile_tag   = 0;
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
        m_id            = VCDUTILS::convert_psz2string(vi.m_ID, sizeof(vi.m_ID));
        m_type          = static_cast<int>(vi.m_type);
        m_profile_tag   = static_cast<int>(vi.m_profile_tag);
        m_album_id      = VCDUTILS::convert_psz2string(vi.m_albumid, sizeof(vi.m_albumid));
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

int VcdInfo::get_type() const
{
    return m_type;
}

std::string VcdInfo::get_type_str() const
{
    return VCDUTILS::get_type_str(m_type);
}

int VcdInfo::get_profile_tag() const
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
