/*
 * FFmpeg audience browser opmimisations source for FFmpegfs
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

#include "ffmpeg_transcoder.h"

FFmpeg_Profiles::FFmpeg_Profiles() {}
FFmpeg_Profiles::~FFmpeg_Profiles() {}

/*
 * Make audience audience specific optimisations, see:
 *
 * https://www.ffmpeg.org/ffmpeg-formats.html#mov_002c-mp4_002c-ismv
 *
 * mov, mp4, ismv
 * ==============
 *
 * MOV/MP4/ISMV (Smooth Streaming) muxer.
 *
 * The mov/mp4/ismv muxer supports fragmentation. Normally, a MOV/MP4 file has all the metadata about all packets
 * stored in one location (written at the end of the file, it can be moved to the start for better playback by
 * adding faststart to the movflags, or using the qt-faststart tool). A fragmented file consists of a number of
 * fragments, where packets and metadata about these packets are stored together. Writing a fragmented file has
 * the advantage that the file is decodable even if the writing is interrupted (while a normal MOV/MP4 is undecodable
 * if it is not properly finished), and it requires less memory when writing very long files (since writing normal
 * MOV/MP4 files stores info about every single packet in memory until the file is closed). The downside is that it
 * is less compatible with other applications.
 *
 * Options
 * =======
 *
 *      Fragmentation is enabled by setting one of the AVOptions that define how to cut the file into fragments:
 *
 *          -moov_size bytes
 *
 *      Reserves space for the moov atom at the beginning of the file instead of placing the moov atom at the end.
 *      If the space reserved is insufficient, muxing will fail.
 *
 *          -movflags frag_keyframe
 *
 *      Start a new fragment at each video keyframe.
 *
 *          -frag_duration duration
 *
 *      Create fragments that are duration microseconds long.
 *
 *          -frag_size size
 *
 *      Create fragments that contain up to size bytes of payload data.
 *
 *          -movflags frag_custom
 *
 *      Allow the caller to manually choose when to cut fragments, by calling av_write_frame(ctx, nullptr) to write a
 *      fragment with the packets written so far. (This is only useful with other applications integrating libavformat,
 *      not from ffmpeg.)
 *
 *          -min_frag_duration duration
 *
 *      Don’t create fragments that are shorter than duration microseconds long.
 *
 *      If more than one condition is specified, fragments are cut when one of the specified conditions is fulfilled.
 *      The exception to this is -min_frag_duration, which has to be fulfilled for any of the other conditions to apply.
 *
 *      Additionally, the way the output file is written can be adjusted through a few other options:
 *
 *          -movflags empty_moov
 *
 *      Write an initial moov atom directly at the start of the file, without describing any samples in it. Generally,
 *      an mdat/moov pair is written at the start of the file, as a normal MOV/MP4 file, containing only a short portion
 *      of the file. With this option set, there is no initial mdat atom, and the moov atom only describes the tracks
 *      but has a zero duration.
 *
 *      This option is implicitly set when writing ismv (Smooth Streaming) files.
 *
 *          -movflags separate_moof
 *
 *      Write a separate moof (movie fragment) atom for each track. Normally, packets for all tracks are written in a
 *      moof atom (which is slightly more efficient), but with this option set, the muxer writes one moof/mdat pair for
 *      each track, making it easier to separate tracks.
 *
 *      This option is implicitly set when writing ismv (Smooth Streaming) files.
 *
 *          -movflags faststart
 *
 *      Run a second pass moving the index (moov atom) to the beginning of the file. This operation can take a while,
 *      and will not work in various situations such as fragmented output, thus it is not enabled by default.
 *
 *          -movflags rtphint
 *
 *      Add RTP hinting tracks to the output file.
 *
 *          -movflags disable_chpl
 *
 *      Disable Nero chapter markers (chpl atom). Normally, both Nero chapters and a QuickTime chapter track are written
 *      to the file. With this option set, only the QuickTime chapter track will be written. Nero chapters can cause
 *      failures when the file is reprocessed with certain tagging programs, like mp3Tag 2.61a and iTunes 11.3, most likely
 *      other versions are affected as well.
 *
 *          -movflags omit_tfhd_offset
 *
 *      Do not write any absolute base_data_offset in tfhd atoms. This avoids tying fragments to absolute byte positions
 *      in the file/streams.
 *
 *          -movflags default_base_moof
 *
 *      Similarly to the omit_tfhd_offset, this flag avoids writing the absolute base_data_offset field in tfhd atoms, but
 *      does so by using the new default-base-is-moof flag instead. This flag is new from 14496-12:2012. This may make the
 *      fragments easier to parse in certain circumstances (avoiding basing track fragment location calculations on the
 *      implicit end of the previous track fragment).
 *
 *          -write_tmcd
 *
 *      Specify on to force writing a timecode track, off to disable it and auto to write a timecode track only for mov and
 *      mp4 output (default).
 *
 *          -movflags negative_cts_offsets
 *
 *      Enables utilization of version 1 of the CTTS box, in which the CTS offsets can be negative. This enables the initial
 *      sample to have DTS/CTS of zero, and reduces the need for edit lists for some cases such as video tracks with B-frames.
 *      Additionally, eases conformance with the DASH-IF interoperability guidelines.
 *
 * Posssible codec options:
 *

    // -profile:v baseline -level 3.0
    { "profile",                "baseline",                 0,  0 },
    { "level",                  "3.0",                      0,  0 },

    // -profile:v high -level 3.1 - REQUIRED FOR PLAYBACK UNDER WIN7
    { "profile",                "high",                     0,  0 },
    { "level",                  "3.1",                      0,  0 },

    // Set speed (changes profile!): utra/veryfast and zerolatency
    { "preset",                 "ultrafast",                0,  0 },
    { "preset",                 "veryfast",                 0,  0 },
    { "tune",                   "zerolatency",              0,  0 },

 *
 * Posssible format options:
 *

    { "moov_size",              "1000000",                  0, OPT_ALL },   // bytes
    { "movflags",               "+frag_keyframe",           0, OPT_ALL },
    { "frag_duration",          "1000000",                  0, OPT_ALL },   // microsenconds
    { "frag_size",              "100000",                   0, OPT_ALL },   // bytes
    { "min_frag_duration",      "1000",                     0, OPT_ALL },   // microsenconds
    { "movflags",               "+empty_moov",              0, OPT_ALL },
    { "movflags",               "+delay_moov",              0, OPT_ALL },
    { "movflags",               "+separate_moof",           0, OPT_ALL },
    { "movflags",               "+faststart",               0, OPT_ALL },
    { "movflags",               "+rtphint",                 0, OPT_ALL },
    { "movflags",               "+disable_chpl",            0, OPT_ALL },
    { "movflags",               "+omit_tfhd_offset",        0, OPT_ALL },
    { "movflags",               "+default_base_moof",       0, OPT_ALL },
    { "write_tmcd",             "on",                       0, OPT_ALL },   // on, off or auto
    { "movflags",               "+negative_cts_offsets",    0, OPT_ALL },
    { "movflags",               "+isml",                    0, OPT_ALL },

 */

