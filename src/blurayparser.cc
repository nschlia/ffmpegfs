/*
 * Copyright (C) 2017-2023 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file blurayparser.cc
 * @brief Blu-ray disk parser implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2023 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "ffmpegfs.h"
#include "blurayparser.h"
#include "transcode.h"
#include "ffmpeg_utils.h"
#include "logging.h"

#include "libbluray/bluray.h"
#include "libbluray/bluray-version.h"

extern "C" {
#include <libavutil/rational.h>
}

static bool audio_stream_info(const std::string &path, BLURAY_STREAM_INFO *ss, int *channels, int *sample_rate);
static bool video_stream_info(const std::string &path, BLURAY_STREAM_INFO *ss, int *width, int *height, AVRational *framerate, bool *interleaved);
static int  parse_find_best_audio_stream();
static int  parse_find_best_video_stream();
static bool create_bluray_virtualfile(BLURAY *bd, const BLURAY_TITLE_INFO* ti, const std::string & path, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler, bool is_main_title, bool full_title, uint32_t title_idx, uint32_t chapter_idx);
static int  parse_bluray(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);

/**
 * @brief Get information about Blu-ray stream
 * @param[in] path - Path to Blu-ray disk.
 * @param[in] ss - BLURAY_STREAM_INFO object.
 * @param[out] channels - Number of audio channels in stream.
 * @param[out] sample_rate - Sample rate of stream.
 * @return Returns true if stream has video, false if not.
 */
static bool audio_stream_info(const std::string & path, BLURAY_STREAM_INFO *ss, int *channels, int *sample_rate)
{
    bool audio = false;

    switch (ss->coding_type)
    {
    // Video
    case 0x01:
    case 0x02:
    case 0xea:
    case 0x1b:
    case 0x24:
    {
        break;
    }
        // Audio
    case 0x03:
    case 0x04:
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0x86:
    case 0xa1:
    case 0xa2:
    {
        switch (ss->format)
        {
        case BLURAY_AUDIO_FORMAT_MONO:
        {
            *channels = 1;  // Mono
            break;
        }
        case BLURAY_AUDIO_FORMAT_STEREO:
        {
            *channels = 2;  // Stereo
            break;
        }
        case BLURAY_AUDIO_FORMAT_MULTI_CHAN:
        {
            *channels = 2;  // Multi Channel
            break;
        }
        case BLURAY_AUDIO_FORMAT_COMBO:
        {
            *channels = 2;  // Stereo ac3/dts
            break;
        }
        default:
        {
            Logging::error(path, "Unknown number of audio channels %1. Assuming 2 channel/stereo - may be totally wrong.", ss->format);
            *channels = 2;  // Stereo
            break;
        }
        }

        switch (ss->rate)
        {
        case BLURAY_AUDIO_RATE_48:
        {
            *sample_rate = 48000;
            break;
        }
        case BLURAY_AUDIO_RATE_96:
        {
            *sample_rate = 96000;
            break;
        }
        case BLURAY_AUDIO_RATE_192:
        {
            *sample_rate = 192000;
            break;
        }
            // 48 or 96 ac3/dts
            // 192 mpl/dts-hd
        case BLURAY_AUDIO_RATE_192_COMBO:
        {
            // *sample_rate = "48/192 Khz";
            break;
        }
            // 48 ac3/dts
            // 96 mpl/dts-hd
        case BLURAY_AUDIO_RATE_96_COMBO:
        {
            // *sample_rate = "48/96 Khz";
            break;
        }
        default:
        {
            Logging::error(path, "Unknown audio sample rate %1. Assuming 48 kHz - may be totally wrong.", ss->rate);
            *sample_rate = 48000;
            break;
        }
        }

        audio = true;
        break;
    }
    case 0x90:
    case 0x91:
    {
        // Language     ss->lang
        break;
    }
    case 0x92:
    {
        // Char Code    ss->char_code);
        // Language     ss->lang);
        break;
    }
    default:
    {
        Logging::error(path, "Unrecognised coding type %<%02x>1.", ss->coding_type);
        break;
    }
    }

    return audio;
}

