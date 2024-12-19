/*
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2012 K. Henriksson
 * Copyright (C) 2017-2024 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file ffmpegfs.cc
 * @brief FFmpeg main function and utilities implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2024 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifdef __cplusplus
extern "C" {
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ffmpegfs.h"
#include "logging.h"
#include "ffmpegfshelp.h"

#include <sys/sysinfo.h>
#include <sqlite3.h>
#include <unistd.h>
#include <fnmatch.h>
#include <iostream>

#ifdef USE_LIBBLURAY
#include <libbluray/bluray-version.h>
#endif

#ifdef __clang__
// Silence this in fuse API code for clang
#pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#endif

FFMPEGFS_FORMAT_ARR ffmpeg_format;                      /**< @brief Two FFmpegfs_Format infos, 0: video file, 1: audio file */
FFMPEGFS_PARAMS     params;                             /**< @brief FFmpegfs command line parameters */

FFMPEGFS_PARAMS::FFMPEGFS_PARAMS()
    : m_basepath("")                                    // required parameter
    , m_mountpath("")                                   // required parameter

    , m_audio_codec(AV_CODEC_ID_NONE)                   // default: use predefined option
    , m_video_codec(AV_CODEC_ID_NONE)                   // default: use predefined option

    , m_autocopy(AUTOCOPY::OFF)                          // default: off
    , m_recodesame(RECODESAME::NO)                       // default: off
    , m_profile(PROFILE::DEFAULT)                        // default: no profile
    , m_level(PRORESLEVEL::NONE)                         // default: no level

    // Format
    // Audio
    , m_audiobitrate(128*1024)                          // default: 128 kBit
    , m_audiosamplerate(44100)                          // default: 44.1 kHz
    , m_audiochannels(2)                                // default: 2 channels
    , m_sample_fmt(SAMPLE_FMT::FMT_DONTCARE)            // default: use source format

    // Video
    , m_videobitrate(2*1024*1024)                       // default: 2 MBit
    , m_videowidth(0)                                   // default: do not change width
    , m_videoheight(0)                                  // default: do not change height
    , m_deinterlace(0)                                  // default: do not interlace video
    , m_segment_duration(10 * AV_TIME_BASE)             // default: 10 seconds
    , m_min_seek_time_diff(30 * AV_TIME_BASE)           // default: 30 seconds
    // Hardware acceleration
    , m_hwaccel_enc_API(HWACCELAPI::NONE)                // default: Use software encoder
    , m_hwaccel_enc_device_type(AV_HWDEVICE_TYPE_NONE)  // default: Use software encoder
    , m_hwaccel_dec_API(HWACCELAPI::NONE)               // default: Use software encoder
    , m_hwaccel_dec_device_type(AV_HWDEVICE_TYPE_NONE)  // default: Use software decoder
    , m_hwaccel_dec_blocked(nullptr)                    // default: No blocked encoders

    // Subtitles
    , m_no_subtitles(0)                                  // default: enable subtitles

    // Album arts
    , m_noalbumarts(0)                                  // default: copy album arts
    // Virtual Script
    , m_enablescript(0)                                 // default: no virtual script
    , m_scriptfile("index.php")                         // default name
    , m_scriptsource("scripts/videotag.php")            // default name
    // Other
    , m_debug(0)                                        // default: no debug messages
    , m_log_maxlevel("INFO")                            // default: INFO level
    , m_log_stderr(0)                                   // default: do not log to stderr
    , m_log_syslog(0)                                   // default: do not use syslog
    , m_logfile("")                                     // default: none
    // Cache/recoding options
    , m_expiry_time((60*60*24 /* d */) * 7)             // default: 1 week)
    , m_max_inactive_suspend(15)                        // default: 15 seconds
    , m_max_inactive_abort(30)                          // default: 30 seconds
    , m_prebuffer_time(0)                               // default: no prebuffer time
    , m_prebuffer_size(100 /* KB */ * 1024)             // default: 100 KB
    , m_max_cache_size(0)                               // default: no limit
    , m_min_diskspace(0)                                // default: no minimum
    , m_cachepath("")                                   // default: $XDG_CACHE_HOME/ffmpegfs
    , m_disable_cache(0)                                // default: enabled
    , m_cache_maintenance((60*60))                      // default: prune every 60 minutes
    , m_prune_cache(0)                                  // default: Do not prune cache immediately
    , m_clear_cache(0)                                  // default: Do not clear cache on startup
    , m_max_threads(0)                                  // default: 16 * CPU cores (this value here is overwritten later)
    , m_decoding_errors(0)                              // default: ignore errors
    , m_min_dvd_chapter_duration(1)                     // default: 1 second
    , m_oldnamescheme(0)                                // default: new scheme
    , m_include_extensions(new (std::nothrow) MATCHVEC)	// default: empty list
    , m_hide_extensions(new (std::nothrow) MATCHVEC)    // default: empty list
    , m_win_smb_fix(1)                                  // default: fix enabled
{
}

FFMPEGFS_PARAMS::FFMPEGFS_PARAMS(const FFMPEGFS_PARAMS & other)
{
    *this = other;
}

FFMPEGFS_PARAMS::~FFMPEGFS_PARAMS()
{
    delete m_hwaccel_dec_blocked;
}

FFMPEGFS_PARAMS& FFMPEGFS_PARAMS::operator=(const FFMPEGFS_PARAMS & other) noexcept
{
    if (this != &other) // Self assignment check
    {
        m_basepath = other.m_basepath;
        m_mountpath = other.m_mountpath;

        m_audio_codec = other.m_audio_codec;
        m_video_codec = other.m_video_codec;

        m_autocopy = other.m_autocopy;
        m_recodesame = other.m_recodesame;
        m_profile = other.m_profile;
        m_level = other.m_level;

        m_audiobitrate = other.m_audiobitrate;
        m_audiosamplerate = other.m_audiosamplerate;
        m_audiochannels = other.m_audiochannels;
        m_sample_fmt = other.m_sample_fmt;

        m_videobitrate = other.m_videobitrate;
        m_videowidth = other.m_videowidth;
        m_videoheight = other.m_videoheight;
        m_deinterlace = other.m_deinterlace;
        m_segment_duration = other.m_segment_duration;
        m_min_seek_time_diff = other.m_min_seek_time_diff;

        m_hwaccel_enc_API = other.m_hwaccel_enc_API;
        m_hwaccel_enc_device_type = other.m_hwaccel_enc_device_type;
        m_hwaccel_enc_device = other.m_hwaccel_enc_device;
        m_hwaccel_dec_API = other.m_hwaccel_dec_API;
        m_hwaccel_dec_device_type = other.m_hwaccel_dec_device_type;
        m_hwaccel_dec_device = other.m_hwaccel_dec_device;
        m_hwaccel_dec_blocked = other.m_hwaccel_dec_blocked;

        m_no_subtitles = other.m_no_subtitles;

        m_noalbumarts = other.m_noalbumarts;

        m_enablescript = other.m_enablescript;
        m_scriptfile = other.m_scriptfile;
        m_scriptsource = other.m_scriptsource;

        m_debug = other.m_debug;
        m_log_maxlevel = other.m_log_maxlevel;
        m_log_stderr = other.m_log_stderr;
        m_log_syslog = other.m_log_syslog;
        m_logfile = other.m_logfile;

        m_expiry_time = other.m_expiry_time;
        m_max_inactive_suspend = other.m_max_inactive_suspend;
        m_max_inactive_abort = other.m_max_inactive_abort;
        m_prebuffer_time = other.m_prebuffer_time;
        m_prebuffer_size = other.m_prebuffer_size;
        m_max_cache_size = other.m_max_cache_size;
        m_min_diskspace = other.m_min_diskspace;
        m_cachepath = other.m_cachepath;
        m_disable_cache = other.m_disable_cache;
        m_cache_maintenance = other.m_cache_maintenance;
        m_prune_cache = other.m_prune_cache;
        m_clear_cache = other.m_clear_cache;
        m_max_threads = other.m_max_threads;
        m_decoding_errors = other.m_decoding_errors;
        m_min_dvd_chapter_duration = other.m_min_dvd_chapter_duration;
        m_oldnamescheme = other.m_oldnamescheme;
        *m_include_extensions = *other.m_include_extensions;
        *m_hide_extensions = *other.m_hide_extensions;
        m_win_smb_fix = other.m_win_smb_fix;
    }

    return *this;
}

bool FFMPEGFS_PARAMS::smart_transcode() const
{
    return (ffmpeg_format[FORMAT::AUDIO].filetype() != FILETYPE::UNKNOWN && ffmpeg_format[FORMAT::VIDEO].filetype() != ffmpeg_format[FORMAT::AUDIO].filetype());
}

const FFmpegfs_Format *FFMPEGFS_PARAMS::current_format(LPCVIRTUALFILE virtualfile) const
{
    if (virtualfile == nullptr || virtualfile->m_format_idx > 1)
    {
        return nullptr;
    }
    return &ffmpeg_format[virtualfile->m_format_idx];
}

enum    // enum class or typedef here is not compatible with Fuse API
{
    KEY_HELP,
    KEY_VERSION,
    KEY_FFMPEG_CAPS,
    KEY_KEEP_OPT,
    // Intelligent parameters
    KEY_DESTTYPE,
    KEY_AUDIOCODEC,
    KEY_VIDEOCODEC,
    KEY_AUDIO_BITRATE,
    KEY_AUDIO_SAMPLERATE,
    KEY_AUDIO_CHANNELS,
    KEY_AUDIO_SAMPLE_FMT,
    KEY_VIDEO_BITRATE,
    KEY_SEGMENT_DURATION,
    KEY_MIN_SEEK_TIME_DIFF,
    KEY_SCRIPTFILE,
    KEY_SCRIPTSOURCE,
    KEY_EXPIRY_TIME,
    KEY_MAX_INACTIVE_SUSPEND_TIME,
    KEY_MAX_INACTIVE_ABORT_TIME,
    KEY_PREBUFFER_TIME,
    KEY_PREBUFFER_SIZE,
    KEY_MAX_CACHE_SIZE,
    KEY_MIN_DISKSPACE_SIZE,
    KEY_CACHEPATH,
    KEY_CACHE_MAINTENANCE,
    KEY_AUTOCOPY,
    KEY_RECODESAME,
    KEY_PROFILE,
    KEY_LEVEL,
    KEY_LOG_MAXLEVEL,
    KEY_LOGFILE,
    KEY_HWACCEL_ENCODER_API,
    KEY_HWACCEL_ENCODER_DEVICE,
    KEY_HWACCEL_DECODER_API,
    KEY_HWACCEL_DECODER_DEVICE,
    KEY_HWACCEL_DECODER_BLOCKED,
    KEY_INCLUDE_EXTENSIONS,
    KEY_HIDE_EXTENSIONS
};

/**
  * Map FFmpegfs options to FUSE parameters
  */
#define FFMPEGFS_OPT(templ, param, value) { templ, offsetof(FFMPEGFS_PARAMS, param), value }

