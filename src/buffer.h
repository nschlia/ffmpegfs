/*
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

/**
 * @file
 * @brief Buffer class
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef BUFFER_H
#define BUFFER_H

#pragma once

#include "fileio.h"

#include <mutex>
#include <stddef.h>

#define CACHE_CHECK_BIT(mask, var)  ((mask) == (mask & (var)))  /**< @brief Check bit in bitmask */

#define CLOSE_CACHE_NOOPT   0x00                                /**< @brief Dummy, do nothing special */
#define CLOSE_CACHE_FREE    0x01                                /**< @brief Free memory for cache entry */
#define CLOSE_CACHE_DELETE  (0x02 | CLOSE_CACHE_FREE)           /**< @brief Delete cache entry, will unlink cached file! Implies CLOSE_CACHE_FREE. */

/**
 * @brief The #Buffer class
 */
class Buffer : public FileIO
{
public:
    /**
     * @brief Create #Buffer object
     */
    explicit Buffer();
    /**
     * @brief Free #Buffer object
     *
     * Release memory and close files
     */
    virtual ~Buffer();
    /**
     * @brief Get type of this virtual file.
     * @return Returns VIRTUALTYPE_BUFFER.
     */
    virtual VIRTUALTYPE type() const;
    /**
     * @brief Initialise cache
     * @param[in] erase_cache - if true delete old file before opening.
     * @return Returns true on success; false on error.
     */
    bool                    init(bool erase_cache = false);
    /**
     * @brief Release cache buffer.
     * @param[in] flags - One of the CLOSE_CACHE_* flags.
     * @return Returns true on success; false on error.
     */
    bool                    release(int flags = CLOSE_CACHE_NOOPT);
    /**
     * @brief Size of this buffer.
     * @return Not applicable, returns 0.
     */
    virtual size_t          bufsize() const;

