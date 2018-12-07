/*
 * DVD parser for ffmpegfs
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

#ifdef USE_LIBDVD

#include "ffmpegfs.h"
#include "dvdparser.h"
#include "transcode.h"
#include "ffmpeg_utils.h"
#include "logging.h"

//#include <dvdnav/dvdnav.h>
#include <dvdread/dvd_reader.h>
//#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
//#include <dvdread/dvd_udf.h>
//#include <dvdread/nav_read.h>
//#include <dvdread/nav_print.h>

int parse_dvd(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    dvd_reader_t *dvd;
    ifo_handle_t *ifo_file;
    tt_srpt_t *tt_srpt;
    int titles;
    int i, j;

    Logging::debug(path, "Parsing DVD.");

    dvd = DVDOpen(path.c_str());
    if ( !dvd )
    {
        Logging::error(path, "Couldn't open DVD.");
        return ENOENT;
    }

    ifo_file = ifoOpen( dvd, 0 );
    if ( !ifo_file )
    {
        Logging::error(path, "Can't open VMG info." );
        DVDClose( dvd );
        return EINVAL;
    }
    tt_srpt = ifo_file->tt_srpt;

    titles = tt_srpt->nr_of_srpts;

    Logging::debug(path, "There are %1 titles.", titles );

    for( i = 0; i < titles; ++i )
    {
        ifo_handle_t *vts_file;
        vts_ptt_srpt_t *vts_ptt_srpt;
        int vtsnum;
        int ttnnum;
        int pgcnum;
        int chapts;

        vtsnum = tt_srpt->title[ i ].title_set_nr;
        ttnnum = tt_srpt->title[ i ].vts_ttn;
        chapts = tt_srpt->title[ i ].nr_of_ptts;

        Logging::trace(path, "Title : %1", i + 1 );
        Logging::trace(path, "In VTS: %1 [TTN %2]", vtsnum, ttnnum );
        Logging::trace(path, "Title has %1 chapters and %2 angles", chapts, tt_srpt->title[ i ].nr_of_angles );

        vts_file = ifoOpen( dvd, vtsnum );
        if ( !vts_file )
        {
            Logging::error(path, "Can't open info file for title %1.", vtsnum );
            DVDClose( dvd );
            return EINVAL;
        }

        vts_ptt_srpt = vts_file->vts_ptt_srpt;
        for( j = 0; j < chapts; ++j )
        {
            pgc_t *cur_pgc;
            int start_cell;
            int pgn;
            int title_no = i + 1;
            int chapter_no = j + 1;

            pgcnum      = vts_ptt_srpt->title[ ttnnum - 1 ].ptt[ j ].pgcn;
            pgn         = vts_ptt_srpt->title[ ttnnum - 1 ].ptt[ j ].pgn;
            cur_pgc     = vts_file->vts_pgcit->pgci_srp[ pgcnum - 1 ].pgc;
            start_cell  = cur_pgc->program_map[ pgn - 1 ] - 1;

            Logging::trace(path, "Chapter %<%3d>1 [PGC %<%2d>2, PG %<%2d>3] starts at Cell %4 [sector %<%x>5-%<%x>6]",
                           j, pgcnum, pgn, start_cell,
                           static_cast<uint32_t>(cur_pgc->cell_playback[ start_cell ].first_sector),
                           static_cast<uint32_t>(cur_pgc->cell_playback[ start_cell ].last_sector));

            {
                char title_buf[PATH_MAX + 1];
                std::string origfile;
                struct stat stbuf;
                size_t size = (cur_pgc->cell_playback[ start_cell ].last_sector - cur_pgc->cell_playback[ start_cell ].first_sector) * DVD_VIDEO_LB_LEN;
                //cur_pgc->playback_time;

                //sprintf(title_buf, "Title %02d VTS %02d [TTN %02d] Chapter %03d [PGC %02d, PG %02d].%s", title_no, vtsnum, ttnnum, chapter_no, pgcnum, pgn, params.get_current_format().m_desttype);
                sprintf(title_buf, "%02d. Chapter %03d.%s", title_no, chapter_no, params.m_format[0].m_desttype.c_str());   // can safely assume this a video

                std::string filename(title_buf);

                origfile = path + filename;

                memcpy(&stbuf, statbuf, sizeof(struct stat));

                stbuf.st_size   = static_cast<__off_t>(size);
                stbuf.st_blocks = (stbuf.st_size + 512 - 1) / 512;

                //init_stat(&st, size, false);

                if (buf != nullptr && filler(buf, filename.c_str(), &stbuf, 0))
                {
                    // break;
                }

                LPVIRTUALFILE virtualfile = insert_file(VIRTUALTYPE_DVD, path + filename, origfile, &stbuf);

                // DVD is video format anyway
                virtualfile->m_format_idx       = 0;
                // Mark title/chapter/angle
                virtualfile->dvd.m_title_no     = title_no;
                virtualfile->dvd.m_chapter_no   = chapter_no;
                virtualfile->dvd.m_angle_no     = 1;            // TODO
            }
        }

        ifoClose( vts_file );
    }

    ifoClose( ifo_file );
    DVDClose( dvd );

    return titles;    // Number of titles on disk
}

// Returns -errno or number or titles on DVD
int check_dvd(const std::string & _path, void *buf, fuse_fill_dir_t filler)
{
    std::string path(_path);
    struct stat st;
    int res = 0;

    append_sep(&path);

    if (stat((path + "VIDEO_TS.IFO").c_str(), &st) == 0 || stat((path + "VIDEO_TS/VIDEO_TS.IFO").c_str(), &st) == 0)
    {
        Logging::trace(path, "DVD detected.");
        if (buf != nullptr && filler != nullptr)
        {
            res = parse_dvd(path, &st, buf, filler);
            Logging::trace(path, "Found %1 titles.", res);
        }
        else
        {
            res = 1;
        }
    }
    return res;
}

#endif // USE_LIBDVD
