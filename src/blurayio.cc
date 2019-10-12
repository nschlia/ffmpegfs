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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * On Debian systems, the complete text of the GNU General Public License
 * Version 3 can be found in `/usr/share/common-licenses/GPL-3'.
 */

#ifdef USE_LIBBLURAY

/**
 * @file
 * @brief BlurayIO class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
 */

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
    , m_end_pos(AV_NOPTS_VALUE)
    , m_full_title(false)
    , m_title_idx(0)
    , m_chapter_idx(0)
    , m_angle_idx(0)
    , m_duration(AV_NOPTS_VALUE)
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

size_t BlurayIO::bufsize() const
{
    return sizeof(m_data);
}

int BlurayIO::open(LPCVIRTUALFILE virtualfile)
{
    std::string filename = set_virtualfile(virtualfile);
    const char *bdpath = nullptr;
    uint32_t title_count;
    uint32_t chapter_end;
    char *keyfile = nullptr;
    BLURAY_TITLE_INFO *ti;

    set_path(filename);

    bdpath = m_path.c_str();

    if (virtualfile != nullptr)
    {
        m_full_title    = virtualfile->m_full_title;
        m_title_idx     = virtualfile->m_bluray.m_title_no - 1;
        m_chapter_idx   = virtualfile->m_bluray.m_chapter_no - 1;
        m_angle_idx     = virtualfile->m_bluray.m_angle_no - 1;
        m_duration      = virtualfile->m_duration;
    }
    else
    {
        m_full_title    = false;
        m_title_idx     = 0;
        m_chapter_idx   = 0;
        m_angle_idx     = 0;
        m_duration      = AV_NOPTS_VALUE;
    }

    chapter_end = m_chapter_idx + 1;

    Logging::debug(bdpath, "Opening input Bluray.");

    m_bd = bd_open(bdpath, keyfile);
    if (m_bd == nullptr)
    {
        Logging::error(bdpath, "Failed to open disc.");
        return 1;
    }

    title_count = bd_get_titles(m_bd, TITLES_RELEVANT, 0);
    if (title_count == 0)
    {
        Logging::error(bdpath, "No titles found.");
        return 1;
    }

    if (!bd_select_title(m_bd, m_title_idx))
    {
        Logging::error(bdpath, "Failed to open title: %1", m_title_idx);
        return 1;
    }
    ti = bd_get_title_info(m_bd, m_title_idx, m_angle_idx);

    if (m_angle_idx >= ti->angle_count)
    {
        Logging::warning(bdpath, "Invalid angle %1 > angle count %2. Using angle 1.", m_angle_idx + 1, ti->angle_count);
        m_angle_idx = 0;
    }

    bd_select_angle(m_bd, m_angle_idx);

    if (m_chapter_idx >= ti->chapter_count)
    {
        Logging::error(bdpath, "First chapter %1 > chapter count %2", m_chapter_idx + 1, ti->chapter_count);
        return 1;
    }

    if (chapter_end >= ti->chapter_count)
    {
        chapter_end = 0;
    }

    if (chapter_end > 0 && !m_full_title)
    {
        m_end_pos = bd_chapter_pos(m_bd, chapter_end);
    }
    else
    {
        m_end_pos = static_cast<int64_t>(bd_get_title_size(m_bd));
    }

    if (m_full_title)
    {
        m_duration = static_cast<int64_t>(ti->duration * AV_TIME_BASE / 90000);
    }
    else
    {
        BLURAY_TITLE_CHAPTER *chapter = &ti->chapters[m_chapter_idx];
        m_duration = static_cast<int64_t>(chapter->duration * AV_TIME_BASE / 90000);
    }

    bd_free_title_info(ti);

    m_start_pos = bd_seek_chapter(m_bd, m_chapter_idx);

    m_rest_size = 0;
    m_rest_pos = 0;

    return 0;
}

size_t BlurayIO::read(void * data, size_t size)
{
    size_t result_len = 0;

    if (m_rest_size)
    {
        result_len = m_rest_size;

        assert(m_rest_size < size);

        memcpy(data, &m_data[m_rest_pos], m_rest_size);

        m_rest_size = m_rest_pos = 0;

        return result_len;
    }

    m_cur_pos = static_cast<int64_t>(bd_tell(m_bd));
    if (m_end_pos < 0 || m_cur_pos < m_end_pos)
    {
        int maxsize = sizeof(m_data);

        if (maxsize > (m_end_pos - m_cur_pos))
        {
            maxsize = static_cast<int>(m_end_pos - m_cur_pos);
        }

        int res = bd_read(m_bd, m_data, maxsize);
        if (res < 0)
        {
            Logging::error(m_path, "bd_read fail");
            return 0;
        }

        size_t bytes = static_cast<size_t>(res);

        m_cur_pos = static_cast<int64_t>(bd_tell(m_bd));

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
    return static_cast<size_t>(m_end_pos - m_start_pos);
}

size_t BlurayIO::tell() const
{
    return static_cast<size_t>(static_cast<int64_t>(bd_tell(m_bd)) - m_start_pos);
}

int BlurayIO::seek(int64_t offset, int whence)
{
    int64_t seek_pos;

    switch (whence)
    {
    case SEEK_SET:
    {
        seek_pos = m_start_pos + offset;
        break;
    }
    case SEEK_CUR:
    {
        seek_pos = m_start_pos + offset + static_cast<int64_t>(bd_tell(m_bd));
        break;
    }
    case SEEK_END:
    {
        seek_pos = m_end_pos + offset;
        break;
    }
    default:
    {
        errno = EINVAL;
        return (EOF);
    }
    }

    if (seek_pos > m_end_pos)
    {
        m_cur_pos = m_end_pos;  // Cannot go beyond EOF. Set position to end, leave errno untouched.
        return 0;
    }

    if (seek_pos < 0)           // Cannot go before head, set errno.
    {
        errno = EINVAL;
        return (EOF);
    }

    int64_t found_pos = bd_seek(m_bd, static_cast<uint64_t>(seek_pos));
    m_cur_pos = static_cast<int64_t>(bd_tell(m_bd));
    return (found_pos == seek_pos ? 0 : -1);
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