    /** @brief Open a virtual file
     * @param[in] virtualfile - LPCVIRTUALFILE of file to open
     * @return Upon successful completion, #open() returns 0. @n
     * On error, an nonzero value is returned and errno is set to indicate the error.
     */
    virtual int             open(LPCVIRTUALFILE virtualfile);
    /**
     * @brief Not implemented.
     * @param[out] data - unused
     * @param[in] size - unused
     * @return Always returns 0 and errno is EPERM.
     */
    virtual size_t          read(void *data, size_t size);
    /**
     * @brief Get last error.
     * @return errno value of last error.
     */
    virtual int             error() const;
    /** @brief Get the duration of the file, in AV_TIME_BASE fractional seconds.
     * @return Not applicable to buffer, always returns AV_NOPTS_VALUE.
     */
    virtual int64_t         duration() const;
    /**
     * @brief Get the value of the internal buffer size pointer.
     * @return Returns the value of the internal buffer size pointer.
     */
    virtual size_t          size() const;
    /**
     * @brief Get the value of the internal read position pointer.
     * @return Returns the value of the internal read position pointer.
     */
    virtual size_t          tell() const;
    /** @brief Seek to position in file
     *
     * Repositions the offset of the open file to the argument offset according to the directive whence.
     * May block for a long time if buffer has not been filled to the requested offset.
     *
     * @param[in] offset - offset in bytes
     * @param[in] whence - how to seek: @n
     * SEEK_SET: The offset is set to offset bytes. @n
     * SEEK_CUR: The offset is set to its current location plus offset bytes. @n
     * SEEK_END: The offset is set to the size of the file plus offset bytes.
     * @return Upon successful completion, #seek() returns the resulting offset location as measured in bytes
     * from the beginning of the file.
     */
    virtual int             seek(long offset, int whence);
    /**
     * @brief Check if at end of file.
     * @return Returns true if at end of buffer.
     */
    virtual bool            eof() const;
    /**
     * @brief Close buffer.
     */
    virtual void            close();
    /**
     * @brief Write data to the current position into the buffer. The position pointer will be updated.
     * @param[in] data - Buffer with data to write.
     * @param[in] length - Length of buffer to write.
     * @return Returns the bytes written to the buffer. If less than length this indicates an error, consult errno for details.
     */
    size_t                  write(const uint8_t* data, size_t length);
    /**
     * @brief Flush buffer to disk
     * @return Returns true on success; false on error. Check errno for details.
     */
    bool                    flush();
    /**
     * @brief Clear (delete) buffer.
     * @return Returns true on success; false on error. Check errno for details.
     */
    bool                    clear();
    /**
     * @brief Reserve memory without changing size to reduce re-allocations.
     * @param[in] size - Size of buffer to reserve.
     * @return Returns true on success; false on error.
     */
    bool                    reserve(size_t size);
    /** @brief Return the current watermark of the file while transcoding
     *
     * While transcoding, this value reflects the current size of the transcoded file.
     * This is the maximum byte offset until the file can be read so far.
     *
     *  @return Returns the current watermark.
     */
    size_t                  buffer_watermark() const;
    /**
     * @brief Copy buffered data into output buffer.
     * @param[in] out_data - Buffer to copy data to.
     * @param[in] offset - Offset in buffer to copy data from.
     * @param[in] bufsize - Size of out_data buffer.
     * @return Returns true on success; false on error.
     */
    bool                    copy(uint8_t* out_data, size_t offset, size_t bufsize);
    /**
     * @brief Get source filename.
     * @return Returns source filename.
     */
    const std::string &     filename() const;
    /**
     * @brief Get cache filename.
     * @return Returns cache filename.
     */
    const std::string &     cachefile() const;
    /**
     * @brief Make up a cache file name including full path
     * @param[out] cachefile - Name of cache file.
     * @param[in] filename - Source file name.
     * @param[in] desttype - Destination type (MP4, WEBM etc.).
     * @return Returns the name of the cache file.
     */
    static const std::string & make_cachefile_name(std::string &cachefile, const std::string & filename, const std::string &desttype);
    /**
     * @brief Remove (unlink) file.
     * @param[in] filename - Name of file to remove.
     * @return Returns true on success; false on error.
     */
    static bool             remove_file(const std::string & filename);

protected:
    /**
     * @brief Remove the cachefile.
     * @return Returns true on success; false on error.
     */
    bool                    remove_cachefile();

private:
    /**
     * @brief Prepare write operation.
     *
     * Ensure the Buffer has sufficient space for a quantity of data and
     * return a pointer where the data may be written. The position pointer
     * should be updated afterward with increment_pos().
     * @param[in] length - Length of buffer to prepare.
     * @return Returns a pointer to the memory to write.
     */
    uint8_t*                write_prepare(size_t length);
    /**
     * @brief Increment buffer position.
     *
     * Increment the location of the internal pointer. This cannot fail and so
     * returns void. It does not ensure the position is valid memory because
     * that is done by the write_prepare methods via reallocate.
     * @param[in] increment - Increment size.
     */
    void                    increment_pos(size_t increment);
    /**
     * @brief Reallocate buffer to new size.
     *
     * Ensure the allocation has at least size bytes available. If not,
     * reallocate memory to make more available. Fill the newly allocated memory
     * with zeroes.
     * @param[in] newsize - New buffer size
     * @return Returns true on success; false on error.
     */
    bool                    reallocate(size_t newsize);
    /**
     * @brief Map memory to file.
     * @param[in] filename - Name of cache file to open.
     * @param[out] fd - The file descriptor of the open cache file.
     * @param[out] p - Memory pointer to cache file.
     * @param[out] filesize - Actual size of the cache file after this call.
     * @param[inout] isdefaultsize -@n
     * In: If false, the file size will be the size of the existing file, returning the size in filesize. If the file does not exist, it will be sized to defaultsize.
     * If true, the defaultsize will be used in any case, resizing an existing file if necessary.@n
     * Out: true if the file size was set to default.
     * @param[out] defaultsize - Default size of the file if it does not exist. This parameter can be zero in which case the size will be set to the system's page size.
     * @return Returns true if successful and fd, p, filesize, isdefaultsize filled in or false on error.
     */
    bool                    map_file(const std::string & filename, int *fd, void **p, size_t *filesize, bool *isdefaultsize, off_t defaultsize) const;
    /**
     * @brief Umnap memory from file.
     * @param[in] filename - Name of cache file to unmap.
     * @param[in] fd - The file descriptor of the open cache file.
     * @param[in] p - Memory pointer to cache file.
     * @param[in] filesize - Actual size of the cache file.
     * @return Returns true on success; false on error.
     */
    bool                    unmap_file(const std::string & filename, int fd, void *p, size_t filesize) const;

private:
    std::recursive_mutex    m_mutex;                        /**< @brief Access mutex */
    std::string             m_filename;                     /**< @brief Source file name */
    volatile bool           m_is_open;                      /**< @brief true if cache is open */
    std::string             m_cachefile;                    /**< @brief Cache file name */
    int                     m_fd;                           /**< @brief File handle for buffer */
    uint8_t *               m_buffer;                       /**< @brief Pointer to buffer memory */
    size_t                  m_buffer_pos;                   /**< @brief Read/write position */
    size_t                  m_buffer_watermark;             /**< @brief Number of bytes in buffer */
    size_t                  m_buffer_size;                  /**< @brief Current buffer size */
};

#endif