// ****************************************************************************************************************

// No opimisations, just plain mp4.
static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_codec_none[] =
{
    // -profile:v high -level 3.1 - REQUIRED FOR PLAYBACK UNDER WIN7
    { "profile",              "high",                       0,  0 },
    { "level",                "3.1",                        0,  0 },

    // Set speed (changes profile!)
    { "preset",               "ultrafast",                  0,  0 },
    { nullptr,                  nullptr,                    0,  0 }
};

static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_format_none[] =
{
    { "movflags",               "+delay_moov",              0, OPT_ALL },
    { nullptr,                  nullptr,                    0,  0 }
};

// ****************************************************************************************************************

// Use: -movflags +empty_moov
//      -frag_duration 1000000  (for audio files only)
// GOOD: Starts immediately while still decoding.
static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_codec_ff[] =
{
    // -profile:v high -level 3.1 - REQUIRED FOR PLAYBACK UNDER WIN7
    { "profile",              "high",                       0,  0 },
    { "level",                "3.1",                        0,  0 },

    // Set speed (changes profile!)
    { "preset",               "ultrafast",                  0,  0 },
    { nullptr,                  nullptr,                    0,  0 }
};

static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_format_ff[] =
{
    { "frag_duration",          "1000000",                  0, OPT_AUDIO },     // microsenconds
    { "movflags",               "+empty_moov",              0, OPT_ALL },
    { nullptr,                  nullptr,                    0,  0 }
};

// ****************************************************************************************************************

// Use: -movflags +faststart+empty_moov+separate_moof -frag_duration 1000000
// GOOD: Starts immediately while still decoding.
static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_codec_edge[] =
{
    // Set speed (changes profile!)
    { "preset",               "ultrafast",                  0,  0 },
    { nullptr,                  nullptr,                    0,  0 }
};

