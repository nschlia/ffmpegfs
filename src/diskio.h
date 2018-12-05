/*
 * diskio class header for ffmpegfs
 *
 * Copyright (C) 2017-2018 Norbert Schlia (nschlia@oblivion-software.de)
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

#ifndef DISKIO_H
#define DISKIO_H

#pragma once

#include "fileio.h"

class diskio : public fileio
{
public:
    diskio();
    virtual ~diskio();

    virtual VIRTUALTYPE type() const;

    virtual int     bufsize() const;
    virtual int     read(void *data, int maxlen);
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
};

#endif // DISKIO_H
