/*
 * Video CD file I/O for FFmpegfs
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

#ifdef USE_LIBVCD
#include "vcdio.h"
#include "vcd/vcdutils.h"
//#include "ffmpeg_utils.h"
#include "ffmpegfs.h"
#include "logging.h"

#include <assert.h>

VcdIO::VcdIO()
    : m_fpi(nullptr)
    , m_track_no(0)
    , m_chapter_no(0)
    , m_start_pos(0)
    , m_end_pos(0)
{

}

VcdIO::~VcdIO()
{
    close();
}

VIRTUALTYPE VcdIO::type() const
{
    return VIRTUALTYPE_VCD;
}

int VcdIO::bufsize() const
{
    return (32 * 1024);
}

int VcdIO::openX(const std::string & filename)
{
    std::string src_filename;

    set_path(filename);

    if (virtualfile() != nullptr)
    {
        m_track_no      = virtualfile()->vcd.m_track_no;
        m_chapter_no    = virtualfile()->vcd.m_chapter_no;
        m_start_pos     = virtualfile()->vcd.m_start_pos;
        m_end_pos       = virtualfile()->vcd.m_end_pos;
    }
    else
    {
        m_track_no      = 1;
        m_chapter_no    = 0;
        m_start_pos     = 0;
        m_end_pos       = size();
    }

    VCDUTILS::locate_video(m_path, m_track_no, src_filename);

    Logging::info(src_filename.c_str(), "Opening input VCD.");

    m_fpi = fopen(src_filename.c_str(), "rb");

    if (m_fpi == nullptr)
    {
        return errno;
    }

    return seek(0, SEEK_SET);
}

int VcdIO::read(void * data, int size)
{
    if (ftell(m_fpi) + static_cast<uint64_t>(size) > m_end_pos)
    {
        size = static_cast<int>(m_end_pos - ftell(m_fpi));
    }

    if (!size)
    {
        return 0;
    }

    return static_cast<int>(fread(data, 1, size, m_fpi));
}

int VcdIO::error() const
{
    return ferror(m_fpi);
}

int64_t VcdIO::duration() const
{
    return AV_NOPTS_VALUE;  // TODO: implement me
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
        return (m_end_pos - m_start_pos);
    }

    struct stat st;
    fstat(fileno(m_fpi), &st);
    return static_cast<size_t>(st.st_size);
}

size_t VcdIO::tell() const
{
    return (ftell(m_fpi) - m_start_pos);
}

int VcdIO::seek(long offset, int whence)
{
    long int seek_pos;
    switch (whence)
    {
    case SEEK_SET:
    {
        seek_pos = m_start_pos + offset;
        break;
    }
    case SEEK_CUR:
    {
        seek_pos = m_start_pos + offset + ftell(m_fpi);
        break;
    }
    case SEEK_END:
    {        
        seek_pos = m_end_pos - offset;
        break;
    }
    default:
    {
        errno = EINVAL;
        return 0;
    }
    }

    if (static_cast<uint64_t>(seek_pos) > m_end_pos)
    {
        errno = EINVAL;
        return -1;
    }

    return fseek(m_fpi, seek_pos, SEEK_SET);
}

bool VcdIO::eof() const
{
    return ((feof(m_fpi) || (static_cast<uint64_t>(ftell(m_fpi)) >= m_end_pos)) ? true : false);
}

void VcdIO::close()
{
    FILE *fpi = m_fpi;
    if (fpi != nullptr)
    {
        m_fpi = nullptr;
        fclose(fpi);
    }
}

#endif // USE_LIBVCD