/**
 * FUSE option descriptions
 *
 * Need to ignore annoying warnings caused by fuse.h
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wsign-conversion"
static struct fuse_opt ffmpegfs_opts[] =    // NOLINT(modernize-avoid-c-arrays)
{
    // Output type
    FUSE_OPT_KEY("--desttype=%s",                   KEY_DESTTYPE),
    FUSE_OPT_KEY("desttype=%s",                     KEY_DESTTYPE),
    FUSE_OPT_KEY("--audiocodec=%s",                 KEY_AUDIOCODEC),
    FUSE_OPT_KEY("audiocodec=%s",                   KEY_AUDIOCODEC),
    FUSE_OPT_KEY("--videocodec=%s",                 KEY_VIDEOCODEC),
    FUSE_OPT_KEY("videocodec=%s",                   KEY_VIDEOCODEC),
    FUSE_OPT_KEY("--profile=%s",                    KEY_PROFILE),
    FUSE_OPT_KEY("profile=%s",                      KEY_PROFILE),
    FUSE_OPT_KEY("--autocopy=%s",                   KEY_AUTOCOPY),
    FUSE_OPT_KEY("autocopy=%s",                     KEY_AUTOCOPY),
    FUSE_OPT_KEY("--recodesame=%s",                 KEY_RECODESAME),
    FUSE_OPT_KEY("recodesame=%s",                   KEY_RECODESAME),
    FUSE_OPT_KEY("--level=%s",                      KEY_LEVEL),
    FUSE_OPT_KEY("level=%s",                        KEY_LEVEL),

    // Audio
    FUSE_OPT_KEY("--audiobitrate=%s",               KEY_AUDIO_BITRATE),
    FUSE_OPT_KEY("audiobitrate=%s",                 KEY_AUDIO_BITRATE),
    FUSE_OPT_KEY("--audiosamplerate=%s",            KEY_AUDIO_SAMPLERATE),
    FUSE_OPT_KEY("audiosamplerate=%s",              KEY_AUDIO_SAMPLERATE),
    FUSE_OPT_KEY("--audiochannels=%s",              KEY_AUDIO_CHANNELS),
    FUSE_OPT_KEY("audiochannels=%s",                KEY_AUDIO_CHANNELS),
    FUSE_OPT_KEY("--audiosamplefmt=%s",             KEY_AUDIO_SAMPLE_FMT),
    FUSE_OPT_KEY("audiosamplefmt=%s",               KEY_AUDIO_SAMPLE_FMT),
    // Video
    FUSE_OPT_KEY("--videobitrate=%s",               KEY_VIDEO_BITRATE),
    FUSE_OPT_KEY("videobitrate=%s",                 KEY_VIDEO_BITRATE),
    FFMPEGFS_OPT("--videoheight=%u",                m_videoheight, 0),
    FFMPEGFS_OPT("videoheight=%u",                  m_videoheight, 0),
    FFMPEGFS_OPT("--videowidth=%u",                 m_videowidth, 0),
    FFMPEGFS_OPT("videowidth=%u",                   m_videowidth, 0),
    FFMPEGFS_OPT("--deinterlace",                   m_deinterlace, 1),
    FFMPEGFS_OPT("deinterlace",                     m_deinterlace, 1),
    // HLS
    FUSE_OPT_KEY("--segment_duration=%s",           KEY_SEGMENT_DURATION),
    FUSE_OPT_KEY("segment_duration=%s",             KEY_SEGMENT_DURATION),
    FUSE_OPT_KEY("--min_seek_time_diff=%s",         KEY_MIN_SEEK_TIME_DIFF),
    FUSE_OPT_KEY("min_seek_time_diff=%s",           KEY_MIN_SEEK_TIME_DIFF),
    // Hardware acceleration
    FUSE_OPT_KEY("--hwaccel_enc=%s",                KEY_HWACCEL_ENCODER_API),
    FUSE_OPT_KEY("hwaccel_enc=%s",                  KEY_HWACCEL_ENCODER_API),
    FUSE_OPT_KEY("--hwaccel_enc_device=%s",         KEY_HWACCEL_ENCODER_DEVICE),
    FUSE_OPT_KEY("hwaccel_enc_device=%s",           KEY_HWACCEL_ENCODER_DEVICE),
    FUSE_OPT_KEY("--hwaccel_dec=%s",                KEY_HWACCEL_DECODER_API),
    FUSE_OPT_KEY("hwaccel_dec=%s",                  KEY_HWACCEL_DECODER_API),
    FUSE_OPT_KEY("--hwaccel_dec_device=%s",         KEY_HWACCEL_DECODER_DEVICE),
    FUSE_OPT_KEY("hwaccel_dec_device=%s",           KEY_HWACCEL_DECODER_DEVICE),
    FUSE_OPT_KEY("--hwaccel_dec_blocked=%s",        KEY_HWACCEL_DECODER_BLOCKED),
    FUSE_OPT_KEY("hwaccel_dec_blocked=%s",          KEY_HWACCEL_DECODER_BLOCKED),
    // Subtitles
    FFMPEGFS_OPT("--no_subtitles",                  m_no_subtitles, 1),
    FFMPEGFS_OPT("no_subtitles",                    m_no_subtitles, 1),
    // Album arts
    FFMPEGFS_OPT("--noalbumarts",                   m_noalbumarts, 1),
    FFMPEGFS_OPT("noalbumarts",                     m_noalbumarts, 1),
    // Virtual script
    FFMPEGFS_OPT("--enablescript",                  m_enablescript, 1),
    FFMPEGFS_OPT("enablescript",                    m_enablescript, 1),
    FUSE_OPT_KEY("--scriptfile=%s",                 KEY_SCRIPTFILE),
    FUSE_OPT_KEY("scriptfile=%s",                   KEY_SCRIPTFILE),
    FUSE_OPT_KEY("--scriptsource=%s",               KEY_SCRIPTSOURCE),
    FUSE_OPT_KEY("scriptsource=%s",                 KEY_SCRIPTSOURCE),
    // Background recoding/caching
    // Cache
    FUSE_OPT_KEY("--expiry_time=%s",                KEY_EXPIRY_TIME),
    FUSE_OPT_KEY("expiry_time=%s",                  KEY_EXPIRY_TIME),
    FUSE_OPT_KEY("--max_inactive_suspend=%s",       KEY_MAX_INACTIVE_SUSPEND_TIME),
    FUSE_OPT_KEY("max_inactive_suspend=%s",         KEY_MAX_INACTIVE_SUSPEND_TIME),
    FUSE_OPT_KEY("--max_inactive_abort=%s",         KEY_MAX_INACTIVE_ABORT_TIME),
    FUSE_OPT_KEY("max_inactive_abort=%s",           KEY_MAX_INACTIVE_ABORT_TIME),
    FUSE_OPT_KEY("--prebuffer_time=%s",             KEY_PREBUFFER_TIME),
    FUSE_OPT_KEY("prebuffer_time=%s",               KEY_PREBUFFER_TIME),
    FUSE_OPT_KEY("--prebuffer_size=%s",             KEY_PREBUFFER_SIZE),
    FUSE_OPT_KEY("prebuffer_size=%s",               KEY_PREBUFFER_SIZE),
    FUSE_OPT_KEY("--max_cache_size=%s",             KEY_MAX_CACHE_SIZE),
    FUSE_OPT_KEY("max_cache_size=%s",               KEY_MAX_CACHE_SIZE),
    FUSE_OPT_KEY("--min_diskspace=%s",              KEY_MIN_DISKSPACE_SIZE),
    FUSE_OPT_KEY("min_diskspace=%s",                KEY_MIN_DISKSPACE_SIZE),
    FUSE_OPT_KEY("--cachepath=%s",                  KEY_CACHEPATH),
    FUSE_OPT_KEY("cachepath=%s",                    KEY_CACHEPATH),
    FFMPEGFS_OPT("--disable_cache",                 m_disable_cache, 1),
    FFMPEGFS_OPT("disable_cache",                   m_disable_cache, 1),
    FUSE_OPT_KEY("--cache_maintenance=%s",          KEY_CACHE_MAINTENANCE),
    FUSE_OPT_KEY("cache_maintenance=%s",            KEY_CACHE_MAINTENANCE),
    FFMPEGFS_OPT("--prune_cache",                   m_prune_cache, 1),
    FFMPEGFS_OPT("--clear_cache",                   m_clear_cache, 1),
    FFMPEGFS_OPT("clear_cache",                     m_clear_cache, 1),

    // Other
    FFMPEGFS_OPT("--max_threads=%u",                m_max_threads, 0),
    FFMPEGFS_OPT("max_threads=%u",                  m_max_threads, 0),
    FFMPEGFS_OPT("--decoding_errors=%u",            m_decoding_errors, 0),
    FFMPEGFS_OPT("decoding_errors=%u",              m_decoding_errors, 0),
    FFMPEGFS_OPT("--min_dvd_chapter_duration=%u",   m_min_dvd_chapter_duration, 0),
    FFMPEGFS_OPT("min_dvd_chapter_duration=%u",     m_min_dvd_chapter_duration, 0),
    FFMPEGFS_OPT("--oldnamescheme=%u",              m_oldnamescheme, 0),
    FFMPEGFS_OPT("oldnamescheme=%u",                m_oldnamescheme, 0),
    FUSE_OPT_KEY("--include_extensions=%s",         KEY_INCLUDE_EXTENSIONS),
    FUSE_OPT_KEY("include_extensions=%s",           KEY_INCLUDE_EXTENSIONS),
    FUSE_OPT_KEY("--hide_extensions=%u",            KEY_HIDE_EXTENSIONS),
    FUSE_OPT_KEY("hide_extensions=%u",              KEY_HIDE_EXTENSIONS),
    // Experimental
    FFMPEGFS_OPT("--win_smb_fix=%u",                m_win_smb_fix, 1),
    FFMPEGFS_OPT("win_smb_fix=%u",                  m_win_smb_fix, 1),
    // FFmpegfs options
    FFMPEGFS_OPT("-d",                              m_debug, 1),
    FFMPEGFS_OPT("debug",                           m_debug, 1),
    FUSE_OPT_KEY("--log_maxlevel=%s",               KEY_LOG_MAXLEVEL),
    FUSE_OPT_KEY("log_maxlevel=%s",                 KEY_LOG_MAXLEVEL),
    FFMPEGFS_OPT("--log_stderr",                    m_log_stderr, 1),
    FFMPEGFS_OPT("log_stderr",                      m_log_stderr, 1),
    FFMPEGFS_OPT("--log_syslog",                    m_log_syslog, 1),
    FFMPEGFS_OPT("log_syslog",                      m_log_syslog, 1),
    FUSE_OPT_KEY("--logfile=%s",                    KEY_LOGFILE),
    FUSE_OPT_KEY("logfile=%s",                      KEY_LOGFILE),

    FUSE_OPT_KEY("-h",                              KEY_HELP),
    FUSE_OPT_KEY("--help",                          KEY_HELP),
    FUSE_OPT_KEY("-V",                              KEY_VERSION),
    FUSE_OPT_KEY("--version",                       KEY_VERSION),
    FUSE_OPT_KEY("-c",                              KEY_FFMPEG_CAPS),
    FUSE_OPT_KEY("--capabilities",                  KEY_FFMPEG_CAPS),
    FUSE_OPT_KEY("-d",                              KEY_KEEP_OPT),
    FUSE_OPT_KEY("debug",                           KEY_KEEP_OPT),
    FUSE_OPT_END
};
#pragma GCC diagnostic pop

typedef std::map<const std::string, const AUTOCOPY, comp> AUTOCOPY_MAP;         /**< @brief Map command line option to AUTOCOPY enum */
typedef std::map<const std::string, const PROFILE, comp> PROFILE_MAP;           /**< @brief Map command line option to PROFILE enum */
typedef std::map<const std::string, const PRORESLEVEL, comp> LEVEL_MAP;         /**< @brief Map command line option to LEVEL enum */
typedef std::map<const std::string, const RECODESAME, comp> RECODESAME_MAP;     /**< @brief Map command line option to RECODESAME enum */

