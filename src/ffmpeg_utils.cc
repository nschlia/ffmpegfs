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

/**
 * @file ffmpeg_utils.cc
 * @brief FFmpegfs utility set implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2024 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifdef __cplusplus
extern "C" {
#endif
// Disable annoying warnings outside our code
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <libswscale/swscale.h>
#include "libavutil/ffversion.h"
#include <libavcodec/avcodec.h>
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

#include "id3v1tag.h"
#include "ffmpegfs.h"

#include <iostream>
#include <libgen.h>
#include <algorithm>
#include <wordexp.h>
#include <memory>
#include <fstream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <vector>
#include <cstring>
#include <functional>
#include <chrono>
#include <thread>

#include <iconv.h>
#ifdef HAVE_CONFIG_H
// This causes problems because it includes defines that collide
// with out config.h.
#undef HAVE_CONFIG_H
#endif // HAVE_CONFIG_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <chardet.h>
#pragma GCC diagnostic pop
#include <fnmatch.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif
// Disable annoying warnings outside our code
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <libswresample/swresample.h>
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

static int is_device(__attribute__((unused)) const AVClass *avclass);
static std::string ffmpeg_libinfo(bool lib_exists, __attribute__((unused)) unsigned int version, __attribute__((unused)) const char *cfg, int version_minor, int version_major, int version_micro, const char * libname);

#ifndef AV_ERROR_MAX_STRING_SIZE
#define AV_ERROR_MAX_STRING_SIZE 128                    /**< @brief Max. length of a FFmpeg error string */
#endif // AV_ERROR_MAX_STRING_SIZE

typedef std::map<const std::string, const FILETYPE, comp> FILETYPE_MAP; /**< @brief Map of file type. One entry per supported type. */

/**
  * List of supported file types
  */
static const FILETYPE_MAP filetype_map =
{
    { "mp3",    FILETYPE::MP3 },
    { "mp4",    FILETYPE::MP4 },
    { "wav",    FILETYPE::WAV },
    { "ogg",    FILETYPE::OGG },
    { "webm",   FILETYPE::WEBM },
    { "mov",    FILETYPE::MOV },
    { "aiff",   FILETYPE::AIFF },
    { "opus",   FILETYPE::OPUS },
    { "prores", FILETYPE::PRORES },
    { "alac",   FILETYPE::ALAC },
    { "png",    FILETYPE::PNG },
    { "jpg",    FILETYPE::JPG },
    { "bmp",    FILETYPE::BMP },
    { "ts",     FILETYPE::TS },
    { "hls",    FILETYPE::HLS },
    { "flac",   FILETYPE::FLAC },
    { "mkv",    FILETYPE::MKV },
};

Format_Options::Format_Options()
    : m_format_map{ { SAMPLE_FMT::FMT_DONTCARE, { { AV_CODEC_ID_NONE }, { AV_CODEC_ID_NONE }, { AV_CODEC_ID_NONE }, AV_SAMPLE_FMT_NONE }}}
    , m_albumart_supported(false)
{
}

Format_Options::Format_Options(
        std::string format_name,
        std::string fileext,
        FORMAT_MAP  format,
        bool        albumart_supported
        )
    : m_format_name(std::move(format_name))
    , m_fileext(std::move(fileext))
    , m_format_map(std::move(format))
    , m_albumart_supported(albumart_supported)
{

}

AVCodecID Format_Options::video_codec() const
{
    FORMAT_MAP::const_iterator it = m_format_map.find(params.m_sample_fmt);
    if (it == m_format_map.cend())
    {
        // Output supports no video. End of story.
        return AV_CODEC_ID_NONE;
    }

    if (params.m_video_codec == AV_CODEC_ID_NONE)
    {
        return (it->second.m_video_codec[0]);    // 1st array entry is the predefined codec
    }

    return params.m_video_codec;
}

bool Format_Options::is_video_codec_supported(AVCodecID codec_id) const
{
    FORMAT_MAP::const_iterator it = m_format_map.find(params.m_sample_fmt);
    if (it != m_format_map.cend())
    {
        for (const AVCodecID & video_codec_id : it->second.m_video_codec)
        {
            if (video_codec_id == codec_id)
            {
                return true;
            }
        }
    }
    return false;
}

std::string Format_Options::video_codec_list() const
{
    std::string buffer;
    FORMAT_MAP::const_iterator it = m_format_map.find(params.m_sample_fmt);
    if (it != m_format_map.cend())
    {
        for (const AVCodecID & video_codec_id : it->second.m_video_codec)
        {
            if (!buffer.empty())
            {
                buffer += ", ";
            }

            buffer += get_video_codec_text(video_codec_id);
        }
    }

    return buffer;
}

AVCodecID Format_Options::audio_codec() const
{
    FORMAT_MAP::const_iterator it = m_format_map.find(params.m_sample_fmt);
    if (it == m_format_map.cend())
    {
        // Output supports no audio??? Well then, end of story.
        return AV_CODEC_ID_NONE;
    }

    if (params.m_audio_codec == AV_CODEC_ID_NONE)
    {
        return (it->second.m_audio_codec[0]);    // 1st array entry is the predefined codec
    }

    return params.m_audio_codec;
}

bool Format_Options::is_audio_codec_supported(AVCodecID codec_id) const
{
    FORMAT_MAP::const_iterator it = m_format_map.find(params.m_sample_fmt);
    if (it != m_format_map.cend())
    {
        for (const AVCodecID & audio_codec_id : it->second.m_audio_codec)
        {
            if (audio_codec_id == codec_id)
            {
                return true;
            }
        }
    }
    return false;
}

std::string Format_Options::audio_codec_list() const
{
    std::string buffer;
    FORMAT_MAP::const_iterator it = m_format_map.find(params.m_sample_fmt);
    if (it != m_format_map.cend())
    {
        for (const AVCodecID & audio_codec_id : it->second.m_audio_codec)
        {
            if (!buffer.empty())
            {
                buffer += ", ";
            }

            buffer += get_audio_codec_text(audio_codec_id);
        }
    }

    return buffer;
}

AVSampleFormat Format_Options::sample_format() const
{
    FORMAT_MAP::const_iterator it = m_format_map.find(params.m_sample_fmt);
    if (it == m_format_map.cend())
    {
        return AV_SAMPLE_FMT_NONE;
    }

    return (it->second.m_sample_format);
}

bool Format_Options::is_sample_fmt_supported() const
{
    FORMAT_MAP::const_iterator it = m_format_map.find(params.m_sample_fmt);
    return (it != m_format_map.cend());
}

std::string Format_Options::sample_fmt_list() const
{
    std::string buffer;
    for (typename FORMAT_MAP::const_iterator it = m_format_map.cbegin(); it != m_format_map.cend();)
    {
        buffer += get_sampleformat_text(it->first);

        if (++it != m_format_map.cend())
        {
            buffer += ", ";
        }
    }

    return buffer;
}

