/*
 * Bluray Disk file I/O for FFmpegfs
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

#ifdef USE_LIBBLURAY
#include "blurayio.h"
#include "ffmpegfs.h"
#include "ffmpeg_utils.h"
#include "logging.h"

#include <libbluray/bluray.h>
#include <assert.h>

BlurayIO::BlurayIO()
    : m_bd(nullptr)
    , m_is_eof(false)
    , m_errno(0)
    , m_rest_size(0)
    , m_rest_pos(0)
    , m_cur_pos(0)
    , m_start_pos(0)
    , m_end_pos(-1)
    , m_title_no(0)
    , m_chapter_no(0)
    , m_angle_no(0)
    , m_duration(-1)
{
    memset(&m_data, 0, sizeof(m_data));
}

BlurayIO::~BlurayIO()
{

}

VIRTUALTYPE BlurayIO::type() const
{
    return VIRTUALTYPE_BLURAY;
}

int BlurayIO::bufsize() const
{
    return sizeof(m_data);
}

int BlurayIO::openX(const std::string & filename)
{
    const char *bdpath = nullptr;
    int title_count;
    int chapter_end = -1;
    char *keyfile = nullptr;
    BLURAY_TITLE_INFO *ti;

    set_path(filename);

    bdpath = m_path.c_str();

    if (virtualfile() != nullptr)
    {
        m_title_no      = virtualfile()->bluray.m_title_no - 1;
        m_chapter_no    = virtualfile()->bluray.m_chapter_no - 1;
        m_angle_no      = virtualfile()->bluray.m_angle_no - 1;
        m_duration     = virtualfile()->bluray.m_duration;
    }
    else
    {
        m_title_no      = 0;
        m_chapter_no    = 0;
        m_angle_no      = 0;
        m_duration     = -1;
    }

    chapter_end = m_chapter_no + 1;
	
    Logging::debug(bdpath, "Opening input Bluray.");

    m_bd = bd_open(bdpath, keyfile);
    if (m_bd == nullptr)
    {
        Logging::error(bdpath, "Failed to open disc.");
        return 1;
    }

    title_count = bd_get_titles(m_bd, TITLES_RELEVANT, 0);
    if (title_count <= 0)
    {
        Logging::error(bdpath, "No titles found.");
        return 1;
    }

//    if (m_title_no >= 0)
    {
        if (!bd_select_title(m_bd, m_title_no))
        {
            Logging::error(bdpath, "Failed to open title: %1", m_title_no);
            return 1;
        }
        ti = bd_get_title_info(m_bd, m_title_no, m_angle_no);

    }

    if (m_angle_no >= ti->angle_count)
    {
        Logging::warning(bdpath, "Invalid angle %1 > angle count %2. Using angle 1.", m_angle_no + 1, ti->angle_count);
        m_angle_no = 0;
    }

    bd_select_angle(m_bd, m_angle_no);

    if (m_chapter_no >= ti->chapter_count)
    {
        Logging::error(bdpath, "First chapter %1 > chapter count %2", m_chapter_no + 1, ti->chapter_count);
        return 1;
    }

    if (chapter_end >= static_cast<int>(ti->chapter_count))
    {
        chapter_end = -1;
    }

    if (chapter_end >= 0)
    {
        m_end_pos = bd_chapter_pos(m_bd, chapter_end);
    }
    else
    {
        m_end_pos = bd_get_title_size(m_bd);
    }

    BLURAY_TITLE_CHAPTER *chapter = &ti->chapters[m_chapter_no];
    m_duration = chapter->duration * AV_TIME_BASE / 90000;

    bd_free_title_info(ti);

    m_start_pos = bd_seek_chapter(m_bd, m_chapter_no);

    m_rest_size = 0;
    m_rest_pos = 0;

    return 0;
}

int BlurayIO::read(void * data, int size)
{
    int result_len = 0;

    if (m_rest_size)
    {
        result_len = static_cast<int>(m_rest_size);

        assert(m_rest_size < static_cast<size_t>(size));

        memcpy(data, &m_data[m_rest_pos], m_rest_size);

        m_rest_size = m_rest_pos = 0;

        return result_len;
    }

    m_cur_pos = bd_tell(m_bd);
    if (m_end_pos < 0 || m_cur_pos < m_end_pos)
    {
        int bytes;
        int maxsize = static_cast<int>(sizeof(m_data));

        if (maxsize > (m_end_pos - m_cur_pos))
        {
            maxsize = static_cast<int>(m_end_pos - m_cur_pos);
        }

//        Logging::error(m_path, "READ %<%11lli>1", m_cur_pos);

        bytes = bd_read(m_bd, m_data, maxsize);
        if (bytes <= 0)
        {
            Logging::error(m_path, "bd_read fail ret = %1", bytes);
            return 0;
        }
        m_cur_pos = bd_tell(m_bd);

        if (bytes > size)
        {
            result_len = size;
            memcpy(data, m_data, result_len);

            m_rest_size = bytes - size;
            m_rest_pos = size;
        }
        else
        {
            result_len = bytes;
            memcpy(data, m_data, result_len);
        }
    }

    return result_len;
}

int BlurayIO::error() const
{
    return m_errno;
}

int64_t BlurayIO::duration() const
{
    return m_duration;
}

size_t BlurayIO::size() const
{
    return (m_end_pos - m_start_pos);
}

size_t BlurayIO::tell() const
{
    return (bd_tell(m_bd) - m_start_pos);
}

int BlurayIO::seek(long offset, int whence)
{
    long int seek_pos;

    //Logging::error(m_path, "SEEK %<%11" PRId64 ">1", offset);

    switch (whence)
    {
    case SEEK_SET:
    {
        seek_pos = m_start_pos + offset;
        break;
    }
    case SEEK_CUR:
    {
        seek_pos = m_start_pos + offset + bd_tell(m_bd);
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

    bool success = (bd_seek(m_bd, seek_pos) != seek_pos);
    m_cur_pos = bd_tell(m_bd);
    return success;
}

bool BlurayIO::eof() const
{
    return (m_cur_pos >= m_end_pos);
}

void BlurayIO::close()
{
    bd_close(m_bd);
}

#endif // USE_LIBBLURAY
