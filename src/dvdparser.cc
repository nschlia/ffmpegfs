/*
 * Copyright (C) 2017-2021 Norbert Schlia (nschlia@oblivion-software.de)
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

#ifdef USE_LIBDVD

/**
 * @file
 * @brief dvdparser class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2021 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "ffmpegfs.h"
#include "dvdparser.h"
#include "transcode.h"
#include "ffmpeg_utils.h"
#include "logging.h"

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_read.h>

extern "C" {
#include <libavutil/rational.h>
}

typedef struct AUDIO_SETTINGS                       /** @brief Audio stream settings */
{
    BITRATE m_audio_bit_rate;                       /**< @brief average bitrate of audio data (in bits per second) */
    int     m_channels;                             /**< @brief number of channels (1: mono, 2: stereo, or more) */
    int     m_sample_rate;                          /**< @brief number of audio samples per second */
} AUDIO_SETTINGS;
typedef AUDIO_SETTINGS const *LPCAUDIO_SETTINGS;    /**< @brief Pointer to const version of AUDIO_SETTINGS */
typedef AUDIO_SETTINGS *LPAUDIO_SETTINGS;           /**< @brief Pointer version of AUDIO_SETTINGS */

typedef struct VIDEO_SETTINGS                       /** @brief Video stream settings */
{
    BITRATE m_video_bit_rate;                       /**< @brief average bitrate of video data (in bits per second) */
    int     m_width;                                /**< @brief video width in pixels */
    int     m_height;                               /**< @brief video height in pixels */
} VIDEO_SETTINGS;
typedef VIDEO_SETTINGS const *LPCVIDEO_SETTINGS;    /**< @brief Pointer to const version of VIDEO_SETTINGS */
typedef VIDEO_SETTINGS *LPVIDEO_SETTINGS;           /**< @brief Pointer version of VIDEO_SETTINGS */

static int          dvd_find_best_audio_stream(const vtsi_mat_t *vtsi_mat, int *best_channels, int *best_sample_frequency);
static AVRational   dvd_frame_rate(const uint8_t * ptr);
static int64_t      BCDtime(const dvd_time_t * dvd_time);
static bool         create_dvd_virtualfile(const ifo_handle_t *vts_file, const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler, bool full_title, int title_idx, int chapter_idx, int angles, int ttnnum, int audio_stream, const AUDIO_SETTINGS & audio_settings, const VIDEO_SETTINGS & video_settings);
static int          parse_dvd(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);

/**
 * @brief Locate best matching audio stream.
 * @param[in] vtsi_mat - Video Title Set Information (VTSI)
 * @param[out] best_channels - Number of channels in best stream.
 * @param[out] best_sample_frequency - Sample frequency in best stream.
 * @return Returns number of best stream (0..8).
 */
