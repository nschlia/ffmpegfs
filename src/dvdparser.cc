/*
 * DVD parser for FFmpegfs
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

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_read.h>

static int dvd_find_best_audio_stream(vtsi_mat_t *vtsi_mat, int *best_channels, int *best_sample_frequency);
static int parse_dvd(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);

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
                && attr->unknown3 == 0)
        {
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

static int parse_dvd(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    dvd_reader_t *dvd;
    ifo_handle_t *ifo_file;
    tt_srpt_t *tt_srpt;
    int titles;
    bool success = true;

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
        Logging::error(path, "Can't open VMG info for DVD.");
        DVDClose(dvd);
        return -EINVAL;
    }
    tt_srpt = ifo_file->tt_srpt;

    titles = tt_srpt->nr_of_srpts;

    Logging::debug(path, "There are %1 titles on this DVD.", titles);

    for (int title_idx = 0; title_idx < titles && success; ++title_idx)
    {
        ifo_handle_t *vts_file;
        int vtsnum      = tt_srpt->title[title_idx].title_set_nr;
        int ttnnum      = tt_srpt->title[title_idx].vts_ttn;
        int chapters    = tt_srpt->title[title_idx].nr_of_ptts;
        int angles      = tt_srpt->title[title_idx].nr_of_angles;

        Logging::trace(path, "Title: %1 VTS: %2 TTN: %3", title_idx + 1, vtsnum, ttnnum);
        Logging::trace(path, "DVD title has %1 chapters and %2 angles.", chapters, angles);

        vts_file = ifoOpen(dvd, vtsnum);
        if (!vts_file)
        {
            Logging::error(path, "Can't open info file for title %1.", vtsnum);
            DVDClose(dvd);
            return -EINVAL;
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
                Logging::warning(path, "DVD video contains invalid picture size attribute.");
            }
            }
        }

        // Check if chapter is valid
        c_adt_t *c_adt = vts_file->menu_c_adt;
        size_t  info_length = 0;

        if (c_adt != nullptr)
        {
            info_length = c_adt->last_byte + 1 - C_ADT_SIZE;
        }

        bool skip = false;

        for (unsigned int n = 0; n < info_length / sizeof(cell_adr_t) && !skip; n++)
        {
            skip = (c_adt->cell_adr_table[n].start_sector >= c_adt->cell_adr_table[n].last_sector);
        }

        if (skip)
        {
            Logging::info(path, "Title %1 has invalid size, ignoring.", title_idx + 1);
        }
        else
        {
            vts_ptt_srpt_t *vts_ptt_srpt = vts_file->vts_ptt_srpt;
            for (int chapter_idx = 0; chapter_idx < chapters; ++chapter_idx)
            {
                int title_no = title_idx + 1;
                int chapter_no = chapter_idx + 1;
                pgc_t *cur_pgc;
                int start_cell;
                int pgn;
                int pgcnum;

                pgcnum      = vts_ptt_srpt->title[ttnnum - 1].ptt[chapter_idx].pgcn;
                pgn         = vts_ptt_srpt->title[ttnnum - 1].ptt[chapter_idx].pgn;
                cur_pgc     = vts_file->vts_pgcit->pgci_srp[pgcnum - 1].pgc;
                start_cell  = cur_pgc->program_map[pgn - 1] - 1;

                Logging::trace(path, "Title %<%3d>1 chapter %<%3d>2 [PGC %<%02d>3, PG %<%02d>4] starts at cell %5 [sector %<%x>6-%<%x>7]",
                               title_no, chapter_no, pgcnum, pgn, start_cell,
                               static_cast<uint32_t>(cur_pgc->cell_playback[start_cell].first_sector),
                               static_cast<uint32_t>(cur_pgc->cell_playback[start_cell].last_sector));

                // Split file if chapter has several angles
                for (int k = 0; k < angles; k++)
                {
                    char title_buf[PATH_MAX + 1];
                    std::string origfile;
                    struct stat stbuf;
                    size_t size = (cur_pgc->cell_playback[start_cell].last_sector - cur_pgc->cell_playback[start_cell].first_sector) * DVD_VIDEO_LB_LEN;
                    int angle_no = k + 1;
                    //cur_pgc->playback_time;

                    if (k && angles > 1)
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
                    virtualfile->m_dvd.m_title_no     = title_no;
                    virtualfile->m_dvd.m_chapter_no   = chapter_no;
                    virtualfile->m_dvd.m_angle_no     = angle_no;

                    if (!transcoder_cached_filesize(virtualfile, &stbuf))
                    {
                        int first_cell = start_cell;

                        // Check if we're entering an angle block.
                        if (cur_pgc->cell_playback[first_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK)
                        {
                            first_cell += virtualfile->m_dvd.m_angle_no;
                        }

                        double frame_rate       = (((cur_pgc->cell_playback[first_cell].playback_time.frame_u & 0xc0) >> 6) == 1) ? 25 : 29.97;
                        int64_t frac            = static_cast<int64_t>((cur_pgc->cell_playback[first_cell].playback_time.frame_u & 0x3f) * AV_TIME_BASE / frame_rate);
                        int64_t duration        = static_cast<int64_t>((cur_pgc->cell_playback[first_cell].playback_time.hour * 60 + cur_pgc->cell_playback[first_cell].playback_time.minute) * 60 + cur_pgc->cell_playback[first_cell].playback_time.second) * AV_TIME_BASE + frac;
                        uint64_t size           = (cur_pgc->cell_playback[first_cell].last_sector - cur_pgc->cell_playback[first_cell].first_sector) * 2048;
                        int interleaved         = cur_pgc->cell_playback[first_cell].interleaved;
                        double secsduration     = static_cast<double>(duration) / AV_TIME_BASE;

                        virtualfile->m_dvd.m_duration = duration;

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
        }

        ifoClose(vts_file);
    }

    ifoClose(ifo_file);
    DVDClose(dvd);

    if (success)
    {
        return titles;    // Number of titles on disk
    }
    else
    {
        return -errno;
    }
}

int check_dvd(const std::string & _path, void *buf, fuse_fill_dir_t filler)
{
    std::string path(_path);
    struct stat st;
    int res = 0;

    append_sep(&path);

    if (stat((path + "VIDEO_TS.IFO").c_str(), &st) == 0 || stat((path + "VIDEO_TS/VIDEO_TS.IFO").c_str(), &st) == 0)
    {
        if (!check_path(path))
        {
            Logging::trace(path, "DVD detected.");
            res = parse_dvd(path, &st, buf, filler);
            Logging::trace(path, "Found %1 titles.", res);
        }
        else
        {
            res = load_path(path, &st, buf, filler);
        }
    }
    return res;
}

#endif // USE_LIBDVD
