// -------------------------------------------------------------------------------
//  Project:		Bully's Media Player
//
//  File:           VcdChapter.h
//
// (c) 1984-2017 by Oblivion Software/Norbert Schlia
// All rights reserved.
// -------------------------------------------------------------------------------
//
#pragma once

#ifndef VCDCHAPTER_H
#define VCDCHAPTER_H

#include <string>

struct VCDCHAPTER;
typedef struct VCDCHAPTER VCDCHAPTER;

class VcdChapter
{
    friend class VcdEntries;

public:
    explicit VcdChapter(bool is_svcd);
    explicit VcdChapter(const VCDCHAPTER & VcdChapter, bool is_svcd);
    explicit VcdChapter(int track_no, int min, int sec, int frame, bool is_svcd);
    virtual ~VcdChapter();

    bool        get_is_vcd() const;     // true for SVCD, false for VCD
    int         get_track_no() const;   // Track no
    int         get_min() const;        // MSF minute
    int         get_sec() const;        // MSF second
    int         get_frame() const;      // MSF frame

    std::string get_filename() const;   // File name and path (e.g. MPEG/AVSEQ##.MPG)
    uint64_t    get_start_pos() const;  // File position of chapter in bytes
    uint64_t    get_end_pos() const;    // End position of chapter in bytes
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
    uint64_t    m_start_pos;
    uint64_t    m_end_pos;
};

#endif // VCDCHAPTER_H
