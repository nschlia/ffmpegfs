/*
 * BLURAY parser for ffmpegfs
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

#include "ffmpegfs.h"
#include "vcdparser.h"
#include "transcode.h"
#include "ffmpeg_utils.h"
#include "logging.h"

#include "vcd/vcdentries.h"

static int parse_vcd(const std::string & path, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler);

static int parse_vcd(const std::string & path, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler)
{
    VcdEntries vcd;

    vcd.load_file(path);

    Logging::debug(path, "Parsing Video CD.");

    for (int chapter_no = 0; chapter_no < vcd.get_number_of_chapters(); chapter_no++)
    {
        const VcdChapter & chapter = vcd.get_chapter(chapter_no);
        char title_buf[PATH_MAX + 1];
        std::string origfile;
        struct stat st;
        size_t size = chapter.get_end_pos() - chapter.get_start_pos();

        sprintf(title_buf, "%02d. Chapter %03d.%s", chapter.get_track_no(), chapter_no + 1, params.m_format[0].real_desttype().c_str()); // can safely assume this a video

        std::string filename(title_buf);

        origfile = path + filename;

        memcpy(&st, statbuf, sizeof(struct stat));

        st.st_size = static_cast<__off_t>(size);
        st.st_blocks = (st.st_size + 512 - 1) / 512;

        //init_stat(&st, size, false);

        if (buf != nullptr && filler(buf, filename.c_str(), &st, 0))
        {
            // break;
        }

        LPVIRTUALFILE virtualfile = insert_file(VIRTUALTYPE_VCD, path + filename, origfile, &st);

        // Video CD is video format anyway
        virtualfile->m_format_idx       = 0;
        // Mark title/chapter/angle
        virtualfile->vcd.m_track_no     = chapter.get_track_no();
        virtualfile->vcd.m_chapter_no   = chapter_no;
        virtualfile->vcd.m_start_pos    = chapter.get_start_pos();
        virtualfile->vcd.m_end_pos      = chapter.get_end_pos();
    }

    return vcd.get_number_of_chapters();
}

int check_vcd(const std::string & _path, void *buf, fuse_fill_dir_t filler)
{
    std::string path(_path);
    struct stat st;
    int res = 0;

    append_sep(&path);

    if (stat((path + "SVCD/INFO.SVD").c_str(), &st) == 0)
    {
        Logging::trace(path, "VCD detected.");
        res = parse_vcd(path, &st, buf, filler);
        Logging::trace(nullptr, "Found %1 titles.", res);
    }
    else if (stat((path + "VCD/INFO.VCD").c_str(), &st) == 0)
    {
        Logging::trace(path, "VCD detected.");
        res = parse_vcd(path, &st, buf, filler);
        Logging::trace(nullptr, "Found %1 titles.", res);
    }
    return res;
}

#endif // USE_LIBVCD
