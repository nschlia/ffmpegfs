/*
 * Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file ffmpeg_base.h
 * @brief FFmpeg transcoder base
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef FFMPEG_BASE_H
#define FFMPEG_BASE_H

#pragma once

#include "ffmpeg_utils.h"

struct VIRTUALFILE;
struct AVCodecContext;

/**
 * @brief The #FFmpeg_Base class
 */
class FFmpeg_Base
{
public:
    /**
     * @brief Construct FFmpeg_Base object.
     */
    explicit FFmpeg_Base();
    /**
     * @brief Destruct FFmpeg_Base object.
     */
    virtual ~FFmpeg_Base();

protected:
#if !LAVC_DEP_AV_INIT_PACKET
    /**
     * @brief Initialise one data packet for reading or writing.
     * @param[in] pkt - Packet to be initialised
     */
    void                init_packet(AVPacket *pkt) const;
#endif // !LAVC_DEP_AV_INIT_PACKET
    /**
     * @brief Allocate a subtitle
     * @return Returns a newly allocated AVSubtitle field, or nullptr if out of memory.
     */
    AVSubtitle*         alloc_subtitle() const;
    /**
     * @brief Set up video stream
     * @param[in] output_codec_ctx - Output codec context.
     * @param[in] output_stream - Output stream object.
     * @param[in] input_codec_ctx - Input codec context.
     * @param[in] framerate - Frame rate of input stream.
     * @param[in] enc_hw_pix_fmt - Forcibly set destination pixel format. Set to AV_PIX_FMT_NONE for automatic selection.
     */
    void                video_stream_setup(AVCodecContext *output_codec_ctx, AVStream* output_stream, AVCodecContext *input_codec_ctx, AVRational framerate, AVPixelFormat enc_hw_pix_fmt) const;
    /**
     * @brief Call av_dict_set and check result code. Displays an error message if appropriate.
     * @param[in] pm - pointer to a pointer to a dictionary struct.
     * @param[in] key - entry key to add to *pm.
     * @param[in] value - entry value to add to *pm.
     * @param[in] flags - AV_DICT_* flags.
     * @param[in] filename - Filename this frame is created for. Used for logging only, may be nullptr.
     * @param[in] nodelete - If true, does not delete tag if value is 0.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                 dict_set_with_check(AVDictionary **pm, const char *key, const char *value, int flags, const char *filename = nullptr, bool nodelete = false) const;
    /**
     * @brief Call av_dict_set and check result code. Displays an error message if appropriate.
     * @param[in] pm - pointer to a pointer to a dictionary struct.
     * @param[in] key - entry key to add to *pm.
     * @param[in] value - entry value to add to *pm.
     * @param[in] flags - AV_DICT_* flags.
     * @param[in] filename - Filename this frame is created for. Used for logging only, may be nullptr.
     * @param[in] nodelete - If true, does not delete tag if value is 0.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                 dict_set_with_check(AVDictionary **pm, const char *key, int64_t value, int flags, const char *filename = nullptr, bool nodelete = false) const;
    /**
     * @brief Call av_opt_set and check result code. Displays an error message if appropriate.
     * @param[in] obj - A struct whose first element is a pointer to an AVClass.
     * @param[in] key - the name of the field to set
     * @param[in] value - The value to set.
     * @param[in] flags - flags passed to av_opt_find2.
     * @param[in] filename - Filename this frame is created for. Used for logging only, may be nullptr.
     * @return On success returns 0; on error negative AVERROR.
     */
    int                 opt_set_with_check(void *obj, const char *key, const char *value, int flags, const char *filename = nullptr) const;
    /**
     * @brief Print info of video stream to log.
     * @param[in] out_file - true if file is output.
     * @param[in] format_ctx - AVFormatContext belonging to stream.
     * @param[in] stream - Stream to show information for.
     */
    void                video_info(bool out_file, const AVFormatContext *format_ctx, const AVStream *stream) const;
    /**
     * @brief Print info of audio stream to log.
     * @param[in] out_file - true if file is output.
     * @param[in] format_ctx - AVFormatContext belonging to stream.
     * @param[in] stream - Stream to show information for.
     */
    void                audio_info(bool out_file, const AVFormatContext *format_ctx, const AVStream *stream) const;
    /**
     * @brief Print info of subtitle stream to log.
     * @param[in] out_file - true if file is output.
     * @param[in] format_ctx - AVFormatContext belonging to stream.
     * @param[in] stream - Stream to show information for.
     */
    void                subtitle_info(bool out_file, const AVFormatContext *format_ctx, const AVStream *stream) const;
    /**
     * @brief Calls av_get_pix_fmt_name and returns a std::string with the pix format name.
     * @param[in] pix_fmt - AVPixelFormat enum to convert.
     * @return Returns a std::string with the pix format name.
     */
    static std::string  get_pix_fmt_name(AVPixelFormat pix_fmt);
    /**
     * @brief Calls av_get_sample_fmt_name and returns a std::string with the format name.
     * @param[in] sample_fmt - AVSampleFormat enum to convert.
     * @return Returns a std::string with the format name.
     */
    static std::string  get_sample_fmt_name(AVSampleFormat sample_fmt);
#if LAVU_DEP_OLD_CHANNEL_LAYOUT
    /**
     * @brief Calls av_channel_layout_describe and returns a std::string with the channel layout.
     * @param[in] ch_layout - Channel layout
     * @return Returns a std::string with the channel layout.
     */
    static std::string get_channel_layout_name(const AVChannelLayout * ch_layout);
#else   // !LAVU_DEP_OLD_CHANNEL_LAYOUT
    /**
     * @brief Calls av_get_channel_layout_string and returns a std::string with the channel layout.
     * @param[in] nb_channels - Number of channels.
     * @param[in] channel_layout - Channel layout index.
     * @return Returns a std::string with the channel layout.
     */
    static std::string  get_channel_layout_name(int nb_channels, uint64_t channel_layout);
#endif  // !LAVU_DEP_OLD_CHANNEL_LAYOUT

