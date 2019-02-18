/*
 * dvdio class header for FFmpegfs
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

#ifndef DVDIO_H
#define DVDIO_H

#pragma once

#ifdef USE_LIBDVD

#include "fileio.h"

#include <dvdread/ifo_read.h>

class DvdIO : public FileIO
{
    typedef enum DSITYPE
    {
        DSITYPE_CONTINUE,
        DSITYPE_EOF_CHAPTER,
        DSITYPE_EOF_TITLE
    } DSITYPE;

public:
    explicit DvdIO();
    virtual ~DvdIO();

    virtual VIRTUALTYPE type() const;

    virtual size_t  bufsize() const;
    virtual size_t  read(void *data, size_t size);
    virtual int     error() const;
    virtual int64_t duration() const;
    virtual size_t  size() const;
    virtual size_t  tell() const;
    virtual int     seek(long offset, int whence);
    virtual bool    eof() const;
    virtual void    close();

protected:
    virtual int     openX(const std::string & filename);

private:
    bool            is_nav_pack(const unsigned char *buffer) const;
    bool            get_packet_size(const uint8_t *p, size_t peek, size_t *size) const;
    int             get_pes_id(const uint8_t *buffer, size_t size) const;
    size_t          demux_pes(uint8_t *out, const uint8_t *in, size_t len) const;
    DSITYPE         handle_DSI(void * _dsi_pack, size_t *cur_output_size, unsigned int & next_vobu, uint8_t *data);
    void            next_cell();
    
protected:
    dvd_reader_t *  m_dvd;
    dvd_file_t *    m_dvd_title;
    ifo_handle_t *  m_vmg_file;
    ifo_handle_t *  m_vts_file;
    pgc_t *         m_cur_pgc;
    int             m_start_cell;
    int             m_end_cell;
    int             m_cur_cell;
    int             m_next_cell;
    int             m_last_cell;
    bool            m_goto_next_cell;
    unsigned int    m_cur_block;
    bool            m_is_eof;
    int             m_errno;
    size_t          m_rest_size;
    size_t          m_rest_pos;
    size_t          m_cur_pos;

    bool            m_full_title;
    int             m_title_idx;
    int             m_chapter_idx;
    int             m_angle_idx;

    unsigned char   m_data[1024 * DVD_VIDEO_LB_LEN];
    unsigned char   m_buffer[1024 * DVD_VIDEO_LB_LEN];

    int64_t         m_duration;
};
#endif // USE_LIBDVD

#endif // DVDIO_H