AVCodecID Format_Options::subtitle_codec(AVCodecID codec_id) const
{
    FORMAT_MAP::const_iterator it = m_format_map.find(params.m_sample_fmt);
    if (it == m_format_map.cend())
    {
        // Output supports no subtitles. End of story.
        return AV_CODEC_ID_NONE;
    }

    // Try to find direct match, prefer same as input stream
    for (const AVCodecID & subtitle_codec_id : it->second.m_subtitle_codec)
    {
        // Also match AV_CODEC_ID_DVD_SUBTITLE to AV_CODEC_ID_DVB_SUBTITLE
        if (subtitle_codec_id == codec_id || (codec_id == AV_CODEC_ID_DVD_SUBTITLE && subtitle_codec_id == AV_CODEC_ID_DVB_SUBTITLE))
        {
            return subtitle_codec_id;
        }
    }

    // No direct match, try to find a text/text or bitmap/bitmap pair
    if (is_text_codec(codec_id))
    {
        // Find a text based codec in the list
        for (const AVCodecID & subtitle_codec_id : it->second.m_subtitle_codec)
        {
            if (is_text_codec(subtitle_codec_id))
            {
                return subtitle_codec_id;
            }
        }
    }
    else
    {
        // Find a bitmap based codec in the list
        for (const AVCodecID & subtitle_codec_id : it->second.m_subtitle_codec)
        {
            if (!is_text_codec(subtitle_codec_id))
            {
                return subtitle_codec_id;
            }
        }
    }

    // No matching codec support
    return AV_CODEC_ID_NONE;
}

const FFmpegfs_Format::OPTIONS_MAP FFmpegfs_Format::m_options_map =
{
    //{
    //     Descriptive name of the format.
    //     File extension: Mostly, but not always, same as format.
    //    {
    //        {
    //            SAMPLE_FMT enum, or SAMPLE_FMT::FMT_DONTCARE if source format decides
    //            {
    //                List of video codecs
    //                List of audio codec(s)
    //                List of subtitle codec(s)
    //                AVSampleFormat to be used in encoding, if AV_SAMPLE_FMT_NONE will be determined by source
    //            }
    //        }
    //    },
    //     If album arts are supported, true; false if no album arts are supported
    //}
    // -----------------------------------------------------------------------------------------------------------------------
    // MP3
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::MP3,
        {
            "mp3",
            "mp3",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_MP3 },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            true
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // MP4
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::MP4,
        {
            "mp4",
            "mp4",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_H264, AV_CODEC_ID_H265, AV_CODEC_ID_MPEG1VIDEO, AV_CODEC_ID_MPEG2VIDEO },
                        { AV_CODEC_ID_AAC, AV_CODEC_ID_MP3 },
                        { AV_CODEC_ID_MOV_TEXT },   // MOV Text (Apple Text Media Handler): should be AV_CODEC_ID_WEBVTT, but we get "codec not currently supported in container"
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // WAV
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::WAV,
        {
            "wav",
            "wav",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_S16LE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_8,                   // 8 bit
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_U8 },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_16,                  // 32 bit
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_S16LE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_24,                  // 24 bit
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_S24LE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_32,                  // 32 bit
                    {

                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_S32LE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_64,                  // 64 bit
                    {

                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_S64LE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_F16,                 // 16 bit float
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_F16LE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_F24,                 // 24 bit float
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_F24LE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_F32,                 // 32 bit float
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_F32LE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_F64,                 // 64 bit float
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_F64LE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // OGG
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::OGG,
        {
            "ogg",
            "ogg",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_THEORA },
                        { AV_CODEC_ID_VORBIS },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // WebM
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::WEBM,
        {
            "webm",
            "webm",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_VP9, AV_CODEC_ID_VP8, AV_CODEC_ID_AV1 },
                        { AV_CODEC_ID_OPUS, AV_CODEC_ID_VORBIS },
                        { AV_CODEC_ID_WEBVTT },
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // MOV
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::MOV,
        {
            "mov",
            "mov",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_H264, AV_CODEC_ID_H265, AV_CODEC_ID_MPEG1VIDEO, AV_CODEC_ID_MPEG2VIDEO },
                        { AV_CODEC_ID_AAC, AV_CODEC_ID_AC3, AV_CODEC_ID_MP3 },
                        { AV_CODEC_ID_MOV_TEXT },   // MOV Text (Apple Text Media Handler): should be AV_CODEC_ID_WEBVTT, but we get "codec not currently supported in container"
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // AIFF
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::AIFF,
        {
            "aiff",
            "aiff",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_S16BE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_16,                  // 16 bit
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_S16BE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_S16,          // 16 bit
                    }
                },
                {
                    SAMPLE_FMT::FMT_32,                  // 32 bit
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_PCM_S32BE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_S32,          // 32 bit
                    }
                },
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // Opus
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::OPUS,
        {
            "opus",
            "opus",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_OPUS },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // Opus
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::PRORES,
        {
            "mov",
            "mov",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_PRORES },
                        { AV_CODEC_ID_PCM_S16LE },
                        { AV_CODEC_ID_MOV_TEXT },   // MOV Text (Apple Text Media Handler): should be AV_CODEC_ID_WEBVTT, but we get "codec not currently supported in container"
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // ALAC
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::ALAC,
        {
            "m4a",
            "m4a",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_ALAC },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_16,                  // 16 bit
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_ALAC },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_S16P,         // 16 bit planar
                    }
                },
                {
                    SAMPLE_FMT::FMT_24,                  // 24 bit
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_ALAC },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_S32P,         // 32 bit planar, creates 24 bit ALAC
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // PNG
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::PNG,
        {
            "png",
            "png",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_PNG },
                        { AV_CODEC_ID_NONE },       // Audio codec(s)
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // JPG
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::JPG,
        {
            "jpg",
            "jpg",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_MJPEG },
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // BMP
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::BMP,
        {
            "bmp",
            "bmp",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_BMP },
                        { AV_CODEC_ID_NONE },       // Audio codec(s)
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // TS
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::TS,
        {
            "mpegts",
            "ts",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_H264, AV_CODEC_ID_H265, AV_CODEC_ID_MPEG1VIDEO, AV_CODEC_ID_MPEG2VIDEO },
                        { AV_CODEC_ID_AAC, AV_CODEC_ID_AC3, AV_CODEC_ID_MP3 },
                        { AV_CODEC_ID_DVB_SUBTITLE },
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false

        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // HLS, same as TS
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::HLS,
        {
            "mpegts",
            "ts",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_H264, AV_CODEC_ID_H265, AV_CODEC_ID_MPEG1VIDEO, AV_CODEC_ID_MPEG2VIDEO },
                        { AV_CODEC_ID_AAC, AV_CODEC_ID_AC3, AV_CODEC_ID_MP3 },
                        { AV_CODEC_ID_DVB_SUBTITLE },
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // FLAC
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::FLAC,
        {
            "flac",
            "flac",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_FLAC },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_NONE,
                    }
                },
                {
                    SAMPLE_FMT::FMT_16,                  // 16 bit
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_FLAC },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_S16,          // Use 16 bit samples
                    }
                },
                {
                    SAMPLE_FMT::FMT_24,                  // 24 bit
                    {
                        { AV_CODEC_ID_NONE },
                        { AV_CODEC_ID_FLAC },
                        { AV_CODEC_ID_NONE },
                        AV_SAMPLE_FMT_S32,          // Use 24 bit samples (yes, S32 creates 24 bit samples)
                    }
                }
            },
            true
        }
    },
    // -----------------------------------------------------------------------------------------------------------------------
    // MKV
    // -----------------------------------------------------------------------------------------------------------------------
    {
        FILETYPE::MKV,
        {
            "matroska",
            "mkv",
            {
                {
                    SAMPLE_FMT::FMT_DONTCARE,
                    {
                        { AV_CODEC_ID_H264, AV_CODEC_ID_H265, AV_CODEC_ID_MPEG1VIDEO, AV_CODEC_ID_MPEG2VIDEO },
                        { AV_CODEC_ID_AAC, AV_CODEC_ID_AC3, AV_CODEC_ID_MP3 },
                        { AV_CODEC_ID_ASS, AV_CODEC_ID_SUBRIP, AV_CODEC_ID_WEBVTT, AV_CODEC_ID_DVB_SUBTITLE },
                        AV_SAMPLE_FMT_NONE,
                    }
                }
            },
            false
        }
    },
};

