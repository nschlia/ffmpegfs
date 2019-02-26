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
 * @brief Direct disk I/O
 *
 * This is only available if built with -DUSE_LIBDVD parameter.
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2018-2019 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef DISKIO_H
#define DISKIO_H

#pragma once

#include "fileio.h"

/** @brief Disk file I/O class
 */
class DiskIO : public FileIO
{
public:
    DiskIO();
    virtual ~DiskIO();

    virtual VIRTUALTYPE type() const;                           /**< @brief Type of the virtual file */

    virtual size_t  bufsize() const;                            /**< @brief Ideal buffer size */
    /** @brief Read data from file
     * @param[out] data - buffer to store read bytes in. Must be large enough to hold up to size bytes.
     * @param[in] size - number of bytes to read
     * @return Upon successful completion, #read() returns the number of bytes read. @n
     * This may be less than size. @n
     * On error, the value 0 is returned and errno is set to indicate the error. @n
     * If at end of file, 0 may be returned by errno not set. error() will return 0 if at EOF.
     */
    virtual size_t  read(void *data, size_t size);
    virtual int     error() const;                              /**< @brief If error occurred return number */
    /** @brief Get the duration of the file, in AV_TIME_BASE fractional seconds.
     *
     * Not applicable to generic disk files, always returns AV_NOPTS_VALUE.
     */
    virtual int64_t duration() const;
    virtual size_t  size() const;                               /**< @brief Get file size */
    virtual size_t  tell() const;                               /**< @brief Get current read position */
    /** @brief Seek to position in file
     *
     * Repositions the offset of the open file to the argument offset according to the directive whence.
     *
     * @param[in] offset - offset in bytes
     * @param[in] whence - how to seek: @n
     * SEEK_SET: The offset is set to offset bytes. @n
     * SEEK_CUR: The offset is set to its current location plus offset bytes. @n
     * SEEK_END: The offset is set to the size of the file plus offset bytes.
     * @return Upon successful completion, #seek() returns the resulting offset location as measured in bytes
     * from the beginning of the file.  @n
     * On error, the value -1 is returned and errno is set to indicate the error.
     */
    virtual int     seek(long offset, int whence);
    virtual bool    eof() const;                                /**< @brief Return true if at end of file */
    virtual void    close();                                    /**< @brief Close virtual file */

protected:
    /** @brief Open a virtual file
     * @param[in] filename - Name of file to open.
     * @return Upon successful completion, #openX() returns 0. @n
     * On error, an nonzero value is returned and errno is set to indicate the error.
     */
    virtual int     openX(const std::string & filename);

protected:
    FILE *          m_fpi;                                      /**< @brief File pointer to source media */
};

#endif // DISKIO_H
