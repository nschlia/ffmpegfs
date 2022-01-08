/*
 * Copyright (C) 2018-2022 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file
 * @brief Bluray I/O
 *
 * This is only available if built with -DUSE_LIBBLURAY parameter.
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2018-2022 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef BLURAYIO_H
#define BLURAYIO_H

#pragma once

#ifdef USE_LIBBLURAY

#include "fileio.h"

typedef struct bluray BLURAY;               /**< @brief Forward declaration of libbluray handle */

/**
 * @brief Bluray I/O class
 */
class BlurayIO : public FileIO
{
public:
    /**
     * @brief Create #BlurayIO object
     */
    explicit BlurayIO();
    /**
     * @brief Free #BlurayIO object
     */
    virtual ~BlurayIO();

    /**
     * @brief Get type of the virtual file
     * @return Returns the type of the virtual file.
     */
    virtual VIRTUALTYPE type() const override;
    /**
     * @brief Get the ideal buffer size.
     * @return Return the ideal buffer size.
     */
    virtual size_t  bufsize() const override;

    /**
     * @brief Open a virtual file
     * @param[in] virtualfile - LPCVIRTUALFILE of file to open
     * @return Upon successful completion, #open() returns 0. @n
     * On error, an nonzero value is returned and errno is set to indicate the error.
     */
    virtual int     open(LPVIRTUALFILE virtualfile) override;
    /**
     * @brief Read data from file
     * @param[out] data - buffer to store read bytes in. Must be large enough to hold up to size bytes.
     * @param[in] size - number of bytes to read
     * @return Upon successful completion, #read() returns the number of bytes read. @n
     * This may be less than size. @n
     * On error, the value 0 is returned and errno is set to indicate the error. @n
     * If at end of file, 0 may be returned by errno not set. error() will return 0 if at EOF.
     */
    virtual size_t  read(void *data, size_t size) override;
    /**
     * @brief Get last error.
     * @return errno value of last error.
     */
    virtual int     error() const override;
    /**
     * @brief Get the duration of the file, in AV_TIME_BASE fractional seconds.
     * @return Returns the duration of the file, in AV_TIME_BASE fractional seconds.
     */
    virtual int64_t duration() const override;
    /**
     * @brief Get the file size.
     * @return Returns the file size.
     */
    virtual size_t  size() const override;
    /**
     * @brief Get current read position.
     * @return Gets the current read position.
     */
    virtual size_t  tell() const override;
    /**
     * @brief Seek to position in file
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
    virtual int     seek(int64_t offset, int whence) override;
    /**
     * @brief Check if at end of file.
     * @return Returns true if at end of file.
     */
    virtual bool    eof() const override;
    /**
     * @brief Close virtual file.
     */
    virtual void    close() override;

private:
    /**
     * @brief Close virtual file.
     * Non-virtual version to be safely called from constructor/destructor
     */
    void            _close();

protected:
    BLURAY *        m_bd;                                       /**< @brief Bluray disk handle */

    bool            m_is_eof;                                   /**< @brief true if at end of virtual file */
    int             m_errno;                                    /**< @brief Last errno */
    size_t          m_rest_size;                                /**< @brief Rest bytes in buffer */
    size_t          m_rest_pos;                                 /**< @brief Position in buffer */
    int64_t         m_cur_pos;                                  /**< @brief Current position in virtual file */
    int64_t         m_start_pos;                                /**< @brief Start offset in bytes */
    int64_t         m_end_pos;                                  /**< @brief End offset in bytes (not including this byte) */

    bool            m_full_title;                               /**< @brief If true, ignore m_chapter_no and provide full track */
    uint32_t        m_title_idx;                                /**< @brief Track index (track number - 1) */
    unsigned        m_chapter_idx;                              /**< @brief Chapter index (chapter number - 1) */
    unsigned        m_angle_idx;                                /**< @brief Selected angle index (angle number -1) */

    uint8_t         m_data[192 * 1024];                         /**< @brief Buffer for read() data */

    int64_t         m_duration;                                 /**< @brief Track/chapter duration, in AV_TIME_BASE fractional seconds. */
};
#endif // USE_LIBBLURAY

#endif // BLURAYIO_H
