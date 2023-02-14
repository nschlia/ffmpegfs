/*
 * Copyright (C) 2017-2023 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file ffmpeg_utils.h
 * @brief Various FFmpegfs utility functions
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2023 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef FFMPEG_UTILS_H
#define FFMPEG_UTILS_H

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if !defined(EXTRA_VERSION)
#define FFMPEFS_VERSION     PACKAGE_VERSION                     /**< @brief FFmpegfs version number */
#else
#define FFMPEFS_VERSION     PACKAGE_VERSION "-" EXTRA_VERSION   /**< @brief FFmpegfs version number */
#endif

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1                                  /**< @brief Force PRId64 defines */
#endif // !__STDC_FORMAT_MACROS

#include "ffmpeg_compat.h"

#include <string>
#include <vector>
#include <regex>
#include <set>
#include <array>
#include <iterator>

#ifndef PATH_MAX
#include <climits>
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Disable annoying warnings outside our code
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <libavformat/avformat.h>
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

#define INVALID_STREAM  -1              /**< @brief Denote an invalid stream */

#ifndef LIBAVUTIL_VERSION_MICRO
#error "LIBAVUTIL_VERSION_MICRO not defined. Missing include header?"
#endif

/**
 * Allow use of av_format_inject_global_side_data when available
 */
#define HAVE_AV_FORMAT_INJECT_GLOBAL_SIDE_DATA  (LIBAVFORMAT_VERSION_MICRO >= 100 && LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 64, 101))

/**
 * Add av_get_media_type_string function if missing
 */
#define HAVE_MEDIA_TYPE_STRING                  (LIBAVUTIL_VERSION_MICRO >= 100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 34, 101))
#if HAVE_MEDIA_TYPE_STRING
/**
 *  Map to av_get_media_type_string function.
 */
#define get_media_type_string                   av_get_media_type_string
#else
/**
 * @brief av_get_media_type_string is missing, so we provide our own.
 * @param[in] media_type - Media type to map.
 * @return Pointer to media type string.
 */
const char *get_media_type_string(enum AVMediaType media_type);
#endif

/**
 * Min. FFmpeg version for VULKAN hardware acceleration support
 * 2020-02-04 - xxxxxxxxxx - lavu 56.39.100 - hwcontext.h
 *   Add AV_PIX_FMT_VULKAN
 *   Add AV_HWDEVICE_TYPE_VULKAN and implementation.
 */
#define HAVE_VULKAN_HWACCEL                     (LIBAVUTIL_VERSION_MICRO >= 100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 39, 100))

#ifndef AV_ROUND_PASS_MINMAX
/**
 *  Ignore if this is missing
 */
#define AV_ROUND_PASS_MINMAX            	0
#endif

// These once had a different name
#if !defined(AV_CODEC_CAP_DELAY) && defined(CODEC_CAP_DELAY)
#define AV_CODEC_CAP_DELAY                      CODEC_CAP_DELAY                         /**< @brief AV_CODEC_CAP_DELAY is missing in older FFmpeg versions */
#endif

#if !defined(AV_CODEC_CAP_TRUNCATED) && defined(CODEC_CAP_TRUNCATED)
#define AV_CODEC_CAP_TRUNCATED                  CODEC_CAP_TRUNCATED                     /**< @brief AV_CODEC_CAP_TRUNCATED is missing in older FFmpeg versions */
#endif

#if !defined(AV_CODEC_FLAG_TRUNCATED) && defined(CODEC_FLAG_TRUNCATED)
#define AV_CODEC_FLAG_TRUNCATED                 CODEC_FLAG_TRUNCATED                    /**< @brief AV_CODEC_FLAG_TRUNCATED is missing in older FFmpeg versions */
#endif

#ifndef AV_CODEC_FLAG_GLOBAL_HEADER
#define AV_CODEC_FLAG_GLOBAL_HEADER             CODEC_FLAG_GLOBAL_HEADER                /**< @brief AV_CODEC_FLAG_GLOBAL_HEADER is missing in older FFmpeg versions */
#endif

#ifndef FF_INPUT_BUFFER_PADDING_SIZE
#define FF_INPUT_BUFFER_PADDING_SIZE            256                                     /**< @brief FF_INPUT_BUFFER_PADDING_SIZE is missing in newer FFmpeg versions */
#endif

#ifndef AV_CODEC_CAP_VARIABLE_FRAME_SIZE
#define AV_CODEC_CAP_VARIABLE_FRAME_SIZE        CODEC_CAP_VARIABLE_FRAME_SIZE           /**< @brief AV_CODEC_CAP_VARIABLE_FRAME_SIZE is missing in older FFmpeg versions */
#endif

#if (LIBAVUTIL_VERSION_MAJOR > 54)
#define BITRATE int64_t                                                             /**< @brief For FFmpeg bit rate is an int64. */
#else
#define BITRATE int                                                                 /**< @brief For FFmpeg bit rate is an int. */
#endif

#define SAFE_VALUE(p, v, d)     (((p) != nullptr) ? (p)->v : d)                     /**< @brief Access struct/class pointer safely, return default if nullptr */

/**
  * File types
  */
