/*
 * Copyright (C) 2017-2023 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file buffer.cc
 * @brief Buffer class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2023 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "buffer.h"
#include "ffmpegfs.h"
#include "ffmpeg_utils.h"
#include "logging.h"

#include <unistd.h>
#include <sys/mman.h>
#include <libgen.h>

// Initially Buffer is empty. It will be allocated as needed.
Buffer::Buffer()
    : m_cur_ci(nullptr)
    , m_cur_open(0)
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

size_t Buffer::bufsize() const
{
    return 0;   // Not applicable
}

int Buffer::openio(LPVIRTUALFILE virtualfile)
{
    if (virtualfile == nullptr)
    {
        errno = EINVAL;
        return (EOF);
    }

    set_virtualfile(virtualfile);

    return 0;
}

bool Buffer::open_file(uint32_t segment_no, uint32_t flags, size_t defaultsize)
{
    uint32_t index = segment_no;
    if (index)
    {
        index--;
    }

    m_ci[index].m_flags |= flags;

    if (m_ci[index].m_fd != -1)
    {
        Logging::trace(m_ci[index].m_cachefile, "Cache file is already open.");

        if (defaultsize)
        {
			// Make sure the requested size is available
            reserve(defaultsize);
        }
        // Already open
        return true;
    }

    if (flags & CACHE_FLAG_RW)
    {
        Logging::debug(m_ci[index].m_cachefile, "Writing to cache file.");
    }
    else
    {
        Logging::debug(m_ci[index].m_cachefile, "Reading from cache file.");
    }

    size_t filesize     = 0;
    bool isdefaultsize  = false;
    uint8_t *p          = nullptr;

    if (!map_file(m_ci[index].m_cachefile, &m_ci[index].m_fd, &p, &filesize, &isdefaultsize, defaultsize, (flags & CACHE_FLAG_RW) ? true : false))
    {
        return false;
    }

    if (!isdefaultsize)
    {
        m_ci[index].m_buffer_pos = m_ci[index].m_buffer_watermark = filesize;
    }

    m_ci[index].m_buffer_size       = filesize;
    m_ci[index].m_buffer            = static_cast<uint8_t*>(p);
    m_ci[index].m_buffer_write_size = 0;
    m_ci[index].m_buffer_writes     = 0;

    ++m_cur_open;   // track open files

    return true;
}

bool Buffer::close_file(uint32_t segment_no, uint32_t flags)
{
    uint32_t index = segment_no;
    if (index)
    {
        index--;
    }

    m_ci[index].m_flags &= ~flags;

    if (m_ci[index].m_flags)
    {
        Logging::trace(m_ci[index].m_cachefile, "While attempting to close, the cache file is still in use. Currently open: %1", m_cur_open);
        return true;
    }

    if (m_ci[index].m_fd == -1)
    {
        // Already closed
        Logging::trace(m_ci[index].m_cachefile, "No need to close the unopened cache file. Currently open: %1", m_cur_open);
        return true;
    }

    Logging::trace(m_ci[index].m_cachefile, "Closing cache file.");

    bool success = unmap_file(m_ci[index].m_cachefile, &m_ci[index].m_fd, &m_ci[index].m_buffer, m_ci[index].m_buffer_size, &m_ci[index].m_buffer_watermark);

    m_ci[index].m_buffer_pos    = 0;
    m_ci[index].m_buffer_size   = 0;

    if (success && m_cur_open > 0)
    {
        --m_cur_open;   // track open files
    }

    return success;
}

bool Buffer::init(bool erase_cache)
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    if (is_open())
    {
        return true;
    }

    bool success = true;

    try
    {
        if ((virtualfile()->m_flags & VIRTUALFLAG_HLS))
        {
            // HLS format: create several segments
            if (virtualfile()->get_segment_count())
            {
                m_ci.resize(virtualfile()->get_segment_count());

                for (uint32_t segment_no = 1; segment_no <= virtualfile()->get_segment_count(); segment_no++)
                {
                    make_cachefile_name(m_ci[segment_no - 1].m_cachefile, filename() + "." + make_filename(segment_no, params.current_format(virtualfile())->fileext()), params.current_format(virtualfile())->fileext(), false);
                }
            }
            else
            {
                Logging::error(filename(), "INTERNAL ERROR: Buffer::init()! Segment count is 0.");
                errno = EINVAL;
                throw false;
            }
        }
        else
        {
            // All other formats: create just a single segment.
            m_ci.resize(1);

            make_cachefile_name(m_ci[0].m_cachefile, filename(), params.current_format(virtualfile())->fileext(), false);
            if ((virtualfile()->m_flags & VIRTUALFLAG_FRAME))
            {
                // Create extra index cash for frame sets only
                make_cachefile_name(m_ci[0].m_cachefile_idx, filename(), params.current_format(virtualfile())->fileext(), true);
            }
        }

        // Set current segment
        m_cur_ci = &m_ci[0];

        // Create the path to the cache file. All paths are the same, so this is required only once.
        char *cachefile = new_strdup(m_ci[0].m_cachefile);

        if (cachefile == nullptr)
        {
            Logging::error(m_ci[0].m_cachefile, "Error opening the cache file: out of memory.");
            errno = ENOMEM;
            throw false;
        }

        if (mktree(dirname(cachefile), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) && errno != EEXIST)
        {
            Logging::error(m_ci[0].m_cachefile, "Error creating cache directory: (%1) %2", errno, strerror(errno));
            delete [] cachefile;
            throw false;
        }
        errno = 0;  // reset EEXIST, error can safely be ignored here

        delete [] cachefile;

        for (uint32_t index = 0; index < segment_count(); index++)
        {
            m_ci[index].m_fd                = -1;
            m_ci[index].m_buffer            = nullptr;
            m_ci[index].m_buffer_pos        = 0;
            m_ci[index].m_buffer_watermark  = 0;
            m_ci[index].m_buffer_size       = 0;
            m_ci[index].m_buffer_write_size = 0;
            m_ci[index].m_buffer_writes     = 0;

            if (erase_cache)
            {
                remove_cachefile(index + 1);
                errno = 0;  // ignore this error
            }
        }

        // Index only required for frame sets and there is only one.
        if (!m_ci[0].m_cachefile_idx.empty())
        {
            if (virtualfile()->m_video_frame_count == 0)
            {
                errno = EINVAL;
                Logging::error(m_ci[0].m_cachefile, "INTERNAL ERROR: Buffer::init()! Frame count is zero (%1) %2", errno, strerror(errno));
                throw false;
            }
            static_assert(sizeof(IMAGE_FRAME) == 32, "sizeof(IMAGE_FRAME) must be 32 bytes");

            size_t filesize     = 0;
            bool isdefaultsize  = false;
            uint8_t *p          = nullptr;

            if (!map_file(m_ci[0].m_cachefile_idx, &m_ci[0].m_fd_idx, &p, &filesize, &isdefaultsize, sizeof(IMAGE_FRAME) * virtualfile()->m_video_frame_count, false))
            {
                throw false;
            }

            m_ci[0].m_buffer_size_idx     = filesize;
            m_ci[0].m_buffer_idx          = static_cast<uint8_t*>(p);
        }
    }
    catch (bool _success)
    {
        success = _success;

        if (!success)
        {
            for (uint32_t index = 0; index < segment_count(); index++)
            {
                m_ci[index].m_fd                = -1;
                m_ci[index].m_buffer            = nullptr;
                m_ci[index].m_buffer_pos        = 0;
                m_ci[index].m_buffer_watermark  = 0;
                m_ci[index].m_buffer_size       = 0;
                m_ci[index].m_buffer_write_size = 0;
                m_ci[index].m_buffer_writes     = 0;
            }
        }
    }

    return success;
}

bool Buffer::set_segment(uint32_t segment_no, size_t size)
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    if (!segment_no || segment_no > segment_count())
    {
        errno = EINVAL;
        return false;
    }

    if (!close_file(current_segment_no(), CACHE_FLAG_RW))
    {
        return false;
    }

    if (!open_file(segment_no, CACHE_FLAG_RW, size))
    {
        return false;
    }

    m_cur_ci = &m_ci[segment_no - 1];

    // Reserve enough buffer space for segment to avoid frequent resizes
    return reserve(size);
}

uint32_t Buffer::segment_count()
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    return static_cast<uint32_t>(m_ci.size());
}

uint32_t Buffer::current_segment_no()
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    if (!segment_count() || m_cur_ci == nullptr)
    {
        return 0;
    }
    return static_cast<uint32_t>(m_cur_ci - &m_ci[0]) + 1;
}

bool Buffer::segment_exists(uint32_t segment_no)
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    if (!segment_count() || m_cur_ci == nullptr)
    {
        return false;
    }
    return file_exists(m_ci[segment_no - 1].m_cachefile);
}

bool Buffer::map_file(const std::string & filename, int *fd, uint8_t **p, size_t *filesize, bool *isdefaultsize, size_t defaultsize, bool truncate) const
{
    bool success = true;

    if (!defaultsize)
    {
        Logging::trace(filename, "Mapping cache file.");
    }
    else
    {
        Logging::trace(filename, "Mapping cache file with %1.", format_size(defaultsize).c_str());
    }

    try
    {
        struct stat sb;

        *fd = ::open(filename.c_str(), O_CREAT | O_RDWR | (truncate ? O_TRUNC : 0), static_cast<mode_t>(0644));
        if (*fd == -1)
        {
            Logging::error(filename, "The cache file could not be opened due to an error: (%1) %2", errno, strerror(errno));
            throw false;
        }

        if (fstat(*fd, &sb) == -1)
        {
            Logging::error(filename, "File stat failed: (%1) %2 (fd = %3)", errno, strerror(errno), *fd);
            throw false;
        }

        if (!S_ISREG(sb.st_mode))
        {
            Logging::error(filename, "Not a file.");
            throw false;
        }

        if (!sb.st_size || defaultsize)
        {
            // If file is empty or did not exist set file size to default

            if (!defaultsize)
            {
                defaultsize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
            }

            if (ftruncate(*fd, static_cast<off_t>(defaultsize)) == -1)
            {
                Logging::error(filename, "Error calling ftruncate() to 'stretch' the file: (%1) %2 (fd = %3)", errno, strerror(errno), *fd);
                throw false;
            }

            *filesize       = defaultsize;
            *isdefaultsize  = true;
        }
        else
        {
            // Keep size
            *filesize       = static_cast<size_t>(sb.st_size);
            *isdefaultsize  = false;
        }

        *p = static_cast<uint8_t *>(mmap(nullptr, *filesize, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0));
        if (*p == MAP_FAILED)
        {
            Logging::error(filename, "File mapping failed: (%1) %2 (fd = %3)", errno, strerror(errno), *fd);
            *p = nullptr;
            throw false;
        }
    }
    catch (bool _success)
    {
        success = _success;

        if (!success && *fd != -1)
        {
            ::close(*fd);
            *fd = -1;
        }
    }

    return success;
}

bool Buffer::unmap_file(const std::string &filename, int * fd, uint8_t **p, size_t len, size_t * filesize) const
{
    bool success = true;

    Logging::trace(filename, "Unmapping cache file.");

    void *  _p         = *p;
    size_t  _filesize  = *filesize;
    int     _fd        = *fd;

    // Clear all variables
    *p          = nullptr;
    *filesize   = 0;
    *fd         = -1;

    if (_p != nullptr)
    {
        if (munmap(_p, len ? len : static_cast<size_t>(sysconf(_SC_PAGESIZE))) == -1) // Make sure we do not unmap a zero size file (spits EINVAL error)
        {
            Logging::error(filename, "Unmapping cache file failed: (%1) %2 Size: %3", errno, strerror(errno), len);
            success = false;
        }
    }

    if (_fd != -1)
    {
        if (_filesize)
        {
            if (ftruncate(_fd, static_cast<off_t>(_filesize)) == -1)
            {
                Logging::error(filename, "Error calling ftruncate() to resize and close the cache file: (%1) %2 (fd = %3) Size: %5", errno, strerror(errno), _fd, _filesize);
                success = false;
            }
            ::close(_fd);
        }
        else
        {
            ::close(_fd);

            if (unlink(filename.c_str()) && errno != ENOENT)    // Ignore if file does not exist
            {
                Logging::error(filename, "Error removing the cache file: (%1) %2 (fd = %3)", errno, strerror(errno), _fd);
                success = false;
            }
        }
    }

    return success;
}

bool Buffer::release(int flags /*= CACHE_CLOSE_NOOPT*/)
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    bool success = true;

    if (!is_open())
    {
        if (CACHE_CHECK_BIT(CACHE_CLOSE_DELETE, flags))
        {
            for (uint32_t n = 1; n <= segment_count(); n++)
            {
                remove_cachefile(n);
            }
            errno = 0;  // ignore this error
        }

        return true;
    }

    // Write active cache to disk
    flush();

    // Close anything that's still open
    for (uint32_t index = 0; index < segment_count(); index++)
    {
        if (!close_file(index + 1, CACHE_FLAG_RO | CACHE_FLAG_RW))
        {
            success = false;
        }

        if (CACHE_CHECK_BIT(CACHE_CLOSE_DELETE, flags))
        {
            remove_cachefile(index + 1);
            errno = 0;  // ignore this error
        }
    }

    // Remove index for frame sets. There is only one.
    if (!m_ci[0].m_cachefile_idx.empty())
    {
        if (!unmap_file(m_ci[0].m_cachefile_idx, &m_ci[0].m_fd_idx, &m_ci[0].m_buffer_idx, m_ci[0].m_buffer_size_idx, &m_ci[0].m_buffer_size_idx))
        {
            success = false;
        }
    }

    return success;
}