typedef struct HWACCEL                                                          /**< @brief Hardware acceleration device and type */
{
    bool                m_supported;                                            /**< @brief true if API supported, false if not */
    HWACCELAPI          m_hwaccel_API;                                          /**< @brief Acceleration API, e.g VAAPI, MMAL or OMX */
    AVHWDeviceType      m_hwaccel_device_type;                                  /**< @brief Hardware buffering type, NONE if not used */
} HWACCEL;

typedef std::map<const std::string, HWACCEL, comp> HWACCEL_MAP;                 /**< @brief Map command line option to HWACCEL struct */
typedef std::map<const std::string, const AVCodecID, comp> CODEC_MAP;           /**< @brief Map command line option to AVCodecID */
typedef std::map<const std::string, const SAMPLE_FMT, comp> SAMPLE_FMT_MAP;     /**< @brief Map command line option to SAMPLE_FMT */

typedef std::map<const std::string, const AVCodecID, comp> AUDIOCODEC_MAP;      /**< @brief Map command line option to audio AVCodecID */
typedef std::map<const std::string, const AVCodecID, comp> VIDEOCODEC_MAP;      /**< @brief Map command line option to video AVCodecID */

/**
 * @brief List of audio codecs
 */
static const AUDIOCODEC_MAP audiocodec_map
{
    { "AAC",            AV_CODEC_ID_AAC },          // TS, MP4, MOV, MKV
    { "AC3",            AV_CODEC_ID_AC3 },          // MP4, MOV, MKV
    { "MP3",            AV_CODEC_ID_MP3 },          // TS, MP4, MOV, MKV
    { "OPUS",           AV_CODEC_ID_OPUS },         // webm

    { "VORBIS",         AV_CODEC_ID_VORBIS },       /** @todo webm: sound skips */
    { "DTS",            AV_CODEC_ID_DTS },          /** @todo invalid argument? */
    { "PCM16",          AV_CODEC_ID_PCM_S16LE },    /** @todo is this useable? */
    { "PCM24",          AV_CODEC_ID_PCM_S24LE },    /** @todo is this useable? */
    { "PCM32",          AV_CODEC_ID_PCM_S32LE },    /** @todo is this useable? */
};

/**
 * @brief List of video codecs
 */
const static VIDEOCODEC_MAP videocodec_map
{
    { "MPEG1",          AV_CODEC_ID_MPEG1VIDEO },   // TS, MP4, MKV
    { "MPEG2",          AV_CODEC_ID_MPEG2VIDEO },   // TS, MP4, MKV
    { "H264",           AV_CODEC_ID_H264 },         // TS, MP4, MKV
    { "H265",           AV_CODEC_ID_H265 },         // TS, MP4, MKV
    { "VP8",            AV_CODEC_ID_VP8 },          // WebM
    { "VP9",            AV_CODEC_ID_VP9 },          // WebM

    //{ "AV1",            AV_CODEC_ID_AV1 },          /** @todo WebM ends with "Could not write video frame (error 'Invalid data found when processing input')." */
};

/**
  * List of AUTOCOPY options
  */
static const AUTOCOPY_MAP autocopy_map
{
    { "OFF",            AUTOCOPY::OFF },
    { "MATCH",          AUTOCOPY::MATCH },
    { "MATCHLIMIT",     AUTOCOPY::MATCHLIMIT },
    { "STRICT",         AUTOCOPY::STRICT },
    { "STRICTLIMIT",    AUTOCOPY::STRICTLIMIT },
};

/**
  * List if MP4 profiles
  */
static const PROFILE_MAP profile_map
{
    { "NONE",           PROFILE::DEFAULT },

    // MP4

    { "FF",             PROFILE::MP4_FF },
    { "EDGE",           PROFILE::MP4_EDGE },
    { "IE",             PROFILE::MP4_IE },
    { "CHROME",         PROFILE::MP4_CHROME },
    { "SAFARI",         PROFILE::MP4_SAFARI },
    { "OPERA",          PROFILE::MP4_OPERA },
    { "MAXTHON",        PROFILE::MP4_MAXTHON },

    // WEBM
};

/**
  * List if ProRes levels.
  */
static const LEVEL_MAP prores_level_map
{
    // ProRes
    { "PROXY",          PRORESLEVEL::PRORES_PROXY },
    { "LT",             PRORESLEVEL::PRORES_LT },
    { "STANDARD",       PRORESLEVEL::PRORES_STANDARD },
    { "HQ",             PRORESLEVEL::PRORES_HQ },
};

/**
  * List if recode options.
  */
static const RECODESAME_MAP recode_map
{
    // Recode to same format
    { "NO",             RECODESAME::NO },
    { "YES",            RECODESAME::YES },
};

/**
  * List if hardware acceleration options.
  * See https://trac.ffmpeg.org/wiki/HWAccelIntro
  *
  * AV_HWDEVICE_TYPE_NONE will be set to the appropriate device type
  * in build_device_type_list() by asking the FFmpeg API for the proper
  * type.
  */
static HWACCEL_MAP hwaccel_map
{
    { "NONE",           { true,     HWACCELAPI::NONE,            AV_HWDEVICE_TYPE_NONE } },

    // **** Supported by Linux ****

    { "VAAPI",          { true,     HWACCELAPI::VAAPI,           AV_HWDEVICE_TYPE_NONE } },  // Video Acceleration API (VA-API), https://trac.ffmpeg.org/wiki/Hardware/VAAPI

    // RaspberryPi

    { "MMAL",           { true,     HWACCELAPI::MMAL,            AV_HWDEVICE_TYPE_NONE } },  // Multimedia Abstraction Layer by Broadcom. Encoding only.
    { "OMX",            { true,     HWACCELAPI::OMX,             AV_HWDEVICE_TYPE_NONE } },  // OpenMAX (Open Media Acceleration). Decoding only.

    #if 0
    // Additional formats
    { "CUDA",           { false,    HWACCELAPI::CUDA,            AV_HWDEVICE_TYPE_NONE } },  // Compute Unified Device Architecture, see https://developer.nvidia.com/ffmpeg and https://en.wikipedia.org/wiki/CUDA
    { "V4L2M2M",        { false,    HWACCELAPI::V4L2M2M,         AV_HWDEVICE_TYPE_NONE } },  // v4l2 mem to mem (Video4linux)
    { "VDPAU",          { false,    HWACCELAPI::VDPAU,           AV_HWDEVICE_TYPE_NONE } },  // Video Decode and Presentation API for Unix, see https://en.wikipedia.org/wiki/VDPAU
    { "QSV",            { false,    HWACCELAPI::QSV,             AV_HWDEVICE_TYPE_NONE } },  // QuickSync, see https://trac.ffmpeg.org/wiki/Hardware/QuickSync
    { "OPENCL",         { false,    HWACCELAPI::OPENCL,          AV_HWDEVICE_TYPE_NONE } },  // Open Standard for Parallel Programming of Heterogeneous Systems, see https://trac.ffmpeg.org/wiki/HWAccelIntro#OpenCL
    #if HAVE_VULKAN_HWACCEL
    { "VULKAN",         { false,    HWACCELAPI::VULKAN,          AV_HWDEVICE_TYPE_NONE } },  // Low-overhead, cross-platform 3D graphics and computing API, requires Libavutil >= 56.30.100, see https://en.wikipedia.org/wiki/Vulkan_(API)
    #endif // HAVE_VULKAN_HWACCEL
    #if __APPLE__
    // MacOS, not supported
    { "VIDEOTOOLBOX",   { false,    HWACCELAPI::VIDEOTOOLBOX,    AV_HWDEVICE_TYPE_NONE } },  // https://trac.ffmpeg.org/wiki/HWAccelIntro#VideoToolbox
    #endif
    #if __ANDROID__
    // Android
    { "MEDIACODEC",     { false,    HWACCELAPI::MEDIACODEC,      AV_HWDEVICE_TYPE_NONE } },  // See https://developer.android.com/reference/android/media/MediaCodec
    #endif
    #if _WIN32
    // **** Not supported ****

    // Digital Rights Management
    { "DRM",            { false,    HWACCELAPI::DRM,             AV_HWDEVICE_TYPE_NONE } },

    // Windows only, not supported
    { "DXVA2",          { false,    HWACCELAPI::DXVA2,           AV_HWDEVICE_TYPE_NONE } },  // Direct3D 9 / DXVA2
    { "D3D11VA",        { false,    HWACCELAPI::D3D11VA,         AV_HWDEVICE_TYPE_NONE } },  // Direct3D 11
    #endif
    #endif
};

/**
  * List of AUTOCOPY options
  */
static const CODEC_MAP hwaccel_codec_map
{
    { "H263",   AV_CODEC_ID_H263 },
    { "H264",   AV_CODEC_ID_H264 },
    { "HEVC",   AV_CODEC_ID_HEVC },
    { "MPEG2",  AV_CODEC_ID_MPEG2VIDEO },
    { "MPEG4",  AV_CODEC_ID_MPEG4 },
    { "VC1",    AV_CODEC_ID_VC1 },
    { "VP8",    AV_CODEC_ID_VP8 },
    { "VP9",    AV_CODEC_ID_VP9 },
    { "WMV3",   AV_CODEC_ID_WMV3 },
};

/**
 * List of sample formats.
 */
static const SAMPLE_FMT_MAP sample_fmt_map
{
    { "0",      SAMPLE_FMT::FMT_DONTCARE },
    { "8",      SAMPLE_FMT::FMT_8 },
    { "16",     SAMPLE_FMT::FMT_16 },
    { "24",     SAMPLE_FMT::FMT_24 },
    { "32",     SAMPLE_FMT::FMT_32 },
    { "64",     SAMPLE_FMT::FMT_64 },
    { "F16",    SAMPLE_FMT::FMT_F16 },
    { "F24",    SAMPLE_FMT::FMT_F24 },
    { "F32",    SAMPLE_FMT::FMT_F32 },
    { "F64",    SAMPLE_FMT::FMT_F64 },
};