typedef enum FILETYPE
{
    FILETYPE_UNKNOWN,
    FILETYPE_MP3,
    FILETYPE_MP4,
    FILETYPE_WAV,
    FILETYPE_OGG,
    FILETYPE_WEBM,
    FILETYPE_MOV,
    FILETYPE_AIFF,
    FILETYPE_OPUS,
    FILETYPE_PRORES,
    FILETYPE_ALAC,
    FILETYPE_PNG,
    FILETYPE_JPG,
    FILETYPE_BMP,
    FILETYPE_TS,
    FILETYPE_HLS,
    FILETYPE_FLAC,
    FILETYPE_MKV,
} FILETYPE;

/**
  * MP4/MOV/ALAC profiles
  */
typedef enum PROFILE
{
    PROFILE_INVALID = -1,                       /**< @brief Profile is invalid */

    PROFILE_DEFAULT = 0,                        /**< @brief No specific profile/Don't care */

    // MP4
    PROFILE_MP4_FF = 1,                         /**< @brief Firefox */
    PROFILE_MP4_EDGE,                           /**< @brief MS Edge */
    PROFILE_MP4_IE,                             /**< @brief MS Internet Explorer */
    PROFILE_MP4_CHROME,                         /**< @brief Google Chrome */
    PROFILE_MP4_SAFARI,                         /**< @brief Apple Safari */
    PROFILE_MP4_OPERA,                          /**< @brief Opera */
    PROFILE_MP4_MAXTHON,                        /**< @brief Maxthon */

    // HLS/ts
    PROFILE_HLS_DEFAULT = PROFILE_DEFAULT,      /**< @brief HLS/ts uses no profile */
    // mov
    PROFILE_MOV_DEFAULT = PROFILE_DEFAULT,      /**< @brief MOV uses no profile */
    // MOV/ProRes
    PROFILE_PRORES_DEFAULT = PROFILE_DEFAULT,   /**< @brief MOV/ProRes uses no profile */
    // MOV/ProRes
    PROFILE_ALAC_DEFAULT = PROFILE_DEFAULT,     /**< @brief MOV/ALAC uses no profile */
    // WebM
    PROFILE_WEBM_DEFAULT = PROFILE_DEFAULT,     /**< @brief WebM uses no profile */

} PROFILE;

/**
  * Prores levels
  */
typedef enum PRORESLEVEL
{
    PRORESLEVEL_NONE = -1,          /**< @brief No level */
    // Prores profiles
    PRORESLEVEL_PRORES_PROXY = 0,   /**< @brief Prores Level: PROXY */
    PRORESLEVEL_PRORES_LT,          /**< @brief Prores Level: LT */
    PRORESLEVEL_PRORES_STANDARD,    /**< @brief Prores Level: STANDARD */
    PRORESLEVEL_PRORES_HQ,          /**< @brief Prores Level: HQ */
} PRORESLEVEL;

/**
  * Auto copy options
  */
typedef enum AUTOCOPY
{
    AUTOCOPY_OFF = 0,      /**< @brief Never copy streams, transcode always. */
    AUTOCOPY_MATCH,        /**< @brief Copy stream if target supports codec. */
    AUTOCOPY_MATCHLIMIT,   /**< @brief Same as MATCH, only copy if target not larger transcode otherwise. */
    AUTOCOPY_STRICT,       /**< @brief Copy stream if codec matches desired target, transcode otherwise. */
    AUTOCOPY_STRICTLIMIT,  /**< @brief Same as STRICT, only copy if target not larger, transcode otherwise. */
} AUTOCOPY;

/**
  * Recode to same format options
  */
typedef enum RECODESAME
{
    RECODESAME_NO = 0,     /**< @brief Never recode to same format. */
    RECODESAME_YES,        /**< @brief Always recode to same format. */
} RECODESAME;

/**
  * List of sample formats.
  * User selection, we don't care about planar or interleaved.
  */
typedef enum SAMPLE_FMT
{
    SAMPLE_FMT_DONTCARE = -1,   /**< @brief Don't care, leave to FFmpegfs to choose */
    SAMPLE_FMT_8,               /**< @brief 8 bit integer */
    SAMPLE_FMT_16,              /**< @brief 16 bit integer */
    SAMPLE_FMT_24,              /**< @brief 24 bit integer */
    SAMPLE_FMT_32,              /**< @brief 32 bit integer */
    SAMPLE_FMT_64,              /**< @brief 64 bit integer */
    SAMPLE_FMT_F16,             /**< @brief 16 bit floating point */
    SAMPLE_FMT_F24,             /**< @brief 24 bit floating point */
    SAMPLE_FMT_F32,             /**< @brief 32 bit floating point */
    SAMPLE_FMT_F64              /**< @brief 64 bit floating point */
} SAMPLE_FMT;

/**
 * Format options: Defines file extension, codecs etc.
 * for each format.
 */
struct Format_Options
{
    friend class FFmpegfs_Format;

    typedef std::vector<AVCodecID>  CODEC_VECT; /**< @brief Vector with valid codec ids for file format */

    /**
      * Format options: Audio/video codecs and sample format
      */
    typedef struct _tagFORMAT
    {
        CODEC_VECT      m_video_codec;          /**< @brief AVCodec used for video encoding */
        CODEC_VECT      m_audio_codec;          /**< @brief AVCodec used for audio encoding */
        CODEC_VECT      m_subtitle_codec;       /**< @brief AVCodec used for subtitle encoding */
        AVSampleFormat  m_sample_format;        /**< @brief AVSampleFormat for audio encoding, may be AV_SAMPLE_FMT_NONE for "don't care" */
    } FORMAT;

