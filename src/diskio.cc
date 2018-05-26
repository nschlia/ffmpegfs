/*
 * Disk I/O base class for ffmpegfs
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

#include "diskio.h"
#include "ffmpegfs.h"

#include <errno.h>
#include <assert.h>

diskio::diskio()
    : m_fpi(NULL)
{

}

diskio::~diskio()
{
    close();
}

VIRTUALTYPE diskio::type() const
{
    return VIRTUALTYPE_REGULAR;
}

int diskio::bufsize() const
{
    return (100 /* KB */ * 1024);
}

int diskio::open(const string & filename)
{
    ffmpegfs_info(filename.c_str(), "Opening input file.");

    set_path(filename);

    m_fpi = fopen(filename.c_str(), "rb");

    if (m_fpi != NULL)
    {
        return 0;
    }
    else
    {
        return errno;
    }
}

int diskio::read(void * data, int maxlen)
{
    return (int)fread(data, 1, maxlen, m_fpi);
}

int diskio::error() const
{
    return ferror(m_fpi);
}

int diskio::duration() const
{
    return -1;  // not applicable
}

size_t diskio::size() const
{
    if (m_fpi == 0)
    {
        errno = EINVAL;
        return 0;
    }

    struct stat st;
    fstat(fileno(m_fpi), &st);
    return st.st_size;
}

size_t diskio::tell() const
{
    // falls nicht möglich:
    // errno = EPERM;
    // return -1;
    return ftell(m_fpi);
}

int diskio::seek(long offset, int whence)
{
    return fseek(m_fpi, offset, whence);
}

bool diskio::eof() const
{
    return feof(m_fpi) ? true : false;
}

void diskio::close()
{
    FILE *fpi = m_fpi;
    if (fpi != NULL)
    {
        m_fpi = NULL;
        fclose(fpi);
    }
}
