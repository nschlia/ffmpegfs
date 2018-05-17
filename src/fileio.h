/*
 * fileio class header for ffmpegfs
 *
 * Copyright (C) 2017-2018 Norbert Schlia (nschlia@oblivion-software.de)
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

#include <sys/stat.h>
#include <string>

using namespace std;

typedef enum _tagVIRTUALTYPE
{
    VIRTUALTYPE_PASSTHROUGH,        // passthrough file, not used
    VIRTUALTYPE_REGULAR,            // Regular file to transcode
    VIRTUALTYPE_SCRIPT,             // Virtual script
    VIRTUALTYPE_BUFFER,             // Buffer file
#ifdef USE_LIBDVDNAV
    VIRTUALTYPE_DVD,                // DVD file
#endif // USE_LIBDVDNAV
} VIRTUALTYPE;
typedef VIRTUALTYPE const *LPCVIRTUALTYPE;
typedef VIRTUALTYPE LPVIRTUALTYPE;

typedef struct _tagVIRTUALFILE
{
    VIRTUALTYPE m_type;
    string      m_origfile;
    struct stat m_st;

    int title;
    int chapter;
    int angle;

} VIRTUALFILE;
typedef VIRTUALFILE const *LPCVIRTUALFILE;
typedef VIRTUALFILE *LPVIRTUALFILE;

class fileio
{
public:
    fileio();
    virtual ~fileio();

    static fileio * alloc(VIRTUALTYPE type);

    virtual VIRTUALTYPE type() const = 0;

    // Ideal buffer size, may be 0 if no recommendation.
    virtual int bufsize() const = 0;
    // Open virtual file
    virtual int open(LPCVIRTUALFILE virtualfile) = 0;
    // Open with file name
    virtual int open(const string & filename) = 0;
    // Read data
    virtual int read(void *data, int size) = 0;
    // If error occurred return number
    virtual int error() const = 0;
    // Get play time in ms
    // This is only possible for file formats that are aware
    // of the play time. May be -1 if the time is not known.
    virtual int duration() const = 0;
    // Get file size
    virtual size_t size() const = 0;
    // Get current read position
    virtual size_t tell() const = 0;
    // Seek to position
    virtual int seek(long offset, int whence) = 0;
    // Return true if at end of file
    virtual bool eof() const = 0;
    // Close virtual file
    virtual void close() = 0;
};

#endif // FILEIO_H
