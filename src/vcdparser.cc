/*
 * Copyright (C) 2017-2024 Norbert Schlia (nschlia@oblivion-software.de)
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

#ifdef USE_LIBVCD

/**
 * @file vcdparser.cc
 * @brief Video/Super Video CD parser implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2024 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "ffmpegfs.h"
#include "vcdparser.h"
#include "logging.h"

#include "vcd/vcdentries.h"

extern "C" {
#include <libavutil/rational.h>
}

static int parse_vcd(const std::string & path, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler);
static bool create_vcd_virtualfile(const VcdEntries &vcd, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler, bool full_title, int chapter_no);

/**
 * @brief Create a virtual file for a video CD.
 * @param[in] vcd - Video CD handle.
 * @param[in] statbuf - File status structure of original file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @param[in] full_title - If true, create virtual file of all title. If false, include single chapter only.
 * @param[in] chapter_no - Chapter number of virtual file.
 * @return Returns true if successful. Returns false on error.
 */
static bool create_vcd_virtualfile(const VcdEntries & vcd, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler, bool full_title, int chapter_no)
{
    const VcdChapter * chapter1 = vcd.get_chapter(chapter_no);
    std::string title_buf;
    size_t size;
    int64_t duration;

    if (!full_title)
    {
        size = static_cast<size_t>(chapter1->get_size());
        duration = chapter1->get_duration();
        strsprintf(&title_buf, "%02d. Chapter %03d [%s].%s", chapter1->get_track_no(), chapter_no + 1, replace_all(format_duration(duration), ":", "-").c_str(), ffmpeg_format[FORMAT::VIDEO].fileext().c_str()); // can safely assume this a video
    }
    else
    {
        size = static_cast<size_t>(vcd.get_size());
        duration = vcd.get_duration();
        strsprintf(&title_buf, "%02d. Title [%s].%s", chapter1->get_track_no(), replace_all(format_duration(duration), ":", "-").c_str(), ffmpeg_format[FORMAT::VIDEO].fileext().c_str()); // can safely assume this a video
    }

    LPVIRTUALFILE virtualfile = nullptr;
    if (!ffmpeg_format[FORMAT::VIDEO].is_multiformat())
    {
        virtualfile = insert_file(VIRTUALTYPE::VCD, vcd.get_disk_path() + title_buf, statbuf);
    }
    else
    {
        virtualfile = insert_dir(VIRTUALTYPE::VCD, vcd.get_disk_path() + title_buf, statbuf);
    }

    if (virtualfile == nullptr)
    {
        Logging::error(vcd.get_disk_path(), "Failed to create virtual path: %1", (vcd.get_disk_path() + title_buf).c_str());
        errno = EIO;
        return false;
    }

    if (add_fuse_entry(buf, filler, title_buf, &virtualfile->m_st, 0))
    {
        // break;
    }

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
    AVRational framerate = av_make_q(25000, 1000);  //*** @todo check disk which framerate is correct, can be 25 or 29.996 fps!
    virtualfile->m_video_frame_count 	= static_cast<uint32_t>(av_rescale_q(duration, av_get_time_base_q(), av_inv_q(framerate)));
    virtualfile->m_predicted_size       = size;

    return true;
}

/**
 * @brief Parse VCD directory and get all VCD chapters as virtual files.
 * @param[in] path - Path to check.
 * @param[in] statbuf - File status structure of original file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @return On success, returns number of chapters found. On error, returns -errno.
 */
static int parse_vcd(const std::string & path, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler)
{
    VcdEntries vcd;
    bool success = true;

    vcd.load_file(path);

    Logging::debug(path, "Parsing the Video CD.");

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

int check_vcd(const std::string & path, void *buf, fuse_fill_dir_t filler)
{
    std::string _path(path);
    struct stat stbuf;
    int res = 0;

    append_sep(&_path);

    if (stat((_path + "SVCD/INFO.SVD").c_str(), &stbuf) == 0)
    {
        if (!check_path(_path))
        {
            Logging::trace(_path, "SVCD detected.");
            res = parse_vcd(_path, &stbuf, buf, filler);
            Logging::trace(nullptr, "%1 titles were discovered.", res);
        }
        else
        {
            res = load_path(_path, &stbuf, buf, filler);
        }

        add_dotdot(buf, filler, &stbuf, 0);
    }
    else if (stat((_path + "VCD/INFO.VCD").c_str(), &stbuf) == 0)
    {
        if (!check_path(_path))
        {
            Logging::trace(_path, "VCD detected.");
            res = parse_vcd(_path, &stbuf, buf, filler);
            Logging::trace(nullptr, "%1 titles were discovered.", res);
        }
        else
        {
            res = load_path(_path, &stbuf, buf, filler);
        }

        add_dotdot(buf, filler, &stbuf, 0);
    }
    return res;
}

#endif // USE_LIBVCD
