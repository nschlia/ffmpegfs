/*
 * cache class header for FFmpegfs
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

#ifndef BUFFER_H
#define BUFFER_H

#pragma once

#include "fileio.h"

#include <mutex>
#include <stddef.h>

#define CACHE_CHECK_BIT(mask, var)  ((mask) == (mask & (var)))

#define CLOSE_CACHE_NOOPT   0x00                        // Dummy, do nothing special
#define CLOSE_CACHE_FREE    0x01                        // Free memory for cache entry
#define CLOSE_CACHE_DELETE  (0x02 | CLOSE_CACHE_FREE)   // Delete cache entry, will unlink cached file! Implies CLOSE_CACHE_FREE.

class Buffer : public FileIO
{
public:
    explicit Buffer();
    virtual ~Buffer();

    virtual VIRTUALTYPE type() const;

    bool                    init(bool erase_cache = false);                         // Initialise cache, if erase_cache = true delete old file before opening
    bool                    release(int flags = CLOSE_CACHE_NOOPT);

    virtual size_t          bufsize() const;
    virtual size_t          read(void *data, size_t size);                          // Not implemented
    virtual int             error() const;
    virtual int64_t         duration() const;
    virtual size_t          size() const;                                           // Give the value of the internal buffer size pointer.
    virtual size_t          tell() const;                                           // Give the value of the internal read position pointer.
    virtual int             seek(long offset, int whence);
    virtual bool            eof() const;
    virtual void            close();

    size_t                  write(const uint8_t* data, size_t length);              // Write data to the current position in the Buffer. The position pointer will be updated.
    bool                    flush();
    bool                    clear();
    bool                    reserve(size_t size);                                   // Reserve memory without changing size to reduce re-allocations
    size_t                  buffer_watermark() const;                               // Number of bytes written to buffer so far (may be less than size())
    bool                    copy(uint8_t* out_data, size_t offset, size_t bufsize); // Copy buffered data into output buffer.

    const std::string &     filename() const;
    const std::string &     cachefile() const;

    static const std::string & make_cachefile_name(std::string &cachefile, const std::string & filename, const std::string &desttype);  // Make up a cache file name including full path
    static bool             remove_file(const std::string & filename);

protected:
    virtual int             openX(const std::string & filename);

    bool                    remove_cachefile();

private:
    uint8_t*                write_prepare(size_t length);
    void                    increment_pos(size_t increment);
    bool                    reallocate(size_t newsize);

private:
    std::recursive_mutex    m_mutex;
    std::string             m_filename;
    std::string             m_cachefile;
    size_t                  m_buffer_pos;           // Read/write position
    size_t                  m_buffer_watermark;     // Number of bytes in buffer
    volatile bool           m_is_open;
    size_t                  m_buffer_size;          // Current buffer size
    uint8_t *               m_buffer;
    int                     m_fd;
};

#endif
