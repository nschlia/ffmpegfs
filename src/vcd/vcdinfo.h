// -------------------------------------------------------------------------------
//  Project:		Bully's Media Player
//
//  File:           vcdinfo.h
//
// (c) 1984-2017 by Oblivion Software/Norbert Schlia
// All rights reserved.
// -------------------------------------------------------------------------------
//
#pragma once

#ifndef VCDINFO_H
#define VCDINFO_H

#include <string>

using namespace std;

class VcdInfo
{
public:
    explicit VcdInfo();
    virtual ~VcdInfo();

    void                clear();
    int                 load_file(const string & path);

    const time_t  &     get_file_date() const;
    int64_t             get_disk_size() const;
    const string   &    get_id() const;
    int                 get_type() const;
    string              get_type_str() const;
    int                 get_profile_tag() const;
    string              get_profile_tag_str() const;
    const string   &    get_album_id() const;
    int                 get_number_of_cds() const;
    int                 get_cd_number() const;

protected:
    // Common data
    string              m_disk_path;        // Path to disk
    time_t              m_file_date;        // File date
    string              m_id;               // ID für die CD.
    int                 m_type;             // Type der CD.
    int                 m_profile_tag;      // System Profile Tag.
    // INFO.XXX data
    string              m_album_id;         // Album ID
    int                 m_number_of_cds;    // Number of CDs in set
    int                 m_cd_number;        // Number of this CD in set
};

#endif // VCDINFO_H