/**
 * @brief Get information about Blu-ray stream
 * @param[in] path - Path to Blu-ray disk.
 * @param[in] ss - BLURAY_STREAM_INFO object.
 * @param[out] width - Width of video stream.
 * @param[out] height - Height of video stream.
 * @param[out] framerate - Frame rate of video stream.
 * @param[out] interleaved - true: video stream is interleaved, false: video stream is not interleaved.
 * @return Returns true if stream has video, false if not.
 */
static bool video_stream_info(const std::string & path, BLURAY_STREAM_INFO *ss, int *width, int *height, AVRational *framerate, bool *interleaved)
{
    bool video = false;

    switch (ss->coding_type)
    {
    // Video
    case 0x01:
    case 0x02:
    case 0xea:
    case 0x1b:
    case 0x24:
    {
        // SD
        // 720×480,   59.94i,    4:3 or 16:9

        // 720×576,   50i,       4:3 or 16:9

        // HD
        // 1280×720,  59.94p,    16:9
        // 1280×720,  50p,       16:9
        // 1280×720,  24p,       16:9
        // 1280×720,  23.976p,   16:9

        // 1440×1080, 59.94i,    16:9
        // 1440×1080, 50i,       16:9
        // 1440×1080, 24p,       16:9
        // 1440×1080, 23.976p,   16:9

        // 1920×1080, 59.94i,    16:9
        // 1920×1080, 50i,       16:9
        // 1920×1080, 24p,       16:9
        // 1920×1080, 23.976p,   16:9

        // HD
        // 1920×1080, 60p,       16:9
        // 1920×1080, 59.94p,    16:9
        // 1920×1080, 50p,       16:9
        // 1920×1080, 25p,       16:9

        // 4K UHD
        // 3840×2160, 60p,       16:9
        // 3840×2160, 59.94p,    16:9
        // 3840×2160, 50p,       16:9
        // 3840×2160, 25p,       16:9
        // 3840×2160, 24p,       16:9
        // 3840×2160, 23.976p,   16:9

        switch (ss->format)
        {
        case BLURAY_VIDEO_FORMAT_480I:  // ITU-R BT.601-5
        {
            *width = 720;
            *height = 480;
            *interleaved = true;
            break;
        }
        case BLURAY_VIDEO_FORMAT_576I:  // ITU-R BT.601-4
        {
            *width = 720;
            *height = 576;
            *interleaved = true;
            break;
        }
        case BLURAY_VIDEO_FORMAT_480P:  // SMPTE 293M
        {
            *width = 720;
            *height = 480;
            *interleaved = false;
            break;
        }
        case BLURAY_VIDEO_FORMAT_1080I: // SMPTE 274M
        {
            *width = 1920;
            *height = 1080;
            *interleaved = true;
            break;
        }
        case BLURAY_VIDEO_FORMAT_720P:  // SMPTE 296M
        {
            *height = 1280;
            *width = 720;
            *interleaved = false;
            break;
        }
        case BLURAY_VIDEO_FORMAT_1080P: // SMPTE 274M
        {
            *width = 1920;
            *height = 1080;
            *interleaved = false;
            break;
        }
        case BLURAY_VIDEO_FORMAT_576P:  // ITU-R BT.1358
        {
            *width = 720;
            *height = 576;
            *interleaved = false;
            break;
        }
            // Added with libluray change 14aa7e9c0 (hpi1 2017-08-28 09:50:43 +0300)
            // Available since version 1.1.0
#if (BLURAY_VERSION_MAJOR > 1 || (BLURAY_VERSION_MAJOR == 1 && BLURAY_VERSION_MINOR >= 1))
        case BLURAY_VIDEO_FORMAT_2160P: // UHD
        {
            *width = 3840;
            *height = 2160;
            *interleaved = false;
            break;
        }
#endif
        default:
        {
            Logging::error(path, "Unknown video format %1. Assuming 1920x1080P - may be totally wrong.", ss->format);
            *width = 1920;
            *height = 1080;
            *interleaved = false;
            break;
        }
        }

        switch (ss->rate)
        {
        case BLURAY_VIDEO_RATE_24000_1001:
        {
            *framerate = av_make_q(24000, 1001);
            break;
        }
        case BLURAY_VIDEO_RATE_24:
        {
            *framerate = av_make_q(24000, 1000);
            break;
        }
        case BLURAY_VIDEO_RATE_25:
        {
            *framerate = av_make_q(25000, 1000);
            break;
        }
        case BLURAY_VIDEO_RATE_30000_1001:
        {
            *framerate = av_make_q(30000, 1001);
            break;
        }
        case BLURAY_VIDEO_RATE_50:
        {
            *framerate = av_make_q(50000, 1000);
            break;
        }
        case BLURAY_VIDEO_RATE_60000_1001:
        {
            *framerate = av_make_q(60000, 1001);
            break;
        }
        default:
        {
            Logging::error(path, "Unknown video frame rate %1. Assuming 25 fps - may be totally wrong.", ss->rate);
            *framerate = av_make_q(25000, 1000);
            break;
        }
        }

        video = true;
        break;
    }
        // Audio
    case 0x03:
    case 0x04:
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0x86:
    case 0xa1:
    case 0xa2:
    {
        break;
    }
    case 0x90:
    case 0x91:
    {
        // Language     ss->lang
        break;
    }
    case 0x92:
    {
        // Char Code    ss->char_code);
        // Language     ss->lang);
        break;
    }
    default:
    {
        Logging::error(path, "Unrecognised coding type %<%02x>1.", ss->coding_type);
        break;
    }
    }

    return video;
}