FFmpegfs_Format::FFmpegfs_Format() :
    m_cur_opts(&m_empty_options),
    m_filetype(FILETYPE::UNKNOWN)
{

}

bool FFmpegfs_Format::init(const std::string & desttype)
{
    OPTIONS_MAP::const_iterator it = m_options_map.find(get_filetype(desttype));
    if (it == m_options_map.cend())
    {
        // Not found/invalid desttype

        m_desttype.clear();
        m_filetype 	= FILETYPE::UNKNOWN;
        m_cur_opts  = &m_empty_options;

        return false;
    }
    else
    {
        // OK
        m_desttype  = desttype;
        m_filetype 	= it->first;
        m_cur_opts  = &it->second;

        return true;
    }
}

const std::string & FFmpegfs_Format::desttype() const
{
    return m_desttype;
}

const std::string & FFmpegfs_Format::format_name() const
{
    return m_cur_opts->m_format_name;
}

const std::string & FFmpegfs_Format::fileext() const
{
    return m_cur_opts->m_fileext;
}

FILETYPE FFmpegfs_Format::filetype() const
{
    return m_filetype;
}

bool FFmpegfs_Format::is_multiformat() const
{
    return  (is_frameset() || is_hls());
}

bool FFmpegfs_Format::is_frameset() const
{
    return (m_filetype == FILETYPE::JPG || m_filetype == FILETYPE::PNG || m_filetype == FILETYPE::BMP);
}

bool FFmpegfs_Format::is_hls() const
{
    return (m_filetype == FILETYPE::HLS);
}

bool FFmpegfs_Format::albumart_supported() const
{
    return m_cur_opts->m_albumart_supported;
}

AVCodecID FFmpegfs_Format::video_codec() const
{
    return m_cur_opts->video_codec();
}

bool FFmpegfs_Format::is_video_codec_supported(AVCodecID codec_id) const
{
    return m_cur_opts->is_video_codec_supported(codec_id);
}

std::string FFmpegfs_Format::video_codec_list() const
{
    return m_cur_opts->video_codec_list();
}

AVCodecID FFmpegfs_Format::audio_codec() const
{
    return m_cur_opts->audio_codec();
}

bool FFmpegfs_Format::is_audio_codec_supported(AVCodecID codec_id) const
{
    return m_cur_opts->is_audio_codec_supported(codec_id);
}

std::string FFmpegfs_Format::audio_codec_list() const
{
    return m_cur_opts->audio_codec_list();
}

AVSampleFormat FFmpegfs_Format::sample_format() const
{
    return m_cur_opts->sample_format();
}

bool FFmpegfs_Format::is_sample_fmt_supported() const
{
    return m_cur_opts->is_sample_fmt_supported();
}

std::string FFmpegfs_Format::sample_fmt_list() const
{
    return m_cur_opts->sample_fmt_list();
}

AVCodecID FFmpegfs_Format::subtitle_codec(AVCodecID codec_id) const
{
    return m_cur_opts->subtitle_codec(codec_id);
}

const std::string & append_sep(std::string * path)
{
    if (path->back() != '/')
    {
        *path += '/';
    }

    return *path;
}

const std::string & append_filename(std::string * path, const std::string & filename)
{
    append_sep(path);

    *path += filename;

    return *path;
}

const std::string & remove_sep(std::string * path)
{
    if (path->back() == '/')
    {
        (*path).pop_back();
    }

    return *path;
}

const std::string & remove_filename(std::string * filepath)
{
    char *p = new_strdup(*filepath);

    if (p == nullptr)
    {
        errno = ENOMEM;
        return *filepath;
    }

    *filepath = dirname(p);
    delete [] p;
    append_sep(filepath);
    return *filepath;
}

const std::string & remove_path(std::string *filepath)
{
    char *p = new_strdup(*filepath);

    if (p == nullptr)
    {
        errno = ENOMEM;
        return *filepath;
    }

    *filepath = basename(p);
    delete [] p;
    return *filepath;
}

const std::string & remove_ext(std::string *filepath)
{
    size_t found;

    found = filepath->rfind('.');

    if (found != std::string::npos)
    {
        // Have extension
        filepath->resize(found);
    }
    return *filepath;
}


bool find_ext(std::string * ext, const std::string & filename)
{
    size_t found;

    found = filename.rfind('.');

    if (found == std::string::npos)
    {
        // No extension
        ext->clear();
        return false;
    }
    else
    {
        // Have extension
        *ext = filename.substr(found + 1);
        return true;
    }
}

bool check_ext(const std::string & ext, const std::string & filename)
{
    std::string ext1;
    return (find_ext(&ext1, filename) && ext1 == ext);
}

const std::string & replace_ext(std::string * filepath, const std::string & ext)
{
    size_t found;

    found = filepath->rfind('.');

    if (found == std::string::npos)
    {
        // No extension, just add
        *filepath += '.';
    }
    else
    {
        // Have extension, so replace
        filepath->resize(found + 1);
    }

    *filepath += ext;

    return *filepath;
}

const std::string & append_ext(std::string * filepath, const std::string & ext)
{
    size_t found;

    found = filepath->rfind('.');

    if (found == std::string::npos || strcasecmp(filepath->substr(found + 1), ext) != 0)
    {
        // No extension or different extension
        *filepath += '.' + ext;
    }

    return *filepath;
}

char * new_strdup(const std::string & str)
{
    size_t n = str.size() + 1;
    char * p = new(std::nothrow) char[n];

    if (p == nullptr)
    {
        errno = ENOMEM;
        return nullptr;
    }

    strncpy(p, str.c_str(), n);
    return p;
}

std::string ffmpeg_geterror(int errnum)
{
    if (errnum < 0)
    {
        std::array<char, AV_ERROR_MAX_STRING_SIZE + 1> error;
        av_strerror(errnum, error.data(), error.size() - 1);
        return error.data();
    }
    else
    {
        return strerror(errnum);
    }
}

int64_t ffmpeg_rescale_q(int64_t ts, const AVRational & timebase_in, const AVRational &timebase_out)
{
    if (ts == AV_NOPTS_VALUE)
    {
        return AV_NOPTS_VALUE;
    }

    if (ts == 0)
    {
        return 0;
    }

    return av_rescale_q(ts, timebase_in, timebase_out);
}

int64_t ffmpeg_rescale_q_rnd(int64_t ts, const AVRational & timebase_in, const AVRational &timebase_out)
{
    if (ts == AV_NOPTS_VALUE)
    {
        return AV_NOPTS_VALUE;
    }

    if (ts == 0)
    {
        return 0;
    }

    return av_rescale_q_rnd(ts, timebase_in, timebase_out, static_cast<AVRounding>(AV_ROUND_UP | AV_ROUND_PASS_MINMAX));
}

#if !HAVE_MEDIA_TYPE_STRING
const char *get_media_type_string(enum AVMediaType media_type)
{
    switch (media_type)
    {
    case AVMEDIA_TYPE_VIDEO:
        return "video";
    case AVMEDIA_TYPE_AUDIO:
        return "audio";
    case AVMEDIA_TYPE_DATA:
        return "data";
    case AVMEDIA_TYPE_SUBTITLE:
        return "subtitle";
    case AVMEDIA_TYPE_ATTACHMENT:
        return "attachment";
    default:
        return "unknown";
    }
}
#endif