static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_format_edge[] =
{
    { "frag_duration",          "1000000",                  0, OPT_ALL },     // microsenconds
    { "movflags",               "+empty_moov",              0, OPT_ALL },
    { "movflags",               "+separate_moof",           0, OPT_ALL },
    { "movflags",               "+faststart",               0, OPT_ALL },
    { nullptr,                  nullptr,                    0,  0 }
};

// ****************************************************************************************************************

// Use: -movflags +faststart+empty_moov+separate_moof -frag_duration 1000000
static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_codec_ie[] =
{
    // -profile:v high -level 3.1 - REQUIRED FOR PLAYBACK UNDER WIN7
    { "profile",              "high",                       0,  0 },
    { "level",                "3.1",                        0,  0 },

    // Set speed (changes profile!)
    { "preset",               "ultrafast",                  0,  0 },
    { nullptr,                  nullptr,                    0,  0 }
};

static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_format_ie[] =
{
    { nullptr,                  nullptr,                    0,  0 }
};

// ****************************************************************************************************************

// NOT GOOD: Playback runs, but must wait until end of decode.
static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_codec_chrome[] =
{

    // Set speed (changes profile!)
    { "preset",               "ultrafast",                  0,  0 },
    { nullptr,                  nullptr,                    0,  0 }
};

static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_format_chrome[] =
{
    { nullptr,                  nullptr,                    0,  0 }
};

// ****************************************************************************************************************

// Safari uses Quicktime for playback. Files must be suitable for playback with Quicktime.
static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_codec_safari[] =
{
    // Set speed (changes profile!)
    { "preset",               "ultrafast",                  0,  0 },
    { nullptr,                  nullptr,                    0,  0 }
};

static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_format_safari[] =
{
    { nullptr,                  nullptr,                    0,  0 }
};

// ****************************************************************************************************************

static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_codec_opera[] =
{
    // Set speed (changes profile!)
    { "preset",               "ultrafast",                  0,  0 },
    { nullptr,                  nullptr,                    0,  0 }
};

static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_format_opera[] =
{
    { nullptr,                  nullptr,                    0,  0 }
};

// ****************************************************************************************************************

// TODO - maybe same as Chrome
static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_codec_maxthon[] =
{
    // -profile:v high -level 3.1
    { "profile",              "high",                       0,  0 },
    { "level",                "3.1",                        0,  0 },

    // Set speed (changes profile!)
    { "preset",               "ultrafast",                  0,  0 },
    { nullptr,                  nullptr,                    0,  0 }
};

static const FFmpeg_Profiles::PROFILE_OPTION m_option_mp4_format_maxthon[] =
{
    { nullptr,                  nullptr,                    0,  0 }
};

// ****************************************************************************************************************

static const FFmpeg_Profiles::PROFILE_OPTION m_option_mov_codec[] =
{
    // Set speed (changes profile!)
    { "preset",                 "ultrafast",                0,  0 },
    { nullptr,                  nullptr,                    0,  0 }
};

static const FFmpeg_Profiles::PROFILE_OPTION m_option_mov_format[] =
{
    { "movflags",               "+delay_moov",              0, OPT_ALL },
    { nullptr,                  nullptr,                    0,  0 }
};

// ****************************************************************************************************************

// Use: -movflags +faststart+empty_moov+separate_moof -frag_duration 1000000
static const FFmpeg_Profiles::PROFILE_OPTION m_option_prores_codec[] =
{
    { nullptr,                  nullptr,                    0,  0 }
};

static const FFmpeg_Profiles::PROFILE_OPTION m_option_prores_format[] =
{
    { "movflags",               "+delay_moov",              0, OPT_ALL },
    { nullptr,                  nullptr,                    0,  0 }
};

/*
 * *******************************************************************************************************************
 *
 * webm
 * ====
 *
 * https://trac.ffmpeg.org/wiki/Encode/VP9
 *
 * Controlling Speed and Quality
 * =============================
 *
 * libvpx-vp9 has two main control knobs for speed and quality:
 *
 * Deadline / Quality
 *
 *  -deadline can be set to realtime, good, or best. For legacy reasons, the option is also accessible with -quality in ffmpeg.
 *
 * good is the default and recommended for most applications.
 * best is recommended if you have lots of time and want the best compression efficiency.
 * realtime is recommended for live / fast encoding.
 *
 * CPU Utilization / Speed
 * =======================
 *
 *  -cpu-used sets how efficient the compression will be. For legacy reasons, the option is also accessible with -speed in ffmpeg.
 *
 * When the deadline/quality parameter is good or best, values for -cpu-used can be set between 0 and 5. The default is 0.
 * Using 1 or 2 will increase encoding speed at the expense of having some impact on quality and rate control accuracy.
 * 4 or 5 will turn off rate distortion optimization, having even more of an impact on quality.
 *
 * When the deadline/quality is set to realtime, the encoder will try to hit a particular CPU utilization target, and encoding quality
 * will depend on CPU speed and the complexity of the clip that you are encoding. See ​the vpx documentation for more info. In this case,
 * the valid values for -cpu-used are between 0 and 15, where the CPU utilization target (in per cent) is calculated as
 * (100*(16-cpu-used)/16)%.
 *
 * *******************************************************************************************************************
 *
 */