    typedef std::map<SAMPLE_FMT, const FORMAT> FORMAT_MAP;   /**< @brief Map of formats. One entry per format derivative. */

public:
    /**
     * @brief Construct Format_Options object with defaults (empty)
     */
    Format_Options();

    /**
     * @brief Construct Format_Options object
     * @param[in] format_name - Descriptive name of the format, e.g. "Opus Audio",
     * @param[in] fileext - File extension: mp4, mp3, flac or other
     * @param[in] format - Format options: Possible audio/video codecs and sample formats
     * @param[in] albumart_supported - true if album arts are supported (eg. mp3) or false if not (e.g. wav, aiff
     */
    Format_Options(std::string format_name,
            std::string fileext,
            FORMAT_MAP format,
            bool albumart_supported
            );

    /**
     * @brief Convert destination type to "real" type, i.e., the file extension to be used.
     * @note Currently "prores" is mapped to "mov".
     * @return Destination type
     */
    const std::string & format_name() const;

    /**
     * @brief Get file extension
     * @return File extension
     */
    const std::string & fileext() const;

    /**
     * @brief Get video codec_id
     * @return Returns video codec_id
     */
    AVCodecID           video_codec() const;
    /**
     * @brief Check if video codec/file format combination is supported
     * @param[in] codec_id - Codec ID to check
     * @return Returns true if supported, false if not.
     */
    bool                is_video_codec_supported(AVCodecID codec_id) const;
    /**
     * @brief Create a list of supported audio codecs for current audio codec
     * @return Returns comma separated list of formats, or empty if not available.
     */
    std::string         video_codec_list() const;

    /**
     * @brief Get audio codec_id
     * @return Returns audio codec_id
     */
    AVCodecID           audio_codec() const;
    /**
     * @brief Check if audio codec/file format combination is supported
     * @param[in] codec_id - Codec ID to check
     * @return Returns true if supported, false if not.
     */
    bool                is_audio_codec_supported(AVCodecID codec_id) const;
    /**
     * @brief Create a list of supported audio codecs for current audio codec
     * @return Returns comma separated list of formats, or empty if not available.
     */
    std::string         audio_codec_list() const;

    /**
     * @brief Get sample format (bit width)
     * @return Returns sample format
     */
    AVSampleFormat      sample_format() const;
    /**
     * @brief Check if audio codec/sample format combination is supported
     * @return Returns true if supported, false if not.
     */
    bool                is_sample_fmt_supported() const;
    /**
     * @brief Create a list of supported sample formats for current audio codec
     * @return Returns comma separated list of formats, or empty if not available.
     */
    std::string         sample_fmt_list() const;

    /**
     * @brief Get subtitle codec_id
     * @param[in] codec_id - Input stream codec ID
     * @return Returns subtitle codec_id that matches the input stream codec ID, or AV_CODEC_ID_NONE of no match.
     */
    AVCodecID           subtitle_codec(AVCodecID codec_id) const;

protected:
    /**
     *  @brief Descriptive name of the format.
     *  Descriptive name of the format, e.g. "opus", "mpegts".
     *  Please note that m_format_name is used to select the FFmpeg container
     *  by passing it to avformat_alloc_output_context2().
     *  Mostly, but not always, same as m_fileext.
     */
    std::string     m_format_name;
    std::string     m_fileext;              /**< @brief File extension: mp4, mp3, flac or other. Mostly, but not always, same as m_format_name. */
    FORMAT_MAP      m_format_map;           /**< @brief Format definition (audio/videocodec, sample format) */
    bool            m_albumart_supported;   /**< @brief true if album arts are supported (eg. mp3) or false if not (e.g. wav, aiff) */
};

/**
 * @brief The #FFmpegfs_Format class
 */
class FFmpegfs_Format
{
    typedef std::map<FILETYPE, const Format_Options> OPTIONS_MAP;   /**< @brief Map of options. One entry per supported destination type. */

public:
    /**
     * @brief Construct FFmpegfs_Format object
     */
    FFmpegfs_Format();

    /**
     * @brief Get codecs for the selected destination type.
     * @param[in] desttype - Destination type (MP4, WEBM etc.).
     * @return Returns true if format was found; false if not.
     */
    bool                init(const std::string & desttype);
    /**
     * @brief Convert destination type to "real" type, i.e., the file extension to be used.
     * @note Currently "prores" is mapped to "mov".
     * @return Destination type
     */
    const std::string & format_name() const;
    /**
     * @brief Get destination type
     * @return Destination type
     */
    const std::string & desttype() const;
    /**
     * @brief Get file extension
     * @return File extension
     */
    const std::string & fileext() const;
    /**
     * @brief Get selected filetype.
     * @return Returns selected filetype.
     */
    FILETYPE            filetype() const;

    /**
     * @brief Get video codec_id
     * @return Returns video codec_id
     */
    AVCodecID           video_codec() const;
    /**
     * @brief Check if video codec/file format combination is supported
     * @param[in] codec_id - Codec ID to check
     * @return Returns true if supported, false if not.
     */
    bool                is_video_codec_supported(AVCodecID codec_id) const;
    /**
     * @brief Create a list of supported audio codecs for current audio codec
     * @return Returns comma separated list of formats, or empty if not available.
     */
    std::string         video_codec_list() const;

