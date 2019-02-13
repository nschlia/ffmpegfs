/*
 * fileio class header for FFmpegfs
 *
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

#ifndef FILEIO_H
#define FILEIO_H

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/stat.h>
#include <string>

typedef enum VIRTUALTYPE
{
    VIRTUALTYPE_PASSTHROUGH,        // passthrough file, not used
    VIRTUALTYPE_REGULAR,            // Regular file to transcode
    VIRTUALTYPE_SCRIPT,             // Virtual script
    VIRTUALTYPE_BUFFER,             // Buffer file
#ifdef USE_LIBVCD
    VIRTUALTYPE_VCD,                // Video CD file
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
    VIRTUALTYPE_DVD,                // DVD file
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
    VIRTUALTYPE_BLURAY,             // Bluray disk file
#endif // USE_LIBBLURAY
} VIRTUALTYPE;
typedef VIRTUALTYPE const *LPCVIRTUALTYPE;
typedef VIRTUALTYPE LPVIRTUALTYPE;

typedef struct VIRTUALFILE
{
    VIRTUALFILE()
        : m_type(VIRTUALTYPE_REGULAR)
        , m_format_idx(0)
    {

    }

    VIRTUALTYPE     m_type;         // Type of this virtual file

    int             m_format_idx;   // Index into params.format[] array
    std::string     m_origfile;     // Sanitised original file name
    struct stat     m_st;

#ifdef USE_LIBVCD
    struct VCD_CHAPTER
    {
        VCD_CHAPTER()
            : m_full_title(false)
            , m_track_no(0)
            , m_chapter_no(0)
            , m_start_pos(0)
            , m_end_pos(0)
            , m_duration(0)
        {}
        bool        m_full_title;
        int         m_track_no;
        int         m_chapter_no;
        uint64_t    m_start_pos;
        uint64_t    m_end_pos;
        int64_t     m_duration;
    }               m_vcd;
#endif //USE_LIBVCD
#ifdef USE_LIBDVD
    struct DVD_CHAPTER
    {
        DVD_CHAPTER()
            : m_full_title(false)
            , m_title_no(0)
            , m_chapter_no(0)
            , m_angle_no(0)
            , m_duration(0)
        {}
        bool        m_full_title;
        int         m_title_no;
        int         m_chapter_no;
        int         m_angle_no;
        int64_t     m_duration;
    }               m_dvd;
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
    struct BLURAY_CHAPTER
    {
        BLURAY_CHAPTER()
            : m_full_title(false)
            , m_title_no(0)
            , m_playlist_no(0)
            , m_chapter_no(0)
            , m_angle_no(0)
            , m_duration(0)
        {}
        bool        m_full_title;
        uint32_t    m_title_no;
        uint32_t    m_playlist_no;
        unsigned    m_chapter_no;
        unsigned    m_angle_no;
        int64_t     m_duration;
    }               m_bluray;
#endif // USE_LIBBLURAY

} VIRTUALFILE;
typedef VIRTUALFILE const *LPCVIRTUALFILE;
typedef VIRTUALFILE *LPVIRTUALFILE;

class FileIO
{
public:
    explicit FileIO();
    virtual ~FileIO();

    static FileIO * alloc(VIRTUALTYPE type);

    virtual VIRTUALTYPE type() const = 0;

    // Ideal buffer size, may be 0 if no recommendation.
    virtual int     bufsize() const = 0;
    // Open virtual file
    virtual int     open(LPCVIRTUALFILE virtualfile);
    // Read data
    virtual int     read(void *data, int size) = 0;
    // If error occurred return number
    virtual int     error() const = 0;
    // Get play time in ms
    // This is only possible for file formats that are aware
    // of the play time. May be -1 if the time is not known.
    virtual int64_t  duration() const = 0;
    // Get file size
    virtual size_t  size() const = 0;
    // Get current read position
    virtual size_t  tell() const = 0;
    // Seek to position
    virtual int     seek(long offset, int whence) = 0;
    // Return true if at end of file
    virtual bool    eof() const = 0;
    // Close virtual file
    virtual void    close() = 0;

    LPCVIRTUALFILE  virtualfile() const;

protected:
    // Open with file name
    virtual int         openX(const std::string & filename) = 0;

    const std::string & set_path(const std::string & path);

protected:
    std::string         m_path;

private:
    LPCVIRTUALFILE      m_virtualfile;
};

#endif // FILEIO_H