bool Buffer::remove_cachefile(uint32_t segment_no) const
{
    LPCCACHEINFO ci = !segment_no ? m_cur_ci : &m_ci[segment_no - 1];
    bool success = remove_file(ci->m_cachefile);

    if (!ci->m_cachefile_idx.empty())
    {
        if (!remove_file(ci->m_cachefile_idx))
        {
            success = false;
        }
    }
    return success;
}

bool Buffer::flush()
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    if (!segment_count() || m_cur_ci == nullptr || m_cur_ci->m_buffer == nullptr)
    {
        errno = EPERM;
        return false;
    }

    if (msync(m_cur_ci->m_buffer, m_cur_ci->m_buffer_size, MS_SYNC) == -1)
    {
        Logging::error(m_cur_ci->m_cachefile, "Could not sync to disk: (%1) %2", errno, strerror(errno));
        return false;
    }

    if (m_cur_ci->m_buffer_idx != nullptr)
    {
        if (msync(m_cur_ci->m_buffer_idx, m_cur_ci->m_buffer_size_idx, MS_SYNC) == -1)
        {
            Logging::error(m_cur_ci->m_cachefile_idx, "Could not sync to disk: (%1) %2", errno, strerror(errno));
            return false;
        }
    }

    return true;
}

bool Buffer::clear()
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    bool success = true;

    for (uint32_t n = 0; n < segment_count(); n++)
    {
        LPCACHEINFO ci = &m_ci[n];

        ci->m_buffer_pos        = 0;
        ci->m_buffer_watermark  = 0;
        ci->m_buffer_size       = 0;
        ci->m_seg_finished      = false;
        ci->m_buffer_write_size = 0;
        ci->m_buffer_writes     = 0;

        if (ci->m_fd != -1)
        {
            // If empty set file size to 1 page
            long filesize = sysconf(_SC_PAGESIZE);

            if (ftruncate(ci->m_fd, filesize) == -1)
            {
                Logging::error(ci->m_cachefile, "Error calling ftruncate() to clear the file: (%1) %2 (fd = %3)", errno, strerror(errno), ci->m_fd);
                success = false;
            }
        }
        else
        {
            remove_file(ci->m_cachefile);
        }

        if (ci->m_fd_idx != -1)
        {
            memset(ci->m_buffer_idx, 0, ci->m_buffer_size_idx);
        }
    }

    return success;
}

