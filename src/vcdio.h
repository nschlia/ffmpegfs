/*
 * vcdio class header for FFmpegfs
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

#ifndef VCDIO_H
#define VCDIO_H

#pragma once

#ifdef USE_LIBVCD

#include "fileio.h"

class VcdIO : public FileIO
{
public:
    explicit VcdIO();
    virtual ~VcdIO();

    virtual VIRTUALTYPE type() const;

    virtual int     bufsize() const;
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
    FILE *          m_fpi;
    int             m_track_no;
    int             m_chapter_no;
    uint64_t        m_start_pos;
    uint64_t        m_end_pos;

};
#endif // USE_LIBVCD

#endif // VCDIO_H