/**
 * @brief Find best match audio stream.
 * @todo Returning 0 is not necessarily the best match. Probably better to parse.
 * For "The life of Brian", e.g., this is the Hungarian audio track. No big deal,
 * though, I can recite mostly all dialogs in English and German (and Latin :),
 * but should be fixed anyway.
 * @return Returns index of best match stream.
 */
static int parse_find_best_audio_stream()
{
    return 0;
}

/**
 * @brief Find best match video stream.
 * @todo Returning 0 is not necessarily the best match. Probably better to parse.
 * Most DVDs contain only one video track anyway, so this does not hurt at the moment.
 * @return Returns index of best match stream.
 */
static int parse_find_best_video_stream()
{
    return 0;
}

/**
 * @brief Create a virtual file entry of a Blu-ray chapter or title.
 * @param[in] bd - Blu-ray disk clip info.
 * @param[in] ti - Blu-ray disk title info.
 * @param[in] path - Path to check.
 * @param[in] statbuf - File status structure of original file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @param[in] is_main_title - true if title_idx is the main title
 * @param[in] full_title - If true, create virtual file of all title. If false, include single chapter only.
 * @param[in] title_idx - Zero-based title index on Blu-ray
 * @param[in] chapter_idx - Zero-based chapter index on Blu-ray
 * @note buf and filler can be nullptr. In that case the call will run faster, so these parameters should only be passed if to be filled in.
 * @return On error, returns false. On success, returns true.
 */