bool Buffer::reserve(size_t size)
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    if (m_cur_ci == nullptr || m_cur_ci->m_buffer == nullptr)
    {
        errno = ENOMEM;
        Logging::error(nullptr, "INTERNAL ERROR: Buffer::reserve()!");
        return false;
    }

    if (m_cur_ci->m_buffer_size >= size)
    {
        // Do not shrink
        return true;
    }

#ifdef HAVE_MREMAP
    m_cur_ci->m_buffer = static_cast<uint8_t*>(mremap(m_cur_ci->m_buffer, m_cur_ci->m_buffer_size, size, MREMAP_MAYMOVE));
    if (m_cur_ci->m_buffer == MAP_FAILED)
    {
        Logging::error(m_cur_ci->m_cachefile, "Error calling mremap() to resize the file: (%1) %2 (fd = %3) Old size: %4 New: %5", errno, strerror(errno), m_cur_ci->m_fd, m_cur_ci->m_buffer_size, size);
        m_cur_ci->m_buffer = nullptr;
        return false;
    }

    // Save size
    m_cur_ci->m_buffer_size = size;

    if (ftruncate(m_cur_ci->m_fd, static_cast<off_t>(m_cur_ci->m_buffer_size)) == -1)
    {
        Logging::error(m_cur_ci->m_cachefile, "Error calling ftruncate() to resize the file: (%1) %2 (fd = %3)", errno, strerror(errno), m_cur_ci->m_fd);
        return false;
    }