/**
     * @brief Get FFmpeg library info.
     * @param[in] lib_exists - Set to true if library exists.
     * @param[in] version - Library version number.
     * @param[in] cfg - Library configuration.
     * @param[in] version_minor - Library version minor.
     * @param[in] version_major - Library version major.
     * @param[in] version_micro - Library version micro.
     * @param[in] libname - Name of the library.
     * @return Formatted library information.
     */
static std::string ffmpeg_libinfo(bool lib_exists, __attribute__((unused)) unsigned int version, __attribute__((unused)) const char *cfg, int version_minor, int version_major, int version_micro, const char * libname)
{
    std::string info;

    if (lib_exists)
    {
        strsprintf(&info, "lib%-17s: %d.%d.%d\n",
                   libname,
                   version_minor,
                   version_major,
                   version_micro);
    }

    return info;
}

#define PRINT_LIB_INFO(libname, LIBNAME) \
    ffmpeg_libinfo(true, libname##_version(), libname##_configuration(), \
    LIB##LIBNAME##_VERSION_MAJOR, LIB##LIBNAME##_VERSION_MINOR, LIB##LIBNAME##_VERSION_MICRO, #libname)     /**< @brief Print info about a FFmpeg library */

std::string ffmpeg_libinfo()
{
    std::string info;

    info = "FFmpeg Version      : " FFMPEG_VERSION "\n";

    // cppcheck-suppress ConfigurationNotChecked
    info += PRINT_LIB_INFO(avutil,      AVUTIL);
    info += PRINT_LIB_INFO(avcodec,     AVCODEC);
    info += PRINT_LIB_INFO(avformat,    AVFORMAT);
    // info += PRINT_LIB_INFO(avdevice,    AVDEVICE);
    // info += PRINT_LIB_INFO(avfilter,    AVFILTER);
    info += PRINT_LIB_INFO(swresample,  SWRESAMPLE);
    info += PRINT_LIB_INFO(swscale,     SWSCALE);
    // info += PRINT_LIB_INFO(postproc,    POSTPROC);

    return info;
}

/**
     * @brief Check if class is a FMmpeg device
     * @todo Currently always returns 0. Must implement real check.
     * @param[in] avclass - Private class object
     * @return Returns 1 if object is a device, 0 if not.
     */
static int is_device(__attribute__((unused)) const AVClass *avclass)
{
    //if (avclass == nullptr)
    //    return 0;

    return 0;
    //return AV_IS_INPUT_DEVICE(avclass->category) || AV_IS_OUTPUT_DEVICE(avclass->category);
}

int show_caps(int device_only)
{
    const AVInputFormat *ifmt  = nullptr;
    const AVOutputFormat *ofmt = nullptr;
    const char *last_name;
    int is_dev;

    std::printf("%s\n"
                " D. = Demuxing supported\n"
                " .E = Muxing supported\n"
                " --\n", device_only ? "Devices:" : "File formats:");
    last_name = "000";
    for (;;)
    {
        int decode = 0;
        int encode = 0;
        const char *name      = nullptr;
        const char *long_name = nullptr;
        const char *extensions = nullptr;

        void *ofmt_opaque = nullptr;
        ofmt_opaque = nullptr;
        while ((ofmt = av_muxer_iterate(&ofmt_opaque)))
        {
            is_dev = is_device(ofmt->priv_class);
            if (!is_dev && device_only)
            {
                continue;
            }

            if ((!name || strcmp(ofmt->name, name) < 0) &&
                    strcmp(ofmt->name, last_name) > 0)
            {
                name        = ofmt->name;
                long_name   = ofmt->long_name;
                encode      = 1;
            }
        }

        void *ifmt_opaque = nullptr;
        ifmt_opaque = nullptr;
        while ((ifmt = av_demuxer_iterate(&ifmt_opaque)) != nullptr)
        {
            is_dev = is_device(ifmt->priv_class);
            if (!is_dev && device_only)
            {
                continue;
            }

            if ((!name || strcmp(ifmt->name, name) < 0) &&
                    strcmp(ifmt->name, last_name) > 0)
            {
                name        = ifmt->name;
                long_name   = ifmt->long_name;
                extensions  = ifmt->extensions;
                encode      = 0;
            }
            if (name && strcmp(ifmt->name, name) == 0)
            {
                decode      = 1;
            }
        }

        if (name == nullptr)
        {
            break;
        }

        last_name = name;
        if (extensions == nullptr)
        {
            continue;
        }

        std::printf(" %s%s %-15s %-15s %s\n",
                    decode ? "D" : " ",
                    encode ? "E" : " ",
                    extensions,
                    name,
                    (long_name != nullptr)  ? long_name : " ");
    }
    return 0;
}

const char * get_codec_name(AVCodecID codec_id, bool long_name)
{
    const AVCodecDescriptor * pCodecDescriptor;
    const char * psz = "unknown";

    pCodecDescriptor = avcodec_descriptor_get(codec_id);

    if (pCodecDescriptor != nullptr)
    {
        if (pCodecDescriptor->long_name != nullptr && long_name)
        {
            psz = pCodecDescriptor->long_name;
        }

        else
        {
            psz = pCodecDescriptor->name;
        }
    }

    return psz;
}

int mktree(const std::string & path, mode_t mode)
{
    char *buffer = new_strdup(path);

    if (buffer == nullptr)
    {
        return ENOMEM;
    }

    std::string dir;
    char *saveptr;
    char *p = strtok_r(buffer, "/", &saveptr);
    int status = 0;

    while (p != nullptr)
    {
        int newstat;

        dir += "/";
        dir += p;

        errno = 0;

        newstat = mkdir(dir.c_str(), mode);

        if (!status && newstat && errno != EEXIST)
        {
            status = -1;
            break;
        }

        status = newstat;

        p = strtok_r(nullptr, "/", &saveptr);
    }

    delete [] buffer;

    return status;
}

void tempdir(std::string & path)
{
    const char *temp = getenv("TMPDIR");

    if (temp != nullptr)
    {
        path = temp;
        return;
    }

    path = P_tmpdir;

    if (!path.empty())
    {
        return;
    }

    path = "/tmp";
}

int supports_albumart(FILETYPE filetype)
{
    // Could also allow OGG but the format requires special handling for album arts
    return (filetype == FILETYPE::MP3 || filetype == FILETYPE::MP4);
}

FILETYPE get_filetype(const std::string & desttype)
{
    try
    {
        return (filetype_map.at(desttype.c_str()));
    }
    catch (const std::out_of_range& /*oor*/)
    {
        //std::cerr << "Out of Range error: " << oor.what() << std::endl;
        return FILETYPE::UNKNOWN;
    }
}

std::string get_filetype_text(FILETYPE filetype)
{
    FILETYPE_MAP::const_iterator it = search_by_value(filetype_map, filetype);
    if (it != filetype_map.cend())
    {
        return it->first;
    }
    return "INVALID";
}


FILETYPE get_filetype_from_list(const std::string & desttypelist)
{
    std::vector<std::string> desttype = split(desttypelist, ",");
    FILETYPE filetype = FILETYPE::UNKNOWN;

    // Find first matching entry
    for (size_t n = 0; n < desttype.size() && filetype != FILETYPE::UNKNOWN; n++)
    {
        filetype = get_filetype(desttype[n]);
    }
    return filetype;
}

void init_id3v1(ID3v1 *id3v1)
{
    // Initialise ID3v1.1 tag structure
    std::memset(id3v1, ' ', sizeof(ID3v1));
    std::memcpy(&id3v1->m_tag, "TAG", 3);
    id3v1->m_padding = '\0';
    id3v1->m_title_no = 0;
    id3v1->m_genre = 0;
}

std::string format_number(int64_t value)
{
    if (!value)
    {
        return "unlimited";
    }

    if (value == AV_NOPTS_VALUE)
    {
        return "unset";
    }

    std::string buffer;
    return strsprintf(&buffer, "%" PRId64, value);
}

std::string format_bitrate(BITRATE value)
{
    if (value == static_cast<BITRATE>(AV_NOPTS_VALUE))
    {
        return "unset";
    }

    if (value > 1000000)
    {
        std::string buffer;
        return strsprintf(&buffer, "%.2f Mbps", static_cast<double>(value) / 1000000);
    }
    else if (value > 1000)
    {
        std::string buffer;
        return strsprintf(&buffer, "%.1f kbps", static_cast<double>(value) / 1000);
    }
    else
    {
        std::string buffer;
        return strsprintf(&buffer, "%" PRId64 " bps", value);
    }
}

std::string format_samplerate(int value)
{
    if (value == static_cast<int>(AV_NOPTS_VALUE))
    {
        return "unset";
    }

    if (value < 1000)
    {
        std::string buffer;
        return strsprintf(&buffer, "%u Hz", value);
    }
    else
    {
        std::string buffer;
        return strsprintf(&buffer, "%.3f kHz", static_cast<double>(value) / 1000);
    }
}

#define STR_VALUE(arg)  #arg                                /**< @brief Convert macro to string */
#define X(name)         STR_VALUE(name)                     /**< @brief Convert macro to string */

std::string format_duration(int64_t value, uint32_t fracs /*= 3*/)
{
    if (value == AV_NOPTS_VALUE)
    {
        return "unset";
    }

    std::string buffer;
    std::string duration;
    unsigned hours   = static_cast<unsigned>((value / AV_TIME_BASE) / (3600));
    unsigned mins    = static_cast<unsigned>(((value / AV_TIME_BASE) % 3600) / 60);
    unsigned secs    = static_cast<unsigned>((value / AV_TIME_BASE) % 60);

    if (hours)
    {
        duration = strsprintf(&buffer, "%02u:", hours);
    }

    duration += strsprintf(&buffer, "%02u:%02u", mins, secs);
    if (fracs)
    {
        unsigned decimals    = static_cast<unsigned>(value % AV_TIME_BASE);
        duration += strsprintf(&buffer, ".%0*u", sizeof(X(AV_TIME_BASE)) - 2, decimals).substr(0, fracs + 1);
    }
    return duration;
}

std::string format_time(time_t value)
{
    if (!value)
    {
        return "unlimited";
    }

    if (value == static_cast<time_t>(AV_NOPTS_VALUE))
    {
        return "unset";
    }

    std::string buffer;
    std::string time;
    int weeks;
    int days;
    int hours;
    int mins;
    int secs;

    weeks = static_cast<int>(value / (60*60*24*7));
    value -= weeks * (60*60*24*7);
    days = static_cast<int>(value / (60*60*24));
    value -= days * (60*60*24);
    hours = static_cast<int>(value / (60*60));
    value -= hours * (60*60);
    mins = static_cast<int>(value / (60));
    value -= mins * (60);
    secs = static_cast<int>(value);

    if (weeks)
    {
        time = strsprintf(&buffer, "%iw ", weeks);
    }
    if (days)
    {
        time += strsprintf(&buffer, "%id ", days);
    }
    if (hours)
    {
        time += strsprintf(&buffer, "%ih ", hours);
    }
    if (mins)
    {
        time += strsprintf(&buffer, "%im ", mins);
    }
    if (secs)
    {
        time += strsprintf(&buffer, "%is ", secs);
    }
    return time;
}

std::string format_size(uint64_t value)
{
    if (!value)
    {
        return "unlimited";
    }

    if (value == static_cast<uint64_t>(AV_NOPTS_VALUE))
    {
        return "unset";
    }

    if (value > 1024*1024*1024*1024LL)
    {
        std::string buffer;
        return strsprintf(&buffer, "%.3f TB", static_cast<double>(value) / (1024*1024*1024*1024LL));
    }
    else if (value > 1024*1024*1024)
    {
        std::string buffer;
        return strsprintf(&buffer, "%.2f GB", static_cast<double>(value) / (1024*1024*1024));
    }
    else if (value > 1024*1024)
    {
        std::string buffer;
        return strsprintf(&buffer, "%.1f MB", static_cast<double>(value) / (1024*1024));
    }
    else if (value > 1024)
    {
        std::string buffer;
        return strsprintf(&buffer, "%.1f KB", static_cast<double>(value) / (1024));
    }
    else
    {
        std::string buffer;
        return strsprintf(&buffer, "%" PRIu64 " bytes", value);
    }
}

std::string format_size_ex(uint64_t value)
{
    std::string buffer;
    return format_size(value) + strsprintf(&buffer, " (%" PRIu64 " bytes)", value);
}

std::string format_result_size(size_t size_resulting, size_t size_predicted)
{
    if (size_resulting >= size_predicted)
    {
        size_t value = size_resulting - size_predicted;
        return format_size(value);
    }
    else
    {
        size_t value = size_predicted - size_resulting;
        return "-" + format_size(value);
    }
}

std::string format_result_size_ex(size_t size_resulting, size_t size_predicted)
{
    if (size_resulting >= size_predicted)
    {
        std::string buffer;
        size_t value = size_resulting - size_predicted;
        return format_size(value) + strsprintf(&buffer, " (%zu bytes)", value);
    }
    else
    {
        std::string buffer;
        size_t value = size_predicted - size_resulting;
        return "-" + format_size(value) + strsprintf(&buffer, " (-%zu bytes)", value);
    }
}

/**
     * @brief Print frames per second.
     * @param[in] d - Frames per second.
     * @param[in] postfix - Postfix text.
     */
static void print_fps(double d, const char *postfix)
{
    long v = lrint(d * 100);
    if (!v)
    {
        std::printf("%1.4f %s\n", d, postfix);
    }
    else if (v % 100)
    {
        std::printf("%3.2f %s\n", d, postfix);
    }
    else if (v % (100 * 1000))
    {
        std::printf("%1.0f %s\n", d, postfix);
    }
    else
    {
        std::printf("%1.0fk %s\n", d / 1000, postfix);
    }
}

int print_stream_info(const AVStream* stream)
{
    int ret = 0;

    AVCodecContext *avctx = avcodec_alloc_context3(nullptr);
    if (avctx == nullptr)
    {
        return AVERROR(ENOMEM);
    }

    ret = avcodec_parameters_to_context(avctx, stream->codecpar);
    if (ret < 0)
    {
        avcodec_free_context(&avctx);
        return ret;
    }

    // Fields which are missing from AVCodecParameters need to be taken from the AVCodecContext
    //            avctx->properties   = output_stream->codec->properties;
    //            avctx->codec        = output_stream->codec->codec;
    //            avctx->qmin         = output_stream->codec->qmin;
    //            avctx->qmax         = output_stream->codec->qmax;
    //            avctx->coded_width  = output_stream->codec->coded_width;
    //            avctx->coded_height = output_stream->codec->coded_height;
    int fps = stream->avg_frame_rate.den && stream->avg_frame_rate.num;
    int tbr = stream->r_frame_rate.den && stream->r_frame_rate.num;
    int tbn = stream->time_base.den && stream->time_base.num;
    int tbc = avctx->time_base.den && avctx->time_base.num; // Even the currently latest (lavf 58.10.100) refers to AVStream codec->time_base member... (See dump.c dump_stream_format)

    if (fps)
        print_fps(av_q2d(stream->avg_frame_rate), "avg fps");
    if (tbr)
        print_fps(av_q2d(stream->r_frame_rate), "Real base framerate (tbr)");
    if (tbn)
        print_fps(1 / av_q2d(stream->time_base), "stream timebase (tbn)");
    if (tbc)
        print_fps(1 / av_q2d(avctx->time_base), "codec timebase (tbc)");

    avcodec_free_context(&avctx);

    return ret;
}

std::string fourcc_make_string(std::string * buf, uint32_t fourcc)
{
    std::string fourcc2str(AV_FOURCC_MAX_STRING_SIZE, '\0');
    av_fourcc_make_string(&fourcc2str[0], fourcc);
    fourcc2str.resize(std::strlen(fourcc2str.c_str()));
    *buf = fourcc2str;
    return *buf;
}

void exepath(std::string * path)
{
    std::array<char, PATH_MAX + 1> result;
    ssize_t count = readlink("/proc/self/exe", result.data(), result.size() - 1);
    if (count != -1)
    {
        *path = dirname(result.data());
        append_sep(path);
    }
    else
    {
        path->clear();
    }
}

std::string &ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not_fn(std::function<int(int)>(isspace))));
    return s;
}

