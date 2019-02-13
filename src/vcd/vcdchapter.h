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

#ifndef VCDCHAPTER_H
#define VCDCHAPTER_H

#include <string>

typedef enum VCDTYPE
{
    VCDTYPE_UNKNOWN = -1,                             
    VCDTYPE_VCD_10_11_SVCD_10_HQVCD = 1,               
    VCDTYPE_VCD_20 = 2                                
} VCDTYPE;
typedef enum VCDPROFILETAG
{
    VCDPROFILETAG_UNKNOWN = -1,                         
    VCDPROFILETAG_VCD_10_20_SVCD_HQVCD = 0,             
    VCDPROFILETAG_VCD_11 = 1                           
} VCDPROFILETAG;

struct VCDCHAPTER;
typedef struct VCDCHAPTER VCDCHAPTER;

class VcdChapter
{
    friend class VcdEntries;

public:
    explicit VcdChapter(bool is_svcd);
    explicit VcdChapter(const VCDCHAPTER & VcdChapter, bool is_svcd);
    explicit VcdChapter(int track_no, int min, int sec, int frame, bool is_svcd, int64_t duration);
    virtual ~VcdChapter();

    bool        get_is_vcd() const;     // true for SVCD, false for VCD
    int         get_track_no() const;   // Track no
    int         get_min() const;        // MSF minute
    int         get_sec() const;        // MSF second
    int         get_frame() const;      // MSF frame
    int64_t     get_duration() const;

    std::string get_filename() const;   // File name and path (e.g. MPEG/AVSEQ##.MPG)
    uint64_t    get_start_pos() const;  // File position of chapter in bytes
    int64_t     get_start_time() const; // Start position of chapter in AV_TIME_BASE units
    uint64_t    get_end_pos() const;    // End position of chapter in bytes
    uint64_t    get_size() const;       // Get chapter size
    int         get_lba() const;        // LBA (large block address)

    VcdChapter & operator= (VcdChapter const & other);

    int operator==(const VcdChapter & other) const;
    int operator<(const VcdChapter & other) const;
    int operator<=(const VcdChapter & other) const;
    int operator>(const VcdChapter & other) const;
    int operator>=(const VcdChapter & other) const;
    int operator!=(const VcdChapter & other) const;

protected:
    int         read(FILE *fpi, int track_no);

protected:
    bool        m_is_svcd;
    int         m_track_no;
    int         m_min;
    int         m_sec;
    int         m_frame;
    int64_t     m_duration;
    uint64_t    m_start_pos;
    uint64_t    m_end_pos;
};

#endif // VCDCHAPTER_H
