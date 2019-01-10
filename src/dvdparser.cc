/*
 * DVD parser for ffmpegfs
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

static int dvd_find_best_audio_stream(vtsi_mat_t *vtsi_mat, int *best_channels, int *best_sample_frequency);

static int dvd_find_best_audio_stream(vtsi_mat_t *vtsi_mat, int *best_channels, int *best_sample_frequency)
{
    int best_stream = -1;
    int best_application_mode = INT_MAX;
    int best_lang_extension = INT_MAX;
    int best_quantization = 0;

    *best_channels = 0;
    *best_sample_frequency = 0;

    for(int i = 0; i < vtsi_mat->nr_of_vts_audio_streams; i++)
    {
        audio_attr_t *attr = &vtsi_mat->vts_audio_attr[i];

        if (attr->audio_format == 0
                && attr->multichannel_extension == 0
                && attr->lang_type == 0
                && attr->application_mode == 0
                && attr->quantization == 0
                && attr->sample_frequency == 0
                && attr->unknown1 == 0
                && attr->channels == 0
                && attr->lang_extension == 0
                && attr->unknown3 == 0) {
            // Unspecified
            continue;
        }

        // Preference in this order, if higher value is same, compare next and so on.
        //
        // application_mode: prefer not specified.
        //  0: not specified
        //  1: karaoke mode
        //  2: surround sound mode
        // lang_extension: prefer not specified or normal audio
        //  0: Not specified
        //  1: Normal audio/Caption
        //  2: visually impaired
        //  3: Director's comments 1
        //  4: Director's comments 2
        // sample_frequency: maybe 48K only
        //  0: 48kHz
        //  1: ??kHz
        // quantization prefer highest bit width or drc
        //  0: 16bit
        //  1: 20bit
        //  2: 24bit
        //  3: drc
        // channels: prefer no extension
        //  multichannel_extension

        //        if ((best_multiframe >  multiframe) ||
        //        (best_multiframe == multiframe && best_bitrate >  bitrate) ||
        //        (best_multiframe == multiframe && best_bitrate == bitrate && best_count >= count))

        // Specs keep the meaning of the values of this field other than 0 secret, so we nail it to 48 kHz.
        int sample_frequency = 48000;

        if ((best_application_mode < attr->application_mode) ||
                (best_application_mode == attr->application_mode && best_lang_extension < attr->lang_extension) ||
                (best_application_mode == attr->application_mode && best_lang_extension == attr->lang_extension && *best_sample_frequency > sample_frequency) ||
                (best_application_mode == attr->application_mode && best_lang_extension == attr->lang_extension && *best_sample_frequency == sample_frequency && *best_channels > attr->channels) ||
                (best_application_mode == attr->application_mode && best_lang_extension == attr->lang_extension && *best_sample_frequency == sample_frequency && *best_channels == attr->channels && best_quantization > attr->quantization)
                )
        {
            continue;
        }

        best_stream             = i;
        best_application_mode   = attr->application_mode;
        best_lang_extension     = attr->lang_extension;
        *best_sample_frequency  = sample_frequency;
        *best_channels          = attr->channels;
        best_quantization       = attr->quantization;
    }

    if (best_stream > -1)
    {
        ++*best_channels;
    }

    return best_stream;
}

int parse_dvd(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    dvd_reader_t *dvd;
    ifo_handle_t *ifo_file;
    tt_srpt_t *tt_srpt;
    int titles;
    int i, j;

    Logging::debug(path, "Parsing DVD.");

    dvd = DVDOpen(path.c_str());
    if (!dvd)
    {
        Logging::error(path, "Couldn't open DVD.");
        return ENOENT;
    }

    ifo_file = ifoOpen(dvd, 0);
    if (!ifo_file)
    {
        Logging::error(path, "Can't open VMG info.");
        DVDClose(dvd);
        return EINVAL;
    }
    tt_srpt = ifo_file->tt_srpt;

    titles = tt_srpt->nr_of_srpts;

    Logging::debug(path, "There are %1 titles.", titles);

    for(i = 0; i < titles; ++i)
    {
        ifo_handle_t *vts_file;
        vts_ptt_srpt_t *vts_ptt_srpt;
        int vtsnum;
        int ttnnum;
        int pgcnum;
        int chapts;

        vtsnum = tt_srpt->title[i].title_set_nr;
        ttnnum = tt_srpt->title[i].vts_ttn;
        chapts = tt_srpt->title[i].nr_of_ptts;

        Logging::trace(path, "Title : %1", i + 1);
        Logging::trace(path, "In VTS: %1 [TTN %2]", vtsnum, ttnnum);
        Logging::trace(path, "Title has %1 chapters and %2 angles", chapts, tt_srpt->title[i].nr_of_angles);

        vts_file = ifoOpen(dvd, vtsnum);
        if (!vts_file)
        {
            Logging::error(path, "Can't open info file for title %1.", vtsnum);
            DVDClose(dvd);
            return EINVAL;
        }

        // Set reasonable defaults
        BITRATE audio_bit_rate = 256000;
        int channels = 2;
        int sample_rate = 48000;
        int audio = 0;

        BITRATE video_bit_rate = 8000000;
        int width = 720;
        int height = 576;

        if (vts_file->vtsi_mat)
        {
            audio = dvd_find_best_audio_stream(vts_file->vtsi_mat, &channels, &sample_rate);

            height = (vts_file->vtsi_mat->vts_video_attr.video_format != 0) ? 576 : 480;

            switch(vts_file->vtsi_mat->vts_video_attr.picture_size)
            {
            case 0:
            {
                width = 720;
                break;
            }
            case 1:
            {
                width = 704;
                break;
            }
            case 2:
            {
                width = 352;
                break;
            }
            case 3:
            {
                width = 352;
                height /= 2;
                break;
            }
            default:
            {
                Logging::warning(path, "DVD video contains invalid picture size atttribute.");
            }
            }
        }

        vts_ptt_srpt = vts_file->vts_ptt_srpt;
        for(j = 0; j < chapts; ++j)
        {
            pgc_t *cur_pgc;
            int start_cell;
            int pgn;
            int title_no = i + 1;
            int chapter_no = j + 1;

            pgcnum      = vts_ptt_srpt->title[ttnnum - 1].ptt[j].pgcn;
            pgn         = vts_ptt_srpt->title[ttnnum - 1].ptt[j].pgn;
            cur_pgc     = vts_file->vts_pgcit->pgci_srp[pgcnum - 1].pgc;
            start_cell  = cur_pgc->program_map[pgn - 1] - 1;

            Logging::trace(path, "Chapter %<%3d>1 [PGC %<%02d>2, PG %<%02d>3] starts at Cell %4 [sector %<%x>5-%<%x>6]",
                           j, pgcnum, pgn, start_cell,
                           static_cast<uint32_t>(cur_pgc->cell_playback[start_cell].first_sector),
                           static_cast<uint32_t>(cur_pgc->cell_playback[start_cell].last_sector));

            // Split file if chapter has several angles
            for (int k = 0; k < tt_srpt->title[i].nr_of_angles; k++)
            {
                char title_buf[PATH_MAX + 1];
                std::string origfile;
                struct stat stbuf;
                size_t size = (cur_pgc->cell_playback[start_cell].last_sector - cur_pgc->cell_playback[start_cell].first_sector) * DVD_VIDEO_LB_LEN;
                int angle_no = k + 1;
                //cur_pgc->playback_time;

                //sprintf(title_buf, "Title %02d VTS %02d [TTN %02d] Chapter %03d [PGC %02d, PG %02d].%s", title_no, vtsnum, ttnnum, chapter_no, pgcnum, pgn, params.get_current_format().real_desttype());
                if (k && tt_srpt->title[i].nr_of_angles > 1)
                {
                    sprintf(title_buf, "%02d. Chapter %03d [Angle %d].%s", title_no, chapter_no, angle_no, params.m_format[0].real_desttype().c_str());   // can safely assume this a video
                }
                else
                {
                    sprintf(title_buf, "%02d. Chapter %03d.%s", title_no, chapter_no, params.m_format[0].real_desttype().c_str());   // can safely assume this a video
                }

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
                virtualfile->dvd.m_angle_no     = angle_no;

                if (!transcoder_cached_filesize(virtualfile, &stbuf))
                {
                    int first_cell = start_cell;

                    // Check if we're entering an angle block.
                    if (cur_pgc->cell_playback[first_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK)
                    {
                        first_cell += virtualfile->dvd.m_angle_no;
                    }

                    double frame_rate       = (((cur_pgc->cell_playback[first_cell].playback_time.frame_u & 0xc0) >> 6) == 1) ? 25 : 29.97;
                    int64_t frac            = static_cast<int64_t>((cur_pgc->cell_playback[first_cell].playback_time.frame_u & 0x3f) * AV_TIME_BASE / frame_rate);
                    int64_t duration        = static_cast<int64_t>((cur_pgc->cell_playback[first_cell].playback_time.hour * 60 + cur_pgc->cell_playback[first_cell].playback_time.minute) * 60 + cur_pgc->cell_playback[first_cell].playback_time.second) * AV_TIME_BASE + frac;
                    uint64_t size           = (cur_pgc->cell_playback[first_cell].last_sector - cur_pgc->cell_playback[first_cell].first_sector) * 2048;
                    int interleaved         = cur_pgc->cell_playback[first_cell].interleaved;
                    double secsduration     = static_cast<double>(duration) / AV_TIME_BASE;

                    virtualfile->dvd.m_duration = duration;

                    if (secsduration != 0.)
                    {
                        video_bit_rate      = static_cast<BITRATE>(static_cast<double>(size) * 8 / secsduration);   // calculate bitrate in bps
                    }

                    Logging::debug(virtualfile->m_origfile, "Video %1 %2x%3@%<%5.2f>4%5 fps %6 [%7]", format_bitrate(video_bit_rate), width, height, frame_rate, interleaved ? "i" : "p", format_size(size), format_duration(static_cast<int64_t>(secsduration * AV_TIME_BASE)));
                    if (audio > -1)
                    {
                        Logging::debug(virtualfile->m_origfile, "Audio %1 Channels %2", channels, sample_rate);
                    }

                    transcoder_set_filesize(virtualfile, secsduration, audio_bit_rate, channels, sample_rate, video_bit_rate, width, height, interleaved, frame_rate);
                }
            }
        }

        ifoClose(vts_file);
    }

    ifoClose(ifo_file);
    DVDClose(dvd);

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
        res = parse_dvd(path, &st, buf, filler);
       	Logging::trace(path, "Found %1 titles.", res);
    }
    return res;
}

#endif // USE_LIBDVD
