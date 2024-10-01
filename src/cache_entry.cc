/*
 * Copyright (C) 2017-2024 Norbert Schlia (nschlia@oblivion-software.de)
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * On Debian systems, the complete text of the GNU General Public License
 * Version 3 can be found in `/usr/share/common-licenses/GPL-3'.
 */

/**
 * @file cache_entry.cc
 * @brief Cache_Entry class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2024 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "cache_entry.h"
#include "ffmpegfs.h"
#include "buffer.h"
#include "logging.h"

#include <cstring>

Cache_Entry::Cache_Entry(Cache *owner, LPVIRTUALFILE virtualfile)
    : m_owner(owner)
    , m_ref_count(0)
    , m_virtualfile(virtualfile)
    , m_is_decoding(false)
    , m_suspend_timeout(false)
    , m_seek_to_no(0)
{
    m_cache_info.m_origfile = virtualfile->m_origfile;
    m_cache_info.m_destfile = virtualfile->m_destfile;

    m_cache_info.m_desttype[0] = '\0';
    strncat(m_cache_info.m_desttype.data(), params.current_format(virtualfile)->desttype().c_str(), m_cache_info.m_desttype.size() - 1);

    m_buffer = new(std::nothrow) Buffer;

    if (m_buffer != nullptr)
    {
        m_buffer->openio(virtualfile);
    }

    clear();

    Logging::trace(filename(), "Created a new cache entry.");
}

Cache_Entry::~Cache_Entry()
{
    std::unique_lock<std::recursive_mutex> lock_active_mutex(m_active_mutex);

    delete m_buffer;

    unlock();

    Logging::trace(filename(), "Deleted buffer.");
}

Cache_Entry * Cache_Entry::create(Cache *owner, LPVIRTUALFILE virtualfile)
{
    return new(std::nothrow) Cache_Entry(owner, virtualfile);
}

bool Cache_Entry::destroy()
{
    delete this;    /** @todo Implement delete later mechanism */
    return true;    /** @todo Return true when deleted, false if kept for delete later */
}

void Cache_Entry::clear(bool fetch_file_time /*= true*/)
{
    m_is_decoding = false;

    // Initialise ID3v1.1 tag structure
    init_id3v1(&m_id3v1);

    //m_cache_info.m_enable_ismv        = params.m_enable_ismv;
    m_cache_info.m_audiobitrate         = params.m_audiobitrate;
    m_cache_info.m_audiosamplerate      = params.m_audiosamplerate;
    m_cache_info.m_videobitrate         = params.m_videobitrate;
    m_cache_info.m_videowidth           = params.m_videowidth;
    m_cache_info.m_videoheight          = params.m_videoheight;
    m_cache_info.m_deinterlace          = params.m_deinterlace;
    m_cache_info.m_predicted_filesize   = 0;
    m_cache_info.m_encoded_filesize     = 0;
    m_cache_info.m_video_frame_count    = 0;
    m_cache_info.m_result               = RESULTCODE_NONE;
    m_cache_info.m_error                = false;
    m_cache_info.m_errno                = 0;
    m_cache_info.m_averror              = 0;
    m_cache_info.m_access_time          = m_cache_info.m_creation_time = time(nullptr);
    m_cache_info.m_access_count         = 0;

    if (fetch_file_time)
    {
        struct stat sb;

        if (stat(filename(), &sb) == -1)
        {
            m_cache_info.m_file_time    = 0;
            m_cache_info.m_file_size    = 0;
        }
        else
        {
            m_cache_info.m_file_time    = sb.st_mtime;
            m_cache_info.m_file_size    = static_cast<size_t>(sb.st_size);
        }
    }

    if (m_buffer != nullptr)
    {
        m_buffer->clear();
    }
}

bool Cache_Entry::read_info()
{
    return m_owner->read_info(&m_cache_info);
}

bool Cache_Entry::write_info()
{
    return m_owner->write_info(&m_cache_info);
}

bool Cache_Entry::delete_info()
{
    return m_owner->delete_info(filename(), m_cache_info.m_desttype.data());
}

bool Cache_Entry::update_access(bool update_database /*= false*/)
{
    m_cache_info.m_access_time = time(nullptr);

    if (update_database)
    {
        return m_owner->write_info(&m_cache_info);
    }
    else
    {
        return true;
    }
}

bool Cache_Entry::openio(bool create_cache /*= true*/)
{
    if (m_buffer == nullptr)
    {
        errno = EINVAL;
        return false;
    }

    if (m_ref_count++ > 0)	// fetch_and_add
    {
        // Already open
        return true;
    }

    bool erase_cache = !read_info();    // If read_info fails, rebuild cache entry

    if (!create_cache)
    {
        return true;
    }

    if (!is_finished())
    {
        // If no database entry found (database is not consistent),
        // or file was not completely transcoded last time,
        // simply create a new file.
        erase_cache = true;
    }

    Logging::trace(filename(), "The last transcode was completed. Result %1 Erasing cache: %2.", m_cache_info.m_result, erase_cache);

    // Store access time
    update_access(true);

    // Open the cache
    if (m_buffer->init(erase_cache))
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
    if (m_buffer->release(flags))
    {
        if (flags)
        {
            delete_info();
        }
    }
}

bool Cache_Entry::closeio(int flags)
{
    write_info();

    if (m_buffer == nullptr)
    {
        errno = EINVAL;
        return false;
    }

    if (!m_ref_count)
    {
        close_buffer(flags);

        return true;
    }

    if (--m_ref_count > 0)	// sub_and_fetch
    {
        // Just flush to disk
        flush();
        return false;
    }

    close_buffer(flags);

    return true;
}

