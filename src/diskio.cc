/*
 * Disk I/O base class
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

#include "diskio.h"
#include "ffmpegfs.h"
#include "logging.h"

#include <errno.h>
#include <assert.h>

DiskIO::DiskIO()
    : m_fpi(nullptr)
{

}

DiskIO::~DiskIO()
{
    close();
}

VIRTUALTYPE DiskIO::type() const
{
    return VIRTUALTYPE_REGULAR;
}

int DiskIO::bufsize() const
{
    return (100 /* KB */ * 1024);
}

int DiskIO::openX(const std::string & filename)
{
    Logging::info(filename, "Opening input file.");

    set_path(filename);

    m_fpi = fopen(filename.c_str(), "rb");

    if (m_fpi != nullptr)
    {
        return 0;
    }
    else
    {
        return errno;
    }
}

int DiskIO::read(void * data, int size)
{
    return static_cast<int>(fread(data, 1, static_cast<size_t>(size), m_fpi));
}

int DiskIO::error() const
{
    return ferror(m_fpi);
}

int64_t DiskIO::duration() const
{
    return AV_NOPTS_VALUE;  // not applicable
}

size_t DiskIO::size() const
{
    if (m_fpi == nullptr)
    {
        errno = EINVAL;
        return 0;
    }

    struct stat st;
    fstat(fileno(m_fpi), &st);
    return st.st_size;
}

size_t DiskIO::tell() const
{
    // falls nicht möglich:
    // errno = EPERM;
    // return -1;
    return static_cast<size_t>(ftell(m_fpi));
}

int DiskIO::seek(long offset, int whence)
{
    return fseek(m_fpi, offset, whence);
}

bool DiskIO::eof() const
{
    return feof(m_fpi) ? true : false;
}

void DiskIO::close()
{
    FILE *fpi = m_fpi;
    if (fpi != nullptr)
    {
        m_fpi = nullptr;
        fclose(fpi);
    }
}
