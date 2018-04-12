/*
 * Cache entry object class for ffmpegfs
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

#include "cache_entry.h"
#include "ffmpeg_transcoder.h"
#include "buffer.h"
#include "transcode.h"

using namespace std;

Cache_Entry::Cache_Entry(Cache *owner, const string & filename)
    : m_owner(owner)
    , m_mutex(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
    , m_ref_count(0)
    , m_thread_id(0)
{
    m_cache_info.m_filename = filename;

    m_cache_info.m_desttype[0] = '\0';
    strncat(m_cache_info.m_desttype, params.m_desttype, sizeof(m_cache_info.m_desttype) - 1);

    m_buffer = new Buffer(m_cache_info.m_filename);

    clear();

    ffmpegfs_trace("%s * Created new cache entry.", m_cache_info.m_filename.c_str());
}

Cache_Entry::~Cache_Entry()
{
    if (m_thread_id && m_thread_id != pthread_self())
    {
        // If not same thread, wait for other to finish
        ffmpegfs_warning("%s * Waiting for thread id %" FFMPEGFS_FORMAT_PTHREAD_T " to terminate.", m_cache_info.m_filename.c_str(), m_thread_id);

        int s = pthread_join(m_thread_id, NULL);
        if (s != 0)
        {
            ffmpegfs_error("%s * Error joining thread id %" FFMPEGFS_FORMAT_PTHREAD_T " : %s", m_cache_info.m_filename.c_str(), m_thread_id, strerror(s));
        }
        else
        {
            ffmpegfs_info("%s * Thread id %" FFMPEGFS_FORMAT_PTHREAD_T " has terminated.", m_cache_info.m_filename.c_str(), m_thread_id);
        }
    }

    delete m_buffer;

    ffmpegfs_trace("%s * Deleted buffer.", m_cache_info.m_filename.c_str());
}

void Cache_Entry::clear(int fetch_file_time)
{
    struct stat sb;

    m_is_decoding = false;

    // Initialise ID3v1.1 tag structure
    init_id3v1(&m_id3v1);

    //string          m_filename;
    //char            m_desttype[11];
	
    //m_cache_info.m_enable_ismv = params.m_enable_ismv;
    m_cache_info.m_audiobitrate = params.m_audiobitrate;
    m_cache_info.m_audiosamplerate = params.m_audiosamplerate;
    m_cache_info.m_videobitrate = params.m_videobitrate;
    m_cache_info.m_videowidth = params.m_videowidth;
    m_cache_info.m_videoheight = params.m_videoheight;
    m_cache_info.m_deinterlace = params.m_deinterlace;
    m_cache_info.m_predicted_filesize = 0;
    m_cache_info.m_encoded_filesize = 0;
    m_cache_info.m_finished = false;
    m_cache_info.m_error = false;
    m_cache_info.m_errno = 0;
    m_cache_info.m_averror = 0;
    m_cache_info.m_access_time = m_cache_info.m_creation_time = time(NULL);

    if (fetch_file_time)
    {
        if (stat(filename().c_str(), &sb) == -1)
        {
            m_cache_info.m_file_time = 0;
            m_cache_info.m_file_size = 0;
        }
        else
        {
            m_cache_info.m_file_time = sb.st_mtime;
            m_cache_info.m_file_size = sb.st_size;
        }
    }

    if (m_buffer != NULL)
    {
        m_buffer->clear();
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
    return m_owner->delete_info(m_cache_info.m_filename, m_cache_info.m_desttype);
}

bool Cache_Entry::update_access(bool bUpdateDB /*= false*/)
{
    m_cache_info.m_access_time = time(NULL);

    if (bUpdateDB)
    {
        return m_owner->write_info(m_cache_info);
    }
    else
    {
        return true;
    }
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

    bool erase_cache = !read_info();    // If read_info fails, rebuild cache entry

    if (!create_cache)
    {
        return true;
    }

    if (!m_cache_info.m_finished)
    {
        // If no database entry found (database is not consistent),
        // or file was not completely transcoded last time,
        // simply create a new file.
        erase_cache = true;
    }

    ffmpegfs_trace("%s * Last transcode finished: %i Erase cache: %i.", m_cache_info.m_filename.c_str(), m_cache_info.m_finished, erase_cache);

    // Store access time
    update_access(true);

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

void Cache_Entry::close_buffer(int flags)
{
    if (m_buffer->close(flags))
    {
        if (flags)
        {
            delete_info();
        }
    }
}

// Returns true if entry may be deleted, false if still in use
bool Cache_Entry::close(int flags)
{
    write_info();

    if (m_buffer == NULL)
    {
        errno = EINVAL;
        return false;
    }

    //lock();

    if (!m_ref_count)
    {
        close_buffer(flags);

        //unlock();

        return true;
    }

    if (__sync_sub_and_fetch(&m_ref_count, 1) > 0)
    {
        // Just flush to disk
        flush();
        return false;
    }

    close_buffer(flags);

    //unlock();

    return true;
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

size_t Cache_Entry::size() const
{
    if (m_cache_info.m_encoded_filesize)
    {
        return m_cache_info.m_encoded_filesize;
    }
    else
    {
        if (m_buffer == NULL)
        {
            return m_cache_info.m_predicted_filesize;
        }
        else
        {
            size_t current_size = m_buffer->buffer_watermark();

            return max(current_size, m_cache_info.m_predicted_filesize);
        }
    }
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
    return (age() > params.m_expiry_time);
}

bool Cache_Entry::suspend_timeout() const
{
    return ((time(NULL) - m_cache_info.m_access_time) > params.m_max_inactive_suspend);
}

bool Cache_Entry::decode_timeout() const
{
    return ((time(NULL) - m_cache_info.m_access_time) > params.m_max_inactive_abort);
}

const string & Cache_Entry::filename()
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

int Cache_Entry::ref_count() const
{
    return m_ref_count;
}

