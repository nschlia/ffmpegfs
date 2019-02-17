/*
 * blurayio class header for FFmpegfs
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

#ifndef BLURAYIO_H
#define BLURAYIO_H

#pragma once

#ifdef USE_LIBBLURAY

#include "fileio.h"

typedef struct bluray BLURAY;

class BlurayIO : public FileIO
{
public:
    explicit BlurayIO();
    virtual ~BlurayIO();

    virtual VIRTUALTYPE type() const;

    virtual size_t  bufsize() const;
    virtual int     read(void *data, int size);
    virtual int     error() const;
    virtual int64_t duration() const;
    virtual size_t  size() const;
    virtual size_t  tell() const;
    virtual int     seek(long offset, int whence);
    virtual bool    eof() const;
    virtual void    close();

protected:
    virtual int     openX(const std::string & filename);

protected:
    BLURAY *        m_bd;

    bool            m_is_eof;
    int             m_errno;
    size_t          m_rest_size;
    size_t          m_rest_pos;
    int64_t         m_cur_pos;
    uint64_t        m_start_pos;
    int64_t         m_end_pos;

    bool            m_full_title;
    uint32_t        m_title_idx;
    unsigned        m_chapter_idx;
    unsigned        m_angle_idx;

    uint8_t         m_data[192 * 1024];

    int64_t         m_duration;
};
#endif // USE_LIBBLURAY

#endif // BLURAYIO_H
