/*
 * File I/O for FFmpegfs
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

#include "fileio.h"
#include "ffmpeg_utils.h"
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

#include <assert.h>

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
    case VIRTUALTYPE_REGULAR:
    {
        return new DiskIO;
    }
    case VIRTUALTYPE_BUFFER:
    {
        return new Buffer;
    }
#ifdef USE_LIBVCD
    case VIRTUALTYPE_VCD:
    {
        return new VcdIO;
    }
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
    case VIRTUALTYPE_DVD:
    {
        return new DvdIO;
    }
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
    case VIRTUALTYPE_BLURAY:
    {
        return new BlurayIO;
    }
#endif // USE_LIBBLURAY
    //case VIRTUALTYPE_PASSTHROUGH:
    //case VIRTUALTYPE_SCRIPT:
    default:
    {
        return nullptr;
    }
    }
}

int FileIO::open(LPCVIRTUALFILE virtualfile)
{
//    assert(virtualfile->m_type == type());

    m_virtualfile = virtualfile;

    return openX(virtualfile->m_origfile);
}

LPCVIRTUALFILE FileIO::virtualfile() const
{
    return m_virtualfile;
}

const std::string & FileIO::set_path(const std::string & path)
{
    m_path = path;

    remove_filename(&m_path);

    return m_path;
}