    /**
     * @brief Get audio codec_id
     * @return Returns audio codec_id
     */
    AVCodecID           audio_codec() const;
    /**
     * @brief Check if audio codec/file format combination is supported
     * @param[in] codec_id - Codec ID to check
     * @return Returns true if supported, false if not.
     */
    bool                is_audio_codec_supported(AVCodecID codec_id) const;
    /**
     * @brief Create a list of supported audio codecs for current audio codec
     * @return Returns comma separated list of formats, or empty if not available.
     */
    std::string         audio_codec_list() const;

    /**
     * @brief Get sample format (bit width)
     * @return Returns sample format
     */
    AVSampleFormat      sample_format() const;
    /**
     * @brief Check if audio codec/sample format combination is supported
     * @return Returns true if supported, false if not.
     */
    bool                is_sample_fmt_supported() const;
    /**
     * @brief Create a list of supported sample formats for current audio codec
     * @return Returns comma separated list of formats, or empty if not available.
     */
    std::string         sample_fmt_list() const;

    /**
     * @brief Get subtitle codec_id
     * @param[in] codec_id - Input stream codec ID
     * @return Returns subtitle codec_id that matches the input stream codec ID, or AV_CODEC_ID_NONE of no match.
     */
    AVCodecID           subtitle_codec(AVCodecID codec_id) const;

    /**
     * @brief Check if this is some sort of multi file format
     * (any of the following: is_frameset() or is_hls()).
     * @return Returns true for a multi file format.
     */
    bool                is_multiformat() const;
    /**
     * @brief Check for an export frame format
     * @return Returns true for formats that export all frames as images.
     */
    bool                is_frameset() const;
    /**
     * @brief Check for HLS format
     * @return Returns true for formats that create an HLS set including the m3u file.
     */
    bool                is_hls() const;
    /**
     * @brief Check if album arts are supported
     * @return true if album arts are supported or false if not
     */
    bool                albumart_supported() const;

protected:
    const Format_Options        m_empty_options;    /**< @brief Set of empty (invalid) options as default */
    const Format_Options *      m_cur_opts;         /**< @brief Currently selected options. Will never be nullptr */
    static const OPTIONS_MAP    m_options_map;      /**< @brief Map of options. One entry per supported destination type. */
    std::string                 m_desttype;         /**< @brief Destination type: mp4, mp3 or other */
    FILETYPE                    m_filetype;         /**< @brief File type, MP3, MP4, OPUS etc. */
};

typedef std::array<FFmpegfs_Format, 2> FFMPEGFS_FORMAT_ARR;         /**< @brief Array of FFmpegfs formats. There are two, for audio and video */

/**
 * @brief Add / to the path if required
 * @param[in] path - Path to add separator to.
 * @return Returns constant reference to path.
 */
const std::string & append_sep(std::string * path);
/**
 * @brief Add filename to path, including / after the path if required
 * @param[in] path - Path to add filename to.
 * @param[in] filename - File name to add.
 * @return Returns constant reference to path.
 */
const std::string & append_filename(std::string * path, const std::string & filename);
/**
 * @brief Remove / from the path
 * @param[in] path - Path to remove separator from.
 * @return Returns constant reference to path.
 */
const std::string & remove_sep(std::string * path);
/**
 * @brief Remove filename from path. Handy dirname alternative.
 * @param[in] filepath - Path to remove filename from.
 * @return Returns constant reference to path.
 */
const std::string & remove_filename(std::string *filepath);
/**
 * @brief Remove path from filename. Handy basename alternative.
 * @param[in] filepath - Filename to remove path from.
 * @return Returns constant reference to filename.
 */
const std::string & remove_path(std::string *filepath);
/**
 * @brief Remove extension from filename.
 * @param[in] filepath - Filename to remove path from.
 * @return Returns constant reference to filename.
 */
const std::string & remove_ext(std::string *filepath);
/**
 * @brief Find extension in filename, if existing.
 * @param[in] ext - Extension, if found.
 * @param[in] filename - Filename to inspect.
 * @return Returns true if extension was found, false if there was none
 */
bool                find_ext(std::string * ext, const std::string & filename);
/**
 * @brief Check if filename has a certain extension. The check is case sensitive.
 * @param ext - Extension to check.
 * @param filename - Filename to check
 * @return Returns true if extension matches, false if not
 */
bool                check_ext(const std::string & ext, const std::string & filename);
/**
 * @brief Replace extension in filename, taking into account that there might not be an extension already.
 * @param[in] filepath - Filename to replace extension.
 * @param[in] ext - Extension to replace.
 * @return Returns constant reference to filename.
 */
const std::string & replace_ext(std::string * filepath, const std::string & ext);
/**
 * @brief Append extension to filename. If ext is the same as
 * @param[in] filepath - Filename to add extension to.
 * @param[in] ext - Extension to add.
 * @return Returns constant reference to filename.
 */
const std::string & append_ext(std::string * filepath, const std::string & ext);
/**
 * @brief strdup() variant taking a std::string as input.
 * @param[in] str - String to duplicate.
 * @return Copy of the input string. Remember to delete the allocated memory.
 */
char *              new_strdup(const std::string & str);
/**
 * @brief Get FFmpeg error string for errnum. Internally calls av_strerror().
 * @param[in] errnum - FFmpeg error code.
 * @return Returns std::string with the error defined by errnum.
 */
