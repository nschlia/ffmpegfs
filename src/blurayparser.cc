/*
 * BLURAY parser for FFmpegfs
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

#ifdef USE_LIBBLURAY

#include "ffmpegfs.h"
#include "blurayparser.h"
#include "transcode.h"
#include "ffmpeg_utils.h"
#include "logging.h"

#include "libbluray/bluray.h"

static void stream_info(const std::string &path, BLURAY_STREAM_INFO *ss, int *channels, int *sample_rate, int *audio, int *width, int *height, double *frame_rate, int *interleaved);
static int parse_find_best_audio_stream();
static int parse_find_best_video_stream();
static int parse_bluray(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);

static void stream_info(const std::string & path, BLURAY_STREAM_INFO *ss, int *channels, int *sample_rate, int *audio, int *width, int *height, double *frame_rate, int *interleaved)
{
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
            *interleaved = 1;
            break;
        }
        case BLURAY_VIDEO_FORMAT_576I:  // ITU-R BT.601-4
        {
            *width = 720;
            *height = 576;
            *interleaved = 1;
            break;
        }
        case BLURAY_VIDEO_FORMAT_480P:  // SMPTE 293M
        {
            *width = 720;
            *height = 480;
            *interleaved = 0;
            break;
        }
        case BLURAY_VIDEO_FORMAT_1080I: // SMPTE 274M
        {
            *width = 1920;
            *height = 1080;
            *interleaved = 1;
            break;
        }
        case BLURAY_VIDEO_FORMAT_720P:  // SMPTE 296M
        {
            *height = 1280;
            *width = 720;
            *interleaved = 0;
            break;
        }
        case BLURAY_VIDEO_FORMAT_1080P: // SMPTE 274M
        {
            *width = 1920;
            *height = 1080;
            *interleaved = 0;
            break;
        }
        case BLURAY_VIDEO_FORMAT_576P:  // ITU-R BT.1358
        {
            *width = 720;
            *height = 576;
            *interleaved = 0;
            break;
        }
        }

        switch (ss->rate)
        {
        case BLURAY_VIDEO_RATE_24000_1001:
        {
            *frame_rate = 23.976;
            break;
        }
        case BLURAY_VIDEO_RATE_24:
        {
            *frame_rate = 24;
            break;
        }
        case BLURAY_VIDEO_RATE_25:
        {
            *frame_rate = 25;
            break;
        }
        case BLURAY_VIDEO_RATE_30000_1001:
        {
            *frame_rate = 29.97;
            break;
        }
        case BLURAY_VIDEO_RATE_50:
        {
            *frame_rate = 50;
            break;
        }
        case BLURAY_VIDEO_RATE_60000_1001:
        {
            *frame_rate = 59.94;
            break;
        }
        }

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
            *channels = 2;  // Multi Channel
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
        }

        *audio = 1;
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
        Logging::error(path, "Unrecognised coding type %<%02x>1", ss->coding_type);
        break;
    }
    }
}

static int parse_find_best_audio_stream()
{
    // TODO: is 0 really OK or shall we better parse?
    return 0;
}

static int parse_find_best_video_stream()
{
    // TODO: is 0 really OK or shall we better parse?
    return 0;
}

static int parse_bluray(const std::string & path, const struct stat * statbuf, void * buf, fuse_fill_dir_t filler)
{
    BLURAY *bd;
    uint32_t title_count;
    int main_title;
    unsigned int seconds = 0;
    uint8_t flags = TITLES_RELEVANT;
    const char *bd_dir = nullptr;

    bd_dir = path.c_str();

    Logging::debug(path, "Parsing Bluray.");

    bd = bd_open(bd_dir, nullptr);

    title_count = bd_get_titles(bd, flags, seconds);
    main_title = bd_get_main_title(bd);
    if (main_title >= 0)
    {
        Logging::trace(path, "Main title: %1", main_title + 1);
    }

    for (uint32_t title_idx = 0; title_idx < title_count; title_idx++)
    {
        BLURAY_TITLE_INFO* ti;

        ti = bd_get_title_info(bd, title_idx, 0);

        //ti->clip_count
        for (uint32_t chapter_idx = 0; chapter_idx < ti->chapter_count; chapter_idx++)
        {
            BLURAY_CLIP_INFO     *clip = &ti->clips[0];
            BLURAY_TITLE_CHAPTER *chapter = &ti->chapters[chapter_idx];
            char title_buf[PATH_MAX + 1];
            std::string origfile;
            struct stat stbuf;

            int64_t duration  = static_cast<int64_t>(chapter->duration) * AV_TIME_BASE / 90000;

            if (duration < AV_TIME_BASE)
            {
                Logging::debug(path, "Title %1 Chapter %2: skipping empty chapter", title_idx + 1, chapter_idx + 1);
                continue;
            }

            sprintf(title_buf, "%02d. Chapter %03d [%s]%s.%s",
                    title_idx + 1,
                    chapter_idx + 1,
                    replace_all(format_duration(duration), ":", "-").c_str(),
                    (main_title >= 0 && title_idx == static_cast<uint32_t>(main_title)) ? "+" : "",
                    params.m_format[0].real_desttype().c_str()); // can safely assume this is a video format

            std::string filename(title_buf);

            origfile = path + filename;

            memcpy(&stbuf, statbuf, sizeof(struct stat));

            stbuf.st_size   = 0; //static_cast<__off_t>(size);
            stbuf.st_blocks = (stbuf.st_size + 512 - 1) / 512;

            if (buf != nullptr && filler(buf, filename.c_str(), &stbuf, 0))
            {
                // break;
            }

            LPVIRTUALFILE virtualfile = insert_file(VIRTUALTYPE_BLURAY, path + filename, origfile, &stbuf);

            // Bluray is video format anyway
            virtualfile->m_format_idx       = 0;
            // Mark title/chapter/angle
            // ti->chapter_count
    		virtualfile->m_bluray.m_title_no      = title_idx + 1;
    		virtualfile->m_bluray.m_playlist_no   = ti->playlist;
    		virtualfile->m_bluray.m_chapter_no    = chapter_idx + 1;
    		virtualfile->m_bluray.m_angle_no      = 1;

            if (!transcoder_cached_filesize(virtualfile, &stbuf))
            {
                BITRATE video_bit_rate   = 1024*1024;
                BITRATE audio_bit_rate   = 256*1024;

                int channels            = 0;
                int sample_rate         = 0;
                int audio               = 0;

                int width               = 0;
                int height              = 0;
                double frame_rate       = 0;
                int interleaved         = 0;

                if (!bd_select_title(bd, title_idx))
                {
                    Logging::error(path, "Failed to open title: %1", title_idx);
                    return 0;
                }

                uint64_t size           = bd_get_title_size(bd);

                double secsduration     = static_cast<double>(duration) / AV_TIME_BASE;

                virtualfile->m_bluray.m_duration = duration;

                if (secsduration != 0.)
                {
                    video_bit_rate      = static_cast<BITRATE>(static_cast<double>(size) * 8 / secsduration);   // calculate bitrate in bps
                }

                // Get details
                stream_info(path, &clip->audio_streams[parse_find_best_audio_stream()], &channels, &sample_rate, &audio, &width, &height, &frame_rate, &interleaved);
                stream_info(path, &clip->video_streams[parse_find_best_video_stream()], &channels, &sample_rate, &audio, &width, &height, &frame_rate, &interleaved);

                Logging::debug(virtualfile->m_origfile, "Video %1 %2x%3@%<%5.2f>4%5 fps %6 [%7]", format_bitrate(video_bit_rate).c_str(), width, height, frame_rate, interleaved ? "i" : "p", format_size(size).c_str(), format_duration(duration).c_str());
                if (audio > -1)
                {
            Logging::debug(virtualfile->m_origfile, "Audio %1 channels %2", channels, format_samplerate(static_cast<unsigned int>(sample_rate)).c_str());
                }

                transcoder_set_filesize(virtualfile, secsduration, audio_bit_rate, channels, sample_rate, video_bit_rate, width, height, interleaved, frame_rate);
            }
        }

        bd_free_title_info(ti);
    }

    bd_close(bd);

    return static_cast<int>(title_count);
}

int check_bluray(const std::string & _path, void *buf, fuse_fill_dir_t filler)
{
    std::string path(_path);
    struct stat st;
    int res = 0;

    append_sep(&path);

    if (stat((path + "BDMV/index.bdmv").c_str(), &st) == 0)
    {
        if (!check_path(path))
        {
            Logging::trace(path, "Bluray detected.");
            res = parse_bluray(path, &st, buf, filler);
            Logging::trace(path, "Found %1 titles.", res);
        }
        else
        {
            res = load_path(path, &st, buf, filler);
        }
    }
    return res;
}

#endif // USE_LIBBLURAY
