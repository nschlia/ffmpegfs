/*
 * Copyright (C) 2017-2019 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file
 * @brief FFmpeg API compatibility
 *
 * This file makes it possible to support FFmpeg 2.x to 4 and Libav.
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef FFMPEG_COMPAT_H
#define FFMPEG_COMPAT_H

/**
 * 2018-01-xx - xxxxxxx - lavf 58.7.100 - avformat.h @n
 *   Deprecate AVFormatContext filename field which had limited length, use the
 *   new dynamically allocated url field instead.
 */
#define LAVF_DEP_FILENAME                   (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 7, 0))
/**
 * 2016-04-21 - 7fc329e - lavc 57.37.100 - avcodec.h @n
 *   Add a new audio/video encoding and decoding API with decoupled input
 *   and output -- avcodec_send_packet(), avcodec_receive_frame(),
 *   avcodec_send_frame() and avcodec_receive_packet().
 */
#define LAVC_NEW_PACKET_INTERFACE           (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 0))
/**
 * 2016-04-11 - 6f69f7a / 9200514 - lavf 57.33.100 / 57.5.0 - avformat.h @n
 *   Add AVStream.codecpar, deprecate AVStream.codec.
 */
#define LAVF_DEP_AVSTREAM_CODEC             (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 33, 0))
/**
 * 2018-xx-xx - xxxxxxx - lavf 58.9.100 - avformat.h @n
 *   Deprecate use of av_register_input_format(), av_register_output_format(),
 *   av_register_all(), av_iformat_next(), av_oformat_next().
 */
#define LAVF_DEP_AV_REGISTER                (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 9, 0))
/**
 * 2018-xx-xx - xxxxxxx - lavc 58.10.100 - avcodec.h @n
 *   Deprecate use of avcodec_register(), avcodec_register_all(),
 *   av_codec_next(), av_register_codec_parser(), and av_parser_next().
 *   Add av_codec_iterate() and av_parser_iterate().
 */
#define LAVC_DEP_AV_CODEC_REGISTER          (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 10, 0))
/**
 * 2018-04-01 - f1805d160d - lavfi 7.14.100 - avfilter.h @n
 *   Deprecate use of avfilter_register(), avfilter_register_all(),
 *   avfilter_next(). Add av_filter_iterate().
 */
#define LAVC_DEP_AV_FILTER_REGISTER         (LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(7, 14, 0))
/**
 * 2015-10-29 - lavc 57.12.100 / 57.8.0 - avcodec.h @n
 *   xxxxxx - Deprecate av_free_packet(). Use av_packet_unref() as replacement,
 *            it resets the packet in a more consistent way.
 *   xxxxxx - Deprecate av_dup_packet(), it is a no-op for most cases. @n
 *            Use av_packet_ref() to make a non-refcounted AVPacket refcounted.
 *   xxxxxx - Add av_packet_alloc(), av_packet_clone(), av_packet_free(). @n
 *            They match the AVFrame functions with the same name.
 */
#define LAVF_DEP_AV_COPY_PACKET             (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 8, 0))
/**
 * 2017-03-29 - bfdcdd6d82 - lavu 55.52.100 - avutil.h @n
 *   add av_fourcc_make_string() function and av_fourcc2str() macro to replace
 *   av_get_codec_tag_string() from lavc.
 */
#define LAVU_DEP_AV_GET_CODEC_TAG           (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 52, 0))

/**
 * Check for FFMPEG version 3+
 */
#define FFMPEG_VERSION3                     (LIBAVCODEC_VERSION_MAJOR > 56)

#endif // FFMPEG_COMPAT_H