static bool create_bluray_virtualfile(BLURAY *bd, const BLURAY_TITLE_INFO* ti, const std::string & path, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler, bool is_main_title, bool full_title, uint32_t title_idx, uint32_t chapter_idx)
{
    BLURAY_CLIP_INFO     *clip = &ti->clips[0];
    BLURAY_TITLE_CHAPTER *chapter = &ti->chapters[chapter_idx];
    std::string title_buf;
    int64_t duration;

    if (full_title)
    {
        duration  = static_cast<int64_t>(ti->duration) * AV_TIME_BASE / 90000;

        if (duration < AV_TIME_BASE)
        {
            Logging::trace(path, "Title %1: skipping empty title.", title_idx + 1);
            return true;
        }

        strsprintf(&title_buf, "%02u. Title [%s]%s.%s",
                 title_idx + 1,
                 replace_all(format_duration(duration), ":", "-").c_str(),
                 is_main_title ? "+" : "",
                 ffmpeg_format[0].fileext().c_str()); // can safely assume this is a video format
    }
    else
    {
        duration  = static_cast<int64_t>(chapter->duration) * AV_TIME_BASE / 90000;

        if (duration < AV_TIME_BASE)
        {
            Logging::trace(path, "Title %1 Chapter %2: skipping empty chapter.", title_idx + 1, chapter_idx + 1);
            return true;
        }

        strsprintf(&title_buf, "%02u. Chapter %03u [%s]%s.%s",
                 title_idx + 1,
                 chapter_idx + 1,
                 replace_all(format_duration(duration), ":", "-").c_str(),
                 is_main_title ? "+" : "",
                 ffmpeg_format[0].fileext().c_str()); // can safely assume this is a video format

    }

    LPVIRTUALFILE virtualfile = nullptr;
    if (!ffmpeg_format[0].is_multiformat())
    {
        virtualfile = insert_file(VIRTUALTYPE_BLURAY, path + title_buf, statbuf);
    }
    else
    {
        virtualfile = insert_dir(VIRTUALTYPE_BLURAY, path + title_buf, statbuf);
    }

    if (virtualfile == nullptr)
    {
        Logging::error(path, "Failed to create virtual path: %1", (path + title_buf).c_str());
        errno = EIO;
        return false;
    }

    if (add_fuse_entry(buf, filler, title_buf, &virtualfile->m_st, 0))
    {
        // break;
    }

    // Blu-ray is video format anyway
    virtualfile->m_format_idx           = 0;
    // Mark title/chapter/angle
    virtualfile->m_full_title           = full_title;
    virtualfile->m_bluray.m_title_no    = title_idx + 1;
    virtualfile->m_bluray.m_playlist_no = ti->playlist;
    virtualfile->m_bluray.m_chapter_no  = chapter_idx + 1;
    virtualfile->m_bluray.m_angle_no    = 1;

    if (!transcoder_cached_filesize(virtualfile, &virtualfile->m_st))
    {
        BITRATE video_bit_rate  = 29*1024*1024; // In case the real bitrate cannot be calculated later, assume 20 Mbit video bitrate
        BITRATE audio_bit_rate  = 256*1024;     // In case the real bitrate cannot be calculated later, assume 256 kBit audio bitrate

        bool audio              = false;

        bool interleaved        = false;

        if (!bd_select_title(bd, title_idx))
        {
            Logging::error(path, "The Blu-ray title %1 could not be opened.", title_idx);
            errno = EIO;
            return false;
        }

        uint64_t size           = bd_get_title_size(bd);

        virtualfile->m_duration = duration;

        if (duration)
        {
            /**
             * @todo We actually calculate the overall Blu-ray bitrate here, including all audio
             * streams, not just the video bitrate. This should be the video bitrate alone. We
             * should also calculate the audio bitrate for the selected stream.
            */
            video_bit_rate      = static_cast<BITRATE>(size * 8LL * AV_TIME_BASE / static_cast<uint64_t>(duration));   // calculate bitrate in bps
        }

        // Get details
        if (clip->audio_stream_count)
        {
            audio_stream_info(path, &clip->audio_streams[parse_find_best_audio_stream()], &virtualfile->m_channels, &virtualfile->m_sample_rate);
        }
        if (clip->video_stream_count)
        {
            audio = video_stream_info(path, &clip->video_streams[parse_find_best_video_stream()], &virtualfile->m_width, &virtualfile->m_height, &virtualfile->m_framerate, &interleaved);
        }

        Logging::trace(path, "Video %1 %2x%3@%<%5.2f>4%5 fps %6 [%7]", format_bitrate(video_bit_rate).c_str(), virtualfile->m_width, virtualfile->m_height, av_q2d(virtualfile->m_framerate), interleaved ? "i" : "p", format_size(size).c_str(), format_duration(duration).c_str());
        if (audio)
        {
            Logging::trace(path, "Audio %1 channels %2", virtualfile->m_channels, format_samplerate(virtualfile->m_sample_rate).c_str());
        }

        transcoder_set_filesize(virtualfile, duration, audio_bit_rate, virtualfile->m_channels, virtualfile->m_sample_rate, AV_SAMPLE_FMT_NONE, video_bit_rate, virtualfile->m_width, virtualfile->m_height, interleaved, virtualfile->m_framerate);

        virtualfile->m_video_frame_count    = static_cast<uint32_t>(av_rescale_q(duration, av_get_time_base_q(), av_inv_q(virtualfile->m_framerate)));
        virtualfile->m_predicted_size       = static_cast<size_t>(size);
    }

    return true;
}

