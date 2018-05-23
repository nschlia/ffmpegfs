/*
 * Video CD file I/O for ffmpegfs
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

#ifdef USE_LIBVCD
#include "vcdio.h"
#include "vcd/vcdutils.h"
#include "ffmpeg_utils.h"
#include "ffmpegfs.h"

#include <assert.h>

vcdio::vcdio()
    : m_fpi(NULL)
    , m_track_no(0)
    , m_chapter_no(0)
    , m_start_pos(0)
    , m_end_pos(0)
{

}

vcdio::~vcdio()
{
    close();
}

VIRTUALTYPE vcdio::type() const
{
    return VIRTUALTYPE_VCD;
}

int vcdio::bufsize() const
{
    return (32 * 1024);
}

int vcdio::open(const string & _path)
{
    string path(_path);
    string filename;

    if (get_virtualfile() != NULL)
    {
        m_track_no      = get_virtualfile()->vcd.m_track_no;
        m_chapter_no    = get_virtualfile()->vcd.m_chapter_no;
        m_start_pos     = get_virtualfile()->vcd.m_start_pos;
        m_end_pos       = get_virtualfile()->vcd.m_end_pos;
    }
    else
    {
        m_track_no      = 1;
        m_chapter_no    = 0;
        m_start_pos     = 0;
        m_end_pos       = size();
    }

    remove_filename(&path);

    VCDUTILS::locate_video(path, m_track_no, filename);

    ffmpegfs_info("Opening input VCD '%s'.", filename.c_str());

    m_fpi = fopen(filename.c_str(), "rb");

    if (m_fpi == NULL)
    {
        return errno;
    }

    return seek(0, SEEK_SET);
}

int vcdio::read(void * data, int maxlen)
{
    if (ftell(m_fpi) + (uint64_t)maxlen > m_end_pos)
    {
        maxlen = (int)(m_end_pos - ftell(m_fpi));
    }

    if (!maxlen)
    {
        return 0;
    }

    return (int)fread(data, 1, maxlen, m_fpi);
}

int vcdio::error() const
{
    return ferror(m_fpi);
}

int vcdio::duration() const
{
    return -1;  // TODO
}

size_t vcdio::size() const
{
    if (m_fpi == 0)
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
    return st.st_size;
}

size_t vcdio::tell() const
{
    return (ftell(m_fpi) - m_start_pos);
}

int vcdio::seek(long offset, int whence)
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

    return fseek(m_fpi, seek_pos, SEEK_SET);
}

bool vcdio::eof() const
{
    return ((feof(m_fpi) || ((uint64_t)ftell(m_fpi) >= m_end_pos)) ? true : false);
}

void vcdio::close()
{
    FILE *fpi = m_fpi;
    if (fpi != NULL)
    {
        m_fpi = NULL;
        fclose(fpi);
    }
}

#endif // USE_LIBVCD
