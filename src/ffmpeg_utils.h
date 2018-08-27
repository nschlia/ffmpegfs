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

// Force PRId64 defines
#define __STDC_FORMAT_MACROS

#include "ffmpeg_compat.h"

#if !defined(USE_LIBSWRESAMPLE) && !defined(USE_LIBAVRESAMPLE)
#error "Must have either libswresample (preferred choice for FFMpeg) or libavresample (with libav)."
#endif

// Prefer libswresample
#ifdef USE_LIBSWRESAMPLE
#define LAVR_DEPRECATE                      1
#else
#define LAVR_DEPRECATE                      0
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

#ifdef __cplusplus
#include <string>

using namespace std;

const string & append_sep(string * path);
const string & append_filename(string * path, const string & filename);
const string & remove_filename(string *path);
const string & remove_path(string *path);
bool find_ext(string * ext, const string & filename);
const string & replace_ext(string * filename, const string & ext);
const string & get_destname(string *destname, const string & filename);
string ffmpeg_geterror(int errnum);
double ffmpeg_cvttime(int64_t ts, const AVRational & time_base);

string format_number(int64_t value);
string format_bitrate(uint64_t value);
string format_samplerate(unsigned int value);
string format_duration(time_t value);
string format_time(time_t value);
string format_size(size_t value);

void exepath(string *path);

std::string &ltrim(std::string &s);
std::string &rtrim(std::string &s);
std::string &trim(std::string &s);

#endif

#ifdef __cplusplus
extern "C" {
#endif
void ffmpeg_libinfo(char * buffer, size_t maxsize);
int show_formats_devices(int device_only);
const char * get_codec_name(enum AVCodecID codec_id, int long_name);
int supports_albumart(FILETYPE filetype);
FILETYPE get_filetype(const char * type);
const char * get_codecs(const char * type, FILETYPE * output_type, enum AVCodecID * audio_codecid, enum AVCodecID * video_codecid);

void format_number(char *output, size_t size, uint64_t value);
void format_bitrate(char *output, size_t size, uint64_t value);
void format_samplerate(char *output, size_t size, unsigned int value);
void format_duration(char *output, size_t size, time_t value);
void format_time(char *output, size_t size, time_t value);
void format_size(char *output, size_t size, size_t value);

int print_info(AVStream* stream);

int compare(const char *value, const char *pattern);

#ifdef __cplusplus
}
#endif

int mktree(const char *path, mode_t mode);
void tempdir(char *dir, size_t size);

#ifdef USING_LIBAV
// Libav does not have these functions
int avformat_alloc_output_context2(AVFormatContext **avctx, AVOutputFormat *oformat, const char *format, const char *filename);
const char *avcodec_get_name(enum AVCodecID id);
#endif

#endif
