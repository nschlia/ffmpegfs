/*
 * data buffer class source for FFmpegfs
 *
 * Copyright (C) 2013 K. Henriksson
 * Copyright (C) 2017-2019 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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

#include "buffer.h"
#include "ffmpegfs.h"
#include "ffmpeg_utils.h"
#include "logging.h"

#include <unistd.h>
#include <sys/mman.h>
#include <libgen.h>
#include <assert.h>

// Initially Buffer is empty. It will be allocated as needed.
Buffer::Buffer()
    : m_buffer_pos(0)
    , m_buffer_watermark(0)
    , m_is_open(false)
    , m_buffer(nullptr)
    , m_fd(-1)
{
}

// If buffer_data was never allocated, this is a no-op.
Buffer::~Buffer()
{
    release();
}

VIRTUALTYPE Buffer::type() const
{
    return VIRTUALTYPE_BUFFER;
}

int Buffer::bufsize() const
{
    return 0;   // Not applicable
}

int Buffer::openX(const std::string & filename)
{
    m_filename = filename;
    make_cachefile_name(m_cachefile, filename, params.current_format(virtualfile())->m_desttype);
    return 0;
}

bool Buffer::init(bool erase_cache)
{
    if (m_is_open)
    {
        return true;
    }

    m_is_open = true;

    bool success = true;

    std::lock_guard<std::recursive_mutex> lck (m_mutex);
    try
    {
        struct stat sb;
        size_t filesize;
        void *p;

        // Create the path to the cache file
        char *cachefile = new_strdup(m_cachefile);
        if (mktree(dirname(cachefile), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) && errno != EEXIST)
        {
            Logging::error(m_cachefile, "Error creating cache directory: %1", strerror(errno));
            delete [] cachefile;
            throw false;
        }
        errno = 0;  // reset EEXIST, error can safely be ignored here

        delete [] cachefile;

        m_buffer_size = 0;
        m_buffer = nullptr;
        m_buffer_pos = 0;
        m_buffer_watermark = 0;

        if (erase_cache)
        {
            remove_cachefile();
            errno = 0;  // ignore this error
        }

        m_fd = ::open(m_cachefile.c_str(), O_CREAT | O_RDWR, static_cast<mode_t>(0644));
        if (m_fd == -1)
        {
            Logging::error(m_cachefile, "Error opening cache file: %1", strerror(errno));
            throw false;
        }

        if (fstat(m_fd, &sb) == -1)
        {
            Logging::error(m_cachefile, "File stat failed: %1 (fd = %2)", strerror(errno), m_fd);
            throw false;
        }

        if (!S_ISREG(sb.st_mode))
        {
            Logging::error(m_cachefile, "Not a file.");
            throw false;
        }

        filesize = sb.st_size;

        if (!filesize)
        {
            // If empty set file size to 1 page
            filesize = sysconf (_SC_PAGESIZE);

            if (ftruncate(m_fd, filesize) == -1)
            {
                Logging::error(m_cachefile, "Error calling ftruncate() to 'stretch' the file: %1 (fd = %2)", strerror(errno), m_fd);
                throw false;
            }
        }
        else
        {
            m_buffer_pos = m_buffer_watermark = filesize;
        }

        p = mmap(nullptr, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
        if (p == MAP_FAILED)
        {
            Logging::error(m_cachefile,  "File mapping failed: %1 (fd = %2)", strerror(errno), m_fd);
            throw false;
        }

        m_buffer_size = filesize;
        m_buffer = static_cast<uint8_t*>(p);
    }
    catch (bool _success)
    {
        success = _success;

        if (!success)
        {
            m_is_open = false;
            if (m_fd != -1)
            {
                ::close(m_fd);
                m_fd = -1;
            }
        }
    }

    return success;
}

bool Buffer::release(int flags /*= CLOSE_CACHE_NOOPT*/)
{
    bool success = true;

    if (!m_is_open)
    {
        if (CACHE_CHECK_BIT(CLOSE_CACHE_DELETE, flags))
        {
            remove_cachefile();
            errno = 0;  // ignore this error
        }

        return true;
    }

    m_is_open       = false;

    std::lock_guard<std::recursive_mutex> lck (m_mutex);

    // Write it now to disk
    flush();

    void *p         = m_buffer;
    size_t size     = m_buffer_size;
    int fd          = m_fd;

    m_buffer        = nullptr;
    m_buffer_size   = 0;
    m_buffer_pos    = 0;
    m_fd = -1;

    if (munmap(p, size) == -1)
    {
        Logging::error(m_cachefile, "File unmapping failed: %1", strerror(errno));
        success = false;
    }

    if (ftruncate(fd, m_buffer_watermark) == -1)
    {
        Logging::error(m_cachefile, "Error calling ftruncate() to resize and close the file: %1 (fd = %2)", strerror(errno), fd);
        success = false;
    }

    ::close(fd);

    if (CACHE_CHECK_BIT(CLOSE_CACHE_DELETE, flags))
    {
        remove_cachefile();
        errno = 0;  // ignore this error
    }

    return success;
}

bool Buffer::remove_cachefile()
{
    return remove_file(m_cachefile);
}

bool Buffer::flush()
{
    std::lock_guard<std::recursive_mutex> lck (m_mutex);
    if (msync(m_buffer, m_buffer_size, MS_SYNC) == -1)
    {
        Logging::error(m_cachefile, "Could not sync to disk: %1", strerror(errno));
    }

    return true;
}

