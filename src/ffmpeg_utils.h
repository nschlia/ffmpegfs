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

// 2018-01-xx - xxxxxxx - lavf 58.7.100 - avformat.h
//  Deprecate AVFormatContext filename field which had limited length, use the
//   new dynamically allocated url field instead.
#define LAVF_DEP_FILENAME                   (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 7, 0))
// 2016-04-21 - 7fc329e - lavc 57.37.100 - avcodec.h
//   Add a new audio/video encoding and decoding API with decoupled input
//   and output -- avcodec_send_packet(), avcodec_receive_frame(),
//   avcodec_send_frame() and avcodec_receive_packet().
#define LAVC_NEW_PACKET_INTERFACE           (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 0))
// 2016-04-11 - 6f69f7a / 9200514 - lavf 57.33.100 / 57.5.0 - avformat.h
//   Add AVStream.codecpar, deprecate AVStream.codec.
#define LAVF_DEP_AVSTREAM_CODEC             (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 33, 0))
// 2018-xx-xx - xxxxxxx - lavf 58.9.100 - avformat.h
//   Deprecate use of av_register_input_format(), av_register_output_format(),
//   av_register_all(), av_iformat_next(), av_oformat_next().
#define LAVF_DEP_AV_REGISTER                (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 9, 0))
// 2018-xx-xx - xxxxxxx - lavc 58.10.100 - avcodec.h
//   Deprecate use of avcodec_register(), avcodec_register_all(),
//   av_codec_next(), av_register_codec_parser(), and av_parser_next().
//   Add av_codec_iterate() and av_parser_iterate().
#define LAVC_DEP_AV_CODEC_REGISTER          (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 10, 0))
// 2018-04-01 - f1805d160d - lavfi 7.14.100 - avfilter.h
//   Deprecate use of avfilter_register(), avfilter_register_all(),
//   avfilter_next(). Add av_filter_iterate().
#define LAVC_DEP_AV_FILTER_REGISTER         (LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(7, 14, 0))
// 2015-10-29 - lavc 57.12.100 / 57.8.0 - avcodec.h
//   xxxxxx - Deprecate av_free_packet(). Use av_packet_unref() as replacement,
//            it resets the packet in a more consistent way.
//   xxxxxx - Deprecate av_dup_packet(), it is a no-op for most cases.
//            Use av_packet_ref() to make a non-refcounted AVPacket refcounted.
//   xxxxxx - Add av_packet_alloc(), av_packet_clone(), av_packet_free().
//            They match the AVFrame functions with the same name.
#define LAVF_DEP_AV_COPY_PACKET             (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 8, 0))

// Check for FFMPEG version 3+
#define FFMPEG_VERSION3                     (LIBAVCODEC_VERSION_MAJOR > 56)

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

typedef enum _tagFILETYPE
{
    FILETYPE_UNKNOWN,
    FILETYPE_MP3,
    FILETYPE_MP4,
    FILETYPE_WAV,
    FILETYPE_OGG
} FILETYPE;

#include <sys/stat.h>

#ifdef __cplusplus
#include <string>

using namespace std;

const string & get_destname(string *destname, const string & filename);
string ffmpeg_geterror(int errnum);
double ffmpeg_cvttime(int64_t ts, const AVRational & time_base);

string format_number(int64_t value);
string format_bitrate(uint64_t value);
string format_samplerate(unsigned int value);
string format_time(time_t value);
string format_size(size_t value);
#endif

#ifdef __cplusplus
extern "C" {
#endif
void ffmpeg_libinfo(char * buffer, size_t maxsize);
int show_formats_devices(int device_only);
const char * get_codec_name(enum AVCodecID codec_id);
int supports_albumart(FILETYPE filetype);
FILETYPE get_filetype(const char * type);
const char * get_codecs(const char * type, FILETYPE * output_type, enum AVCodecID * audio_codecid, enum AVCodecID * video_codecid);

void format_number(char *output, size_t size, uint64_t value);
void format_bitrate(char *output, size_t size, uint64_t value);
void format_samplerate(char *output, size_t size, unsigned int value);
void format_time(char *output, size_t size, time_t value);
void format_size(char *output, size_t size, size_t value);

int print_info(AVStream* stream);
void exepath(char * path);

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