std::string &rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not_fn(std::function<int(int)>(isspace))).base(), s.end());
    return s;
}

std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}

std::string replace_all(std::string str, const std::string& from, const std::string& to)
{
    return replace_all(&str, from, to);
}

std::string replace_all(std::string *str, const std::string& from, const std::string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str->find(from, start_pos)) != std::string::npos)
    {
        str->replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return *str;
}

bool replace_start(std::string *str, const std::string& from, const std::string& to)
{
#if __cplusplus >= 202002L
    // C++20 (and later) code
    if (str->starts_with(from) == 0)
#else
    if (str->find(from, 0) == 0)
#endif
    {
        str->replace(0, from.length(), to);
        return true;
    }

    return false;
}

int strcasecmp(const std::string & s1, const std::string & s2)
{
    return ::strcasecmp(s1.c_str(), s2.c_str());
}

int reg_compare(const std::string & value, const std::string & pattern, std::regex::flag_type flag)
{
    int reti;

    try
    {
        std::regex rgx(pattern, flag);

        reti = (std::regex_search(value, rgx) == true) ? 0 : 1;
    }
    catch(const std::regex_error& e)
    {
        std::cerr << "regex_error caught: " << e.what() << std::endl;
        if(e.code() == std::regex_constants::error_brack)
            std::cerr << "The code was error_brack" << std::endl;

        reti = -1;
    }

    return reti;
}