static int          get_bitrate(const std::string & arg, BITRATE *bitrate);
static int          get_samplerate(const std::string & arg, int * samplerate);
static int          get_sampleformat(const std::string & arg, SAMPLE_FMT * sample_fmt);
static int          get_time(const std::string & arg, time_t *time);
static int          get_size(const std::string & arg, size_t *size);
static int          get_desttype(const std::string & arg, FFMPEGFS_FORMAT_ARR & format);
static int          get_audiocodec(const std::string & arg, AVCodecID *audio_codec);
static int          get_videocodec(const std::string & arg, AVCodecID *video_codec);
static int          get_autocopy(const std::string & arg, AUTOCOPY *autocopy);
static int          get_recodesame(const std::string & arg, RECODESAME *recode);
static int          get_profile(const std::string & arg, PROFILE *profile);
static int          get_level(const std::string & arg, PRORESLEVEL *level);
static int          get_segment_duration(const std::string & arg, int64_t *value);
static int          get_seek_time_diff(const std::string & arg, int64_t *value);
static int          get_hwaccel(const std::string & arg, HWACCELAPI *hwaccel_API, AVHWDeviceType *hwaccel_device_type);
static int          get_codec(const std::string & codec, AVCodecID *codec_id);
static int          get_hwaccel_dec_blocked(const std::string & arg, HWACCEL_BLOCKED_MAP **hwaccel_dec_blocked);
static int          get_value(const std::string & arg, int *value);
static int          get_value(const std::string & arg, std::string *value);
static int          get_value(const std::string & arg, MATCHVEC *value);
//static int          get_value(const std::string & arg, std::optional<std::string> *value);
static int          get_value(const std::string & arg, double *value);

static int          ffmpegfs_opt_proc(__attribute__((unused)) void* data, const char* arg, int key, struct fuse_args *outargs);
static bool         set_defaults();
static void         build_device_type_list();
static void         print_params();
static void         usage();
static void         ffmpeg_log(void *ptr, int level, const char *fmt, va_list vl);
static bool         init_logging(const std::string &logfile, const std::string & max_level, bool to_stderr, bool to_syslog);

/**
 * @brief Print program usage info.
 */
static void usage()
{
    std::string help;
    size_t pos;

    help.assign(reinterpret_cast<const char*>(ffmpegfshelp), ffmpegfshelp_len);
    pos = help.find("OPTIONS\n");

    std::cout << help.substr(pos + sizeof("OPTIONS\n"));
}

/**
 * @brief Iterate through all elements in map print all keys
 * @param[in] info - Informative text, will be printed before the list. May be nullptr.
 * @param[in] map - Map to go through.
 */
template<typename T>
static void list_options(const char * info, const T & map)
{
    std::string buffer;
    for (typename T::const_iterator it = map.cbegin(); it != map.cend();)
    {
        buffer += it->first.c_str();

        if (++it != map.cend())
        {
            buffer += ", ";
        }
    }

    if (info != nullptr)
    {
        std::fprintf(stderr, "%s: %s\n", info, buffer.c_str());
    }
    else
    {
        std::fprintf(stderr, "%s\n", buffer.c_str());
    }
}

/**
 * @brief Get formatted bitrate.
 @verbatim
 Supported formats:
 In bit/s:  #  or #bps
 In kbit/s: #M or #Mbps
 In Mbit/s: #M or #Mbps
 @endverbatim
 * @param[in] arg - Bitrate as string.
 * @param[in] bitrate - On return, contains parsed bitrate.
 * @return Returns 0 if valid; if invalid returns -1.
 */
static int get_bitrate(const std::string & arg, BITRATE *bitrate)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));
        int reti;

        // Check for decimal number
        reti = reg_compare(data, "^([1-9][0-9]*|0)?(bps)?$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *bitrate = static_cast<BITRATE>(std::stol(data));
            return 0;   // OK
        }

        // Check for number with optional descimal point and K modifier
        reti = reg_compare(data, "^[1-9][0-9]*(\\.[0-9]+)?K(bps)?$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *bitrate = static_cast<BITRATE>(std::stof(data) * 1000);
            return 0;   // OK
        }

        // Check for number with optional descimal point and M modifier
        reti = reg_compare(data, "^[1-9][0-9]*(\\.[0-9]+)?M(bps)?$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *bitrate = static_cast<BITRATE>(std::stof(data) * 1000000);
            return 0;   // OK
        }

        std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid bit rate '%s'\n", param.c_str(), data.c_str());
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

/**
 * @brief Get formatted sample rate.
 @verbatim
 Supported formats:
 In Hz:  #  or #Hz
 In kHz: #K or #KHz
 @endverbatim
 * @param[in] arg - Samplerate as string.
 * @param[in] samplerate - On return, contains parsed sample rate.
 * @return Returns 0 if valid; if invalid returns -1.
 */
static int get_samplerate(const std::string & arg, int * samplerate)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));
        int reti;

        // Check for decimal number
        reti = reg_compare(data, "^([1-9][0-9]*|0)(Hz)?$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *samplerate = std::stoi(data);
            return 0;   // OK
        }

        // Check for number with optional descimal point and K modifier
        reti = reg_compare(data, "^[1-9][0-9]*(\\.[0-9]+)?K(Hz)?$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *samplerate = static_cast<int>(std::stof(data) * 1000);
            return 0;   // OK
        }

        std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid sample rate '%s'\n", param.c_str(), data.c_str());
    }
    else
    {
        std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());
    }

    return -1;
}

/**
 * @brief Get sample format
 * @param[in] arg - Sample format as string.
 * @param[in] sample_fmt - On return, contains parsed sample format.
 * @return Returns 0 if valid; if invalid returns -1.
 */
static int get_sampleformat(const std::string & arg, SAMPLE_FMT * sample_fmt)
{
    size_t pos = arg.find('=');

    *sample_fmt = SAMPLE_FMT::FMT_DONTCARE;

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));

        SAMPLE_FMT_MAP::const_iterator it = sample_fmt_map.find(data);

        if (it == sample_fmt_map.cend())
        {
            std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid sample format option: %s\n", param.c_str(), data.c_str());

            list_options("Valid sample formats are", sample_fmt_map);

            return -1;
        }

        // May fail later: Can only be checked when destination format is known.
        *sample_fmt = it->second;

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;

}

std::string get_sampleformat_text(SAMPLE_FMT sample_fmt)
{
    SAMPLE_FMT_MAP::const_iterator it = search_by_value(sample_fmt_map, sample_fmt);
    if (it != sample_fmt_map.cend())
    {
        return it->first;
    }
    return "INVALID";
}

/**
 * @brief Get formatted time,
 @verbatim
 Supported formats:
 Seconds: # @n
 Minutes: #m @n
 Hours:   #h @n
 Days:    #d @n
 Weeks:   #w
 @endverbatim
 * @param[in] arg - Time as string.
 * @param[in] time - On return, contains parsed time.
 * @return Returns 0 if valid; if invalid returns -1.
 */
static int get_time(const std::string & arg, time_t *time)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));
        int reti;

        // Check for decimal number
        reti = reg_compare(data, "^([1-9][0-9]*|0)?s?$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *time = static_cast<time_t>(std::stol(data));
            return 0;   // OK
        }

        // Check for number with optional descimal point and m modifier
        reti = reg_compare(data, "^[1-9][0-9]*(\\.[0-9]+)?m$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *time = static_cast<time_t>(std::stof(data) * 60);
            return 0;   // OK
        }

        // Check for number with optional descimal point and h modifier
        reti = reg_compare(data, "^[1-9][0-9]*(\\.[0-9]+)?h$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *time = static_cast<time_t>(std::stof(data) * 60 * 60);
            return 0;   // OK
        }

        // Check for number with optional descimal point and d modifier
        reti = reg_compare(data, "^[1-9][0-9]*(\\.[0-9]+)?d$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *time = static_cast<time_t>(std::stof(data) * 60 * 60 * 24);
            return 0;   // OK
        }

        // Check for number with optional descimal point and w modifier
        reti = reg_compare(data, "^[1-9][0-9]*(\\.[0-9]+)?w$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *time = static_cast<time_t>(std::stof(data) * 60 * 60 * 24 * 7);
            return 0;   // OK
        }

        std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid time format '%s'\n", param.c_str(), data.c_str());
    }
    else
    {
        std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid time format\n", arg.c_str());
    }

    return -1;
}

/**
 * @brief Read size: @n
 @verbatim
 Supported formats:
 In bytes:  # or #B @n
 In KBytes: #K or #KB @n
 In MBytes: #B or #MB @n
 In GBytes: #G or #GB @n
 In TBytes: #T or #TB
 @endverbatim
 * @param[in] arg - Time as string.
 * @param[out] size - On return, contains parsed size.
 * @return Returns 0 if valid; if invalid returns -1.
 */
static int get_size(const std::string & arg, size_t *size)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));
        int reti;

        // Check for decimal number
        reti = reg_compare(data, "^([1-9][0-9]*|0)?B?$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *size = static_cast<size_t>(std::stol(data));
            return 0;   // OK
        }

        // Check for number with optional descimal point and K/KB modifier
        reti = reg_compare(data, "^[1-9][0-9]*(\\.[0-9]+)?KB?$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *size = static_cast<size_t>(std::stof(data) * 1024);
            return 0;   // OK
        }

        // Check for number with optional descimal point and M/MB modifier
        reti = reg_compare(data, "^[1-9][0-9]*(\\.[0-9]+)?MB?$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *size = static_cast<size_t>(std::stof(data) * 1024 * 1024);
            return 0;   // OK
        }

        // Check for number with optional descimal point and G/GB modifier
        reti = reg_compare(data, "^[1-9][0-9]*(\\.[0-9]+)?GB?$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *size = static_cast<size_t>(std::stof(data) * 1024 * 1024 * 1024);
            return 0;   // OK
        }

        // Check for number with optional descimal point and T/TB modifier
        reti = reg_compare(data, "^[1-9][0-9]*(\\.[0-9]+)?TB?$", std::regex::icase);

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *size = static_cast<size_t>(std::stof(data) * 1024 * 1024 * 1024 * 1024);
            return 0;   // OK
        }

        std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid size '%s'\n", param.c_str(), data.c_str());
    }
    else
    {
        std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid size\n", arg.c_str());
    }

    return -1;
}