#else   // !HAVE_MREMAP
    size_t buffer_watermark = m_cur_ci->m_buffer_watermark;

    // No mremap() available, so close and reopen file
    if (!unmap_file(m_cur_ci->m_cachefile, &m_cur_ci->m_fd, &m_cur_ci->m_buffer, m_cur_ci->m_buffer_size, &buffer_watermark))
    {
        return false;
    }

    size_t filesize     = size;
    bool isdefaultsize  = true;
    uint8_t *p          = nullptr;

    if (!map_file(m_cur_ci->m_cachefile, &m_cur_ci->m_fd, &p, &filesize, &isdefaultsize, filesize, false))
    {
        return false;
    }

    m_cur_ci->m_buffer_size       = filesize;
    m_cur_ci->m_buffer            = static_cast<uint8_t*>(p);

#endif   // !HAVE_MREMAP

    return true;
}

size_t Buffer::writeio(const uint8_t* data, size_t length)
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    if (m_cur_ci == nullptr || m_cur_ci->m_buffer == nullptr)
    {
        errno = ENOMEM;
        return 0;
    }

    uint8_t* write_ptr = write_prepare(length);
    if (write_ptr == nullptr)
    {
        length = 0;
    }
    else
    {
        m_cur_ci->m_buffer_write_size += length;
        m_cur_ci->m_buffer_writes++;

        memcpy(write_ptr, data, length);
        increment_pos(length);
    }

    return length;
}