std::string         ffmpeg_geterror(int errnum);

/**
 * @brief Convert a FFmpeg time from in timebase to outtime base.
 *
 * If out time base is omitted, returns standard AV_TIME_BASE fractional seconds
 * Avoids conversion of AV_NOPTS_VALUE.
 *
 * @param[in] ts - Time in input timebase.
 * @param[in] timebase_in - Input timebase.
 * @param[in] timebase_out - Output timebase, defaults to AV_TIMEBASE if unset.
 * @return Returns converted value, or AV_NOPTS_VALUE if ts is AV_NOPTS_VALUE.
 */
int64_t             ffmpeg_rescale_q(int64_t ts, const AVRational & timebase_in, const AVRational & timebase_out = av_get_time_base_q());
/**
 * @brief Convert a FFmpeg time from in timebase to out timebase with rounding.
 *
 * If out time base is omitted, returns standard AV_TIME_BASE fractional seconds
 * Avoids conversion of AV_NOPTS_VALUE.
 *
 * @param[in] ts - Time in input timebase.
 * @param[in] timebase_in - Input timebase.
 * @param[in] timebase_out - Output timebase, defaults to AV_TIMEBASE if unset.
 * @return Returns converted value, or AV_NOPTS_VALUE if ts is AV_NOPTS_VALUE.
 */
int64_t             ffmpeg_rescale_q_rnd(int64_t ts, const AVRational & timebase_in, const AVRational & timebase_out = av_get_time_base_q());
/**
 * @brief Format numeric value.
 * @param[in] value - Value to format.
 * @return Returns std::string with formatted value; if value == AV_NOPTS_VALUE returns "unset"; "unlimited" if value == 0.
 */
std::string         format_number(int64_t value);
/**
 * @brief Format a bit rate.
 * @param[in] value - Bit rate to format.
 * @return Returns std::string with formatted value in bit/s, kbit/s or Mbit/s. If value == AV_NOPTS_VALUE returns "unset".
 */
std::string         format_bitrate(BITRATE value);
/**
 * @brief Format a samplerate.
 * @param[in] value - Sample rate to format.
 * @return Returns std::string with formatted value in Hz or kHz. If value == AV_NOPTS_VALUE returns "unset".
 */
std::string         format_samplerate(int value);
/**
 * @brief Format a time in format HH:MM:SS.fract
 * @param[in] value - Time value in AV_TIME_BASE factional seconds.
 * @param[in] fracs - Fractional digits.
 * @return Returns std::string with formatted value. If value == AV_NOPTS_VALUE returns "unset".
 */
std::string         format_duration(int64_t value, uint32_t fracs = 3);
/**
 * @brief Format a time in format "w d m s".
 * @param[in] value - Time value in AV_TIME_BASE factional seconds.
 * @return Returns std::string with formatted value. If value == AV_NOPTS_VALUE returns "unset".
 */
std::string         format_time(time_t value);
/**
 * @brief Format size.
 * @param[in] value - Size to format.
 * @return Returns std::string with formatted value in bytes, KB, MB or TB; if value == AV_NOPTS_VALUE returns "unset"; "unlimited" if value == 0.
 */
std::string         format_size(uint64_t value);
/**
 * @brief Format size.
 * @param[in] value - Size to format.
 * @return Returns std::string with formatted value in bytes plus KB, MB or TB; if value == AV_NOPTS_VALUE returns "unset"; "unlimited" if value == 0.
 */
std::string         format_size_ex(uint64_t value);
/**
 * @brief Format size of transcoded file including difference between predicted and resulting size.
 * @param[in] size_resulting - Resulting size.
 * @param[in] size_predicted - Predicted size.
 * @return Returns std::string with formatted value in bytes plus difference; if value == AV_NOPTS_VALUE returns "unset"; "unlimited" if value == 0.
 */
std::string         format_result_size(size_t size_resulting, size_t size_predicted);
/**
 * @brief Format size of transcoded file including difference between predicted and resulting size.
 * @param[in] size_resulting - Resulting size.
 * @param[in] size_predicted - Predicted size.
 * @return Returns std::string with formatted value in bytes plus KB, MB or TB and difference; if value == AV_NOPTS_VALUE returns "unset"; "unlimited" if value == 0.
 */
std::string         format_result_size_ex(size_t size_resulting, size_t size_predicted);
/**
 * @brief Path to FFmpegfs binary.
 * @param[in] path - Path to FFmpegfs binary.
 */
void                exepath(std::string *path);
/**
 * @brief trim from start
 * @param[in] s - String to trim.
 * @return Reference to string s.
 */
std::string &       ltrim(std::string &s);
/**
 * @brief trim from end
 * @param[in] s - String to trim.
 * @return Reference to string s.
 */
std::string &       rtrim(std::string &s);
/**
 * @brief trim from both ends
 * @param[in] s - String to trim.
 * @return Reference to string s.
 */
std::string &       trim(std::string &s);
/**
 * @brief Same as std::string replace(), but replaces all occurrences.
 * @param[inout] str - Source string.
 * @param[in] from - String to replace.
 * @param[in] to - Replacement string.
 * @return Source string with all occurrences of from replaced with to.
 */