/**
 * @brief Get destination type.
 * @param[in] arg - Format as string (MP4, OGG etc.).
 * @param[out] format - Index 0: Selected video format.@n
 * Index 1: Selected audio format.
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_desttype(const std::string & arg, FFMPEGFS_FORMAT_ARR & format)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::vector<std::string> results =  split(arg.substr(pos + 1), "\\+");

        if (results.size() > 0 && results.size() < 3)
        {
            // Check for valid destination type and obtain codecs and file type.
            if (!format[0].init(results[0]))
            {
                std::fprintf(stderr, "INVALID PARAMETER (%s): No codecs available for desttype: %s\n", param.c_str(), results[0].c_str());
                return 1;
            }

            if (results.size() == 2)
            {
                if (format[0].video_codec() == AV_CODEC_ID_NONE)
                {
                    std::fprintf(stderr, "INVALID PARAMETER (%s): First format %s does not support video\n", param.c_str(), results[0].c_str());
                    return 1;
                }

                if (!format[1].init(results[1]))
                {
                    std::fprintf(stderr, "INVALID PARAMETER (%s): No codecs available for desttype: %s\n", param.c_str(), results[1].c_str());
                    return 1;
                }

                if (format[1].video_codec() != AV_CODEC_ID_NONE)
                {
                    std::fprintf(stderr, "INVALID PARAMETER (%s): Second format %s should be audio only\n", param.c_str(), results[1].c_str());
                    return 1;
                }
            }

            return 0;
        }
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

/**
 * @brief Get the audio codec.
 * @param[in] arg - One of the possible audio codecs.
 * @param[out] audio_codec - Upon return contains selected AVCodecID enum.
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_audiocodec(const std::string & arg, AVCodecID *audio_codec)
{
    *audio_codec = AV_CODEC_ID_NONE;

    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));

        AUDIOCODEC_MAP::const_iterator it = audiocodec_map.find(data);

        if (it == audiocodec_map.cend())
        {
            std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid videocodec option: %s\n", param.c_str(), data.c_str());

            list_options("Valid audio codecs", audiocodec_map);

            return -1;
        }

        *audio_codec = it->second;

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

/**
 * @brief Get the video codec.
 * @param[in] arg - One of the possible video codecs.
 * @param[out] video_codec - Upon return contains selected AVCodecID enum.
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_videocodec(const std::string & arg, AVCodecID *video_codec)
{
    *video_codec = AV_CODEC_ID_NONE;

    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));

        VIDEOCODEC_MAP::const_iterator it = videocodec_map.find(data);

        if (it == videocodec_map.cend())
        {
            std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid videocodec option: %s\n", param.c_str(), data.c_str());

            list_options("Valid video codecs", videocodec_map);

            return -1;
        }

        *video_codec = it->second;

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

/**
 * @brief Get autocopy option.
 * @param[in] arg - One of the auto copy options.
 * @param[out] autocopy - Upon return contains selected AUTOCOPY enum.
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_autocopy(const std::string & arg, AUTOCOPY *autocopy)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));

        AUTOCOPY_MAP::const_iterator it = autocopy_map.find(data);

        if (it == autocopy_map.cend())
        {
            std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid autocopy option: %s\n", param.c_str(), data.c_str());

            list_options("Valid autocopy options are", autocopy_map);

            return -1;
        }

        *autocopy = it->second;

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

std::string get_audio_codec_text(AVCodecID audio_codec)
{
    AUDIOCODEC_MAP::const_iterator it = search_by_value(audiocodec_map, audio_codec);
    if (it != audiocodec_map.cend())
    {
        return it->first;
    }
    return "INVALID";
}

std::string get_video_codec_text(AVCodecID video_codec)
{
    AUDIOCODEC_MAP::const_iterator it = search_by_value(videocodec_map, video_codec);
    if (it != videocodec_map.cend())
    {
        return it->first;
    }
    return "INVALID";
}

std::string get_autocopy_text(AUTOCOPY autocopy)
{
    AUTOCOPY_MAP::const_iterator it = search_by_value(autocopy_map, autocopy);
    if (it != autocopy_map.cend())
    {
        return it->first;
    }
    return "INVALID";
}

/**
 * @brief Get recode option.
 * @param[in] arg - One of the recode options.
 * @param[out] recode - Upon return contains selected RECODESAME enum.
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_recodesame(const std::string & arg, RECODESAME *recode)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));

        RECODESAME_MAP::const_iterator it = recode_map.find(data);

        if (it == recode_map.cend())
        {
            std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid recode option: %s\n", param.c_str(), data.c_str());

            list_options("Valid recode options are", recode_map);

            return -1;
        }

        *recode = it->second;

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

std::string get_recodesame_text(RECODESAME recode)
{
    RECODESAME_MAP::const_iterator it = search_by_value(recode_map, recode);
    if (it != recode_map.cend())
    {
        return it->first;
    }
    return "INVALID";
}

/**
 * @brief Get profile option.
 * @param[in] arg - One of the auto profile options.
 * @param[out] profile - Upon return contains selected PROFILE enum.
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_profile(const std::string & arg, PROFILE *profile)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));

        PROFILE_MAP::const_iterator it = profile_map.find(data);

        if (it == profile_map.cend())
        {
            std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid profile: %s\n", param.c_str(), data.c_str());

            list_options("Valid profiles are", profile_map);

            return -1;
        }

        *profile = it->second;

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

std::string get_profile_text(PROFILE profile)
{
    PROFILE_MAP::const_iterator it = search_by_value(profile_map, profile);
    if (it != profile_map.cend())
    {
        return it->first;
    }
    return "INVALID";
}

// Read level
/**
 * @brief Get ProRes level
 * @param[in] arg - One of the ProRes levels.
 * @param[out] level - Upon return contains selected PRORESLEVEL enum.
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_level(const std::string & arg, PRORESLEVEL *level)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));

        LEVEL_MAP::const_iterator it = prores_level_map.find(data);

        if (it == prores_level_map.cend())
        {
            std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid level: %s\n", param.c_str(), data.c_str());

            list_options("Valid levels are", prores_level_map);

            return -1;
        }

        *level = it->second;

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

// Get level text
std::string get_level_text(PRORESLEVEL level)
{
    LEVEL_MAP::const_iterator it = search_by_value(prores_level_map, level);
    if (it != prores_level_map.cend())
    {
        return it->first;
    }
    return "INVALID";
}

/**
 * @brief Get HLS segment duration. Input value must be in seconds.
 * @param[in] arg - Segment duration in seconds. Must be greater than 0.
 * @param[out] value - Upon return contains segment duration in AV_TIME_BASE units.
 * @return Returns 0 if valid; if out of range returns -1.
 */
static int get_segment_duration(const std::string & arg, int64_t *value)
{
    double duration;
    if (get_value(arg, &duration) < 0)
    {
        return -1;
    }

    if (*value <= 0)
    {
        std::fprintf(stderr, "INVALID PARAMETER: segment_duration %.1f is out of range. For obvious reasons this must be greater than zero.\n", duration);
        return -1;
    }

    *value = static_cast<int>(duration * AV_TIME_BASE);

    return 0;
}

/**
 * @brief Get seek time diff. Input value must be in seconds.
 * @param[in] arg - Segment duration in seconds. Must be greater or equal 0.
 * @param[out] value - Upon return contains seek time diff in AV_TIME_BASE units.
 * @return Returns 0 if valid; if out of range returns -1.
 */
static int get_seek_time_diff(const std::string & arg, int64_t *value)
{
    double duration;
    if (get_value(arg, &duration) < 0)
    {
        return -1;
    }

    if (*value <= 0)
    {
        std::fprintf(stderr, "INVALID PARAMETER: seek time %.1f is out of range. For obvious reasons this must be greater than or equal zero.\n", duration);
        return -1;
    }

    *value = static_cast<int>(duration * AV_TIME_BASE);

    return 0;
}

/**
 * @brief Get type of hardware acceleration.
 * To keep it simple, currently all values are accepted.
 * @param[in] arg - One of the hardware acceleration types, e.g. VAAPI.
 * @param[out] hwaccel_API - Upon return contains the hardware acceleration API.
 * @param[out] hwaccel_device_type - Upon return contains the hardware acceleration device type.
 * @return Returns 0 if found; if not found returns -1.
 */

static int get_hwaccel(const std::string & arg, HWACCELAPI *hwaccel_API, AVHWDeviceType *hwaccel_device_type)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::string data(arg.substr(pos + 1));

        HWACCEL_MAP::const_iterator it = hwaccel_map.find(data);

        if (it == hwaccel_map.cend())
        {
            std::fprintf(stderr, "INVALID PARAMETER (%s): Invalid hardware acceleration API: %s\n", param.c_str(), data.c_str());

            list_options("Valid hardware acceleration APIs are", hwaccel_map);

            return -1;
        }

        const HWACCEL & hwaccel = it->second;

        if (!hwaccel.m_supported)
        {
            std::fprintf(stderr, "INVALID PARAMETER (%s): Unsupported hardware acceleration API: %s\n", param.c_str(), data.c_str());
            return -1;
        }

        *hwaccel_API            = hwaccel.m_hwaccel_API;
        *hwaccel_device_type    = hwaccel.m_hwaccel_device_type;
        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

/**
 * @brief Get AVCodecID for codec string
 * @param[in] codec - Codec string
 * @param[out] codec_id - AVCodecID of codec string
 * @return Returns 0 if found; if not found returns -1 and codec_id set to AV_CODEC_ID_NONE.
 */
static int get_codec(const std::string & codec, AVCodecID *codec_id)
{
    CODEC_MAP::const_iterator it = hwaccel_codec_map.find(codec);

    if (it == hwaccel_codec_map.cend())
    {
        std::fprintf(stderr, "INVALID PARAMETER: Unknown codec '%s'.\n", codec.c_str());

        list_options("Valid hardware acceleration APIs are", hwaccel_codec_map);

        *codec_id = AV_CODEC_ID_NONE;
        return -1;
    }

    *codec_id = it->second;

    return 0;
}

/**
 * @brief Get list of codecs and optional profiles blocked for hardware accelerated decoding
 * @param[in] arg - Parameter with codec string and optional profile
 * @param[out] hwaccel_dec_blocked - Map with blocked codecs and profiles. Will be allocated if necessary.
 * @return Returns 0 on success; on error returns -1.
 */
static int get_hwaccel_dec_blocked(const std::string & arg, HWACCEL_BLOCKED_MAP **hwaccel_dec_blocked)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string param(arg.substr(0, pos));
        std::stringstream data(arg.substr(pos + 1));
        std::string codec;

        if (*hwaccel_dec_blocked == nullptr)
        {
            *hwaccel_dec_blocked = new (std::nothrow) HWACCEL_BLOCKED_MAP;
        }

        if (!std::getline(data, codec, ':'))
        {
            std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", param.c_str());
            return -1;
        }

        AVCodecID codec_id;

        if (get_codec(codec, &codec_id))
        {
            std::fprintf(stderr, "INVALID PARAMETER (%s): Unknown codec '%s'\n", param.c_str(), codec.c_str());
            return -1;
        }

        int nProfilesFound = 0;
        for (std::string profile; std::getline(data, profile, ':');)
        {
            nProfilesFound++;
            // Block codec and profile
            (*hwaccel_dec_blocked)->insert(std::pair<AVCodecID, int>(codec_id, std::stoi(profile)));
        }

        if (!nProfilesFound)
        {
            // No profile
            (*hwaccel_dec_blocked)->insert(std::pair<AVCodecID, int>(codec_id, FF_PROFILE_UNKNOWN));
        }

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

bool check_hwaccel_dec_blocked(AVCodecID codec_id, int profile)
{
    if (params.m_hwaccel_dec_blocked == nullptr)
    {
        return false;   // Nothing blocked
    }

    for (HWACCEL_BLOCKED_MAP::const_iterator it = params.m_hwaccel_dec_blocked->find(codec_id); it != params.m_hwaccel_dec_blocked->cend(); ++it)
    {
        if (it->first == codec_id && (it->second == profile || it->second == FF_PROFILE_UNKNOWN))
        {
            return true;
        }
    }

    return false;
}