size_t Buffer::write_frame(const uint8_t *data, size_t length, uint32_t frame_no)
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    if (data == nullptr || m_cur_ci == nullptr || m_cur_ci->m_buffer_idx == nullptr || frame_no < 1 || frame_no > virtualfile()->m_video_frame_count)
    {
        // Invalid parameter
        errno = EINVAL;
        return 0;
    }

    LPIMAGE_FRAME old_image_frame;
    IMAGE_FRAME new_image_frame;
    size_t bytes_written;
    size_t start = static_cast<size_t>(frame_no - 1) * sizeof(IMAGE_FRAME);

    old_image_frame = reinterpret_cast<LPIMAGE_FRAME>(m_cur_ci->m_buffer_idx + start);

    if (old_image_frame->m_frame_no && (old_image_frame->m_size <= static_cast<uint32_t>(length)))
    {
        // Frame already exists and has enough space
        old_image_frame->m_size         = static_cast<uint32_t>(length);

        // Write image
        seek(static_cast<long>(old_image_frame->m_offset), SEEK_SET);
        bytes_written = writeio(data, old_image_frame->m_size);
        if (bytes_written != old_image_frame->m_size)
        {
            return 0;
        }
    }
    else
    {
        // Create new frame if not existing or not enough space
        memset(&new_image_frame, 0xFF, sizeof(new_image_frame));
        memcpy(new_image_frame.m_tag, IMAGE_FRAME_TAG, sizeof(new_image_frame.m_tag));
        new_image_frame.m_frame_no      = frame_no;
        new_image_frame.m_offset        = buffer_watermark();
        new_image_frame.m_size          = static_cast<uint32_t>(length);

        // Write image
        seek(static_cast<long>(new_image_frame.m_offset), SEEK_SET);
        bytes_written = writeio(data, new_image_frame.m_size);
        if (bytes_written != new_image_frame.m_size)
        {
            return 0;
        }

        memcpy(reinterpret_cast<void *>(m_cur_ci->m_buffer_idx + start), &new_image_frame, sizeof(IMAGE_FRAME));
    }

    return bytes_written;
}