/**
 * @brief Parse Blu-ray directory and get all Blu-ray titles and chapters as virtual files.
 * @param[in] path - Path to check.
 * @param[in] statbuf - File status structure of original file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @return On success, returns number of chapters found. On error, returns -errno.
 */
static int parse_bluray(const std::string & path, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler)
{
    BLURAY *bd;
    uint32_t title_count;
    int main_title;
    unsigned int seconds = 0;
    uint8_t flags = TITLES_RELEVANT;
    const char *bd_dir = nullptr;
    bool success = true;

    bd_dir = path.c_str();

    Logging::debug(path, "Parsing Blu-ray.");

    bd = bd_open(bd_dir, nullptr);

    title_count = bd_get_titles(bd, flags, seconds);
    main_title = bd_get_main_title(bd);
    if (main_title >= 0)
    {
        Logging::trace(path, "Main title: %1", main_title + 1);
    }

    for (uint32_t title_idx = 0; title_idx < title_count && success; title_idx++)
    {
        BLURAY_TITLE_INFO* ti = bd_get_title_info(bd, title_idx, 0);
        bool is_main_title = (main_title >= 0 && title_idx == static_cast<uint32_t>(main_title));

        // Add separate chapters
        for (uint32_t chapter_idx = 0; chapter_idx < ti->chapter_count && success; chapter_idx++)
        {
            success = create_bluray_virtualfile(bd, ti, path, statbuf, buf, filler, is_main_title, false, title_idx, chapter_idx);
        }

        if (success && ti->chapter_count > 1)
        {
            // If more than 1 chapter, add full title as well
            success = create_bluray_virtualfile(bd, ti, path, statbuf, buf, filler, is_main_title, true, title_idx, 0);
        }

        bd_free_title_info(ti);
    }

    bd_close(bd);

    if (success)
    {
        return static_cast<int>(title_count);
    }
    else
    {
        return -errno;
    }
}

int check_bluray(const std::string & path, void *buf, fuse_fill_dir_t filler)
{
    std::string _path(path);
    struct stat stbuf;
    int res = 0;

    append_sep(&_path);

    if (stat((_path + "BDMV/index.bdmv").c_str(), &stbuf) == 0)
    {
        if (!check_path(_path))
        {
            Logging::trace(_path, "Blu-ray detected.");
            res = parse_bluray(_path, &stbuf, buf, filler);
            Logging::trace(_path, "%1 titles were discovered.", res);
        }
        else
        {
            res = load_path(_path, &stbuf, buf, filler);
        }

        add_dotdot(buf, filler, &stbuf, 0);
    }
    return res;
}

#endif // USE_LIBBLURAY
