/*
 * Cache entry object class for mp3fs
 *
 * Copyright (c) 2017 by Norbert Schlia (nschlia@oblivion-software.de)
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

#include "cache_entry.h"
#include "ffmpeg_transcoder.h"
#include "buffer.h"
#include "transcode.h"

#include <unistd.h>
#include <fstream>
#include <iostream>
#include <libgen.h>
#include <assert.h>

using namespace std;

Cache_Entry::Cache_Entry(Cache *owner, const std::string & filename)
    : m_owner(owner)
    , m_mutex(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
    , m_thread_id(0)
    , m_ref_count(0)
    , m_transcoder(new FFMPEG_Transcoder)
{
	m_cache_info.m_filename = filename;
    
    m_cache_info.m_target_format[0] = '\0';
    strncat(m_cache_info.m_target_format, params.desttype, sizeof(m_cache_info.m_target_format) - 1);

    m_buffer = new Buffer(m_cache_info.m_filename);

    clear();

    mp3fs_debug("Created new Cache_Entry.");
}

Cache_Entry::~Cache_Entry()
{
    if (m_thread_id && m_thread_id != pthread_self())
    {
        // If not same thread, wait for other to finish
        mp3fs_debug("Waiting for thread id %zx to terminate.", m_thread_id);

        int s = pthread_join(m_thread_id, NULL);
        if (s != 0)
        {
            mp3fs_error("Error joining thread id %zu: %s", m_thread_id, strerror(s));
        }
        else
        {
            mp3fs_debug("Thread id %zx has terminated.", m_thread_id);
        }
    }

    //assert(errno == 0); // DEBUG

    delete m_buffer;
    delete m_transcoder;
    mp3fs_debug("Deleted Cache_Entry.");
}

void Cache_Entry::clear(int fetch_file_time)
{
    struct stat sb;

    m_is_decoding = false;

    m_cache_info.m_encoded_filesize = 0;
    m_cache_info.m_finished = false;
    m_cache_info.m_access_time = m_cache_info.m_creation_time = time(NULL);
    m_cache_info.m_error = false;

    if (fetch_file_time) {
        if (stat(filename().c_str(), &sb) == -1) {
            m_cache_info.m_file_time = 0;
            m_cache_info.m_file_size = 0;
        }
        else {
            m_cache_info.m_file_time = sb.st_mtime;
            m_cache_info.m_file_size = sb.st_size;
        }
    }
}

bool Cache_Entry::read_info()
{
    return m_owner->read_info(m_cache_info);
}

bool Cache_Entry::write_info()
{
    return m_owner->write_info(m_cache_info);
}

bool Cache_Entry::delete_info()
{
    return m_owner->delete_info(m_cache_info);
}

void Cache_Entry::update_access()
{
    m_cache_info.m_access_time = time(NULL);
}

bool Cache_Entry::open(bool create_cache /*= true*/)
{
    if (m_buffer == NULL)
    {
        errno = EINVAL;
        return false;
    }

    if (__sync_fetch_and_add(&m_ref_count, 1) > 0)
    {
        return true;
    }

    bool erase_cache = !read_info();

    if (!create_cache)
    {
        return true;
    }

    // Open the cache
    if (m_buffer->open(erase_cache))
    {
        return true;
    }
    else
    {
        clear(false);
        return false;
    }
}

bool Cache_Entry::close(bool erase_cache /*= false*/)
{    
    write_info();

    if (m_buffer == NULL)
    {
        errno = EINVAL;
        return false;
    }

    if (!m_ref_count)
    {
        return true;
    }

    if (__sync_sub_and_fetch(&m_ref_count, 1) > 0)
    {
        // Just flush to disk
        flush();
        return true;
    }

    if (m_buffer->close(erase_cache))
    {
        if (erase_cache)
        {
            return delete_info();
        }
        else
        {
            return true;
        }
    }
    else
    {
        return false;
    }
}

bool Cache_Entry::flush()
{
    if (m_buffer == NULL)
    {
        errno = EINVAL;
        return false;
    }

    m_buffer->flush();
    write_info();

    return true;
}

time_t Cache_Entry::mtime() const
{
    return m_transcoder->mtime();
}

size_t Cache_Entry::calculate_size() const
{
    return m_transcoder->calculate_size();
}

const ID3v1 * Cache_Entry::id3v1tag() const
{
    return m_transcoder->id3v1tag();
}

time_t Cache_Entry::age() const
{
    return (time(NULL) - m_cache_info.m_creation_time);
}

time_t Cache_Entry::last_access() const
{
    return m_cache_info.m_access_time;
}

bool Cache_Entry::expired() const
{
    return (age() > params.expiry_time);
}

bool Cache_Entry::suspend_timeout() const
{
    return ((time(NULL) - m_cache_info.m_access_time) > params.max_inactive_suspend);
}

bool Cache_Entry::decode_timeout() const
{
    return ((time(NULL) - m_cache_info.m_access_time) > params.max_inactive_abort);
}

const std::string & Cache_Entry::filename()
{
    return m_cache_info.m_filename;
}

void Cache_Entry::lock()
{
    pthread_mutex_lock(&m_mutex);
}

void Cache_Entry::unlock()
{
    pthread_mutex_unlock(&m_mutex);
}
