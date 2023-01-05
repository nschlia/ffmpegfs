/*
 * Copyright (C) 2017-2023 Norbert Schlia (nschlia@oblivion-software.de)
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
 *
 * On Debian systems, the complete text of the GNU General Public License
 * Version 3 can be found in `/usr/share/common-licenses/GPL-3'.
 */

#ifdef USE_LIBVCD

/**
 * @file vcdio.cc
 * @brief VcdIO class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2023 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "vcdio.h"
#include "vcd/vcdutils.h"
#include "ffmpegfs.h"
#include "logging.h"

VcdIO::VcdIO()
    : m_fpi(nullptr)
    , m_full_title(false)
    , m_track_no(0)
    , m_chapter_no(0)
    , m_start_pos(0)
    , m_end_pos(0)
{

}

VcdIO::~VcdIO()
{
    _close();
}

VIRTUALTYPE VcdIO::type() const
{
    return VIRTUALTYPE_VCD;
}

size_t VcdIO::bufsize() const
{
    return (32 * 1024);
}

int VcdIO::open(LPVIRTUALFILE virtualfile)
{
    std::string src_filename;

    set_virtualfile(virtualfile);

    if (virtualfile != nullptr)
    {
        m_full_title    = virtualfile->m_full_title;
        m_track_no      = virtualfile->m_vcd.m_track_no;
        m_chapter_no    = virtualfile->m_vcd.m_chapter_no;
        m_start_pos     = virtualfile->m_vcd.m_start_pos;
        m_end_pos       = virtualfile->m_vcd.m_end_pos;
    }
    else
    {
        m_full_title    = false;
        m_track_no      = 1;
        m_chapter_no    = 0;
        m_start_pos     = 0;
        m_end_pos       = size();
    }

    VCDUTILS::locate_video(path(), m_track_no, src_filename);

    Logging::info(src_filename.c_str(), "Opening the input VCD.");

    m_fpi = fopen(src_filename.c_str(), "rb");

    if (m_fpi == nullptr)
    {
        return errno;
    }

    return seek(0, SEEK_SET);
}

size_t VcdIO::read(void * data, size_t size)
{
    if (static_cast<size_t>(ftell(m_fpi)) + size > m_end_pos)
    {
        size = static_cast<size_t>(m_end_pos - static_cast<uint64_t>(ftell(m_fpi)));
    }

    if (!size)
    {
        return 0;
    }

    return fread(data, 1, size, m_fpi);
}

int VcdIO::error() const
{
    return ferror(m_fpi);
}

int64_t VcdIO::duration() const
{
    return AV_NOPTS_VALUE;
}

size_t VcdIO::size() const
{
    if (m_fpi == nullptr)
    {
        errno = EINVAL;
        return 0;
    }

    if (m_end_pos)
    {
        return static_cast<size_t>(m_end_pos - m_start_pos);
    }

    struct stat stbuf;
    fstat(fileno(m_fpi), &stbuf);
    return static_cast<size_t>(stbuf.st_size);
}

size_t VcdIO::tell() const
{
    return static_cast<size_t>(static_cast<uint64_t>(ftell(m_fpi)) - m_start_pos);
}

int VcdIO::seek(int64_t offset, int whence)
{
    off_t seek_pos;
    switch (whence)
    {
    case SEEK_SET:
    {
        seek_pos = static_cast<off_t>(m_start_pos) + offset;
        break;
    }
    case SEEK_CUR:
    {
        seek_pos = static_cast<off_t>(m_start_pos) + ftell(m_fpi) + offset;
        break;
    }
    case SEEK_END:
    {
        seek_pos = static_cast<off_t>(m_end_pos) - offset;
        break;
    }
    default:
    {
        errno = EINVAL;
        return (EOF);
    }
    }

    if (static_cast<uint64_t>(seek_pos) > m_end_pos)
    {
        seek_pos = static_cast<off_t>(m_end_pos);           // Cannot go beyond EOF. Set position to end, leave errno untouched.
    }

    if (static_cast<uint64_t>(seek_pos) < m_start_pos)      // Cannot go before head, leave position untouched, set errno.
    {
        errno = EINVAL;
        return (EOF);
    }

    return fseek(m_fpi, static_cast<long int>(seek_pos), SEEK_SET);
}

bool VcdIO::eof() const
{
    return ((feof(m_fpi) || (static_cast<uint64_t>(ftell(m_fpi)) >= m_end_pos)) ? true : false);
}

void VcdIO::close()
{
    _close();
}

void VcdIO::_close()
{
    FILE *fpi = m_fpi;
    if (fpi != nullptr)
    {
        m_fpi = nullptr;
        fclose(fpi);
    }
}

#endif // USE_LIBVCD