    /**
     * @brief Return source filename. Must be implemented in child class.
     * @return Returns filename.
     */
    virtual const char *filename() const = 0;
    /**
     * @brief Return destination filename. Must be implemented in child class.
     * @return Returns filename.
     */
    virtual const char *destname() const = 0;
    /**
     * @brief Convert PTS value to frame number.
     * @param[in] stream - Source video stream.
     * @param[in] pts - PTS of current frame in stream's time_base units.
     * @return Returns frame number.
     */
    uint32_t            pts_to_frame(AVStream* stream, int64_t pts) const;
    /**
     * @brief FrameToPts
     * @param[in] stream - Source video stream.
     * @param[in] frame_no - Number of frame.
     * @return Returns PTS of frame in stream's time_base units.
     */
    int64_t              frame_to_pts(AVStream* stream, uint32_t frame_no) const;
    /**
     * @brief Get number of channels from AVCodecParameters
     * @param[in] codecpar - AVCodecParameters to check
     * @return Returns number of channels
     */
    int                 get_channels(const AVCodecParameters *codecpar) const;
    /**
     * @brief Set number of channels from AVCodecParameters
     * @param[inout] codecpar_out - AVCodecParameters to set
     * @param[in] codecpar_in - AVCodecParameters to get channels from
     */
    void                set_channels(AVCodecParameters *codecpar_out, const AVCodecParameters *codecpar_in) const;
    /**
     * @brief Get number of channels from AVCodecContext
     * @param[in] codec_ctx - AVCodecContext to check
     * @return Returns number of channels
     */
    int                 get_channels(const AVCodecContext *codec_ctx) const;
    /**
     * @brief Set number of channels from AVCodecContext
     * @param[inout] codec_ctx_out - AVCodecContext to set channels for
     * @param[in] codec_ctx_in - AVCodecContext to copy channels from
     */
    void                set_channels(AVCodecContext *codec_ctx_out, const AVCodecContext *codec_ctx_in) const;
    /**
     * @brief Set number of channels from AVCodecContext
     * @param[inout] codec_ctx_out - AVCodecContext to set
     * @param[in] channels - Number of channels to set
     */
    void                set_channels(AVCodecContext *codec_ctx_out, int channels) const;

protected:
    VIRTUALFILE *       m_virtualfile;            /**< @brief Underlying virtual file object */
};

#endif // FFMPEG_BASE_H