bool Buffer::clear()
{
    if (!m_is_open)
    {
        errno = EBADF;
        return false;
    }

    bool success = true;

    std::lock_guard<std::recursive_mutex> lck (m_mutex);

    m_buffer_pos = 0;
    m_buffer_watermark = 0;

    // If empty set file size to 1 page
    long filesize = sysconf (_SC_PAGESIZE);

    if (ftruncate(m_fd, filesize) == -1)
    {
        Logging::error(m_cachefile, "Error calling ftruncate() to clear the file: %1 (fd = %2)", strerror(errno), m_fd);
        success = false;
    }

    return success;
}

bool Buffer::reserve(size_t size)
{
    if (!m_is_open)
    {
        errno = EBADF;
        return false;
    }

    bool success = true;

    std::lock_guard<std::recursive_mutex> lck (m_mutex);

    if (!size)
    {
        size = m_buffer_size;
    }

    m_buffer = static_cast<uint8_t*>(mremap (m_buffer, m_buffer_size, size, MREMAP_MAYMOVE));
    if (m_buffer != nullptr)
    {
        m_buffer_size = size;
    }

    if (ftruncate(m_fd, m_buffer_size) == -1)
    {
        Logging::error(m_cachefile, "Error calling ftruncate() to resize the file: %1 (fd = %2)", strerror(errno), m_fd);
        success = false;
    }

    return ((m_buffer != nullptr) && success);
}

size_t Buffer::write(const uint8_t* data, size_t length)
{
    if (!m_is_open)
    {
        errno = EBADF;
        return 0;
    }

    std::lock_guard<std::recursive_mutex> lck (m_mutex);

    uint8_t* write_ptr = write_prepare(length);
    if (!write_ptr)
    {
        length = 0;
    }
    else
    {
        memcpy(write_ptr, data, length);
        increment_pos(length);
    }

    return length;
}

// Ensure the Buffer has sufficient space for a quantity of data and
// return a pointer where the data may be written. The position pointer
// should be updated afterward with increment_pos().
uint8_t* Buffer::write_prepare(size_t length)
{
    if (reallocate(m_buffer_pos + length))
    {
        if (m_buffer_watermark < m_buffer_pos + length)
        {
            m_buffer_watermark = m_buffer_pos + length;
        }
        return m_buffer + m_buffer_pos;
    }
    else
    {
        errno = ESPIPE;
        return nullptr;
    }
}

// Increment the location of the internal pointer. This cannot fail and so
// returns void. It does not ensure the position is valid memory because
// that is done by the write_prepare methods via reallocate.
void Buffer::increment_pos(ptrdiff_t increment)
{
    m_buffer_pos += increment;
}

int Buffer::seek(long offset, int whence)
{
    if (!m_is_open)
    {
        errno = EBADF;
        return -1;
    }

    size_t pos;

    switch (whence)
    {
    case SEEK_CUR:
    {
        pos = tell() + offset;
        break;
    }
    case SEEK_END:
    {
        if (size() > static_cast<size_t>(offset))
        {
            pos = size() - offset;
        }
        else
        {
            pos = 0;
        }
        break;
    }
    default: // SEEK_SET
    {
        pos = offset;
        break;
    }
    }

    if (pos <= size())
    {
        m_buffer_pos = pos;
        return 0;
    }
    else
    {
        m_buffer_pos = size();
        return -1;
    }
}

size_t Buffer::tell() const
{
    return m_buffer_pos;
}

int64_t Buffer::duration() const
{
    return AV_NOPTS_VALUE;  // not applicable
}

size_t Buffer::size() const
{
    return m_buffer_size;
}

size_t Buffer::buffer_watermark() const
{
    return m_buffer_watermark;
}

bool Buffer::copy(uint8_t* out_data, size_t offset, size_t bufsize)
{
    if (!m_is_open)
    {
        errno = EBADF;
        return false;
    }

    bool success = true;

    std::lock_guard<std::recursive_mutex> lck (m_mutex);

    if (size() >= offset && m_buffer != nullptr)
    {
        if (size() < offset + bufsize)
        {
            bufsize = size() - offset - 1;
        }

        memcpy(out_data, m_buffer + offset, bufsize);
    }
    else
    {
        errno = ENOMEM;
        success = false;
    }

    return success;
}

// Ensure the allocation has at least size bytes available. If not,
// reallocate memory to make more available. Fill the newly allocated memory
// with zeroes.
bool Buffer::reallocate(size_t newsize)
{
    if (newsize > size())
    {
        size_t oldsize = size();

        if (!reserve(newsize))
        {
            return false;
        }

        Logging::trace(m_filename, "Buffer reallocate: %1 -> %2.", oldsize, newsize);
    }
    return true;
}

const std::string & Buffer::filename() const
{
    return m_filename;
}

const std::string & Buffer::cachefile() const
{
    return m_cachefile;
}

const std::string & Buffer::make_cachefile_name(std::string & cachefile, const std::string & filename, const std::string & desttype)
{
    transcoder_cache_path(cachefile);

    cachefile += params.m_mountpath;
    cachefile += filename;
    cachefile += ".cache.";
    cachefile += desttype;

    return cachefile;
}

bool Buffer::remove_file(const std::string & filename)
{
    if (unlink(filename.c_str()) && errno != ENOENT)
    {
        Logging::warning(filename, "Cannot unlink the file: %1", strerror(errno));
        return false;
    }
    else
    {
        errno = 0;
        return true;
    }
}

int Buffer::read(void * /*data*/, int /*size*/)
{
    // Not implemented
    errno = EPERM;
    return -1;
}

int Buffer::error() const
{
    return errno;
}

bool Buffer::eof() const
{
    return (tell() == size());
}

void Buffer::close()
{
    release();
}