static int dvd_find_best_audio_stream(const vtsi_mat_t *vtsi_mat, int *best_channels, int *best_sample_frequency)
{
    int best_stream = -1;
    int best_application_mode = INT_MAX;
    int best_lang_extension = INT_MAX;
    int best_quantization = 0;

    *best_channels = 0;
    *best_sample_frequency = 0;

    for(int i = 0; i < vtsi_mat->nr_of_vts_audio_streams; i++)
    {
        const audio_attr_t *attr = &vtsi_mat->vts_audio_attr[i];

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

/**
 * @brief Get the frame rate of the DVD. Can be 25 fps (PAL) or  29.97 (NTCS).
 * @param[in] ptr - Pointer to frame_u element in dvd_time_t structure.
 * @return On success returns AVRational with frame rate. On error returns frame rate { 0, 0 }.
 */
static AVRational dvd_frame_rate(const uint8_t * ptr)
{
    AVRational framerate = { 0, 0 };

    // 11 = 30 fps, 10 = illegal, 01 = 25 fps, 00 = illegal
    unsigned fps = ((ptr[3] & 0xC0) >> 6) & 0x03;

    switch (fps)
    {
    case 3: // PAL
    {
        framerate = av_make_q(25000, 1000);
        break;
    }
    case 1: // NTSC
    {
        framerate = av_make_q(30000, 10001);
        break;
    }
    default:
    {
        // Frame rate is illegal, so we need to set anything. Assume PAL.
        framerate = av_make_q(25000, 1000);
        break;
    }
    }

    return framerate;
}

/**
 * @brief Convert a time in BCD format into AV_TIMEBASE fractional seconds.
 * @param[in] dvd_time - dvd_time_t object.
 * @return Time in AV_TIMEBASE fractional seconds.
 */
static int64_t BCDtime(const dvd_time_t * dvd_time)
{
    int64_t time[4];
    AVRational  framerate = dvd_frame_rate(&dvd_time->frame_u);

    if (!framerate.den)
    {
        framerate = av_make_q(25000, 1000);                    // Avoid divisions by 0
    }
    time[0] = dvd_time->hour;
    time[1] = dvd_time->minute;
    time[2] = dvd_time->second;
    time[3] = dvd_time->frame_u & 0x3F;     // Number of frame

    // convert BCD (two digits) to binary
    for (int i = 0; i < 4; i++)
    {
        time[i] = ((time[i] & 0xf0) >> 4) * 10 + (time[i] & 0x0f);
    }

    return (AV_TIME_BASE * (time[0] * 3600 + time[1] * 60 + time[2]) + static_cast<int64_t>(static_cast<double>(AV_TIME_BASE * time[3]) / av_q2d(framerate)));
}

/**
 * @brief Create a virtual file for a DVD.
 * @param[in] vts_file - Structure defines an IFO file
 * @param[in] path - Path to DVD files.
 * @param[in] statbuf - File status structure of original file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @param[in] full_title - If true, create virtual file of all title. If false, include single chapter only.
 * @param[in] title_idx - Index of DVD title.
 * @param[in] chapter_idx - Index of DVD chapter.
 * @param[in] angles - Number of angles.
 * @param[in] ttnnum  - DVD title number.
 * @param[in] audio_stream  - Audio stream index.
 * @param[in] audio_settings - Audio stream settings.
 * @param[in] video_settings - Video stream settings.
 * @return Returns true if successful. Returns false on error.
 */
static bool create_dvd_virtualfile(const ifo_handle_t *vts_file, const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler, bool full_title, int title_idx, int chapter_idx, int angles, int ttnnum, int audio_stream, const AUDIO_SETTINGS & audio_settings, const VIDEO_SETTINGS & video_settings)
{
    const vts_ptt_srpt_t *vts_ptt_srpt = vts_file->vts_ptt_srpt;
    int title_no            = title_idx + 1;
    int chapter_no          = chapter_idx + 1;
    int pgcnum              = vts_ptt_srpt->title[ttnnum - 1].ptt[chapter_idx].pgcn;
    int pgn                 = vts_ptt_srpt->title[ttnnum - 1].ptt[chapter_idx].pgn;
    const pgc_t *cur_pgc    = vts_file->vts_pgcit->pgci_srp[pgcnum - 1].pgc;
    AVRational framerate    = { 0, 0 };
    int64_t duration        = 0;
    uint64_t size           = 0;
    int interleaved         = 0;
    int start_cell          = cur_pgc->program_map[pgn - 1] - 1;
    int end_cell            = 0;

    if (pgn < cur_pgc->nr_of_programs && !full_title)
    {
        end_cell    = cur_pgc->program_map[pgn] - 1;
    }
    else
    {
        end_cell    = cur_pgc->nr_of_cells;
    }

    interleaved         = cur_pgc->cell_playback[start_cell].interleaved;
    framerate           = dvd_frame_rate(&cur_pgc->cell_playback[start_cell].playback_time.frame_u);

    bool has_angles = false;

    for (int cell_no = start_cell; cell_no < end_cell; cell_no++)
    {
        cell_playback_t *cell_playback = &cur_pgc->cell_playback[cell_no];

        // Only count normal cells and the first of an angle to avoid duplicate sizes
        if (cell_playback->block_mode == BLOCK_MODE_NOT_IN_BLOCK || cell_playback->block_mode == BLOCK_MODE_FIRST_CELL)
        {
            size        += (cell_playback->last_sector - cell_playback->first_sector) * 2048;
            duration    += BCDtime(&cell_playback->playback_time);
        }

        if (cell_playback->block_type == BLOCK_TYPE_ANGLE_BLOCK)
        {
            has_angles = true;
        }
    }

    if (duration < params.m_min_dvd_chapter_duration * AV_TIME_BASE)
    {
        Logging::debug(nullptr, "Skipping short DVD chapter.");
        return true;
    }

    if (!has_angles)
    {
        // If this chapter has no angle cells, reset angles to 1
        angles = 1;
    }

    // Split file if chapter has several angles
    for (int angle_idx = 0; angle_idx < angles; angle_idx++)
    {
        char title_buf[PATH_MAX + 1];
        int angle_no        = angle_idx + 1;

        // can safely assume this a video
        if (!full_title)
        {
            // Single chapter
            if (angles > 1)
            {
                snprintf(title_buf, sizeof(title_buf) - 1, "%02d. Chapter %03d (Angle %d) [%s].%s",
                        title_no,
                        chapter_no,
                        angle_no,
                        replace_all(format_duration(duration), ":", "-").c_str(),
                        params.m_format[0].fileext().c_str());
            }
            else
            {
                snprintf(title_buf, sizeof(title_buf) - 1, "%02d. Chapter %03d [%s].%s",
                        title_no,
                        chapter_no,
                        replace_all(format_duration(duration), ":", "-").c_str(),
                        params.m_format[0].fileext().c_str());
            }
        }
        else
        {
            // Full title
            if (angles > 1)
            {
                snprintf(title_buf, sizeof(title_buf) - 1, "%02d. Title (Angle %d) [%s].%s",
                        title_no,
                        angle_no,
                        replace_all(format_duration(duration), ":", "-").c_str(),
                        params.m_format[0].fileext().c_str());
            }
            else
            {
                snprintf(title_buf, sizeof(title_buf) - 1, "%02d. Title [%s].%s",
                        title_no,
                        replace_all(format_duration(duration), ":", "-").c_str(),
                        params.m_format[0].fileext().c_str());
            }
        }

        std::string filename(title_buf);

        LPVIRTUALFILE virtualfile = nullptr;
        if (!params.m_format[0].is_multiformat())
        {
            virtualfile = insert_file(VIRTUALTYPE_DVD, path + filename, statbuf);
        }
        else
        {
            virtualfile = insert_dir(VIRTUALTYPE_DVD, path + filename, statbuf);
        }

        if (virtualfile == nullptr)
        {
            Logging::error(path, "Failed to create virtual path: %1", (path + filename).c_str());
            errno = EIO;
            return false;
        }

        if (add_fuse_entry(buf, filler, title_buf, &virtualfile->m_st, 0))
        {
            // break;
        }

        // DVD is video format anyway
        virtualfile->m_format_idx       = 0;
        // Mark title/chapter/angle
        virtualfile->m_full_title       = full_title;
        virtualfile->m_dvd.m_title_no   = title_no;
        virtualfile->m_dvd.m_chapter_no = chapter_no;
        virtualfile->m_dvd.m_angle_no   = angle_no;

        if (!transcoder_cached_filesize(virtualfile, &virtualfile->m_st))
        {
            virtualfile->m_duration = duration;

            BITRATE video_bit_rate = 8*1024*1024;   // In case the real bitrate cannot be calculated later, assume 8 Mbit video bitrate
            if (duration)
            {
                /**
                 * @todo We actually calculate the overall DVD bitrate here, including all audio
                 * streams, not just the video bitrate. This should be the video bitrate alone. We
                 * should also calculate the audio bitrate for the selected stream.
                */
                video_bit_rate      = static_cast<BITRATE>(size * 8LL * AV_TIME_BASE / static_cast<uint64_t>(duration));   // calculate bitrate in bps
            }

            Logging::trace(virtualfile->m_origfile, "Video %1 %2x%3@%<%5.2f>4%5 fps %6 [%7]", format_bitrate(video_settings.m_video_bit_rate).c_str(), video_settings.m_width, video_settings.m_height, av_q2d(framerate), interleaved ? "i" : "p", format_size(size).c_str(), format_duration(duration).c_str());
            if (audio_stream > -1)
            {
                Logging::trace(virtualfile->m_origfile, "Audio %1 Channels %2", audio_settings.m_channels, audio_settings.m_sample_rate);
            }

            transcoder_set_filesize(virtualfile, duration, audio_settings.m_audio_bit_rate, audio_settings.m_channels, audio_settings.m_sample_rate, video_bit_rate, video_settings.m_width, video_settings.m_height, interleaved, framerate);

            virtualfile->m_video_frame_count    = static_cast<uint32_t>(av_rescale_q(duration, av_get_time_base_q(), av_inv_q(framerate)));
            virtualfile->m_predicted_size       = static_cast<size_t>(size);
        }
    }

    return true;
}

/**
 * @brief Parse DVD directory and get all DVD titles and chapters as virtual files.
 * @param[in] path - Path to check.
 * @param[in] statbuf - File status structure of original file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @return On success, returns number of chapters found. On error, returns -errno.
 */
static int parse_dvd(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    dvd_reader_t *dvd;
    ifo_handle_t *ifo_file;
    tt_srpt_t *tt_srpt;
    int titles;
    bool success = true;

    Logging::debug(path, "Parsing DVD.");

    dvd = DVDOpen(path.c_str());
    if (dvd == nullptr)
    {
        Logging::error(path, "Couldn't open DVD.");
        return ENOENT;
    }

    ifo_file = ifoOpen(dvd, 0);
    if (ifo_file == nullptr)
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
        if (vts_file == nullptr)
        {
            Logging::error(path, "Can't open info file for title %1.", vtsnum);
            DVDClose(dvd);
            return -EINVAL;
        }

        // Set reasonable defaults
        AUDIO_SETTINGS audio_settings;
        audio_settings.m_audio_bit_rate   = 256000;
        audio_settings.m_channels         = 2;
        audio_settings.m_sample_rate      = 48000;
        int audio_stream = 0;

        VIDEO_SETTINGS video_settings;
        video_settings.m_video_bit_rate   = 8000000;
        video_settings.m_width            = 720;
        video_settings.m_height           = 576;

        if (vts_file->vtsi_mat)
        {
            audio_stream = dvd_find_best_audio_stream(vts_file->vtsi_mat, &audio_settings.m_channels, &audio_settings.m_sample_rate);

            video_settings.m_height = (vts_file->vtsi_mat->vts_video_attr.video_format != 0) ? 576 : 480;

            switch(vts_file->vtsi_mat->vts_video_attr.picture_size)
            {
            case 0:
            {
                video_settings.m_width = 720;
                break;
            }
            case 1:
            {
                video_settings.m_width = 704;
                break;
            }
            case 2:
            {
                video_settings.m_width = 352;
                break;
            }
            case 3:
            {
                video_settings.m_width = 352;
                video_settings.m_height /= 2;
                break;
            }
            default:
            {
                Logging::warning(path, "DVD video contains invalid picture size attribute.");
            }
            }
        }

        // Add separate chapters
        for (int chapter_idx = 0; chapter_idx < chapters && success; ++chapter_idx)
        {
            success = create_dvd_virtualfile(vts_file, path, statbuf, buf, filler, false, title_idx, chapter_idx, angles, ttnnum, audio_stream, audio_settings, video_settings);
        }

        if (success && chapters > 1)
        {
            // If more than 1 chapter, add full title as well
            success = create_dvd_virtualfile(vts_file, path, statbuf, buf, filler, true, title_idx, 0, 1, ttnnum, audio_stream, audio_settings, video_settings);
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
    struct stat stbuf;
    int res = 0;

    append_sep(&path);

    if (stat((path + "VIDEO_TS.IFO").c_str(), &stbuf) == 0 || stat((path + "VIDEO_TS/VIDEO_TS.IFO").c_str(), &stbuf) == 0)
    {
        if (!check_path(path))
        {
            Logging::trace(path, "DVD detected.");
            res = parse_dvd(path, &stbuf, buf, filler);
            Logging::trace(path, "Found %1 titles.", res);
        }
        else
        {
            res = load_path(path, &stbuf, buf, filler);
        }
    }
    return res;
}

#endif // USE_LIBDVD