std::string         replace_all(std::string str, const std::string& from, const std::string& to);
/**
 * @brief Same as std::string replace(), but replaces string in-place.
 * @param[inout] str - Source string.
 * @param[in] from - String to replace.
 * @param[in] to - Replacement string.
 * @return Source string with all occurrences of from replaced with to.
 */
std::string         replace_all(std::string * str, const std::string& from, const std::string& to = "");
/**
 * @brief Replace start of string from "from" to "to".
 * @param[inout] str - Source string.
 * @param[in] from - String to replace.
 * @param[in] to - Replacement string.
 * @return True if from has been replaced, false if not.
 */
bool                replace_start(std::string *str, const std::string& from, const std::string& to = "");
/**
 * @brief Format a std::string sprintf-like.
 * @param[out] str - The pointer to std::string object where the resulting string is stored.
 * @param[in] format - sprintf-like format string.
 * @param[in] args - Arguments.
 * @return Returns the formatted string.
 */
template<typename ... Args>
const std::string & strsprintf(std::string *str, const std::string & format, Args ... args)
{
    size_t size = static_cast<size_t>(snprintf(nullptr, 0, format.c_str(), args ...)) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new(std::nothrow) char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    str->clear();
    str->insert(0, buf.get(), size - 1); // We don't want the '\0' inside
    return *str;
}
/**
 * @brief strcasecmp() equivalent for std::string.
 * @param[in] s1 - std:string #1
 * @param[in] s2 - std:string #2
 * @return Returns same as strcasecmp() for char *.
 */
int                 strcasecmp(const std::string & s1, const std::string & s2);
/**
 * @brief Convert string to upper case
 * @param[inout] input String to convert
 */
void                make_upper(std::string * input);
/**
 * @brief Convert string to lower case
 * @param[inout] input String to convert
 */
void                make_lower(std::string * input);
/**
 * @brief Get info about the FFmpeg libraries used.
 * @return std::tring with info about the linked FFmpeg libraries.
 */
std::string         ffmpeg_libinfo();
/**
 * @brief Lists all supported codecs and devices.
 * @param[in] device_only - If true lists devices only.
 * @return On success, returns 0; on error, a negative AVERROR value.
 */
int                 show_caps(int device_only);
/**
 * @brief Safe way to get the codec name. Function never fails, will return "unknown" on error.
 * @param[in] codec_id - ID of codec
 * @param[in] long_name - If true, gets the long name.
 * @return Returns the codec name or "unknown" on error.
 */
const char *        get_codec_name(AVCodecID codec_id, bool long_name);
/**
 * @brief Check if file type supports album arts.
 * @param[in] filetype - File type: MP3, MP4 etc.
 * @return Returns true if album arts are supported, false if not.
 */
int                 supports_albumart(FILETYPE filetype);
/**
 * @brief Get the FFmpegfs filetype, desttype must be one of FFmpeg's "official" short names for formats.
 * @param[in] desttype - Destination type (MP4, WEBM etc.).
 * @return On success, returns FILETYPE enum; On error, returns FILETYPE_UNKNOWN.
 */
FILETYPE            get_filetype(const std::string & desttype);
/**
* @brief Convert FILETYPE enum to human readable text.
 * @param[in] filetype - FILETYPE enum value to convert.
 * @return FILETYPE enum as text or "INVALID" if not known.
 */
std::string         get_filetype_text(FILETYPE filetype);
/**
 * @brief Get the FFmpegfs filetype, desttypelist must be a comma separated list of FFmpeg's "official" short names for formats.
 * Will return the first match. Same as get_filetype, but accepts a comma separated list.
 * @param[in] desttypelist - Destination type list (MP4, WEBM etc.) separated by commas.
 * @return On success, returns FILETYPE enum; On error, returns FILETYPE_UNKNOWN.
 */
FILETYPE            get_filetype_from_list(const std::string & desttypelist);
/**
 * @brief Print info about an AVStream.
 * @param[in] stream - Stream to print.
 * @return On success, returns 0; on error, a negative AVERROR value.
 */
int                 print_stream_info(const AVStream* stream);
/**
 * Fill the provided buffer with a string containing a FourCC (four-character
 * code) representation.
 *
 * @param[in] buf - Upon return, filled in with the FourCC representation.
 * @param[in] fourcc - The fourcc to represent
 * @return The buffer in input.
 */
std::string         fourcc_make_string(std::string * buf, uint32_t fourcc);
/**
 * @brief Compare value with pattern.
 * @param[in] value - Value to check.
 * @param[in] pattern - Regexp pattern to match.
 * @param[in] flag - On of the flag_type constants, see https://en.cppreference.com/w/cpp/regex/basic_regex for options. Mostly std::regex::icase is used to make matches case insensitive.
 * @return Returns 0 if pattern matches; 1 if not; -1 if pattern is no valid regex
 */
int                 reg_compare(const std::string &value, const std::string &pattern, std::regex::flag_type flag = std::regex::ECMAScript);
/**
 * @brief Expand path, e.g., expand ~/ to home directory.
 * @param[out] tgt - Expanded source path.
 * @param[in] src - Path to expand.
 * @return On success, returns expanded source path.
 */
const std::string & expand_path(std::string *tgt, const std::string &src);
/**
 * @brief Check if path is a mount.
 * @param[in] path - Path to check.
 * @return Returns 1 if path is a mount point; 0 if not. On error, returns -1. Check errorno for details.
 */