uint8_t* Buffer::write_prepare(size_t length)
{
    if (reallocate(m_cur_ci->m_buffer_pos + length))
    {
        if (m_cur_ci->m_buffer_watermark < m_cur_ci->m_buffer_pos + length)
        {
            m_cur_ci->m_buffer_watermark = m_cur_ci->m_buffer_pos + length;
        }
        return m_cur_ci->m_buffer + m_cur_ci->m_buffer_pos;
    }
    else
    {
        errno = ESPIPE;
        return nullptr;
    }
}

void Buffer::increment_pos(size_t increment)
{
    m_cur_ci->m_buffer_pos += increment;
}

int Buffer::seek(int64_t offset, int whence)
{
    return seek(offset, whence, 0);
}

int Buffer::seek(int64_t offset, int whence, uint32_t segment_no)
{
    LPCACHEINFO ci = cacheinfo(segment_no);

    if (ci == nullptr || ci->m_buffer == nullptr)
    {
        errno = ENOMEM;
        return (EOF);
    }

    off_t seek_pos;

    switch (whence)
    {
    case SEEK_SET:
    {
        seek_pos = offset;
        break;
    }
    case SEEK_CUR:
    {
        seek_pos = static_cast<off_t>(tell(segment_no)) + offset;
        break;
    }
    case SEEK_END:
    {
        seek_pos = static_cast<off_t>(size(segment_no)) + offset;
        break;
    }
    default:
    {
        errno = EINVAL;
        return (EOF);
    }
    }

    if (seek_pos > static_cast<off_t>(size(segment_no)))
    {
        ci->m_buffer_pos = size(segment_no);  // Cannot go beyond EOF. Set position to end, leave errno untouched.
        return 0;
    }

    if (seek_pos < 0)           // Cannot go before head, leave position untouched, set errno.
    {
        errno = EINVAL;
        return (EOF);
    }

    ci->m_buffer_pos = static_cast<size_t>(seek_pos);
    return 0;
}

size_t Buffer::tell() const
{
    return tell(0);
}

