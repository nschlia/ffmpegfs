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
 * @brief FileIO class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "fileio.h"
#include "ffmpegfs.h"
#include "buffer.h"
#include "diskio.h"
#ifdef USE_LIBVCD
#include "vcdio.h"
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
#include "dvdio.h"
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
#include "blurayio.h"
#endif // USE_LIBBLURAY

//#include <assert.h>

uint32_t VIRTUALFILE::get_segment_count() const
{
    if (m_duration && params.m_segment_duration)
    {
        return static_cast<uint32_t>((m_duration - 200000) / params.m_segment_duration) + 1;
    }
    else
    {
        return 0;
    }
}

FileIO::FileIO()
    : m_virtualfile(nullptr)
{

}

FileIO::~FileIO()
{

}

FileIO * FileIO::alloc(VIRTUALTYPE type)
{
    switch (type)
    {
    case VIRTUALTYPE_DISK:
    {
        return new(std::nothrow) DiskIO;
    }
#ifdef USE_LIBVCD
    case VIRTUALTYPE_VCD:
    {
        return new(std::nothrow) VcdIO;
    }
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
    case VIRTUALTYPE_DVD:
    {
        return new(std::nothrow) DvdIO;
    }
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
    case VIRTUALTYPE_BLURAY:
    {
        return new(std::nothrow) BlurayIO;
    }
#endif // USE_LIBBLURAY
    default:
    {
        return new(std::nothrow) DiskIO;
    }
    }
}

void FileIO::set_virtualfile(LPVIRTUALFILE virtualfile)
{
    m_virtualfile = virtualfile;

    if (virtualfile != nullptr)
    {
        // Store path to original file without file name for fast access
        m_path = m_virtualfile->m_origfile;

        remove_filename(&m_path);
    }
    else
    {
        m_path.clear();
    }
}

LPVIRTUALFILE FileIO::virtualfile()
{
    return m_virtualfile;
}

const std::string & FileIO::path() const
{
    return m_path;
}

const std::string & FileIO::filename() const
{
    if (m_virtualfile == nullptr)
    {
        static const std::string empty;
        return empty;
    }

    return m_virtualfile->m_destfile;
}