int                 is_mount(const std::string & path);
/**
 * @brief Make directory tree.
 * @param[in] path - Path to create
 * @param[in] mode - Directory mode, see mkdir() function.
 * @return On success, returns 0; on error, returns non-zero errno value.
 */
int                 mktree(const std::string & path, mode_t mode);
/**
 * @brief Get temporary directory.
 * @param[out] path - Path to temporary directory.
 */
void                tempdir(std::string & path);

/**
 * @brief Split string into an array delimited by a regular expression.
 * @param[in] input - Input string.
 * @param[in] regex - Regular expression to match.
 * @return Returns an array with separate elements.
 */
std::vector<std::string> split(const std::string& input, const std::string & regex);

/**
 * Safe countof() implementation: Retuns number of elements in an array.
 */
template <typename T, std::size_t N>
constexpr std::size_t countof(T const (&)[N]) noexcept
{
    return N;
}

/**
 * @brief Sanitise file name. Calls realpath() to remove duplicate // or resolve ../.. etc.
 * @param[in] filepath - File name and path to sanitise.
 * @return Returns sanitised file name and path.
 */
std::string         sanitise_filepath(const std::string & filepath);
/**
 * @brief Sanitise file name. Calls realpath() to remove duplicate // or resolve ../.. etc.
 * Changes the path in place.
 * @param[in] filepath - File name and path to sanitise.
 * @return Returns sanitised file name and path.
 */
std::string         sanitise_filepath(std::string * filepath);

/**
 * @brief Translate file names from FUSE to the original absolute path.
 * @param[out] origpath - Upon return, contains the name and path of the original file.
 * @param[in] path - Filename and relative path of the original file.
 */
void                 append_basepath(std::string *origpath, const char* path);

/**
 * @brief Minimal check if codec is an album art.
 * Requires frame_rate to decide whether this is a video stream if codec_id is
 * not BMP or PNG (which means its undoubtedly an album art). For MJPEG this may
 * also be a video stream if the frame rate is high enough.
 * @param[in] codec_id - ID of codec.
 * @param[in] frame_rate - Video frame rate, if known.
 * @return Returns true if codec is for an image; false if not.
 */
bool                is_album_art(AVCodecID codec_id, const AVRational *frame_rate = nullptr);

/**
 * @brief nocasecompare to make std::string find operations case insensitive
 * @param[in] lhs - left hand string
 * @param[in] rhs - right hand string
 * @return -1 if lhs < rhs; 0 if lhs == rhs and 1 if lhs > rhs
 */
int                 nocasecompare(const std::string & lhs, const std::string &rhs);

/**
 * @brief The comp struct to make std::string find operations case insensitive
 */
struct comp
{
    /**
     * @brief operator () to make std::string find operations case insensitive
     * @param[in] lhs - left hand string
     * @param[in] rhs - right hand string
     * @return true if lhs < rhs; false if lhs >= rhs
     */
    bool operator() (const std::string& lhs, const std::string& rhs) const
    {
        return (nocasecompare(lhs, rhs) < 0);
    }
};

/**
 * @brief Get free disk space.
 * @param[in] path - Path or file on disk.
 * @return Returns the free disk space.
 */
size_t              get_disk_free(std::string & path);

/**
 * @brief For use with win_smb_fix=1: Check if this an illegal access offset by Windows
 * @param[in] size - sizeof of the file
 * @param[in] offset - offset at which file is accessed
 * @return If request should be ignored, returns true; otherwise false
 */
bool                check_ignore(size_t size, size_t offset);

/**
 * @brief Make a file name from file number and file extension.
 * @param[in] file_no - File number 1...n
 * @param[in] fileext - Extension of file (e.g mp4, webm)
 * @return Returns the file name.
 */
std::string         make_filename(uint32_t file_no, const std::string &fileext);

/**
 * @brief Check if file exists.
 * @param[in] filename - File to check.
 * @return Returns true if file exists, false if not.
 */
bool                file_exists(const std::string & filename);

/** Save version of hwdevice_get_type_name:
 * Get the string name of an AVHWDeviceType.
 *
 * @param[in] dev_type - Type from enum AVHWDeviceType.
 * @return Pointer to a static string containing the name, or "unknown" if the type
 *         is not valid.
 */
const char *        hwdevice_get_type_name(AVHWDeviceType dev_type);

/**
  * Detected encoding types
  */
typedef enum ENCODING
{
    ENCODING_ASCII          = -1,       /**< @brief Some sort of ASCII encoding. */
    ENCODING_UTF8_BOM       = -2,       /**< @brief UTF-8 with bottom mark. */
    ENCODING_UTF16LE_BOM    = -3,       /**< @brief UTF-16 little-endian with bottom mark. */
    ENCODING_UTF16BE_BOM    = -4,       /**< @brief UTF-16 big-endian with bottom mark. */
    ENCODING_UTF32LE_BOM    = -5,       /**< @brief UTF-16 little-endian with bottom mark. */
    ENCODING_UTF32BE_BOM    = -6,       /**< @brief UTF-16 big-endian with bottom mark. */
} ENCODING;

/**
 * @brief Convert almost any encoding to UTF-8.
 * To get a list of all possible encodings run "iconv --list".
 * @param[in] text - Text to be converted
 * @param[in] encoding - Encoding of input text.
 * @return Returns 0 if successful and the converted text,
 * or errno value On error, and text is unchanged.
 */
