/*
 * Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @brief DiskIO class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "diskio.h"
#include "ffmpegfs.h"
#include "logging.h"

#include <errno.h>

DiskIO::DiskIO()
    : m_fpi(nullptr)
{

}

DiskIO::~DiskIO()
{
    _close();
}

VIRTUALTYPE DiskIO::type() const
{
    return VIRTUALTYPE_DISK;
}

size_t DiskIO::bufsize() const
{
    return (100 /* KB */ * 1024);
}

int DiskIO::open(LPVIRTUALFILE virtualfile)
{
    set_virtualfile(virtualfile);

    Logging::debug(virtualfile->m_origfile, "Opening input file.");

    m_fpi = fopen(virtualfile->m_origfile.c_str(), "rb");

    if (m_fpi != nullptr)
    {
        return 0;
    }
    else
    {
        return errno;
    }
}

size_t DiskIO::read(void * data, size_t size)
{
    return fread(data, 1, size, m_fpi);
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

    struct stat stbuf;
    fstat(fileno(m_fpi), &stbuf);
    return static_cast<size_t>(stbuf.st_size);
}

size_t DiskIO::tell() const
{
    return static_cast<size_t>(ftell(m_fpi));
}

int DiskIO::seek(int64_t offset, int whence)
{
    return fseek(m_fpi, static_cast<long int>(offset), whence);
}

bool DiskIO::eof() const
{
    return feof(m_fpi) ? true : false;
}

void DiskIO::close()
{
    _close();
}

void DiskIO::_close()
{
    FILE *fpi = m_fpi;
    if (fpi != nullptr)
    {
        m_fpi = nullptr;
        fclose(fpi);
    }
}