size_t Buffer::tell(uint32_t segment_no) const
{
    LPCCACHEINFO ci = const_cacheinfo(segment_no);

    if (ci == nullptr)
    {
        errno = EBADF;
        return 0;
    }

    return ci->m_buffer_pos;
}

int64_t Buffer::duration() const
{
    return AV_NOPTS_VALUE;  // not applicable
}

size_t Buffer::size() const
{
    return size(0);
}

size_t Buffer::size(uint32_t segment_no) const
{
    LPCCACHEINFO ci = const_cacheinfo(segment_no);

    if (ci == nullptr)
    {
        errno = EBADF;
        return 0;
    }

    return ci->m_buffer_size;
}

size_t Buffer::buffer_watermark(uint32_t segment_no) const
{
    LPCCACHEINFO ci = const_cacheinfo(segment_no);

    if (ci == nullptr)
    {
        errno = EBADF;
        return 0;
    }

    return ci->m_buffer_watermark;
}

bool Buffer::copy(std::vector<uint8_t> * out_data, size_t offset, uint32_t segment_no)
{
    return copy(out_data->data(), offset, out_data->size(), segment_no);
}

bool Buffer::copy(uint8_t* out_data, size_t offset, size_t bufsize, uint32_t segment_no)
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    LPCCACHEINFO ci = const_cacheinfo(segment_no);

    if (ci == nullptr)
    {
        errno = EBADF;
        return false;
    }

    if (ci->m_buffer == nullptr)
    {
        errno = ENOMEM;
        return false;
    }

    size_t segment_size = size(segment_no);

    if (!segment_size && errno)
    {
        Logging::error(m_cur_ci->m_cachefile, "INTERNAL ERROR: size(segment_no) returned error. Segment: %1 (%2) %3", segment_no, errno, strerror(errno));
        return false;
    }

    if (ci->m_buffer_size != segment_size)
    {
        Logging::error(m_cur_ci->m_cachefile, "INTERNAL ERROR: Buffer::copy()! ci->m_buffer_size != segment_size - Segment: %1 ci->m_buffer_size: %2 segment_size: %3", segment_no, ci->m_buffer_size, segment_size);
        errno = ESPIPE; // Comes from size()
        return false;
    }

    bool success = true;

    if (segment_size >= offset)
    {
        if (segment_size < offset + bufsize)
        {
            bufsize = segment_size - offset - 1;
        }

        memcpy(out_data, ci->m_buffer + offset, bufsize);
    }
    else
    {
        Logging::error(m_cur_ci->m_cachefile, "INTERNAL ERROR: Buffer::copy()! size(segment_no) < offset - Segment: %1 Segment Size: %2 Offset: %3", segment_no, segment_size, offset);
        errno = ESPIPE;
        success = false;
    }

    return success;
}

bool Buffer::reallocate(size_t newsize)
{
    if (newsize > size())
    {
        if (m_cur_ci->m_buffer_writes)
        {
            size_t alloc_size = newsize - size();
            size_t write_avg = m_cur_ci->m_buffer_write_size / m_cur_ci->m_buffer_writes;
            size_t write_size = PREALLOC_FACTOR * write_avg;
            if (write_size > alloc_size)
            {
                alloc_size = write_size;

                newsize = size() + alloc_size;

            }
        }

        Logging::trace(filename(), "Buffer reallocate: %1 -> %2 (Diff %3).", size(), newsize, newsize - size());

        if (!reserve(newsize))
        {
            return false;
        }
    }
    return true;
}

const std::string & Buffer::cachefile(uint32_t segment_no) const
{
    LPCCACHEINFO ci = const_cacheinfo(segment_no);

    if (ci == nullptr)
    {
        static std::string empty;
        errno = EBADF;
        return empty;
    }

    return ci->m_cachefile;
}

