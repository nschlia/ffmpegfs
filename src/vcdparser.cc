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

#ifdef USE_LIBVCD

/**
 * @file
 * @brief Video/Super Video CD parser implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "ffmpegfs.h"
#include "vcdparser.h"
#include "transcode.h"
#include "ffmpeg_utils.h"
#include "logging.h"

#include "vcd/vcdentries.h"

static int parse_vcd(const std::string & path, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler);
static bool create_vcd_virtualfile(const VcdEntries &vcd, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler, bool full_title, int chapter_no);

/**
 * @brief Creat a virtual file for a video CD.
 * @param vcd - Video CD handle.
 * @param statbuf
 * @param buf
 * @param filler
 * @param full_title
 * @param chapter_no
 * @return
 */
static bool create_vcd_virtualfile(const VcdEntries & vcd, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler, bool full_title, int chapter_no)
{
    const VcdChapter * chapter1 = vcd.get_chapter(chapter_no);
    char title_buf[PATH_MAX + 1];
    struct stat st;
    size_t size;
    int64_t duration;

    if (!full_title)
    {
        size = chapter1->get_size();
        duration = chapter1->get_duration();
        sprintf(title_buf, "%02d. Chapter %03d [%s].%s", chapter1->get_track_no(), chapter_no + 1, replace_all(format_duration(duration), ":", "-").c_str(), params.m_format[0].format_name().c_str()); // can safely assume this a video
    }
    else
    {
        size = vcd.get_size();
        duration = vcd.get_duration();
        sprintf(title_buf, "%02d. Title [%s].%s", chapter1->get_track_no(), replace_all(format_duration(duration), ":", "-").c_str(), params.m_format[0].format_name().c_str()); // can safely assume this a video
    }

    std::string filename(title_buf);

    memcpy(&st, statbuf, sizeof(struct stat));

    st.st_size = static_cast<__off_t>(size);
    st.st_blocks = (st.st_size + 512 - 1) / 512;

    //init_stat(&st, size, false);

    if (buf != nullptr && filler(buf, title_buf, &st, 0))
    {
        // break;
    }

    LPVIRTUALFILE virtualfile = insert_file(VIRTUALTYPE_VCD, vcd.get_disk_path() + filename, &st);

    // Video CD is video format anyway
    virtualfile->m_format_idx           = 0;
    // Mark title/chapter/angle
    virtualfile->m_full_title           = full_title;
    virtualfile->m_vcd.m_track_no       = chapter1->get_track_no();
    virtualfile->m_vcd.m_chapter_no     = chapter_no;
    virtualfile->m_vcd.m_start_pos      = chapter1->get_start_pos();
    if (!full_title)
    {
        virtualfile->m_vcd.m_end_pos    = chapter1->get_end_pos();
    }
    else
    {
        virtualfile->m_vcd.m_end_pos    = size;
    }
    virtualfile->m_duration             = duration;

    return true;
}

static int parse_vcd(const std::string & path, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler)
{
    VcdEntries vcd;
    bool success = true;

    vcd.load_file(path);

    Logging::debug(path, "Parsing Video CD.");

    for (int chapter_no = 0; chapter_no < vcd.get_number_of_chapters() && success; chapter_no++)
    {
        success = create_vcd_virtualfile(vcd, statbuf, buf, filler, false, chapter_no);
    }

    if (success && vcd.get_number_of_chapters() > 1)
    {
        success = create_vcd_virtualfile(vcd, statbuf, buf, filler, true, 0);
    }

    if (success)
    {
        return vcd.get_number_of_chapters();
    }
    else
    {
        return -errno;
    }
}

int check_vcd(const std::string & _path, void *buf, fuse_fill_dir_t filler)
{
    std::string path(_path);
    struct stat st;
    int res = 0;

    append_sep(&path);

    if (stat((path + "SVCD/INFO.SVD").c_str(), &st) == 0)
    {
        if (!check_path(path))
        {
            Logging::trace(path, "VCD detected.");
            res = parse_vcd(path, &st, buf, filler);
            Logging::trace(nullptr, "Found %1 titles.", res);
        }
        else
        {
            res = load_path(path, &st, buf, filler);
        }
    }
    else if (stat((path + "VCD/INFO.VCD").c_str(), &st) == 0)
    {
        if (!check_path(path))
        {
            Logging::trace(path, "VCD detected.");
            res = parse_vcd(path, &st, buf, filler);
            Logging::trace(nullptr, "Found %1 titles.", res);
        }
        else
        {
            res = load_path(path, &st, buf, filler);
        }
    }
    return res;
}

#endif // USE_LIBVCD
