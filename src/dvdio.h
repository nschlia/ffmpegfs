/*
 * Copyright (C) 2018-2019 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @brief DVD I/O
 *
 * This is only available if built with -DUSE_LIBDVD parameter.
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2018-2019 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef DVDIO_H
#define DVDIO_H

#pragma once

#ifdef USE_LIBDVD

#include "fileio.h"

#include <dvdread/ifo_read.h>

/** @brief DVD I/O class
 */
class DvdIO : public FileIO
{
    /**
      * @brief Type of a DSI block.
      *
      * This is used to identify a DSI block on a DVD: Until end of a chapter
      * we simply continue. At the end of chapters we may stop continue to
      * the end of a title
      */
    typedef enum DSITYPE
    {
        DSITYPE_CONTINUE,                                       /**< @brief Chapter continues */
        DSITYPE_EOF_CHAPTER,                                    /**< @brief End of chapter */
        DSITYPE_EOF_TITLE                                       /**< @brief End of title */
    } DSITYPE;

public:
    /**
     * @brief Create #DvdIO object
     */
    explicit DvdIO();
    /**
     * @brief Free #DvdIO object
     */
    virtual ~DvdIO();

    /**
     * @brief Get type of the virtual file
     * @return Returns the type of the virtual file.
     */
    virtual VIRTUALTYPE type() const;
    /**
     * @brief Get the ideal buffer size.
     * @return Return the ideal buffer size.
     */
    virtual size_t  bufsize() const;

    /** @brief Open a virtual file
     * @param[in] virtualfile - LPCVIRTUALFILE of file to open
     * @return Upon successful completion, #open() returns 0. @n
     * On error, an nonzero value is returned and errno is set to indicate the error.
     */
    virtual int     open(LPVIRTUALFILE virtualfile);
    /** @brief Read data from file
     * @param[out] data - buffer to store read bytes in. Must be large enough to hold up to size bytes.
     * Special case: If set to nullptr as many bytes as possible are "read" (and discarded). Can be used
     * to determine the net file size of the virtual stream.
     * @param[in] size - number of bytes to read
     * @return Upon successful completion, #read() returns the number of bytes read. @n
     * This may be less than size. @n
     * On error, the value 0 is returned and errno is set to indicate the error. @n
     * If at end of file, 0 may be returned by errno not set. error() will return 0 if at EOF.
     */
    virtual size_t  read(void *data, size_t size);
    /**
     * @brief Get last error.
     * @return errno value of last error.
     */
    virtual int     error() const;
    /** @brief Get the duration of the file, in AV_TIME_BASE fractional seconds.
     * @return Returns the duration of the file, in AV_TIME_BASE fractional seconds.
     */
    virtual int64_t duration() const;
    /**
     * @brief Get the file size.
     * @return Returns the file size.
     */
    virtual size_t  size() const;
    /**
     * @brief Get current read position.
     * @return Gets the current read position.
     */
    virtual size_t  tell() const;
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
    virtual int     seek(int64_t offset, int whence);
    /**
     * @brief Check if at end of file.
     * @return Returns true if at end of file.
     */
    virtual bool    eof() const;
    /**
     * @brief Close virtual file.
     */
    virtual void    close();

private:
    /**
     * @brief Do a rough check if this is really a navigation packet.
     * @param[in] buffer - Buffer with data.
     * @return true if the pack is a NAV pack.
     */
    bool            is_nav_pack(const unsigned char *buffer) const;
    /**
     * @brief return the size of the next packet
     * @param[in] p - Buffer with PES data.
     * @param[in] peek - How many bytes to peek.
     * @param[in] size - Size of the next packet
     * @return Size ofr ID invalid.
     */
    bool            get_packet_size(const uint8_t *p, size_t peek, size_t *size) const;
    /**
     * @brief return the id of a Packetized Elementary Stream (PES) (should be valid)
     * @param[in] buffer - Buffer with PES data.
     * @param[in] size - Size of buffer.
     * @return The id of a PES (should be valid)
     */
    int             get_pes_id(const uint8_t *buffer, size_t size) const;
    /**
     * @brief Extract only the interesting portion of the VOB input stream
     * @param[out] out - Stream stripped from unnessessary data
     * @param[in] in - Input raw stream
     * @param[in] len - Length of input raw stream
     * @return Size of restulting data block.
     */
    size_t          demux_pes(uint8_t *out, const uint8_t *in, size_t len) const;
    /**
     * @brief Handle DSI (Data Search Information) packet
     * @param[in] _dsi_pack - buffer with DSI packet.
     * @param[out] cur_output_size - Net size of packet.
     * @param[in] next_vobu - Address of next VOBU (Video Object Unit).
     * @param[in] data - Data extracted from DSI packet.
     * @return
     */
    DSITYPE         handle_DSI(void * _dsi_pack, size_t *cur_output_size, unsigned int * next_vobu, uint8_t *data);
    /**
     * @brief Goto next DVD cell
     */
    void            next_cell();
    /**
     * @brief Rewind to start of stream
     */
    void            rewind();

protected:
    dvd_reader_t *  m_dvd;                                      /**< @brief DVD reader handle */
    dvd_file_t *    m_dvd_title;                                /**< @brief DVD title handle */
    ifo_handle_t *  m_vmg_file;                                 /**< @brief DVD video manager handle */
    ifo_handle_t *  m_vts_file;                                 /**< @brief DVD video title stream handle */
    pgc_t *         m_cur_pgc;                                  /**< @brief Current program chain */
    int             m_start_cell;                               /**< @brief Start cell */
    int             m_end_cell;                                 /**< @brief End cell (of title) */
    int             m_cur_cell;                                 /**< @brief Current cell */
    int             m_next_cell;                                /**< @brief Next cell to be processed */
    bool            m_goto_next_cell;                           /**< @brief If logc needs to go to next cell before next read */
    unsigned int    m_cur_block;                                /**< @brief Current processing block */
    bool            m_is_eof;                                   /**< @brief true if at "end of file", i.e, end of chapter or title */
    int             m_errno;                                    /**< @brief errno of last operation */
    size_t          m_rest_size;                                /**< @brief Rest bytes in buffer */
    size_t          m_rest_pos;                                 /**< @brief Position in buffer */
    size_t          m_cur_pos;                                  /**< @brief Current position in virtual file */

    bool            m_full_title;                               /**< @brief If true, ignore m_chapter_no and provide full track */
    int             m_title_idx;                                /**< @brief Track index (track number - 1) */
    int             m_chapter_idx;                              /**< @brief Chapter index (chapter number - 1) */
    int             m_angle_idx;                                /**< @brief Selected angle index (angle number -1) */

    unsigned char   m_data[1024 * DVD_VIDEO_LB_LEN];            /**< @brief Buffer for read() data */
    unsigned char   m_buffer[1024 * DVD_VIDEO_LB_LEN];          /**< @brief Buffer for data extracted from VOB file */

    int64_t         m_duration;                                 /**< @brief Track/chapter duration, in AV_TIME_BASE fractional seconds. */
    size_t          m_size;                                     /**< @brief Size of virtual file */
};
#endif // USE_LIBDVD

#endif // DVDIO_H
