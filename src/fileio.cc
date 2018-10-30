/*
 * File I/O for ffmpegfs
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

fileio::fileio()
    : m_virtualfile(nullptr)
{

}

fileio::~fileio()
{

}

fileio * fileio::alloc(VIRTUALTYPE type)
{
    switch (type)
    {
    case VIRTUALTYPE_REGULAR:
    {
        return new diskio;
    }
    case VIRTUALTYPE_BUFFER:
    {
        return new Buffer;
    }
#ifdef USE_LIBVCD
    case VIRTUALTYPE_VCD:
    {
        return new vcdio;
    }
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
    case VIRTUALTYPE_DVD:
    {
        return new dvdio;
    }
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
    case VIRTUALTYPE_BLURAY:
    {
        return new blurayio;
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

int fileio::open(LPCVIRTUALFILE virtualfile)
{
    assert(virtualfile->m_type == type());

    m_virtualfile = virtualfile;

    return open(virtualfile->m_origfile);
}

LPCVIRTUALFILE fileio::get_virtualfile() const
{
    return m_virtualfile;
}

const string & fileio::set_path(const string & _path)
{
    path = _path;

    remove_filename(&path);

    return path;
}

