/*
 * Copyright (C) 2017-2021 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * This file makes it possible to support FFmpeg 2.x to 4.
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2021 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef FFMPEG_COMPAT_H
#define FFMPEG_COMPAT_H

/**
 * FFmpeg compatibility layer: Keep support for older versions, but remove deprecated
 * functions when necessary.
 *
 * see doc/APIchanges
 */

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
 * 2021-03-17 - f7db77bd87 - lavc 58.133.100 - codec.h
 *   Deprecated av_init_packet(). Once removed, sizeof(AVPacket) will
 *   no longer be a part of the public ABI.
 *   Deprecated AVPacketList.
 *
 * Note from libacodec/packet.h:
 *
 * sizeof(AVPacket) being a part of the public ABI is deprecated. once
 * av_init_packet() is removed, new packets will only be able to be allocated
 * with av_packet_alloc(), and new fields may be added to the end of the struct
 * with a minor bump.
 *
 *
 * see av_packet_alloc
 * see av_packet_ref
 * see av_packet_unref
 */
#define LAVC_DEP_AV_INIT_PACKET             (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 133, 0))
/**
 * This is big fun... Of course making these const is a good idea, but nevertheless
 * a PITA to keep the code working with newer AND older versions...
 *
 * 2021-04-27 - cb3ac722f4 - lavc 59.0.100 - avcodec.h
 *   Constified AVCodecParserContext.parser.
 *
 * 2021-04-27 - 8b3e6ce5f4 - lavd 59.0.100 - avdevice.h
 *   The av_*_device_next API functions now accept and return
 *   pointers to const AVInputFormat resp. AVOutputFormat.

 * 2021-04-27 - d7e0d428fa - lavd 59.0.100 - avdevice.h
 *   avdevice_list_input_sources and avdevice_list_output_sinks now accept
 *   pointers to const AVInputFormat resp. const AVOutputFormat.
 *
 * 2021-04-27 - 46dac8cf3d - lavf 59.0.100 - avformat.h
 *   av_find_best_stream now uses a const AVCodec ** parameter
 *   for the returned decoder.
 *
 * 2021-04-27 - 626535f6a1 - lavc 59.0.100 - codec.h
 *   avcodec_find_encoder_by_name(), avcodec_find_encoder(),
 *   avcodec_find_decoder_by_name() and avcodec_find_decoder()
 *   now return a pointer to const AVCodec.
 *
 * 2021-04-27 - 14fa0a4efb - lavf 59.0.100 - avformat.h
 *   Constified AVFormatContext.*_codec.
 *
 * 2021-04-27 - 56450a0ee4 - lavf 59.0.100 - avformat.h
 *   Constified the pointers to AVInputFormats and AVOutputFormats
 *   in AVFormatContext, avformat_alloc_output_context2(),
 *   av_find_input_format(), av_probe_input_format(),
 *   av_probe_input_format2(), av_probe_input_format3(),
 *   av_probe_input_buffer2(), av_probe_input_buffer(),
 *   avformat_open_input(), av_guess_format() and av_guess_codec().
 *   Furthermore, constified the AVProbeData in av_probe_input_format(),
 *   av_probe_input_format2() and av_probe_input_format3().
*/
#define IF_DECLARED_CONST                   (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 0, 0))

/**
 * Check for FFMPEG version 3+
 */
#define FFMPEG_VERSION3                     (LIBAVCODEC_VERSION_MAJOR > 56)

#endif // FFMPEG_COMPAT_H
