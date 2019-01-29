/*
 * FFmpeg transcoder class header for FFmpegfs
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

#ifndef CACHE_ENTRY_H
#define CACHE_ENTRY_H

#pragma once

#include "cache.h"

#include "id3v1tag.h"

class Buffer;

class Cache_Entry
{
private:
    explicit Cache_Entry(Cache *owner, LPVIRTUALFILE virtualfile);
    virtual ~Cache_Entry();

public:
    static Cache_Entry *    create(Cache *owner, LPVIRTUALFILE virtualfile);
    bool                    destroy();

    bool                    open(bool create_cache = true);
    bool                    flush();
    void                    clear(int fetch_file_time = true);
    size_t                  size() const;
    time_t                  age() const;
    time_t                  last_access() const;
    bool                    expired() const;
    bool                    suspend_timeout() const;
    bool                    decode_timeout() const;
    const std::string &     filename() const;
    const std::string &     destname() const;
    bool                    update_access(bool bUpdateDB = false);

    void                    lock();
    void                    unlock();

    int                     ref_count() const;

    // Check if cache entry needs to be recoded
    bool                    outdated() const;

    LPVIRTUALFILE           virtualfile();

    // Returns true if entry may be deleted, false if still in use
    bool                    close(int flags);

protected:
    void                    close_buffer(int flags);

    bool                    read_info();
    bool                    write_info();
    bool                    delete_info();

protected:
    Cache *                 m_owner;
    std::recursive_mutex    m_mutex;

    int                     m_ref_count;

    LPVIRTUALFILE           m_virtualfile;

public:
    Buffer *                m_buffer;
    bool                    m_is_decoding;
    pthread_t               m_thread_id;

    t_cache_info            m_cache_info;

    ID3v1                   m_id3v1;
};

#endif // CACHE_ENTRY_H