const std::string & expand_path(std::string *tgt, const std::string & src)
{
    wordexp_t exp_result;
    if (!wordexp(replace_all(src, " ", "\\ ").c_str(), &exp_result, 0))
    {
        *tgt = exp_result.we_wordv[0];
        wordfree(&exp_result);
    }
    else
    {
        *tgt =  src;
    }

    return *tgt;
}

int is_mount(const std::string & path)
{
    char* orig_name = nullptr;
    int ret = 0;

    try
    {
        struct stat file_stat;
        struct stat parent_stat;
        char * parent_name = nullptr;

        orig_name = new_strdup(path);

        if (orig_name == nullptr)
        {
            std::fprintf(stderr, "is_mount(): Out of memory\n");
            errno = ENOMEM;
            throw -1;
        }

        // get the parent directory of the file
        parent_name = dirname(orig_name);

        // get the file's stat info
        if (-1 == stat(path.c_str(), &file_stat))
        {
            std::fprintf(stderr, "is_mount(): (%i) %s\n", errno, strerror(errno));
            throw -1;
        }

        //determine whether the supplied file is a directory
        // if it isn't, then it can't be a mountpoint.
        if (!(file_stat.st_mode & S_IFDIR))
        {
            std::fprintf(stderr, "is_mount(): %s is not a directory.\n", path.c_str());
            throw -1;
        }

        // get the parent's stat info
        if (-1 == stat(parent_name, &parent_stat))
        {
            std::fprintf(stderr, "is_mount(): (%i) %s\n", errno, strerror(errno));
            throw -1;
        }

        // if file and parent have different device ids,
        // then the file is a mount point
        // or, if they refer to the same file,
        // then it's probably the root directory
        // and therefore a mountpoint
        // style: Redundant condition: file_stat.st_dev==parent_stat.st_dev.
        // 'A || (!A && B)' is equivalent to 'A || B' [redundantCondition]
        //if (file_stat.st_dev != parent_stat.st_dev ||
        //        (file_stat.st_dev == parent_stat.st_dev &&
        //         file_stat.st_ino == parent_stat.st_ino))
        if (file_stat.st_dev != parent_stat.st_dev || file_stat.st_ino == parent_stat.st_ino)
        {
            // IS a mountpoint
            ret = 1;
        }
        else
        {
            // is NOT a mountpoint
            ret = 0;
        }
    }
    catch (int _ret)
    {
        ret = _ret;
    }

    delete [] orig_name;

    return ret;
}

std::vector<std::string> split(const std::string& input, const std::string & regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator first{input.cbegin(), input.cend(), re, -1},
    last;
    return {first, last};
}

std::string sanitise_filepath(std::string * filepath)
{
    std::array<char, PATH_MAX + 1> resolved_name;

    if (realpath(filepath->c_str(), resolved_name.data()) != nullptr)
    {
        *filepath = resolved_name.data();
        return *filepath;
    }

    // realpath has the strange feature to remove a trailing slash if there.
    // To mimick its behaviour, if realpath fails, at least remove it.
    std::string _filepath(*filepath);
    remove_sep(&_filepath);
    return _filepath;
}

std::string sanitise_filepath(const std::string & filepath)
{
    std::string buffer(filepath);
    return sanitise_filepath(&buffer);
}

void append_basepath(std::string *origpath, const char* path)
{
    *origpath = params.m_basepath;
    if (*path == '/')
    {
        ++path;
    }
    *origpath += path;

    sanitise_filepath(origpath);
}

bool is_album_art(AVCodecID codec_id, const AVRational * frame_rate)
{
    if (codec_id == AV_CODEC_ID_PNG || codec_id == AV_CODEC_ID_BMP)
    {
        // PNG or BMP: must be an album art stream
        return true;
    }

    if (codec_id != AV_CODEC_ID_MJPEG)
    {
        // Anything else than MJPEG is never an album art stream
        return false;
    }

    if (frame_rate != nullptr && frame_rate->den)
    {
        double dbFrameRate = static_cast<double>(frame_rate->num) / frame_rate->den;

        // If frame rate is < 300 fps this most likely is a video
        if (dbFrameRate < 300)
        {
            // This is a video
            return false;
        }
    }

    return true;
}

int nocasecompare(const std::string & lhs, const std::string &rhs)
{
    return (strcasecmp(lhs, rhs));
}

size_t get_disk_free(std::string & path)
{
    struct statvfs buf;

    if (statvfs(path.c_str(), &buf))
    {
        return 0;
    }

    return static_cast<size_t>(buf.f_bfree * buf.f_bsize);
}

bool check_ignore(size_t size, size_t offset)
{
    std::array<size_t, 3> blocksize_arr = { 0x2000, 0x8000, 0x10000 };
    bool ignore = false;

    for (const size_t & blocksize: blocksize_arr)
    {
        size_t rest;
        bool match;

        match = !(offset % blocksize);              // Must be multiple of block size
        if (!match)
        {
            continue;
        }

        rest = size % offset;                       // Calculate rest.
        ignore = match && (rest < blocksize);       // Ignore of rest is less than block size

        if (ignore)
        {
            break;
        }
    }

    return ignore;
}

