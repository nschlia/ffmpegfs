/*
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

/**
 * @file
 * @brief FileIO class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
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

//#include <assert.h>

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
    case VIRTUALTYPE_DIRECTORY_FRAMES:
    {
        return new(std::nothrow) DiskIO; /**< @todo eigener typ */
    }
        //case VIRTUALTYPE_DIRECTORY:
        //case VIRTUALTYPE_PASSTHROUGH:
        //case VIRTUALTYPE_SCRIPT:
    default:
    {
		errno = EINVAL;
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

