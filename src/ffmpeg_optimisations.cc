/*
 * FFmpeg target browser opmimisations source for ffmpegfs
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

#include "ffmpeg_transcoder.h"
/*
 * Make target audience specific optimisations, see:
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
 *      Allow the caller to manually choose when to cut fragments, by calling av_write_frame(ctx, NULL) to write a
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
 */

// ****************************************************************************************************************

// No opimisations, just plain mp4.
const FFMPEG_Transcoder::MP4_PROFILE FFMPEG_Transcoder::m_opt_codec_none[] =
{
    // -profile:v baseline -level 3.0
    //{ "profile",              "baseline",                 0,  0 },
    //{ "level",                "3.0",                      0,  0 },

    // -profile:v high -level 3.1
    //{ "profile",              "high",                     0,  0 },
    //{ "level",                "3.1",                      0,  0 },

    // Set speed (changes profile!)
    { "preset",               "ultrafast",                0,  0 },
    //{ "preset",               "veryfast",                 0,  0 },
    //{ "tune",                 "zerolatency",              0,  0 },
};

const FFMPEG_Transcoder::MP4_PROFILE FFMPEG_Transcoder::m_opt_format_none[] =
{
    //{ "moov_size",              "1000000",                 0, OPT_ALL },     // bytes
    //{ "movflags",               "+frag_keyframe",          0, OPT_ALL },
    //{ "frag_duration",          "1000000",                 0, OPT_ALL },     // microsenconds
    //{ "frag_size",              "100000",                  0, OPT_ALL },     // bytes
    //{ "min_frag_duration",      "1000",                    0, OPT_ALL },     // microsenconds
    //{ "movflags",               "+empty_moov",             0, OPT_ALL },
    //{ "movflags",               "+separate_moof",          0, OPT_ALL },
    //{ "movflags",               "+faststart",              0, OPT_ALL },
    //{ "movflags",               "+rtphint",                0, OPT_ALL },
    //{ "movflags",               "+disable_chpl",           0, OPT_ALL },
    //{ "movflags",               "+omit_tfhd_offset",       0, OPT_ALL },
    //{ "movflags",               "+default_base_moof",      0, OPT_ALL },
    //{ "write_tmcd",             "on",                      0, OPT_ALL },      // on, off or auto
    //{ "movflags",               "+negative_cts_offsets",   0, OPT_ALL },
    //{ "movflags",               "+isml",                   0, OPT_ALL },
    { NULL, NULL, 0, 0 }
};

// ****************************************************************************************************************

// Use: -movflags +empty_moov
//      -frag_duration 1000000  (for audio files only)
// GOOD: Starts immediately while still decoding.
const FFMPEG_Transcoder::MP4_PROFILE FFMPEG_Transcoder::m_opt_codec_ff[] =
{
    // -profile:v baseline -level 3.0
    //{ "profile",              "baseline",                 0,  0 },
    //{ "level",                "3.0",                      0,  0 },

    // -profile:v high -level 3.1
    //{ "profile",              "high",                     0,  0 },
    //{ "level",                "3.1",                      0,  0 },

    // Set speed (changes profile!)
    { "preset",               "ultrafast",                0,  0 },
    //{ "preset",               "veryfast",                 0,  0 },
    //{ "tune",                 "zerolatency",              0,  0 },
};

const FFMPEG_Transcoder::MP4_PROFILE FFMPEG_Transcoder::m_opt_format_ff[] =
{
    //{ "moov_size",              "1000000",                 0, OPT_ALL },       // bytes
    //{ "movflags",               "+frag_keyframe",          0, OPT_ALL },
    { "frag_duration",          "1000000",                  0, OPT_AUDIO },     // microsenconds
    //{ "frag_size",              "100000",                  0, OPT_ALL },       // bytes
    //{ "min_frag_duration",      "1000",                    0, OPT_ALL },       // microsenconds
    { "movflags",               "+empty_moov",              0, OPT_ALL },
    //{ "movflags",               "+separate_moof",          0, OPT_ALL },
    //{ "movflags",               "+faststart",              0, OPT_ALL },
    //{ "movflags",               "+rtphint",                0, OPT_ALL },
    //{ "movflags",               "+disable_chpl",           0, OPT_ALL },
    //{ "movflags",               "+omit_tfhd_offset",       0, OPT_ALL },
    //{ "movflags",               "+default_base_moof",      0, OPT_ALL },
    //{ "write_tmcd",             "on",                      0, OPT_ALL },       // on, off or auto
    //{ "movflags",               "+negative_cts_offsets",   0, OPT_ALL },
    //{ "movflags",               "+isml",                   0, OPT_ALL },
    { NULL, NULL, 0, 0 }
};

// ****************************************************************************************************************

// Use: -movflags +faststart+empty_moov+separate_moof -frag_duration 1000000
// GOOD: Starts immediately while still decoding.
const FFMPEG_Transcoder::MP4_PROFILE FFMPEG_Transcoder::m_opt_codec_edge[] =
{
    // -profile:v baseline -level 3.0
    //{ "profile",              "baseline",                 0,  0 },
    //{ "level",                "3.0",                      0,  0 },

    // -profile:v high -level 3.1
    //{ "profile",              "high",                     0,  0 },
    //{ "level",                "3.1",                      0,  0 },

    // Set speed (changes profile!)
    { "preset",               "ultrafast",                0,  0 },
    //{ "preset",               "veryfast",                 0,  0 },
    //{ "tune",                 "zerolatency",              0,  0 },
};

const FFMPEG_Transcoder::MP4_PROFILE FFMPEG_Transcoder::m_opt_format_edge[] =
{
    //{ "moov_size",              "1000000",                 0, OPT_ALL },       // bytes
    //{ "movflags",               "frag_keyframe",          0, OPT_ALL },
    { "frag_duration",          "1000000",                  0, OPT_ALL },     // microsenconds
    //{ "frag_size",              "100000",                  0, OPT_ALL },       // bytes
    //{ "min_frag_duration",      "1000",                    0, OPT_ALL },       // microsenconds
    { "movflags",               "+empty_moov",              0, OPT_ALL },
    { "movflags",               "+separate_moof",           0, OPT_ALL },
    { "movflags",               "+faststart",               0, OPT_ALL },
    //{ "movflags",               "+rtphint",                0, OPT_ALL },
    //{ "movflags",               "+disable_chpl",           0, OPT_ALL },
    //{ "movflags",               "+omit_tfhd_offset",       0, OPT_ALL },
    //{ "movflags",               "+default_base_moof",      0, OPT_ALL },
    //{ "write_tmcd",             "on",                      0, OPT_ALL },      // on, off or auto
    //{ "movflags",               "+negative_cts_offsets",   0, OPT_ALL },
    //{ "movflags",               "+isml",                   0, OPT_ALL },
    { NULL, NULL, 0, 0 }
};

// ****************************************************************************************************************