std::string get_hwaccel_API_text(HWACCELAPI hwaccel_API)
{
    HWACCEL_MAP::const_iterator it = hwaccel_map.cbegin();
    while (it != hwaccel_map.cend())
    {
        if (it->second.m_hwaccel_API == hwaccel_API)
        {
            return it->first;
        }
        ++it;
    }

    return "INVALID";
}

/**
 * @brief Get value from command line string.
 * Finds whatever is after the "=" sign.
 * @param[in] arg - Command line option.
 * @param[in] value - Upon return, contains the value after the "=" sign.
 * @return Returns 0 if valid; if invalid returns -1.
 */
static int get_value(const std::string & arg, int *value)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        *value = std::stoi(arg.substr(pos + 1));

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

/**
 * @brief Get value from command line string.
 * Finds whatever is after the "=" sign.
 * @param[in] arg - Command line option.
 * @param[in] value - Upon return, contains the value after the "=" sign.
 * @return Returns 0 if valid; if invalid returns -1.
 */
static int get_value(const std::string & arg, std::string *value)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        *value = arg.substr(pos + 1);

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

/**
 * @brief Get comma separated values from command line string.
 * Finds whatever is after the "=" sign.
 * @param[in] arg - Command line option.
 * @param[in] value - Upon return, contains a set of the values after the "=" sign.
 * @return Returns 0 if valid; if invalid returns -1.
 */
static int get_value(const std::string & arg, MATCHVEC *value)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::vector<std::string> v = split(arg.substr(pos + 1), ",");

        for (const std::string & str : v)
        {
            int res = fnmatch(str.c_str(), "", 0);

            if (res != 0 && res != FNM_NOMATCH)
            {
                std::fprintf(stderr, "INVALID PARAMETER (%s): Error in wildcard pattern\n", str.c_str());

                return -1;
            }
        }

        value->insert(value->end(), v.begin(), v.end());

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

#if 0
/**
 * @brief Get value from command line string.
 * Finds whatever is after the "=" sign.
 * @param[in] arg - Command line option.
 * @param[in] value - Upon return, contains the value after the "=" sign.
 * @return Returns 0 if valid; if invalid returns -1.
 */
static int get_value(const std::string & arg, std::optional<std::string> *value)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        *value = arg.substr(pos + 1);

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}
#endif

/**
 * @brief Get value from command line string.
 * Finds whatever is after the "=" sign.
 * @param[in] arg - Command line option.
 * @param[in] value - Upon return, contains the value after the "=" sign.
 * @return Returns 0 if valid; if invalid returns -1.
 */
static int get_value(const std::string & arg, double *value)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        *value = std::stof(arg.substr(pos + 1));

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER (%s): Missing argument\n", arg.c_str());

    return -1;
}

/**
 * @brief FUSE option parsing function.
 * @param[in] data - is the user data passed to the fuse_opt_parse() function
 * @param[in] arg - is the whole argument or option
 * @param[in] key - determines why the processing function was called
 * @param[in] outargs - the current output argument list
 * @return -1 on error, 0 if arg is to be discarded, 1 if arg should be kept
 */
static int ffmpegfs_opt_proc(__attribute__((unused)) void* data, const char* arg, int key, struct fuse_args *outargs)
{
    switch (key)
    {
    case FUSE_OPT_KEY_NONOPT:
    {
        static int n;

        // check for basepath and bitrate parameters
        if (n == 0 && params.m_basepath.empty())
        {
            expand_path(&params.m_basepath, arg);
            sanitise_filepath(&params.m_basepath);
            append_sep(&params.m_basepath);
            n++;
            return 0;
        }
        else if (n == 1 && params.m_mountpath.empty())
        {
            expand_path(&params.m_mountpath, arg);
            sanitise_filepath(&params.m_mountpath);
            append_sep(&params.m_mountpath);
            if (!docker_client)
            {
                switch (is_mount(params.m_mountpath))
                {
                case 1:
                {
                    std::fprintf(stderr, "%-25s: already mounted\n", params.m_mountpath.c_str());
                    exit(1);
                }
                    //case -1:
                    //{
                    //  // Error already reported
                    //  exit(1);
                    //}
                }
            }

            n++;
            return 1;
        }

        break;
    }
    case KEY_HELP:
    {
        usage();
#ifndef _WIN32
        // Bug in WinSFP: Help displayed garbled
        fuse_opt_add_arg(outargs, "-ho");
        fuse_main(outargs->argc, outargs->argv, &ffmpegfs_ops, nullptr);
#endif
        exit(1);
    }
    case KEY_VERSION:
    {
        std::printf("-------------------------------------------------------------------------------------------\n");

#ifdef __GNUC__
#ifndef __clang_version__
        std::printf("%-20s: %s (%s)\n", "Built with", "gcc " __VERSION__, HOST_OS);
#else
        std::printf("%-20s: %s (%s)\n", "Built with", "clang " __clang_version__, HOST_OS);
#endif
#endif
        std::printf("%-20s: %s\n\n", "configuration", CONFIGURE_ARGS);

        std::printf("%-20s: %s\n", PACKAGE_NAME " Version", FFMPEFS_VERSION);

        std::printf("%s", ffmpeg_libinfo().c_str());

#ifdef USE_LIBVCD
        std::printf("%-20s: %s\n", "Video CD Library", "enabled");
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
        std::printf("%-20s: %s\n", "DVD Library", "enabled");
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
        std::printf("%-20s: %s\n", "Blu-ray Library", BLURAY_VERSION_STRING);
#endif // USE_LIBBLURAY

        fuse_opt_add_arg(outargs, "--version");
        fuse_main(outargs->argc, outargs->argv, &ffmpegfs_ops, nullptr);

        exit(0);
    }
    case KEY_FFMPEG_CAPS:
    {
        std::printf("-------------------------------------------------------------------------------------------\n\n");

        std::printf("%-20s: %s\n", PACKAGE_NAME " Version", FFMPEFS_VERSION);
        std::printf("%s", ffmpeg_libinfo().c_str());

        std::printf("\nFFMpeg Capabilities\n\n");

        show_caps(0);

        exit(0);
    }
    case KEY_DESTTYPE:
    {
        return get_desttype(arg, ffmpeg_format);
    }
    case KEY_AUDIOCODEC:
    {
        return get_audiocodec(arg, &params.m_audio_codec);
    }
    case KEY_VIDEOCODEC:
    {
        return get_videocodec(arg, &params.m_video_codec);
    }
    case KEY_AUTOCOPY:
    {
        return get_autocopy(arg, &params.m_autocopy);
    }
    case KEY_RECODESAME:
    {
        return get_recodesame(arg, &params.m_recodesame);
    }
    case KEY_PROFILE:
    {
        return get_profile(arg, &params.m_profile);
    }
    case KEY_LEVEL:
    {
        return get_level(arg, &params.m_level);
    }
    case KEY_AUDIO_BITRATE:
    {
        return get_bitrate(arg, &params.m_audiobitrate);
    }
    case KEY_AUDIO_SAMPLERATE:
    {
        return get_samplerate(arg, &params.m_audiosamplerate);
    }
    case KEY_AUDIO_CHANNELS:
    {
        return get_value(arg, &params.m_audiochannels);
    }
    case KEY_AUDIO_SAMPLE_FMT:
    {
        return get_sampleformat(arg, &params.m_sample_fmt);
    }
    case KEY_SCRIPTFILE:
    {
        return get_value(arg, &params.m_scriptfile);
    }
    case KEY_SCRIPTSOURCE:
    {
        return get_value(arg, &params.m_scriptsource);
    }
    case KEY_VIDEO_BITRATE:
    {
        return get_bitrate(arg, &params.m_videobitrate);
    }
    case KEY_SEGMENT_DURATION:
    {
        return get_segment_duration(arg, &params.m_segment_duration);
    }
    case KEY_MIN_SEEK_TIME_DIFF:
    {
        return get_seek_time_diff(arg, &params.m_min_seek_time_diff);
    }
    case KEY_HWACCEL_ENCODER_API:
    {
        return get_hwaccel(arg, &params.m_hwaccel_enc_API, &params.m_hwaccel_enc_device_type);
    }
    case KEY_HWACCEL_ENCODER_DEVICE:
    {
        return get_value(arg, &params.m_hwaccel_enc_device);
    }
    case KEY_HWACCEL_DECODER_API:
    {
        return get_hwaccel(arg, &params.m_hwaccel_dec_API, &params.m_hwaccel_dec_device_type);
    }
    case KEY_HWACCEL_DECODER_DEVICE:
    {
        return get_value(arg, &params.m_hwaccel_dec_device);
    }
    case KEY_HWACCEL_DECODER_BLOCKED:
    {
        return get_hwaccel_dec_blocked(arg, &params.m_hwaccel_dec_blocked);
    }
    case KEY_EXPIRY_TIME:
    {
        return get_time(arg, &params.m_expiry_time);
    }
    case KEY_MAX_INACTIVE_SUSPEND_TIME:
    {
        return get_time(arg, &params.m_max_inactive_suspend);
    }
    case KEY_MAX_INACTIVE_ABORT_TIME:
    {
        return get_time(arg, &params.m_max_inactive_abort);
    }
    case KEY_PREBUFFER_TIME:
    {
        return get_time(arg, &params.m_prebuffer_time);
    }
    case KEY_PREBUFFER_SIZE:
    {
        return get_size(arg, &params.m_prebuffer_size);
    }
    case KEY_MAX_CACHE_SIZE:
    {
        return get_size(arg, &params.m_max_cache_size);
    }
    case KEY_MIN_DISKSPACE_SIZE:
    {
        return get_size(arg, &params.m_min_diskspace);
    }
    case KEY_CACHEPATH:
    {
        return get_value(arg, &params.m_cachepath);
    }
    case KEY_CACHE_MAINTENANCE:
    {
        return get_time(arg, &params.m_cache_maintenance);
    }
    case KEY_LOG_MAXLEVEL:
    {
        return get_value(arg, &params.m_log_maxlevel);
    }
    case KEY_LOGFILE:
    {
        std::string logfile;
        int res = get_value(arg, &logfile);

        if (res)
        {
            return res;
        }

        expand_path(&params.m_logfile, logfile);
        sanitise_filepath(&params.m_logfile);

        return 0;
    }
    case KEY_INCLUDE_EXTENSIONS:
    {
        return get_value(arg, params.m_include_extensions.get());
    }
    case KEY_HIDE_EXTENSIONS:
    {
        return get_value(arg, params.m_hide_extensions.get());
    }
    }

    return 1;
}

/**
 * @brief Set default values.
 * @return Returns true if options are OK, false if option combination is invalid.
 */
static bool set_defaults()
{
    if (ffmpeg_format[FORMAT::VIDEO].video_codec() == AV_CODEC_ID_PRORES)
    {
        if (params.m_level == PRORESLEVEL::NONE)
        {
            params.m_level = PRORESLEVEL::PRORES_HQ;
        }
    }

    return true;
}