std::string make_filename(uint32_t file_no, const std::string & fileext)
{
    std::string buffer;
    return strsprintf(&buffer, "%06u.%s", file_no, fileext.c_str());
}

bool file_exists(const std::string & filename)
{
    return (access(filename.c_str(), F_OK) != -1);
}

void make_upper(std::string * input)
{
    std::for_each(std::begin(*input), std::end(*input), [](char& c) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    });
}

void make_lower(std::string * input)
{
    std::for_each(std::begin(*input), std::end(*input), [](char& c) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
}

const char * hwdevice_get_type_name(AVHWDeviceType dev_type)
{
    const char *type_name = av_hwdevice_get_type_name(dev_type);
    return (type_name != nullptr ? type_name : "unknown");
}

int to_utf8(std::string & text, const std::string & encoding)
{
    iconv_t conv = iconv_open("UTF-8", encoding.c_str());
    if (conv == (iconv_t) -1)
    {
        // Error in iconv_open, errno in return code.
        return errno;
    }

    std::vector<char> src;
    std::vector<char> dst;
    size_t srclen = text.size();
    size_t dstlen = 2 * srclen;

    src.reserve(srclen + 1);
    dst.reserve(dstlen + 2);

    char * pIn = src.data();
    char * pOut = dst.data();

    strncpy(pIn, text.c_str(), srclen);

    size_t len = iconv(conv, &pIn, &srclen, &pOut, &dstlen);
    if (len != (size_t) -1)
    {
        *pOut = '\0';

        iconv_close(conv);

        text = dst.data();

        return 0;   // Conversion OK
    }
    else
    {
        int orgerrno = errno;

        iconv_close(conv);

        // Error in iconv, errno in return code.
        return orgerrno;
    }
}

int get_encoding (const char * str, std::string & encoding)
{
    DetectObj *obj = detect_obj_init();

    if (obj == nullptr)
    {
        // Memory Allocation failed
        return ENOMEM; // CHARDET_MEM_ALLOCATED_FAIL;
    }

#ifndef CHARDET_BINARY_SAFE
    // before 1.0.5. This API is deprecated on 1.0.5
    switch (detect (str, &obj))
#else
    // from 1.0.5
    switch (detect_r (str, strlen (str), &obj))
#endif
    {
    case CHARDET_OUT_OF_MEMORY :
        // Out of memory on handle processing
        detect_obj_free (&obj);
        return ENOMEM; // CHARDET_OUT_OF_MEMORY;
    case CHARDET_NULL_OBJECT :
        // 1st argument of chardet() must be allocated with detect_obj_init API
        return EINVAL; // CHARDET_NULL_OBJECT;
    }

    //#ifndef CHARDET_BOM_CHECK
    //    printf ("encoding: %s, confidence: %f\n", obj->encoding, obj->confidence);
    //#else
    //    // from 1.0.6 support return whether exists BOM
    //    printf (
    //                "encoding: %s, confidence: %f, exist BOM: %d\n",
    //                obj->encoding, obj->confidence, obj->bom
    //                );
    //#endif
    encoding = obj->encoding;
    detect_obj_free (&obj);

    return 0;
}

int read_file(const std::string & path, std::string & result)
{
    constexpr std::array<char, 3> UTF_8_BOM     = { '\xEF', '\xBB', '\xBF' };
    constexpr std::array<char, 2> UTF_16_BE_BOM = { '\xFE', '\xFF' };
    constexpr std::array<char, 2> UTF_16_LE_BOM = { '\xFF', '\xFE' };
    constexpr std::array<char, 4> UTF_32_BE_BOM = { '\x00', '\x00', '\xFE', '\xFF' };
    constexpr std::array<char, 4> UTF_32_LE_BOM = { '\xFF', '\xFE', '\x00', '\x00' };

    std::ifstream ifs;
    ENCODING encoding = ENCODING::ASCII;
    int res = 0;

    try
    {
        ifs.open(path, std::ios::binary);

        if (!ifs.is_open())
        {
            // Unable to read file
            result.clear();
            throw errno;
        }

        if (ifs.eof())
        {
            // Empty file
            result.clear();
            throw ENCODING::ASCII;
        }

        // Read the bottom mark
        std::array<char, 4> BOM;
        ifs.read(BOM.data(), BOM.size());

        // If you feel tempted to reorder these checks please note
        // that UTF_32_LE_BOM must be done before UTF_16_LE_BOM to
        // avoid misdetection :)
        if (!memcmp(BOM.data(), UTF_32_LE_BOM.data(), UTF_32_LE_BOM.size()))
        {
            // The file contains UTF-32LE BOM
            encoding = ENCODING::UTF32LE_BOM;
            ifs.seekg(UTF_32_LE_BOM.size());
        }
        else if (!memcmp(BOM.data(), UTF_32_BE_BOM.data(), UTF_32_BE_BOM.size()))
        {
            // The file contains UTF-32BE BOM
            encoding = ENCODING::UTF32BE_BOM;
            ifs.seekg(UTF_32_BE_BOM.size());
        }
        else if (!memcmp(BOM.data(), UTF_16_LE_BOM.data(), UTF_16_LE_BOM.size()))
        {
            // The file contains UTF-16LE BOM
            encoding = ENCODING::UTF16LE_BOM;
            ifs.seekg(UTF_16_LE_BOM.size());
        }
        else if (!memcmp(BOM.data(), UTF_16_BE_BOM.data(), UTF_16_BE_BOM.size()))
        {
            // The file contains UTF-16BE BOM
            encoding = ENCODING::UTF16BE_BOM;
            ifs.seekg(UTF_16_BE_BOM.size());
        }
        else if (!memcmp(BOM.data(), UTF_8_BOM.data(), UTF_8_BOM.size()))
        {
            // The file contains UTF-8 BOM
            encoding = ENCODING::UTF8_BOM;
            ifs.seekg(UTF_8_BOM.size());
        }
        else
        {
            // The file does not have BOM
            encoding = ENCODING::ASCII;
            ifs.seekg(0);
        }

        switch (encoding)
        {
        case ENCODING::UTF16LE_BOM:
        {
            std::u16string in;
            // For Windows, wchar_t is uint16_t, but for Linux and others
            // it's uint32_t, so we need to convert in a portable way
            for (char16_t ch; ifs.read((char*)&ch, sizeof(ch));)
            {
#if __BYTE_ORDER == __BIG_ENDIAN
                in.push_back((char16_t)__builtin_bswap16(ch));
#else
                in.push_back(ch);
#endif
            }
            // As of c++11 UTF-16 to UTF-8 conversion nicely comes out-of-the-box
            std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> utfconv;
            result = utfconv.to_bytes(in);
            break;
        }
        case ENCODING::UTF16BE_BOM:
        {
            std::u16string in;
            // For Windows, wchar_t is uint16_t, but for Linux and others
            // it's uint32_t, so we need to convert in a portable way
            for (char16_t ch; ifs.read((char*)&ch, sizeof(ch));)
            {
#if __BYTE_ORDER == __BIG_ENDIAN
                in.push_back(ch);
#else
                in.push_back((char16_t)__builtin_bswap16(ch));
#endif
            }
            // As of c++11 UTF-16 to UTF-8 conversion nicely comes out-of-the-box
            std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> utfconv;
            result = utfconv.to_bytes(in);
            break;
        }
        case ENCODING::UTF32LE_BOM:
        {
            std::u32string in;
            // For Windows, wchar_t is uint16_t, but for Linux and others
            // it's uint32_t, so we need to convert in a portable way.
            // Read characters 32 bitwise:
            for (char32_t ch; ifs.read((char*)&ch, sizeof(ch));)
            {
#if __BYTE_ORDER == __BIG_ENDIAN
                in.push_back((char32_t)__builtin_bswap32(ch));
#else
                in.push_back(ch);
#endif
            }
            // As of c++11 UTF-32 to UTF-8 conversion nicely comes out-of-the-box
            std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> utfconv;
            result = utfconv.to_bytes(in);
            break;
        }
        case ENCODING::UTF32BE_BOM:
        {
            std::u32string in;
            // For Windows, wchar_t is uint16_t, but for Linux and others
            // it's uint32_t, so we need to convert in a portable way
            // Read characters 32 bitwise:
            for (char32_t ch; ifs.read((char*)&ch, sizeof(ch));)
            {
#if __BYTE_ORDER == __BIG_ENDIAN
                in.push_back(ch);
#else
                in.push_back((char32_t)__builtin_bswap32(ch));
#endif
            }
            // As of c++11 UTF-32 to UTF-8 conversion nicely comes out-of-the-box
            std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> utfconv;
            result = utfconv.to_bytes(in);
            break;
        }
        case ENCODING::UTF8_BOM:
        {
            // Already UTF-8, nothing to do
            std::stringstream ss;
            ss << ifs.rdbuf();
            result = ss.str();
            break;
        }
        default:    // ENCODING::ASCII
        {
            // This is a bit tricky, we have to try to determine the actual encoding.
            std::stringstream ss;
            ss << ifs.rdbuf();
            result = ss.str();

            // Using libchardet to guess the encoding
            std::string encoding_name;
            res = get_encoding(result.c_str(), encoding_name);
            if (res)
            {
                throw res;
            }

            if (encoding_name != "UTF-8")
            {
                // If not UTF-8, do the actual conversion
                res = to_utf8(result, encoding_name);
                if (res)
                {
                    throw res;
                }
            }
            break;
        }
        }
        res = static_cast<int>(encoding);
    }
    catch (const std::system_error& e)
    {
        res = errno;
    }
    catch (int _res)
    {
        res = _res;
    }
    return res;
}

void stat_set_size(struct stat *st, size_t size)
{
#if defined __x86_64__ || !defined __USE_FILE_OFFSET64
    st->st_size       = static_cast<__off_t>(size);
#else
    st->st_size       = static_cast<__off64_t>(size);
#endif
    st->st_blocks     = (st->st_size + 512 - 1) / 512;
}

bool detect_docker()
{
    try
    {
        std::ifstream const in_stream("/proc/self/cgroup");
        std::stringstream buffer;
        buffer << in_stream.rdbuf();
        auto const& content_as_string = buffer.str();
        return std::string::npos != content_as_string.find("/docker");
    }
    catch (std::exception const& ex)
    {
        std::fprintf(stderr, "detect_docker(): Unable check if running in docker or not, exception: %s.", ex.what());
        return false;
    }
}

bool is_text_codec(AVCodecID codec_id)
{
    //AV_CODEC_ID_DVD_SUBTITLE = 0x17000,
    //AV_CODEC_ID_DVB_SUBTITLE,
    //AV_CODEC_ID_TEXT,  ///< raw UTF-8 text
    //AV_CODEC_ID_XSUB,
    //AV_CODEC_ID_SSA,
    //AV_CODEC_ID_MOV_TEXT,
    //AV_CODEC_ID_HDMV_PGS_SUBTITLE,
    //AV_CODEC_ID_DVB_TELETEXT,
    //AV_CODEC_ID_SRT,
    //AV_CODEC_ID_MICRODVD,
    //AV_CODEC_ID_EIA_608,
    //AV_CODEC_ID_JACOSUB,
    //AV_CODEC_ID_SAMI,
    //AV_CODEC_ID_REALTEXT,
    //AV_CODEC_ID_STL,
    //AV_CODEC_ID_SUBVIEWER1,
    //AV_CODEC_ID_SUBVIEWER,
    //AV_CODEC_ID_SUBRIP,
    //AV_CODEC_ID_WEBVTT,
    //AV_CODEC_ID_MPL2,
    //AV_CODEC_ID_VPLAYER,
    //AV_CODEC_ID_PJS,
    //AV_CODEC_ID_ASS,
    //AV_CODEC_ID_HDMV_TEXT_SUBTITLE,
    //AV_CODEC_ID_TTML,
    //AV_CODEC_ID_ARIB_CAPTION,

    return (codec_id != AV_CODEC_ID_DVD_SUBTITLE && codec_id != AV_CODEC_ID_DVB_SUBTITLE && codec_id != AV_CODEC_ID_HDMV_PGS_SUBTITLE);
}

int get_audio_props(AVFormatContext *format_ctx, int *channels, int *samplerate)
{
    int ret;

    ret = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, INVALID_STREAM, INVALID_STREAM, nullptr, 0);
    if (ret >= 0)
    {
#if LAVU_DEP_OLD_CHANNEL_LAYOUT
        *channels    = format_ctx->streams[ret]->codecpar->ch_layout.nb_channels;
#else   // !LAVU_DEP_OLD_CHANNEL_LAYOUT
        *channels    = format_ctx->streams[ret]->codecpar->channels;
#endif  // !LAVU_DEP_OLD_CHANNEL_LAYOUT
        *samplerate  = format_ctx->streams[ret]->codecpar->sample_rate;
    }

    return ret;
}