static const FFmpeg_Profiles::PROFILE_OPTION m_option_webm_codec_none[] =
{
    { "deadline",               "realtime",                 0,  0 },

    { "cpu-used",               "8",                        0,  0 },

//    ffmpeg -i <source> -c:v libvpx-vp9 -pass 2 -b:v 1000K -threads 8 -speed 1 
//      -tile-columns 6 -frame-parallel 1 -auto-alt-ref 1 -lag-in-frames 25 
//      -c:a libopus -b:a 64k -f webm out.webm


//    Most of the current VP9 decoders use tile-based, multi-threaded decoding. In order for the decoders to take advantage
//    of multiple cores, the encoder must set tile-columns and frame-parallel.

//    Setting auto-alt-ref and lag-in-frames >= 12 will turn on VP9's alt-ref frames, a VP9 feature that enhances quality.

//    speed 4 tells VP9 to encode really fast, sacrificing quality. Useful to speed up the first pass.

//    speed 1 is a good speed vs. quality compromise. Produces output quality typically very close to speed 0, but usually encodes much faster.

//    Multi-threaded encoding may be used if -threads > 1 and -tile-columns > 0.


//    { "threads",                "8",                        0,  0 },
//    { "speed",                  "4",                        0,  0 },
    { "tile-columns",           "6",                        0,  0 },
    { "frame-parallel",         "1",                        0,  0 },
    { "auto-alt-ref",           "1",                        0,  0 },
    { "lag-in-frames",          "25",                        0,  0 },

    { nullptr,                  nullptr,                    0,  0 }
};

static const FFmpeg_Profiles::PROFILE_OPTION m_option_webm_format_none[] =
{
    { nullptr,                  nullptr,                    0,  0 }
};

// ****************************************************************************************************************

const FFmpeg_Profiles::PROFILE_LIST FFmpeg_Profiles::m_profile[] =
{
    // MP4

    {
        FILETYPE_MP4,
        PROFILE_NONE,
        m_option_mp4_codec_none,
        m_option_mp4_format_none
    },
    {
        FILETYPE_MP4,
        PROFILE_MP4_FF,
        m_option_mp4_codec_ff,
        m_option_mp4_format_ff
    },
    {
        FILETYPE_MP4,
        PROFILE_MP4_EDGE,
        m_option_mp4_codec_edge,
        m_option_mp4_format_edge
    },
    {
        FILETYPE_MP4,
        PROFILE_MP4_IE,
        m_option_mp4_codec_ie,
        m_option_mp4_format_ie
    },
    {
        FILETYPE_MP4,
        PROFILE_MP4_CHROME,
        m_option_mp4_codec_chrome,
        m_option_mp4_format_chrome
    },
    {
        FILETYPE_MP4,
        PROFILE_MP4_SAFARI,
        m_option_mp4_codec_safari,
        m_option_mp4_format_safari
    },
    {
        FILETYPE_MP4,
        PROFILE_MP4_OPERA,
        m_option_mp4_codec_opera,
        m_option_mp4_format_opera
    },
    {
        FILETYPE_MP4,
        PROFILE_MP4_MAXTHON,
        m_option_mp4_codec_maxthon,
        m_option_mp4_format_maxthon
    },

    // MOV
    {
        FILETYPE_MOV,
        PROFILE_MOV_NONE,
        m_option_mov_codec,
        m_option_mov_format
    },

    // prores
    {
        FILETYPE_PRORES,
        PROFILE_PRORES_NONE,
        m_option_prores_codec,
        m_option_prores_format
    },

    // WebM

    {
        FILETYPE_WEBM,
        PROFILE_NONE,
        m_option_webm_codec_none,
        m_option_webm_format_none
    },

    // Must be last entry
    {
        FILETYPE_UNKNOWN,
        PROFILE_INVALID,
        nullptr,
        nullptr
    }
};