/**
 * @brief Build list of available device types.
 * Builds a list of device types supported by the current
 * FFmpeg libary.
 */
static void build_device_type_list()
{
    for (AVHWDeviceType device_type = AV_HWDEVICE_TYPE_NONE; (device_type = av_hwdevice_iterate_types(device_type)) != AV_HWDEVICE_TYPE_NONE;)
    {
        HWACCEL_MAP::iterator it = hwaccel_map.find(av_hwdevice_get_type_name(device_type));

        if (it == hwaccel_map.end())
        {
            continue;
        }

        it->second.m_hwaccel_device_type = device_type;
    }
}

/**
 * @brief Print currently selected parameters.
 */
static void print_params()
{
    std::string cachepath;

    transcoder_cache_path(&cachepath);

    Logging::trace(nullptr, "********* " PACKAGE_NAME " Options *********");
    Logging::trace(nullptr, "Base Path         : %1", params.m_basepath.c_str());
    Logging::trace(nullptr, "Mount Path        : %1", params.m_mountpath.c_str());
    Logging::trace(nullptr, "--------- Format ---------");
    if (ffmpeg_format[FORMAT::AUDIO].filetype() != FILETYPE::UNKNOWN)
    {
        Logging::trace(nullptr, "Audio File Type   : %1", ffmpeg_format[FORMAT::AUDIO].desttype().c_str());
        if (ffmpeg_format[FORMAT::AUDIO].audio_codec() != AV_CODEC_ID_NONE)
        {
            Logging::trace(nullptr, "Audio Codec       : %1 (%2)", get_codec_name(ffmpeg_format[FORMAT::AUDIO].audio_codec(), false), get_codec_name(ffmpeg_format[FORMAT::AUDIO].audio_codec(), true));
        }
        Logging::trace(nullptr, "Video File Type   : %1", ffmpeg_format[FORMAT::VIDEO].desttype().c_str());
        if (ffmpeg_format[FORMAT::VIDEO].audio_codec() != AV_CODEC_ID_NONE)
        {
            Logging::trace(nullptr, "Audio Codec       : %1 (%2)", get_codec_name(ffmpeg_format[FORMAT::VIDEO].audio_codec(), false), get_codec_name(ffmpeg_format[FORMAT::VIDEO].audio_codec(), true));
        }
        if (ffmpeg_format[FORMAT::VIDEO].video_codec() != AV_CODEC_ID_NONE)
        {
            Logging::trace(nullptr, "Video Codec       : %1 (%2)", get_codec_name(ffmpeg_format[FORMAT::VIDEO].video_codec(), false), get_codec_name(ffmpeg_format[FORMAT::VIDEO].video_codec(), true));
        }
    }
    else
    {
        Logging::trace(nullptr, "File Type         : %1", ffmpeg_format[FORMAT::VIDEO].desttype().c_str());
        if (ffmpeg_format[FORMAT::VIDEO].audio_codec() != AV_CODEC_ID_NONE)
        {
            Logging::trace(nullptr, "Audio Codec       : %1 (%2)", get_codec_name(ffmpeg_format[FORMAT::VIDEO].audio_codec(), false), get_codec_name(ffmpeg_format[FORMAT::VIDEO].audio_codec(), true));
        }
        if (ffmpeg_format[FORMAT::VIDEO].video_codec() != AV_CODEC_ID_NONE)
        {
            Logging::trace(nullptr, "Video Codec       : %1 (%2)", get_codec_name(ffmpeg_format[FORMAT::VIDEO].video_codec(), false), get_codec_name(ffmpeg_format[FORMAT::VIDEO].video_codec(), true));
        }
    }
    Logging::trace(nullptr, "Smart Transcode   : %1", params.smart_transcode() ? "yes" : "no");
    Logging::trace(nullptr, "Auto Copy         : %1", get_autocopy_text(params.m_autocopy).c_str());
    Logging::trace(nullptr, "Recode to same fmt: %1", get_recodesame_text(params.m_recodesame).c_str());
    Logging::trace(nullptr, "Profile           : %1", get_profile_text(params.m_profile).c_str());
    Logging::trace(nullptr, "Level             : %1", get_level_text(params.m_level).c_str());
    Logging::trace(nullptr, "Include Extensions: %1", implode(*params.m_include_extensions).c_str());
    Logging::trace(nullptr, "Hide Extensions   : %1", implode(*params.m_hide_extensions).c_str());
    Logging::trace(nullptr, "--------- Audio ---------");
    Logging::trace(nullptr, "Codecs            : %1+%2", get_codec_name(ffmpeg_format[FORMAT::VIDEO].audio_codec(), true), get_codec_name(ffmpeg_format[FORMAT::AUDIO].audio_codec(), true));
    Logging::trace(nullptr, "Bitrate           : %1", format_bitrate(params.m_audiobitrate).c_str());
    Logging::trace(nullptr, "Sample Rate       : %1", format_samplerate(params.m_audiosamplerate).c_str());
    Logging::trace(nullptr, "Max. Channels     : %1", params.m_audiochannels);
    if (params.m_sample_fmt != SAMPLE_FMT::FMT_DONTCARE)
    {
        Logging::trace(nullptr, "Sample Format     : %1", get_sampleformat_text(params.m_sample_fmt).c_str());
    }
    Logging::trace(nullptr, "--------- Video ---------");
    Logging::trace(nullptr, "Codec             : %1", get_codec_name(ffmpeg_format[FORMAT::VIDEO].video_codec(), true));
    Logging::trace(nullptr, "Bitrate           : %1", format_bitrate(params.m_videobitrate).c_str());
    Logging::trace(nullptr, "Dimension         : width=%1 height=%2", format_number(params.m_videowidth).c_str(), format_number(params.m_videoheight).c_str());
    Logging::trace(nullptr, "Deinterlace       : %1", params.m_deinterlace ? "yes" : "no");
    Logging::trace(nullptr, "--------- HLS Options ---------");
    Logging::trace(nullptr, "Segment Duration  : %1", format_time(static_cast<time_t>(params.m_segment_duration / AV_TIME_BASE)).c_str());
    Logging::trace(nullptr, "Seek Time Diff    : %1", format_time(static_cast<time_t>(params.m_min_seek_time_diff / AV_TIME_BASE)).c_str());
    Logging::trace(nullptr, "---- Hardware Acceleration ----");
    Logging::trace(nullptr, "Hardware Decoder:");
    Logging::trace(nullptr, "API               : %1", get_hwaccel_API_text(params.m_hwaccel_dec_API).c_str());
    Logging::trace(nullptr, "Frame Buffering   : %1", av_hwdevice_get_type_name(params.m_hwaccel_dec_device_type));
    Logging::trace(nullptr, "Device            : %1", params.m_hwaccel_dec_device.c_str());
    Logging::trace(nullptr, "Hardware Encoder:");
    Logging::trace(nullptr, "API               : %1", get_hwaccel_API_text(params.m_hwaccel_enc_API).c_str());
    Logging::trace(nullptr, "Frame Buffering   : %1", av_hwdevice_get_type_name(params.m_hwaccel_enc_device_type));
    Logging::trace(nullptr, "Device            : %1", params.m_hwaccel_enc_device.c_str());
    Logging::trace(nullptr, "--------- Subtitles ---------");
    Logging::trace(nullptr, "No subtitles      : %1", params.m_no_subtitles ? "yes" : "no");
    Logging::trace(nullptr, "--------- Virtual Script ---------");
    Logging::trace(nullptr, "Create script     : %1", params.m_enablescript ? "yes" : "no");
    Logging::trace(nullptr, "Script file name  : %1", params.m_scriptfile.c_str());
    Logging::trace(nullptr, "Input file        : %1", params.m_scriptsource.c_str());
    Logging::trace(nullptr, "--------- Logging ---------");
    Logging::trace(nullptr, "Max. Log Level    : %1", params.m_log_maxlevel.c_str());
    Logging::trace(nullptr, "Log to stderr     : %1", params.m_log_stderr ? "yes" : "no");
    Logging::trace(nullptr, "Log to syslog     : %1", params.m_log_syslog ? "yes" : "no");
    Logging::trace(nullptr, "Logfile           : %1", !params.m_logfile.empty() ? params.m_logfile.c_str() : "none");
    Logging::trace(nullptr, "--------- Cache Settings ---------");
    Logging::trace(nullptr, "Expiry Time       : %1", format_time(params.m_expiry_time).c_str());
    Logging::trace(nullptr, "Inactivity Suspend: %1", format_time(params.m_max_inactive_suspend).c_str());
    Logging::trace(nullptr, "Inactivity Abort  : %1", format_time(params.m_max_inactive_abort).c_str());
    Logging::trace(nullptr, "Pre-buffer Time   : %1", format_time(params.m_prebuffer_time).c_str());
    Logging::trace(nullptr, "Pre-buffer Size   : %1", format_size(params.m_prebuffer_size).c_str());
    Logging::trace(nullptr, "Max. Cache Size   : %1", format_size(params.m_max_cache_size).c_str());
    Logging::trace(nullptr, "Min. Disk Space   : %1", format_size(params.m_min_diskspace).c_str());
    Logging::trace(nullptr, "Cache Path        : %1", cachepath.c_str());
    Logging::trace(nullptr, "Disable Cache     : %1", params.m_disable_cache ? "yes" : "no");
    Logging::trace(nullptr, "Maintenance Timer : %1", params.m_cache_maintenance ? format_time(params.m_cache_maintenance).c_str() : "inactive");
    Logging::trace(nullptr, "Clear Cache       : %1", params.m_clear_cache ? "yes" : "no");
    Logging::trace(nullptr, "--------- Various Options ---------");
    Logging::trace(nullptr, "Remove Album Arts : %1", params.m_noalbumarts ? "yes" : "no");
    Logging::trace(nullptr, "Max. Threads      : %1", format_number(params.m_max_threads).c_str());
    Logging::trace(nullptr, "Decoding Errors   : %1", params.m_decoding_errors ? "break transcode" : "ignore");
    Logging::trace(nullptr, "Min. DVD Chapter  : %1", format_duration(params.m_min_dvd_chapter_duration * AV_TIME_BASE).c_str());
    Logging::trace(nullptr, "Old Name Scheme   : %1", params.m_oldnamescheme ? "yes" : "no");
    Logging::trace(nullptr, "--------- Experimental Options ---------");
    Logging::trace(nullptr, "Windows 10 Fix    : %1", params.m_win_smb_fix ? "SMB Lockup Fix Active" : "inactive");
}

/**
 * @brief Custom FFmpeg log function. Used with av_log_set_callback().
 * @param[in] ptr - See av_log_set_callback() in FFmpeg API.
 * @param[in] level - See av_log_set_callback() in FFmpeg API.
 * @param[in] fmt - See av_log_set_callback() in FFmpeg API.
 * @param[in] vl - See av_log_set_callback() in FFmpeg API.
 */