const std::string & regex_escape(std::string * str)
{
    // Escape characters that are meaningful to regexp.
    // Note that "\\" must be first so we do not escape our own escapes...
    const std::vector<std::string> charlist {"\\", "+", "*", "?", "^", "$", "(", ")", "[", "]", "{", "}", "|"};

    for (const std::string & ch : charlist)
    {
        replace_all(str, ch, "\\" + ch);
    }

    replace_all(str, ".", "[.]");

    return *str;
}

bool is_selected(const std::string & ext)
{
    if (params.m_include_extensions->empty())
    {
        // If set is empty, allow all extensions
        return true;
    }

    auto is_match = [ext](const std::string & regex_string) { return (fnmatch(regex_string.c_str(), ext.c_str(), 0) == 0); };

    return (find_if(begin(*params.m_include_extensions), end(*params.m_include_extensions), is_match) != end(*params.m_include_extensions));
}

bool is_blocked(const std::string & filename)
{
    std::string ext;

    if (!find_ext(&ext, filename))
    {
        return false; // no extension
    }

    // These are blocked by default, they confuse players like VLC or mpv which
    // auto load them. As they get incorporated as subtitle tracks by FFmpegfs
    // they would end up as duplicates.
    if (!strcasecmp(ext, "srt") || !strcasecmp(ext, "vtt"))
    {
        return true;
    }

    auto is_match = [ext](const std::string & regex_string) { return (fnmatch(regex_string.c_str(), ext.c_str(), 0) == 0); };

    // Check block list
    return (find_if(begin(*params.m_hide_extensions), end(*params.m_hide_extensions), is_match) != end(*params.m_hide_extensions));
}

void save_free(void **p)
{
    void * tmp = __atomic_exchange_n(p, nullptr, __ATOMIC_RELEASE);
    if (tmp != nullptr)
    {
        free(tmp);
    }
}

void mssleep(int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void ussleep(int microseconds)
{
    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

void nssleep(int nanoseconds)
{
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
}
