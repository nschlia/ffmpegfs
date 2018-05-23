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

//#include <dvdnav/dvdnav.h>
#include <dvdread/dvd_reader.h>
//#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
//#include <dvdread/dvd_udf.h>
//#include <dvdread/nav_read.h>
//#include <dvdread/nav_print.h>

LPVIRTUALFILE insert_file(VIRTUALTYPE type, const string &filename, const string & origfile, const struct stat *st);

int parse_dvd(const string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    dvd_reader_t *dvd;
    ifo_handle_t *ifo_file;
    tt_srpt_t *tt_srpt;
    int titles;
    int i, j;

    dvd = DVDOpen(path.c_str());
    if ( !dvd )
    {
        ffmpegfs_error("Couldn't open DVD: %s\n", path.c_str());
        return ENOENT;
    }

    ifo_file = ifoOpen( dvd, 0 );
    if ( !ifo_file )
    {
        ffmpegfs_error("Can't open VMG info." );
        DVDClose( dvd );
        return EINVAL;
    }
    tt_srpt = ifo_file->tt_srpt;

    titles = tt_srpt->nr_of_srpts;

    ffmpegfs_debug( "There are %d titles.", titles );

    for( i = 0; i < titles; ++i )
    {
        ifo_handle_t *vts_file;
        vts_ptt_srpt_t *vts_ptt_srpt;
        int vtsnum, ttnnum, pgcnum, chapts;

        vtsnum = tt_srpt->title[ i ].title_set_nr;
        ttnnum = tt_srpt->title[ i ].vts_ttn;
        chapts = tt_srpt->title[ i ].nr_of_ptts;

        ffmpegfs_trace( "Title : %d", i + 1 );
        ffmpegfs_trace( "In VTS: %d [TTN %d]", vtsnum, ttnnum );
        ffmpegfs_trace( "Title has %d chapters and %d angles", chapts, tt_srpt->title[ i ].nr_of_angles );

        vts_file = ifoOpen( dvd, vtsnum );
        if ( !vts_file )
        {
            ffmpegfs_error("Can't open info file for title %d.", vtsnum );
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

            pgcnum = vts_ptt_srpt->title[ ttnnum - 1 ].ptt[ j ].pgcn;
            pgn = vts_ptt_srpt->title[ ttnnum - 1 ].ptt[ j ].pgn;
            cur_pgc = vts_file->vts_pgcit->pgci_srp[ pgcnum - 1 ].pgc;
            start_cell = cur_pgc->program_map[ pgn - 1 ] - 1;

            ffmpegfs_trace("Chapter %3d [PGC %2d, PG %2d] starts at Cell %2d [sector %x-%x]",
                           j, pgcnum, pgn, start_cell,
                           cur_pgc->cell_playback[ start_cell ].first_sector,
                           cur_pgc->cell_playback[ start_cell ].last_sector );

            {
                char title_buf[PATH_MAX + 1];
                string origfile;
                struct stat st;
                size_t size = (cur_pgc->cell_playback[ start_cell ].last_sector - cur_pgc->cell_playback[ start_cell ].first_sector) * DVD_VIDEO_LB_LEN;
                //cur_pgc->playback_time;

                //sprintf(title_buf, "Title %02d VTS %02d [TTN %02d] Chapter %03d [PGC %02d, PG %02d].%s", title_no, vtsnum, ttnnum, chapter_no, pgcnum, pgn, params.m_desttype);
                sprintf(title_buf, "%02d. Chapter %03d.%s", title_no, chapter_no, params.m_desttype);

                string filename(title_buf);

                origfile = path + filename;

                memcpy(&st, statbuf, sizeof(struct stat));

                st.st_size = size;
                st.st_blocks = (st.st_size + 512 - 1) / 512;

                //init_stat(&st, size, false);

                if (buf != NULL && filler(buf, filename.c_str(), &st, 0))
                {
                    // break;
                }

                LPVIRTUALFILE virtualfile = insert_file(VIRTUALTYPE_DVD, path + filename, origfile, &st);

                // Mark title/chapter/angle
                virtualfile->dvd.m_title_no    = title_no;
                virtualfile->dvd.m_chapter_no  = chapter_no;
                virtualfile->dvd.m_angle_no    = 1;            // TODO
            }
        }

        ifoClose( vts_file );
    }

    ifoClose( ifo_file );
    DVDClose( dvd );

    return titles;    // Number of titles on disk
}

// Returns -errno or number or titles on DVD
int check_dvd(const string & _path, void *buf, fuse_fill_dir_t filler)
{
    string path(_path);
    struct stat st;
    int res = 0;

    append_sep(&path);

    if (stat((path + "VIDEO_TS.IFO").c_str(), &st) == 0 || stat((path + "VIDEO_TS/VIDEO_TS.IFO").c_str(), &st) == 0)
    {
        ffmpegfs_debug("DVD detected: %s", path.c_str());

        res = parse_dvd(path, &st, buf, filler);

        ffmpegfs_debug("Found %i titles", res);
    }
    return res;
}

#endif // USE_LIBDVD