static void ffmpeg_log(void *ptr, int level, const char *fmt, va_list vl)
{
    Logging::LOGLEVEL ffmpegfs_level;

    // Map log level
    // AV_LOG_PANIC     0
    // AV_LOG_FATAL     8
    // AV_LOG_ERROR    16
    if (level <= AV_LOG_ERROR)
    {
        ffmpegfs_level = LOGERROR;
    }
    // AV_LOG_WARNING  24
    else if (level <= AV_LOG_WARNING)
    {
        ffmpegfs_level = LOGWARN;
    }
#ifdef AV_LOG_TRACE
    // AV_LOG_INFO     32
    //else if (level <= AV_LOG_INFO)
    //{
    //    ffmpegfs_level = LOGINFO;
    //}
    // AV_LOG_VERBOSE  40
    // AV_LOG_DEBUG    48
    else if (level < AV_LOG_DEBUG)
    {
        ffmpegfs_level = LOGDEBUG;
    }
    // AV_LOG_TRACE    56
    else   // if (level <= AV_LOG_TRACE)
    {
        ffmpegfs_level = LOGTRACE;
    }
#else
    // AV_LOG_INFO     32
    else   // if (level <= AV_LOG_INFO)
    {
        ffmpegfs_level = DEBUG;
    }
#endif

    if (!Logging::show(ffmpegfs_level))
    {
        return;
    }

    va_list vl2;
    static int print_prefix = 1;

#if (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 23, 0))
    char * line;
    int line_size;
    std::string category;

    if (ptr != nullptr)
    {
        AVClass* avc = *(AVClass **)ptr;

        switch (avc->category)
        {
        case AV_CLASS_CATEGORY_NA:
        {
            break;
        }
        case AV_CLASS_CATEGORY_INPUT:
        {
            category = "INPUT   ";
            break;
        }
        case AV_CLASS_CATEGORY_OUTPUT:
        {
            category = "OUTPUT  ";
            break;
        }
        case AV_CLASS_CATEGORY_MUXER:
        {
            category = "MUXER   ";
            break;
        }
        case AV_CLASS_CATEGORY_DEMUXER:
        {
            category = "DEMUXER ";
            break;
        }
        case AV_CLASS_CATEGORY_ENCODER:
        {
            category = "ENCODER ";
            break;
        }
        case AV_CLASS_CATEGORY_DECODER:
        {
            category = "DECODER ";
            break;
        }
        case AV_CLASS_CATEGORY_FILTER:
        {
            category = "FILTER  ";
            break;
        }
        case AV_CLASS_CATEGORY_BITSTREAM_FILTER:
        {
            category = "BITFILT ";
            break;
        }
        case AV_CLASS_CATEGORY_SWSCALER:
        {
            category = "SWSCALE ";
            break;
        }
        case AV_CLASS_CATEGORY_SWRESAMPLER:
        {
            category = "SWRESAM ";
            break;
        }
        default:
        {
            strsprintf(&category, "CAT %3i ", static_cast<int>(avc->category));
            break;
        }
        }
    }

    va_copy(vl2, vl);
    av_log_default_callback(ptr, level, fmt, vl);
    line_size = av_log_format_line2(ptr, level, fmt, vl2, nullptr, 0, &print_prefix);
    if (line_size < 0)
    {
        va_end(vl2);
        return;
    }
    line = static_cast<char *>(av_malloc(static_cast<size_t>(line_size)));
    if (line == nullptr)
    {
        return;
    }
    av_log_format_line2(ptr, level, fmt, vl2, line, line_size, &print_prefix);
    va_end(vl2);
#else
    char line[1024];

    va_copy(vl2, vl);
    av_log_default_callback(ptr, level, fmt, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);
#endif

    Logging::log_with_level(ffmpegfs_level, "", category + line);

#if (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 23, 0))
    av_free(line);
#endif
}

/**
 * @brief Inititalise logging facility
 * @param[in] logfile - Name of log file if file writing is selected.
 * @param[in] max_level - Maximum level to log.
 * @param[in] to_stderr - If true, log to stderr.
 * @param[in] to_syslog - If true, log to syslog.
 * @return Returns true on success; false on error.
 */
static bool init_logging(const std::string &logfile, const std::string & max_level, bool to_stderr, bool to_syslog)
{
    static const std::map<const std::string, const Logging::LOGLEVEL, comp> log_level_map =
    {
        { "ERROR",      LOGERROR },
        { "WARNING",    LOGWARN },
        { "INFO",       LOGINFO },
        { "DEBUG",      LOGDEBUG },
        { "TRACE",      LOGTRACE },
    };

    std::map<const std::string, const Logging::LOGLEVEL, comp>::const_iterator it = log_level_map.find(max_level);

    if (it == log_level_map.cend())
    {
        std::fprintf(stderr, "Invalid logging level string: %s\n", max_level.c_str());
        return false;
    }

    return Logging::init_logging(logfile, it->second, to_stderr, to_syslog);
}

/**
 * @brief Main program entry point.
 * @param[in] argc - Number of command line arguments.
 * @param[in] argv - Command line argument array.
 * @return Return value will be the errorlevel of the executable.
 * Returns 0 on success, 1 on error.
 */
int main(int argc, char *argv[])
{
    int ret;

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    // Check if run from other process group like mount and if so, inhibit startup message
    if (getppid() == getpgid(0))
    {
        std::printf("%s V%s\n", PACKAGE_NAME, FFMPEFS_VERSION);
        std::printf("Copyright (C) 2017-2024 Norbert Schlia (nschlia@oblivion-software.de)\n"
                    "David Collett (2006-2008) and K. Henriksson (2008-2012)\n\n");
    }

    // Check if run under Docker
    docker_client = detect_docker();

    init_fuse_ops();

    // Redirect FFmpeg logs
    av_log_set_callback(ffmpeg_log);

    // Set default
    params.m_max_threads = static_cast<unsigned int>(get_nprocs() * 16);

    // Build list of supported device types
    build_device_type_list();

    if (fuse_opt_parse(&args, &params, ffmpegfs_opts, ffmpegfs_opt_proc))
    {
        std::fprintf(stderr, "\nError parsing command line options.\n\n");
        //usage(argv[0]);
        return 1;
    }

    // Expand cache path
    if (!params.m_cachepath.empty())
    {
        expand_path(&params.m_cachepath, params.m_cachepath);
        append_sep(&params.m_cachepath);
    }

    // Log to the screen, and enable debug messages, if debug is enabled.
    if (params.m_debug)
    {
        params.m_log_stderr = 1;
        params.m_log_maxlevel = "DEBUG";
        av_log_set_level(AV_LOG_INFO); // Do not use AV_LOG_DEBUG; AV_LOG_INFO is chatty enough
    }
    else
    {
        av_log_set_level(AV_LOG_QUIET);
    }

    if (!init_logging(params.m_logfile, params.m_log_maxlevel, params.m_log_stderr ? true : false, params.m_log_syslog ? true : false))
    {
        std::fprintf(stderr, "ERROR: Failed to initialise logging module.\n");
        std::fprintf(stderr, "Maybe log file couldn't be opened for writing?\n\n");
        return 1;
    }

    if (params.m_prune_cache)
    {
        if (args.argc > 1)
        {
            std::fprintf(stderr, "INVALID PARAMETER: Invalid additional parameters for --prune_cache:\n");
            for (int n = 1; n < args.argc; n++)
            {
                std::fprintf(stderr, "Invalid: '%s'\n", args.argv[n]);
            }
            return 1;
        }

        // Prune cache and exit
        if (!transcoder_init())
        {
            return 1;
        }
        transcoder_cache_maintenance();
        return 0;
    }

    if (params.m_basepath.empty())
    {
        std::fprintf(stderr, "INVALID PARAMETER: No valid basepath specified.\n\n");
        return 1;
    }

#ifndef __CYGWIN__
    // No requirement for Windows, must be drive letter in fact
    if (params.m_basepath.front() != '/')
    {
        std::fprintf(stderr, "INVALID PARAMETER: basepath must be an absolute path.\n\n");
        return 1;
    }
#endif  // !__CYGWIN__

    struct stat stbuf;
    if (stat(params.m_basepath.c_str(), &stbuf) != 0 || !S_ISDIR(stbuf.st_mode))
    {
        std::fprintf(stderr, "INVALID PARAMETER: basepath is not a valid directory: %s\n\n", params.m_basepath.c_str());
        return 1;
    }

    if (params.m_mountpath.empty())
    {
        std::fprintf(stderr, "INVALID PARAMETER: No valid mountpath specified.\n\n");
        return 1;
    }

#ifndef __CYGWIN__
    // No requirement for Windows, must be drive letter in fact
    if (params.m_mountpath.front() != '/')
    {
        std::fprintf(stderr, "INVALID PARAMETER: mountpath must be an absolute path.\n\n");
        return 1;
    }

    if (stat(params.m_mountpath.c_str(), &stbuf) != 0 || !S_ISDIR(stbuf.st_mode))
    {
        std::fprintf(stderr, "INVALID PARAMETER: mountpath is not a valid directory: %s\n\n", params.m_mountpath.c_str());
        return 1;
    }
#endif  // !__CYGWIN__

    // Check if sample format is supported
    for (const FFmpegfs_Format & fmt : ffmpeg_format)
    {
        if (fmt.filetype() != FILETYPE::UNKNOWN && !fmt.is_sample_fmt_supported())
        {
            std::fprintf(stderr, "INVALID PARAMETER: %s does not support the sample format %s\n\n", fmt.desttype().c_str(), get_sampleformat_text(params.m_sample_fmt).c_str());
            std::fprintf(stderr, "Supported formats: %s\n\n", fmt.sample_fmt_list().c_str());
            return 1;
        }
    }

    // Check if audio or video codec is supported
    for (const FFmpegfs_Format & fmt : ffmpeg_format)
    {
        if (fmt.filetype() != FILETYPE::UNKNOWN)
        {
            if (params.m_audio_codec != AV_CODEC_ID_NONE && !fmt.is_audio_codec_supported(params.m_audio_codec))
            {
                std::fprintf(stderr, "INVALID PARAMETER: %s does not support audio codec %s\n\n", fmt.desttype().c_str(), get_audio_codec_text(params.m_audio_codec).c_str());
                std::fprintf(stderr, "Supported formats: %s\n\n", fmt.audio_codec_list().c_str());
                return 1;
            }

            if (params.m_video_codec != AV_CODEC_ID_NONE && !fmt.is_video_codec_supported(params.m_video_codec))
            {
                std::fprintf(stderr, "INVALID PARAMETER: %s does not support video codec %s\n\n", fmt.desttype().c_str(), get_video_codec_text(params.m_video_codec).c_str());
                std::fprintf(stderr, "Supported formats: %s\n\n", fmt.video_codec_list().c_str());
                return 1;
            }
        }
    }

    if (!set_defaults())
    {
        return 1;
    }

    if (!transcoder_init())
    {
        return 1;
    }

    print_params();

    if (params.m_clear_cache)
    {
        // Prune cache and exit
        if (!transcoder_cache_clear())
        {
            return 1;
        }
    }

    // start FUSE
    ret = fuse_main(args.argc, args.argv, &ffmpegfs_ops, nullptr);

    fuse_opt_free_args(&args);

    return ret;
}