const std::string & Buffer::make_cachefile_name(std::string & cachefile, const std::string & filename, const std::string & fileext, bool is_idx)
{
    transcoder_cache_path(cachefile);

    cachefile += params.m_mountpath;
    cachefile += filename;

    if (is_idx)
    {
        cachefile += ".idx.";
    }
    else
    {
        cachefile += ".cache.";
    }
    cachefile += fileext;

    return cachefile;
}

bool Buffer::remove_file(const std::string & filename)
{
    if (unlink(filename.c_str()) && errno != ENOENT)
    {
        Logging::warning(filename, "Cannot unlink the file: (%1) %2", errno, strerror(errno));
        return false;
    }
    else
    {
        errno = 0;
        return true;
    }
}

size_t Buffer::readio(void * /*data*/, size_t /*size*/)
{
    // Not implemented
    errno = EPERM;
    return 0;
}

size_t Buffer::read_frame(std::vector<uint8_t> * data, uint32_t frame_no)
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    if (data == nullptr || m_cur_ci->m_buffer_idx == nullptr || frame_no < 1 || frame_no > virtualfile()->m_video_frame_count)
    {
        // Invalid parameter
        errno = EINVAL;
        return 0;
    }

    LPCIMAGE_FRAME image_frame;
    size_t start = static_cast<size_t>(frame_no - 1) * sizeof(IMAGE_FRAME);

    image_frame = reinterpret_cast<LPCIMAGE_FRAME>(m_cur_ci->m_buffer_idx + start);

    if (!image_frame->m_frame_no)
    {
        errno = EAGAIN;
        return 0;
    }

    data->resize(image_frame->m_size);

    return copy(data, static_cast<size_t>(image_frame->m_offset));
}

int Buffer::error() const
{
    return errno;
}

bool Buffer::eof() const
{
    return eof(0);
}

bool Buffer::eof(uint32_t segment_no) const
{
    return (tell(segment_no) == size(segment_no));
}

void Buffer::closeio()
{
    release();
}

bool Buffer::have_frame(uint32_t frame_no)
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    if (m_cur_ci->m_buffer_idx == nullptr || frame_no < 1 || frame_no > virtualfile()->m_video_frame_count)
    {
        // Invalid parameter
        errno = EINVAL;
        return false;
    }

    LPCIMAGE_FRAME image_frame;
    size_t start = static_cast<size_t>(frame_no - 1) * sizeof(IMAGE_FRAME);

    image_frame = reinterpret_cast<LPCIMAGE_FRAME>(m_cur_ci->m_buffer_idx + start);

    return (image_frame->m_frame_no ? true : false);
}

bool Buffer::is_open()
{
    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    for (uint32_t index = 0; index < segment_count(); index++)
    {
        if ((m_ci[index].m_fd != -1 && (fcntl(m_ci[index].m_fd, F_GETFL) != -1 || errno != EBADF)))
        {
            return true;
        }
    }

    return false;
}

void Buffer::finished_segment()
{
    if (m_cur_ci == nullptr)
    {
        return;
    }

    m_cur_ci->m_seg_finished = true;

    flush();
}

bool Buffer::is_segment_finished(uint32_t segment_no) const
{
    LPCCACHEINFO ci = const_cacheinfo(segment_no);

    if (ci == nullptr)
    {
        errno = EBADF;
        return false;
    }

    return ci->m_seg_finished;
}

Buffer::LPCACHEINFO Buffer::cacheinfo(uint32_t segment_no)
{
    if (segment_no)
    {
        segment_no--;

        if (segment_no >= segment_count())
        {
            return nullptr;
        }
        return (&m_ci[segment_no]);
    }

    return m_cur_ci;
}

Buffer::LPCCACHEINFO Buffer::const_cacheinfo(uint32_t segment_no) const
{
    if (segment_no)
    {
        segment_no--;

        if (segment_no >= m_ci.size())
        {
            return nullptr;
        }
        return (&m_ci[segment_no]);
    }

    return m_cur_ci;
}
