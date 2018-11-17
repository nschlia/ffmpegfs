// -------------------------------------------------------------------------------
//  Project:		Bully's Media Player
//
//  File:           vcdentries.h
//
// (c) 1984-2017 by Oblivion Software/Norbert Schlia
// All rights reserved.
// -------------------------------------------------------------------------------
//
#pragma once

#ifndef VCDENTRIES_H
#define VCDENTRIES_H

#include "vcdchapter.h"

#include <vector>

class VcdEntries
{
public:
    typedef enum _tagSEEKRES
    {
        SEEKRES_NOTFOUND,
        SEEKRES_FOUND,
        SEEKRES_ERROR
    } SEEKRES, *LPSEEKRES;
    typedef const SEEKRES * LPCSEEKRES;

public:
    explicit VcdEntries();
    virtual ~VcdEntries();

    void                clear();
    int                 load_file(const std::string & path);

    time_t              get_file_date() const;
    const std::string   &    get_id() const;
    int                 get_type() const;
    std::string         get_type_str() const;
    int                 get_profile_tag() const;
    std::string         get_profile_tag_str() const;
    int                 get_number_of_chapters() const;
    const VcdChapter  & get_chapter(int chapter_no) const;

protected:
    int                 scan_chapters();
    SEEKRES             seek_sync(FILE *fpi, const char * sync, int len) const;

protected:
    // Common data
    time_t              m_file_date;        // File date
    std::string         m_id;               // ID of CD.
    int                 m_type;             // Type of CD.
    int                 m_profile_tag;      // System profile tag.
    // ENTRIES.XXX data
    std::vector<VcdChapter>  m_chapters;

    // misc.
    std::string         m_disk_path;
};

#endif // VCDENTRIES_H