int                 to_utf8(std::string & text, const std::string & encoding);
/**
 * @brief Try to detect the encoding of str. This is relatively realiable,
 * but may be wrong.
 * @param[in] str - Text string to be checked.
 * @param[out] encoding - Detected encoding.
 * @return Returns 0 if successful, or CHARDET_OUT_OF_MEMORY/CHARDET_MEM_ALLOCATED_FAIL
 * on error.
 */
int                 get_encoding (const char * str, std::string & encoding);
/**
 * @brief Read text file and return in UTF-8 format, no matter in which
 * encoding the input file is. UTF-8/16/32 with BOM will always return a
 * correct result. For all other encodings the function tries to detect it,
 * that may fail.
 * @param[in] path - Path and filename of input file
 * @param[out] result - File contents as UTF-8
 * @return Returns one of the ENCODING enum values on success,
 * or errno on error. Basically a return code > 0 means there is an error.
 */
int                 read_file(const std::string & path, std::string & result);

/**
 * @brief Properly fill in all size related members in stat struct
 * @param[inout] st stat structure to update
 * @param[in] size size value to copy
 */
void                stat_set_size(struct stat *st, size_t size);

/**
 * @brief Detect if we are running under Docker.
 * @return Returns true, if running under Docker, or false if not.
 */
bool                detect_docker();

/**
 * @brief Iterate through all elements in map and search for the passed element.
 * @param[in] mapOfWords - map to search.
 * @param[in] value - Search value
 * @return If found, retuns const_iterator to element. Returns mapOfWords.cend() if not.
 */
template <typename T>
typename std::map<const std::string, const T, comp>::const_iterator search_by_value(const std::map<const std::string, const T, comp> & mapOfWords, T value)
{
    typename std::map<const std::string, const T, comp>::const_iterator it = mapOfWords.cbegin();
    while (it != mapOfWords.cend())
    {
        if (it->second == value)
        {
            return it;
        }
        ++it;
    }
    return mapOfWords.cend();
}

/**
 * @brief Check if subtitle codec is a text or graphical codec
 * @param[in] codec_id - Codec to check, must be one of the subtitle codecs.
 * @return Returns true if codec_id is a text based codec, false if it is bitmap based.
 */
bool                is_text_codec(AVCodecID codec_id);

/**
 * @brief Get first audio stream
 * @param[in] format_ctx - Format context of file
 * @param[out] channels - Number of audio channels in stream
 * @param[out] samplerate - Audio sample rate of stream
 * @return Returns stream number (value greater or equal zero) or negative errno value
 */
int                 get_audio_props(AVFormatContext *format_ctx, int *channels, int *samplerate);

/**
 * @brief Escape characters that are meaningful to regexp.
 * @param[in] str - String to escape
 * @return Returns reference to string.
 */
const std::string & regex_escape(std::string *str);

/**
 * @brief Find extension in include list, if existing.
 * @param[in] ext - Extension, if found.
 * @return Returns true if extension was found, false if there was none
 */
bool                is_selected(const std::string & ext);
/**
 * @brief Check if filename should be hidden from output path
 * @param[in] filename - Name to check
 * @return Returns true, if filename is blocked, false if not.
 */
bool                is_blocked(const std::string & filename);

typedef std::vector<std::string> MATCHVEC;                     /**< @brief Array of strings, sorted/search case insensitive */

/**
 * @brief Combine array of strings into comma separated list.
 * @param[in] s - std::set object to combine
 * @return List of strings, separated by commas.
 */
template<class T>
std::string implode(const T &s)
{
    std::ostringstream stream;
    std::copy(s.begin(), s.end(), std::ostream_iterator<std::string>(stream, ","));
    std::string str(stream.str());
    // Remove trailing ,
    if (str.size())
    {
        str.pop_back();
    }
    return str;
}

/**
 * @brief Savely delete memory: Pointer will be set to nullptr before deleted is actually called.
 * @param[inout] p - Pointer to delete
 */
template<class T>
void save_delete(T **p)
{
    T * tmp = __atomic_exchange_n(p, nullptr, __ATOMIC_RELEASE);
    if (tmp != nullptr)
    {
        delete tmp;
    }
}

/**
 * @brief Savely free memory: Pointer will be set to nullptr before it is actually freed.
 * @param[inout] p - Pointer to delete
 */
void save_free(void **p);

/**
 * @brief Sleep for specified time
 * @param milliseconds - Milliseconds to sleep
 */
void mssleep(int milliseconds);
/**
 * @brief Sleep for specified time
 * @param microseconds - Microseconds to sleep
 */
void ussleep(int microseconds);
/**
 * @brief Sleep for specified time
 * @param nanoseconds - Nanoseconds to sleep
 */
void nssleep(int nanoseconds);

#ifdef __CYGWIN__

/**
 * @brief Inbound Win32 path to POSIX path conversion. Stick to the relative path of the incoming path.
 * @param[in] win32 - Windows path to convert
 * @return Path for POSIX
 */
std::string win2posix(const char * win32);
/**
 * @brief Inbound POSIX path to Win32 path conversion. Stick to the relative path of the incoming path.
 * @param[in] posix - POSIX path to convert
 * @return Path for Windows
 */
std::string posix2win(const char * posix);
#endif  // __CYGWIN__

#endif
