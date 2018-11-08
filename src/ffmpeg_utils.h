/*
 * FFmpeg decoder class header for ffmpegfs
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

#ifndef FFMPEG_UTILS_H
#define FFMPEG_UTILS_H

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// Force PRId64 defines
#define __STDC_FORMAT_MACROS

#include "ffmpeg_compat.h"

#include <string>

#if !defined(USE_LIBSWRESAMPLE) && !defined(USE_LIBAVRESAMPLE)
#error "Must have either libswresample (preferred choice for FFMpeg) or libavresample (with libav)."
#endif

// Prefer libswresample
#ifdef USE_LIBSWRESAMPLE
#define LAVR_DEPRECATE                      1
#else
#define LAVR_DEPRECATE                      0
#endif

#ifndef PATH_MAX
#include <linux/limits.h>
#endif

// Disable annoying warnings outside our code
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#ifdef __GNUC__
#  include <features.h>
#  if __GNUC_PREREQ(5,0) || defined(__clang__)
// GCC >= 5.0
#     pragma GCC diagnostic ignored "-Wfloat-conversion"
#  elif __GNUC_PREREQ(4,8)
// GCC >= 4.8
#  else
#     error("GCC < 4.8 not supported");
#  endif
#endif
#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif
#pragma GCC diagnostic pop

#ifndef LIBAVUTIL_VERSION_MICRO
#error "LIBAVUTIL_VERSION_MICRO not defined. Missing include header?"
#endif

// Libav detection:
// FFmpeg has library micro version >= 100
// Libav  has library micro version < 100
// So if micro < 100, it's Libav, else it's FFmpeg.
#if LIBAVUTIL_VERSION_MICRO < 100
#define USING_LIBAV
#endif

// Allow use of av_format_inject_global_side_data when available
#define HAVE_AV_FORMAT_INJECT_GLOBAL_SIDE_DATA  (LIBAVFORMAT_VERSION_MICRO >= 100 && LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT( 57, 64, 101 ))

// Add av_get_media_type_string function if missing
#define HAVE_MEDIA_TYPE_STRING                  (LIBAVUTIL_VERSION_MICRO >= 100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 55, 34, 101 ))
#if HAVE_MEDIA_TYPE_STRING
#define get_media_type_string               av_get_media_type_string
#else
const char *get_media_type_string(enum 		AVMediaType media_type);
#endif

// Ignore if this is missing
#ifndef AV_ROUND_PASS_MINMAX
#define AV_ROUND_PASS_MINMAX            	0
#endif

// These once had a different name
#if !defined(AV_CODEC_CAP_DELAY) && defined(CODEC_CAP_DELAY)
#define AV_CODEC_CAP_DELAY              	CODEC_CAP_DELAY
#endif

#if !defined(AV_CODEC_CAP_TRUNCATED) && defined(CODEC_CAP_TRUNCATED)
#define AV_CODEC_CAP_TRUNCATED          	CODEC_CAP_TRUNCATED
#endif

#if !defined(AV_CODEC_FLAG_TRUNCATED) && defined(CODEC_FLAG_TRUNCATED)
#define AV_CODEC_FLAG_TRUNCATED         	CODEC_FLAG_TRUNCATED
#endif

#ifndef AV_CODEC_FLAG_GLOBAL_HEADER
#define AV_CODEC_FLAG_GLOBAL_HEADER     	CODEC_FLAG_GLOBAL_HEADER
#endif

#ifndef FF_INPUT_BUFFER_PADDING_SIZE
#define FF_INPUT_BUFFER_PADDING_SIZE    	256
#endif

#ifndef AV_CODEC_CAP_VARIABLE_FRAME_SIZE
#define AV_CODEC_CAP_VARIABLE_FRAME_SIZE	CODEC_CAP_VARIABLE_FRAME_SIZE
#endif

#if !defined(USING_LIBAV) && (LIBAVUTIL_VERSION_MAJOR > 54)
#define BITRATE int64_t
#else // USING_LIBAV
#define BITRATE int
#endif

// Make access possible over codecpar if available
#if LAVF_DEP_AVSTREAM_CODEC
#define CODECPAR(s)     ((s)->codecpar)
#else
#define CODECPAR(s)     ((s)->codec)
#endif

typedef enum _tagFILETYPE
{
    FILETYPE_UNKNOWN,
    FILETYPE_MP3,
    FILETYPE_MP4,
    FILETYPE_WAV,
    FILETYPE_OGG,
    FILETYPE_WEBM,
    FILETYPE_MOV,
    FILETYPE_AIFF,
    FILETYPE_OPUS
} FILETYPE;

typedef enum _tagPROFILE
{
    PROFILE_INVALID = -1,

    PROFILE_NONE,               // no specific profile

    // MP4

    PROFILE_MP4_FF,				// Firefox
    PROFILE_MP4_EDGE,			// MS Edge
    PROFILE_MP4_IE,				// MS Internet Explorer
    PROFILE_MP4_CHROME,			// Google Chrome
    PROFILE_MP4_SAFARI,			// Apple Safari
    PROFILE_MP4_OPERA,			// Opera
    PROFILE_MP4_MAXTHON         // Maxthon

    // WEBM

} PROFILE;

typedef struct ffmpegfs_format {
    ffmpegfs_format(const char * format_name, FILETYPE filetype, AVCodecID video_codecid, AVCodecID audio_codecid)
        : m_format_name(format_name)
        , m_filetype(filetype)
        , m_video_codecid(video_codecid)
        , m_audio_codecid(audio_codecid)
    {}

    std::string m_format_name;
    FILETYPE    m_filetype;
    // Video
    AVCodecID   m_video_codecid;
    // Audio
    AVCodecID   m_audio_codecid;
} ffmpegfs_format;

const std::string & append_sep(std::string * path);
const std::string & append_filename(std::string * path, const std::string & filename);
const std::string & remove_filename(std::string *path);
const std::string & remove_path(std::string *path);
bool find_ext(std::string * ext, const std::string & filename);
const std::string & replace_ext(std::string * filename, const std::string & ext);
const std::string & get_destname(std::string *destname, const std::string & filename);
std::string ffmpeg_geterror(int errnum);
double ffmpeg_cvttime(int64_t ts, const AVRational & time_base);

std::string format_number(int64_t value);
std::string format_bitrate(uint64_t value);
std::string format_samplerate(unsigned int value);
std::string format_duration(time_t value);
std::string format_time(time_t value);
std::string format_size(size_t value);

void exepath(std::string *path);

std::string &ltrim(std::string &s);
std::string &rtrim(std::string &s);
std::string &trim(std::string &s);

std::string replace_all(std::string str, const std::string& from, const std::string& to);
template<typename ... Args> std::string string_format(const std::string& format, Args ... args);

int strcasecmp(const std::string & s1, const std::string & s2);

std::string ffmpeg_libinfo();
int show_formats_devices(int device_only);
const char * get_codec_name(AVCodecID codec_id, bool long_name);
int supports_albumart(FILETYPE filetype);
FILETYPE get_filetype(const std::string &type);
int get_codecs(const std::string & type, ffmpegfs_format *video_format);

int print_info(const AVStream* stream);

int compare(const std::string &value, const std::string &pattern);

const std::string &expand_path(std::string *tgt, const std::string &src);

int is_mount(const std::string &filename);

int mktree(const std::string & filename, mode_t mode);
void tempdir(std::string & dir);

#ifdef USING_LIBAV
// Libav does not have these functions
int avformat_alloc_output_context2(AVFormatContext **avctx, AVOutputFormat *oformat, const char *format, const char *filename);
const char *avcodec_get_name(AVCodecID id);
#endif

#endif
