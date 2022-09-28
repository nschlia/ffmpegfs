/*
 * Copyright (C) 2017-2022 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file ffmpeg_compat.h
 * @brief FFmpeg API compatibility
 *
 * This file makes it possible to support FFmpeg 2.x to 4.
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef FFMPEG_COMPAT_H
#define FFMPEG_COMPAT_H

#pragma once

/**
 * FFmpeg compatibility layer: Maintain support for older versions while removing
 * deprecated functions as needed.
 *
 * See doc/APIchanges
 */

// FFmpeg 4.1.8 "al-Khwarizmi" or newer:
//
// libavutil (>= 56.22.100)
// libavcodec (>= 58.35.100)
// libavformat (>= 58.20.100)
// libavfilter (>= 7.40.101)
// libswscale (>= 5.3.100)
// libswresample (>= 3.3.100)

/**
  * 2022-07-xx - xxxxxxxxxx - lavu 57.30.100 - frame.h
  *   Add AVFrame.duration, deprecate AVFrame.pkt_duration.
  */
#define LAVU_DEP_PKT_DURATION               (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 30, 0))

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
 *
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
  * 2022-03-15 - cdba98bb80 - lavu 57.24.100 - channel_layout.h frame.h opt.h
  * Add new channel layout API based on the AVChannelLayout struct.
  * Add support for Ambisonic audio.
  * Deprecate previous channel layout API based on uint64 bitmasks.
*/
#define LAVU_DEP_OLD_CHANNEL_LAYOUT         (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 0))
/**
  * 2022-03-15 - cdba98bb80 - swr 4.5.100 - swresample.h
  * Add swr_alloc_set_opts2() and swr_build_matrix2().
  * Deprecate swr_alloc_set_opts() and swr_build_matrix().
*/
#define SWR_DEP_ALLOC_SET_OPTS              (LIBSWRESAMPLE_VERSION_INT >= AV_VERSION_INT(4, 5, 0))

#endif // FFMPEG_COMPAT_H