bool Cache_Entry::flush()
{
    if (m_buffer == nullptr)
    {
        errno = EINVAL;
        return false;
    }

    m_buffer->flush();
    //write_info();

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
        if (m_buffer == nullptr)
        {
            return m_cache_info.m_predicted_filesize;
        }
        else
        {
            size_t current_size = m_buffer->buffer_watermark();

            return std::max(current_size, m_cache_info.m_predicted_filesize);
        }
    }
}

uint32_t Cache_Entry::video_frame_count() const
{
    return m_cache_info.m_video_frame_count;
}

time_t Cache_Entry::age() const
{
    return (time(nullptr) - m_cache_info.m_creation_time);
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
    return (((time(nullptr) - m_cache_info.m_access_time) >= params.m_max_inactive_suspend) && m_ref_count <= 1);
}

bool Cache_Entry::decode_timeout() const
{
    return (((time(nullptr) - m_cache_info.m_access_time) >= params.m_max_inactive_abort) && m_ref_count <= 1);
}

const char * Cache_Entry::filename() const
{
    return (m_virtualfile != nullptr ? m_virtualfile->m_origfile.c_str() : "");
}

const char *Cache_Entry::destname() const
{
    return (m_virtualfile != nullptr ? m_virtualfile->m_destfile.c_str() : "");
}

const char * Cache_Entry::virtname() const
{
    return (m_virtualfile != nullptr ? m_virtualfile->m_virtfile.c_str() : "");
}

void Cache_Entry::lock()
{
    m_mutex.lock();
}

void Cache_Entry::unlock()
{
    m_mutex.unlock();
}

int Cache_Entry::ref_count() const
{
    return m_ref_count;
}

int Cache_Entry::inc_refcount()
{
    return m_ref_count++;	// fetch_and_add
}

int Cache_Entry::decr_refcount()
{
    return --m_ref_count;	// sub_and_fetch
}

bool Cache_Entry::outdated() const
{
    struct stat sb;

    if (m_cache_info.m_audiobitrate != params.m_audiobitrate)
    {
        if (m_cache_info.m_audiobitrate)
        {
            // Report if old rate is known
            Logging::debug(filename(), "Triggering re-transcode: Selected audio bitrate changed from %1 to %2.", m_cache_info.m_audiobitrate, params.m_audiobitrate);
        }
        return true;
    }

    if (m_cache_info.m_audiosamplerate != params.m_audiosamplerate)
    {
        if (m_cache_info.m_audiosamplerate)
        {
            // Report if old rate is known
            Logging::debug(filename(), "Triggering re-transcode: Selected audio samplerate changed from %1 to %2.", m_cache_info.m_audiosamplerate, params.m_audiosamplerate);
        }
        return true;
    }

    if (m_cache_info.m_videobitrate != params.m_videobitrate)
    {
        if (m_cache_info.m_videobitrate)
        {
            // Report if old rate is known
            Logging::debug(filename(), "Triggering re-transcode: Selected video bitrate changed from %1 to %2.", m_cache_info.m_audiobitrate, params.m_audiobitrate);
        }
        return true;
    }

    if (m_cache_info.m_videowidth != params.m_videowidth || m_cache_info.m_videoheight != params.m_videoheight)
    {
        if (m_cache_info.m_videowidth && m_cache_info.m_videoheight)
        {
            // Report if old dimensions is known
            Logging::debug(filename(), "Triggering re-transcode: Selected video witdh/height changed from %1/%2 to %3/%4.", m_cache_info.m_videowidth, m_cache_info.m_videoheight, params.m_videowidth, params.m_videoheight);
        }
        return true;
    }

    if (m_cache_info.m_deinterlace != (params.m_deinterlace ? true : false))
    {
        Logging::debug(filename(), "Triggering re-transcode: Selected video deinterlace changed from %1 to %2.", m_cache_info.m_deinterlace, params.m_deinterlace);
        return true;
    }

    if (stat(filename(), &sb) != -1)
    {
        // If source file exists, check file date/size
        if (m_cache_info.m_file_time < sb.st_mtime)
        {
            Logging::debug(filename(), "Triggering re-transcode: File time has gone forward.");
            return true;
        }

        if (m_cache_info.m_file_size != static_cast<size_t>(sb.st_size))
        {
            Logging::debug(filename(), "Triggering re-transcode: File size has changed.");
            return true;
        }
    }

    return false;
}

LPVIRTUALFILE Cache_Entry::virtualfile()
{
    return m_virtualfile;
}

void Cache_Entry::update_read_count()
{
    m_cache_info.m_access_count++;
}

unsigned int Cache_Entry::read_count() const
{
    return m_cache_info.m_access_count;
}

bool Cache_Entry::is_finished() const
{
    return (m_cache_info.m_result != RESULTCODE_NONE);
}

bool Cache_Entry::is_finished_incomplete() const
{
    return (m_cache_info.m_result == RESULTCODE_FINISHED_INCOMPLETE);
}

bool Cache_Entry::is_finished_success() const
{
    return (m_cache_info.m_result == RESULTCODE_FINISHED_SUCCESS);
}

bool Cache_Entry::is_finished_error() const
{
    return (m_cache_info.m_result == RESULTCODE_FINISHED_ERROR);
}
