/*
 * BLURAY parser for ffmpegfs
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

#ifdef USE_LIBBLURAY

#include "ffmpegfs.h"
#include "blurayparser.h"
#include "transcode.h"
#include "ffmpeg_utils.h"
#include "logging.h"

#include "libbluray/bluray.h"

int parse_bluray(const std::string & path, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler)
{
    BLURAY *bd;
    uint32_t title_count;
    int main_title;
    unsigned int seconds = 0;
    uint8_t flags = TITLES_RELEVANT;
    const char *bd_dir = nullptr;

    bd_dir = path.c_str();
    //flags = TITLES_ALL;
    //seconds = strtol(optarg, nullptr, 0);

    Logging::debug(path, "Parsing Bluray.");

    bd = bd_open(bd_dir, nullptr);

    title_count = bd_get_titles(bd, flags, seconds);
    main_title = bd_get_main_title(bd);
    if (main_title >= 0)
    {
        Logging::trace(nullptr, "Main title: %1", main_title + 1);
    }

    for (uint32_t title_no = 0; title_no < title_count; title_no++)
    {
        BLURAY_TITLE_INFO* ti;

        ti = bd_get_title_info(bd, title_no, 0);

        for (uint32_t chapter_no = 0; chapter_no < ti->chapter_count; chapter_no++)
        {
            char title_buf[PATH_MAX + 1];
            std::string origfile;
            struct stat st;

            //        sprintf(title_buf, "index%c%02d duration %02" PRIu64 "-%02" PRIu64 "-%02" PRIu64 " chapters %3d angles %2u clips %3u (playlist %05d.mpls) V %d A %-2d PG %-2d IG %-2d SV %d SA %d.%s",
            //                (ii == main_title) ? '+' : ' ',
            //                ii + 1,
            //                (ti->duration / 90000) / (3600),
            //                ((ti->duration / 90000) % 3600) / 60,
            //                ((ti->duration / 90000) % 60),
            //                ti->chapter_count, ti->angle_count, ti->clip_count, ti->playlist,
            //                ti->clips[0].video_stream_count,
            //                ti->clips[0].audio_stream_count,
            //                ti->clips[0].pg_stream_count,
            //                ti->clips[0].ig_stream_count,
            //                ti->clips[0].sec_video_stream_count,
            //                ti->clips[0].sec_audio_stream_count,
            //                params.get_current_format().m_desttype
            //                );

            sprintf(title_buf, "%02d.%cChapter %03d [%02" PRIu64 "-%02" PRIu64 "-%02" PRIu64 "].%s",
                    title_no + 1,
                    (main_title >= 0 && title_no == (uint32_t)main_title) ? '+' : ' ',
                    chapter_no + 1,
                                    (ti->chapters[chapter_no].duration / 90000) / (3600),
                                    ((ti->chapters[chapter_no].duration / 90000) % 3600) / 60,
                                    ((ti->chapters[chapter_no].duration / 90000) % 60),
                    params.m_format[0].m_desttype.c_str()); // can safely assume this is a video format

            std::string filename(title_buf);

            origfile = path + filename;

            memcpy(&st, statbuf, sizeof(struct stat));

            st.st_size = 0;
            st.st_blocks = (st.st_size + 512 - 1) / 512;

            if (buf != nullptr && filler(buf, filename.c_str(), &st, 0))
            {
                // break;
            }

            LPVIRTUALFILE virtualfile = insert_file(VIRTUALTYPE_BLURAY, path + filename, origfile, &st);

            // Bluray is video format anyway
            virtualfile->m_format_idx       = 0;
            // Mark title/chapter/angle
            // ti->chapter_count
            virtualfile->bluray.m_title_no      = title_no + 1;
            virtualfile->bluray.m_playlist_no   = ti->playlist;
            virtualfile->bluray.m_chapter_no    = chapter_no + 1;
            virtualfile->bluray.m_angle_no      = 1;
        }

        bd_free_title_info(ti);
    }

    bd_close(bd);

    return title_count;
}

// Returns -errno or number or titles on BLURAY
int check_bluray(const std::string & _path, void *buf, fuse_fill_dir_t filler)
{
    std::string path(_path);
    struct stat st;
    int res = 0;

    append_sep(&path);

    if (stat((path + "BDMV/index.bdmv").c_str(), &st) == 0)
    {
        Logging::trace(path, "Bluray detected.");
        res = parse_bluray(path, &st, buf, filler);
        Logging::trace(path, "Found %1 titles.", res);
    }
    return res;
}

#endif // USE_LIBBLURAY
