/*
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2012 K. Henriksson
 * Copyright (C) 2017-2019 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @brief FFmpeg main function and utilities implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2019 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "ffmpegfs.h"
#include "logging.h"
#include "ffmpegfshelp.h"

#include <sys/sysinfo.h>
#include <sqlite3.h>
#include <unistd.h>

#include <iostream>

#ifdef USE_LIBBLURAY
#include <libbluray/bluray-version.h>
#endif

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
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#ifdef __cplusplus
}
#endif
#pragma GCC diagnostic pop

#include "ffmpeg_utils.h"

FFMPEGFS_PARAMS     params;                     /**< @brief FFmpegfs command line parameters */

FFMPEGFS_PARAMS::FFMPEGFS_PARAMS()
    : m_basepath("")                            // required parameter
    , m_mountpath("")                           // required parameter

    , m_autocopy(AUTOCOPY_OFF)                  // default: off
    , m_profile(PROFILE_NONE)                   // default: no profile
    , m_level(PRORESLEVEL_NONE)                 // default: no level

    // Format
    , m_audiobitrate(128*1024)                  // default: 128 kBit
    , m_audiosamplerate(44100)                  // default: 44.1 kHz

    , m_videobitrate(2*1024*1024)               // default: 2 MBit
    , m_videowidth(0)                           // default: do not change width
    , m_videoheight(0)                          // default: do not change height
    #ifndef USING_LIBAV
    , m_deinterlace(0)                          // default: do not interlace video
    #endif  // !USING_LIBAV
    // Album arts
    , m_noalbumarts(0)                          // default: copy album arts
    // Virtual Script
    , m_enablescript(0)                         // default: no virtual script
    , m_scriptfile("index.php")                 // default name
    , m_scriptsource("scripts/videotag.php")    // default name
    // Other
    , m_debug(0)                                // default: no debug messages
    , m_log_maxlevel("INFO")                    // default: INFO level
    , m_log_stderr(0)                           // default: do not log to stderr
    , m_log_syslog(0)                           // default: do not use syslog
    , m_logfile("")                             // default: none
    // Cache/recoding options
    , m_expiry_time((60*60*24 /* d */) * 7)     // default: 1 week)
    , m_max_inactive_suspend(15)                // default: 15 seconds
    , m_max_inactive_abort(30)                  // default: 30 seconds
    , m_prebuffer_size(100 /* KB */ * 1024)     // default: 100 KB
    , m_max_cache_size(0)                       // default: no limit
    , m_min_diskspace(0)                        // default: no minimum
    , m_cachepath("")                           // default: /var/cache/ffmpegfs
    , m_disable_cache(0)                        // default: enabled
    , m_cache_maintenance((60*60))              // default: prune every 60 minutes
    , m_prune_cache(0)                          // default: Do not prune cache immediately
    , m_clear_cache(0)                          // default: Do not clear cache on startup
    , m_max_threads(0)                          // default: 16 * CPU cores (this value here is overwritten later)
    , m_decoding_errors(0)                      // default: ignore errors
    , m_min_dvd_chapter_duration(1)             // default: 1 second
    , m_win_smb_fix(0)                          // default: no fix
{
}

bool FFMPEGFS_PARAMS::smart_transcode(void) const
{
    return (params.m_format[1].filetype() != FILETYPE_UNKNOWN && params.m_format[0].filetype() != params.m_format[1].filetype());
}

int FFMPEGFS_PARAMS::guess_format_idx(const std::string & filepath) const
{
    AVOutputFormat* oformat = av_guess_format(nullptr, filepath.c_str(), nullptr);

    if (oformat != nullptr)
    {
        if (!params.smart_transcode())
        {
            // Not smart encoding: use first format (video file)
            return 0;
        }
        else
        {
            // Smart transcoding
            if (params.m_format[0].video_codec_id() != AV_CODEC_ID_NONE && oformat->video_codec != AV_CODEC_ID_NONE && !is_album_art(oformat->video_codec))
            {
                // Is a video: use first format (video file)
                return 0;
            }
            else if (params.m_format[1].audio_codec_id() != AV_CODEC_ID_NONE && oformat->audio_codec != AV_CODEC_ID_NONE)
            {
                // For audio only, use second format (audio only file)
                return 1;
            }
        }
    }

    return 0;
}


FFmpegfs_Format * FFMPEGFS_PARAMS::current_format(const std::string & filepath)
{
    LPCVIRTUALFILE virtualfile = find_file(filepath);

    if (virtualfile != nullptr)
    {
        // We know the file
        return current_format(virtualfile);
    }

    // Guess the result
    int format_idx = guess_format_idx(filepath);

    if (format_idx > -1)
    {
        return &m_format[format_idx];
    }

    return nullptr;
}

FFmpegfs_Format *FFMPEGFS_PARAMS::current_format(LPCVIRTUALFILE virtualfile)
{
    if (virtualfile->m_format_idx < 0 || virtualfile->m_format_idx > 1)
    {
        return nullptr;
    }
    return &m_format[virtualfile->m_format_idx];
}

enum
{
    KEY_HELP,
    KEY_VERSION,
    KEY_KEEP_OPT,
    // Intelligent parameters
    KEY_DESTTYPE,
    KEY_AUDIO_BITRATE,
    KEY_AUDIO_SAMPLERATE,
    KEY_VIDEO_BITRATE,
    KEY_SCRIPTFILE,
    KEY_SCRIPTSOURCE,
    KEY_EXPIRY_TIME,
    KEY_MAX_INACTIVE_SUSPEND_TIME,
    KEY_MAX_INACTIVE_ABORT_TIME,
    KEY_PREBUFFER_SIZE,
    KEY_MAX_CACHE_SIZE,
    KEY_MIN_DISKSPACE_SIZE,
    KEY_CACHEPATH,
    KEY_CACHE_MAINTENANCE,
    KEY_AUTOCOPY,
    KEY_PROFILE,
    KEY_LEVEL,
    KEY_LOG_MAXLEVEL,
    KEY_LOGFILE
};

/**
  * Map FFmpegfs options to FUSE parameters
  */
#define FFMPEGFS_OPT(templ, param, value) { templ, offsetof(FFMPEGFS_PARAMS, param), value }

/**
 * FUSE option descriptions
 */
static struct fuse_opt ffmpegfs_opts[] =
{
    // Output type
    FUSE_OPT_KEY("--desttype=%s",                   KEY_DESTTYPE),
    FUSE_OPT_KEY("desttype=%s",                     KEY_DESTTYPE),
    FUSE_OPT_KEY("--profile=%s",                    KEY_PROFILE),
    FUSE_OPT_KEY("profile=%s",                      KEY_PROFILE),
    FUSE_OPT_KEY("--autocopy=%s",                   KEY_AUTOCOPY),
    FUSE_OPT_KEY("autocopy=%s",                     KEY_AUTOCOPY),
    FUSE_OPT_KEY("--level=%s",                      KEY_LEVEL),
    FUSE_OPT_KEY("level=%s",                        KEY_LEVEL),

    // Audio
    FUSE_OPT_KEY("--audiobitrate=%s",               KEY_AUDIO_BITRATE),
    FUSE_OPT_KEY("audiobitrate=%s",                 KEY_AUDIO_BITRATE),
    FUSE_OPT_KEY("--audiosamplerate=%s",            KEY_AUDIO_SAMPLERATE),
    FUSE_OPT_KEY("audiosamplerate=%s",              KEY_AUDIO_SAMPLERATE),

    // Video
    FUSE_OPT_KEY("--videobitrate=%s",               KEY_VIDEO_BITRATE),
    FUSE_OPT_KEY("videobitrate=%s",                 KEY_VIDEO_BITRATE),
    FFMPEGFS_OPT("--videoheight=%u",                m_videoheight, 0),
    FFMPEGFS_OPT("videoheight=%u",                  m_videoheight, 0),
    FFMPEGFS_OPT("--videowidth=%u",                 m_videowidth, 0),
    FFMPEGFS_OPT("videowidth=%u",                   m_videowidth, 0),
#ifndef USING_LIBAV
    FFMPEGFS_OPT("--deinterlace",                   m_deinterlace, 1),
    FFMPEGFS_OPT("deinterlace",                     m_deinterlace, 1),
#endif  // !USING_LIBAV
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
    FFMPEGFS_OPT("--win_smb_fix=%u",                m_win_smb_fix, 0),
    FFMPEGFS_OPT("win_smb_fix=%u",                  m_win_smb_fix, 0),
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
    FUSE_OPT_KEY("-d",                              KEY_KEEP_OPT),
    FUSE_OPT_KEY("debug",                           KEY_KEEP_OPT),
    FUSE_OPT_END
};

typedef std::map<std::string, AUTOCOPY, comp> AUTOCOPY_MAP;     /**< @brief Map command line option to AUTOCOPY enum */
typedef std::map<std::string, PROFILE, comp> PROFILE_MAP;       /**< @brief Map command line option to PROFILE enum  */
typedef std::map<std::string, PRORESLEVEL, comp> LEVEL_MAP;     /**< @brief Map command line option to LEVEL enum  */

/**
  * List of AUTOCOPY options
  */
static const AUTOCOPY_MAP autocopy_map =
{
    { "NONE",           AUTOCOPY_OFF },
    { "MATCH",          AUTOCOPY_MATCH },
    { "MATCHLIMIT",     AUTOCOPY_MATCHLIMIT },
    { "STRICT",         AUTOCOPY_STRICT },
    { "STRICTLIMIT",    AUTOCOPY_STRICTLIMIT },
};

/**
  * List if MP4 profiles
  */
static const PROFILE_MAP profile_map =
{
    { "NONE",           PROFILE_NONE },

    // MP4

    { "FF",             PROFILE_MP4_FF },
    { "EDGE",           PROFILE_MP4_EDGE },
    { "IE",             PROFILE_MP4_IE },
    { "CHROME",         PROFILE_MP4_CHROME },
    { "SAFARI",         PROFILE_MP4_SAFARI },
    { "OPERA",          PROFILE_MP4_OPERA },
    { "MAXTHON",        PROFILE_MP4_MAXTHON },

    // WEBM
};

/**
  * List if ProRes levels.
  */
static const LEVEL_MAP level_map =
{
    // ProRes
    { "PROXY",          PRORESLEVEL_PRORES_PROXY },
    { "LT",             PRORESLEVEL_PRORES_LT },
    { "STANDARD",       PRORESLEVEL_PRORES_STANDARD },
    { "HQ",             PRORESLEVEL_PRORES_HQ },
};

static int          get_bitrate(const std::string & arg, BITRATE *bitrate);
static int          get_samplerate(const std::string & arg, int *samplerate);
static int          get_time(const std::string & arg, time_t *time);
static int          get_size(const std::string & arg, size_t *size);
static int          get_desttype(const std::string & arg, FFmpegfs_Format *video_format, FFmpegfs_Format *audio_format);
static int          get_autocopy(const std::string & arg, AUTOCOPY *autocopy);
static std::string  get_autocopy_text(AUTOCOPY autocopy);
static int          get_profile(const std::string & arg, PROFILE *profile);
static std::string  get_profile_text(PROFILE profile);
static int          get_level(const std::string & arg, PRORESLEVEL *level);
static std::string  get_level_text(PRORESLEVEL level);
static int          get_value(const std::string & arg, std::string *value);

static int          ffmpegfs_opt_proc(void* data, const char* arg, int key, struct fuse_args *outargs);
static bool         set_defaults(void);
static void         print_params(void);
static void         usage();

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
 * @brief Iterate through all elements in map and search for the passed element.
 * @param[in] mapOfWords - map to search.
 * @param[in] value - Search value
 * @return If found, retuns const_iterator to element. Returns mapOfWords.end() if not.
 */
template <typename T>
static typename std::map<std::string, T, comp>::const_iterator search_by_value(const std::map<std::string, T, comp> & mapOfWords, T value)
{
    typename std::map<std::string, T, comp>::const_iterator it = mapOfWords.begin();
    while (it != mapOfWords.end())
    {
        if (it->second == value)
        {
            return it;
        }
        it++;
    }
    return mapOfWords.end();
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
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_bitrate(const std::string & arg, BITRATE *bitrate)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string data(arg.substr(pos + 1));
        int reti;

        // Check for decimal number
        reti = compare(data, "^([1-9][0-9]*|0)?(bps)?$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *bitrate = static_cast<BITRATE>(atol(data.c_str()));
            return 0;   // OK
        }

        // Check for number with optional descimal point and K modifier
        reti = compare(data, "^[1-9][0-9]*(\\.[0-9]+)?K(bps)?$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *bitrate = static_cast<BITRATE>(atof(data.c_str()) * 1000);
            return 0;   // OK
        }

        // Check for number with optional descimal point and M modifier
        reti = compare(data, "^[1-9][0-9]*(\\.[0-9]+)?M(bps)?$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *bitrate = static_cast<BITRATE>(atof(data.c_str()) * 1000000);
            return 0;   // OK
        }

        std::fprintf(stderr, "INVALID PARAMETER: Invalid bit rate '%s'\n", data.c_str());
    }
    else
    {
        std::fprintf(stderr, "INVALID PARAMETER: Invalid bit rate\n");
    }

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
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_samplerate(const std::string & arg, int * samplerate)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string data(arg.substr(pos + 1));
        int reti;

        // Check for decimal number
        reti = compare(data, "^([1-9][0-9]*|0)(Hz)?$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *samplerate = atoi(data.c_str());
            return 0;   // OK
        }

        // Check for number with optional descimal point and K modifier
        reti = compare(data, "^[1-9][0-9]*(\\.[0-9]+)?K(Hz)?$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *samplerate = static_cast<int>(atof(data.c_str()) * 1000);
            return 0;   // OK
        }

        std::fprintf(stderr, "INVALID PARAMETER: Invalid sample rate '%s'\n", data.c_str());
    }
    else
    {
        std::fprintf(stderr, "INVALID PARAMETER: Invalid sample rate\n");
    }

    return -1;
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
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_time(const std::string & arg, time_t *time)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string data(arg.substr(pos + 1));
        int reti;

        // Check for decimal number
        reti = compare(data, "^([1-9][0-9]*|0)?s?$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *time = static_cast<time_t>(atol(data.c_str()));
            return 0;   // OK
        }

        // Check for number with optional descimal point and m modifier
        reti = compare(data, "^[1-9][0-9]*(\\.[0-9]+)?m$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *time = static_cast<time_t>(atof(data.c_str()) * 60);
            return 0;   // OK
        }

        // Check for number with optional descimal point and h modifier
        reti = compare(data, "^[1-9][0-9]*(\\.[0-9]+)?h$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *time = static_cast<time_t>(atof(data.c_str()) * 60 * 60);
            return 0;   // OK
        }

        // Check for number with optional descimal point and d modifier
        reti = compare(data, "^[1-9][0-9]*(\\.[0-9]+)?d$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *time = static_cast<time_t>(atof(data.c_str()) * 60 * 60 * 24);
            return 0;   // OK
        }

        // Check for number with optional descimal point and w modifier
        reti = compare(data, "^[1-9][0-9]*(\\.[0-9]+)?w$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *time = static_cast<time_t>(atof(data.c_str()) * 60 * 60 * 24 * 7);
            return 0;   // OK
        }

        std::fprintf(stderr, "INVALID PARAMETER: Invalid time format '%s'\n", data.c_str());
    }
    else
    {
        std::fprintf(stderr, "INVALID PARAMETER: Invalid time format\n");
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
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_size(const std::string & arg, size_t *size)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::string data(arg.substr(pos + 1));
        int reti;

        // Check for decimal number
        reti = compare(data, "^([1-9][0-9]*|0)?B?$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *size = static_cast<size_t>(atol(data.c_str()));
            return 0;   // OK
        }

        // Check for number with optional descimal point and K/KB modifier
        reti = compare(data, "^[1-9][0-9]*(\\.[0-9]+)?KB?$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *size = static_cast<size_t>(atof(data.c_str()) * 1024);
            return 0;   // OK
        }

        // Check for number with optional descimal point and M/MB modifier
        reti = compare(data, "^[1-9][0-9]*(\\.[0-9]+)?MB?$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *size = static_cast<size_t>(atof(data.c_str()) * 1024 * 1024);
            return 0;   // OK
        }

        // Check for number with optional descimal point and G/GB modifier
        reti = compare(data, "^[1-9][0-9]*(\\.[0-9]+)?GB?$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *size = static_cast<size_t>(atof(data.c_str()) * 1024 * 1024 * 1024);
            return 0;   // OK
        }

        // Check for number with optional descimal point and T/TB modifier
        reti = compare(data, "^[1-9][0-9]*(\\.[0-9]+)?TB?$");

        if (reti == -1)
        {
            return -1;
        }
        else if (!reti)
        {
            *size = static_cast<size_t>(atof(data.c_str()) * 1024 * 1024 * 1024 * 1024);
            return 0;   // OK
        }

        std::fprintf(stderr, "INVALID PARAMETER: Invalid size '%s'\n", data.c_str());
    }
    else
    {
        std::fprintf(stderr, "INVALID PARAMETER: Invalid size\n");
    }

    return -1;
}

/**
 * @brief Get destination type.
 * @param[in] arg - Format as string (MP4, OGG etc.).
 * @param[out] video_format - Selected video format.
 * @param[out] audio_format - Selected audio format.
 * @return Returns 0 if found; if not found returns -1.
 */
int get_desttype(const std::string & arg, FFmpegfs_Format *video_format, FFmpegfs_Format * audio_format)
{
    /** @todo: evaluate this function */
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::vector<std::string> results =  split(arg.substr(pos + 1), "\\+");

        if (results.size() > 0 && results.size() < 3)
        {
            // Check for valid destination type and obtain codecs and file type.
            if (!video_format->init(results[0]))
            {
                std::fprintf(stderr, "INVALID PARAMETER: No codecs available for desttype: %s\n", results[0].c_str());
                return 1;
            }

            if (results.size() == 2)
            {
                if (!audio_format->init(results[1]))
                {
                    std::fprintf(stderr, "INVALID PARAMETER: No codecs available for desttype: %s\n", results[1].c_str());
                    return 1;
                }
            }

            return 0;
        }
    }

    std::fprintf(stderr, "INVALID PARAMETER: Missing destination type string\n");

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
        std::string data(arg.substr(pos + 1));

        auto it = autocopy_map.find(data);

        if (it == autocopy_map.end())
        {
            std::fprintf(stderr, "INVALID PARAMETER: Invalid autocopy option: %s\n", data.c_str());
            return -1;
        }

        *autocopy = it->second;

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER: Missing autocopy string\n");

    return -1;
}

/**
 * @brief Convert AUTOCOPY enum to human readable text.
 * @param[in] autocopy - AUTOCOPY enum value to convert.
 * @return AUTOCOPY enum as text or "INVALID" if not known.
 */
static std::string get_autocopy_text(AUTOCOPY autocopy)
{
    AUTOCOPY_MAP::const_iterator it = search_by_value(autocopy_map, autocopy);
    if (it != autocopy_map.end())
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
        std::string data(arg.substr(pos + 1));

        auto it = profile_map.find(data);

        if (it == profile_map.end())
        {
            std::fprintf(stderr, "INVALID PARAMETER: Invalid profile: %s\n", data.c_str());
            return -1;
        }

        *profile = it->second;

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER: Missing profile string\n");

    return -1;
}

/**
 * @brief Convert PROFILE enum to human readable text.
 * @param[in] profile - PROFILE enum value to convert.
 * @return PROFILE enum as text or "INVALID" if not known.
 */
static std::string get_profile_text(PROFILE profile)
{
    PROFILE_MAP::const_iterator it = search_by_value(profile_map, profile);
    if (it != profile_map.end())
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
        std::string data(arg.substr(pos + 1));

        auto it = level_map.find(data);

        if (it == level_map.end())
        {
            std::fprintf(stderr, "INVALID PARAMETER: Invalid level: %s\n", data.c_str());
            return -1;
        }

        *level = it->second;

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER: Missing level string\n");

    return -1;
}

// Get level text
/**
 * @brief Convert PRORESLEVEL enum to human readable text.
 * @param[in] level - PRORESLEVEL enum value to convert.
 * @return PRORESLEVEL enum as text or "INVALID" if not known.
 */
static std::string get_level_text(PRORESLEVEL level)
{
    LEVEL_MAP::const_iterator it = search_by_value(level_map, level);
    if (it != level_map.end())
    {
        return it->first;
    }
    return "INVALID";
}

/**
 * @brief Get value form command line string.
 * Finds whatever is after the "=" sign.
 * @param[in] arg - Command line option.
 * @param[in] value - Upon returnm, contains the value after the "=" sign.
 * @return Returns 0 if found; if not found returns -1.
 */
static int get_value(const std::string & arg, std::string *value)
{
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        *value = arg.substr(pos + 1);

        return 0;
    }

    std::fprintf(stderr, "INVALID PARAMETER: Missing value\n");

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
static int ffmpegfs_opt_proc(void* data, const char* arg, int key, struct fuse_args *outargs)
{
    static int n;
    (void)data;

    switch (key)
    {
    case FUSE_OPT_KEY_NONOPT:
    {
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

            n++;
            return 1;
        }

        break;
    }
    case KEY_HELP:
    {
        usage();
        fuse_opt_add_arg(outargs, "-ho");
        fuse_main(outargs->argc, outargs->argv, &ffmpegfs_ops, nullptr);
        exit(1);
    }
    case KEY_VERSION:
    {
        /** @todo: Also output this information in debug mode */
        std::printf("-------------------------------------------------------------------------------------------\n");

#ifdef __GNUC__
#ifndef __clang_version__
        std::printf("%-20s: %s (%s)\n", "Built with", "gcc " __VERSION__, HOST_OS);
#else
        std::printf("%-20s: %s (%s)\n", "Built with", "clang " __clang_version__, HOST_OS);
#endif
#endif
        std::printf("%-20s: %s\n\n", "configuration", CONFIGURE_ARGS);

        std::printf("%-20s: %s\n", PACKAGE_NAME " Version", PACKAGE_VERSION);

        std::printf("%s", ffmpeg_libinfo().c_str());

#ifdef USE_LIBVCD
        std::printf("%-20s: %s\n", "Video CD Library", "enabled");
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
        std::printf("%-20s: %s\n", "DVD Library", "enabled");
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
        std::printf("%-20s: %s\n", "Bluray Library", BLURAY_VERSION_STRING);
#endif // USE_LIBBLURAY

        fuse_opt_add_arg(outargs, "--version");
        fuse_main(outargs->argc, outargs->argv, &ffmpegfs_ops, nullptr);

        std::printf("-------------------------------------------------------------------------------------------\n\n");
        std::printf("FFMpeg capabilities\n\n");

        show_formats_devices(0);

        exit(0);
    }
    case KEY_DESTTYPE:
    {
        return get_desttype(arg, &params.m_format[0], &params.m_format[1]);
    }
    case KEY_AUTOCOPY:
    {
        return get_autocopy(arg, &params.m_autocopy);
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
        return get_value(arg, &params.m_logfile);
    }
    }

    return 1;
}

/**
 * @brief Set default values.
 * @return Returns true if options are OK, false if option combination is invalid.
 */
static bool set_defaults(void)
{
    if (params.m_format[0].video_codec_id() == AV_CODEC_ID_PRORES)
    {
        if (params.m_level == PRORESLEVEL_NONE)
        {
            params.m_level = PRORESLEVEL_PRORES_HQ;
        }
    }

    return true;
}

/**
 * @brief Print currently selected parameters.
 */
static void print_params(void)
{
    std::string cachepath;

    transcoder_cache_path(cachepath);

    Logging::trace(nullptr, PACKAGE_NAME " options:\n\n"
                                         "Base Path         : %1\n"
                                         "Mount Path        : %2\n\n"
                                         "Smart Transcode   : %3\n"
                                         "Auto Copy         : %4\n"
                                         "Audio File Type   : %5\n"
                                         "Video File Type   : %6\n"
                                         "Profile           : %7\n"
                                         "Level             : %8\n"
                                         "\nAudio\n\n"
                                         "Audio Codecs      : %9+%10\n"
                                         "Audio Bitrate     : %11\n"
                                         "Audio Sample Rate : %12\n"
                                         "\nVideo\n\n"
                                         "Video Size/Pixels : width=%13 height=%14\n"
                                         "Deinterlace       : %15\n"
                                         "Remove Album Arts : %16\n"
                                         "Video Codec       : %17\n"
                                         "Video Bitrate     : %18\n"
                                         "\nVirtual Script\n\n"
                                         "Create script     : %19\n"
                                         "Script file name  : %20\n"
                                         "Input file        : %21\n"
                                         "\nLogging\n\n"
                                         "Max. Log Level    : %22\n"
                                         "Log to stderr     : %23\n"
                                         "Log to syslog     : %24\n"
                                         "Logfile           : %25\n"
                                         "\nCache Settings\n\n"
                                         "Expiry Time       : %26\n"
                                         "Inactivity Suspend: %27\n"
                                         "Inactivity Abort  : %28\n"
                                         "Pre-buffer size   : %29\n"
                                         "Max. Cache Size   : %20\n"
                                         "Min. Disk Space   : %31\n"
                                         "Cache Path        : %32\n"
                                         "Disable Cache     : %33\n"
                                         "Maintenance Timer : %34\n"
                                         "Clear Cache       : %35\n"
                                         "\nVarious Options\n\n"
                                         "Max. Threads      : %36\n"
                                         "Decoding Errors   : %37\n"
                                         "Min. DVD chapter  : %38\n"
                                         "\nExperimental Options\n\n"
                                         "Windows 10 Fix    : %39\n",
                   params.m_basepath.c_str(),
                   params.m_mountpath.c_str(),
                   params.smart_transcode() ? "yes" : "no",
                   get_autocopy_text(params.m_autocopy).c_str(),
                   params.m_format[1].desttype().c_str(),
            params.m_format[0].desttype().c_str(),
            get_profile_text(params.m_profile).c_str(),
            get_level_text(params.m_level).c_str(),
            get_codec_name(params.m_format[0].audio_codec_id(), true),
            get_codec_name(params.m_format[1].audio_codec_id(), true),
            format_bitrate(params.m_audiobitrate).c_str(),
            format_samplerate(params.m_audiosamplerate).c_str(),
            format_number(params.m_videowidth).c_str(),
            format_number(params.m_videoheight).c_str(),
        #ifndef USING_LIBAV
            params.m_deinterlace ? "yes" : "no",
        #else
            "not supported",
        #endif  // !USING_LIBAV
            params.m_noalbumarts ? "yes" : "no",
            get_codec_name(params.m_format[0].video_codec_id(), true),
            format_bitrate(params.m_videobitrate).c_str(),
            params.m_enablescript ? "yes" : "no",
            params.m_scriptfile.c_str(),
            params.m_scriptsource.c_str(),
            params.m_log_maxlevel.c_str(),
            params.m_log_stderr ? "yes" : "no",
            params.m_log_syslog ? "yes" : "no",
            !params.m_logfile.empty() ? params.m_logfile.c_str() : "none",
            format_time(params.m_expiry_time).c_str(),
            format_time(params.m_max_inactive_suspend).c_str(),
            format_time(params.m_max_inactive_abort).c_str(),
            format_size(params.m_prebuffer_size).c_str(),
            format_size(params.m_max_cache_size).c_str(),
            format_size(params.m_min_diskspace).c_str(),
            cachepath.c_str(),
            params.m_disable_cache ? "yes" : "no",
            params.m_cache_maintenance ? format_time(params.m_cache_maintenance).c_str() : "inactive",
            params.m_clear_cache ? "yes" : "no",
            format_number(params.m_max_threads).c_str(),
            params.m_decoding_errors ? "break transcode" : "ignore",
            format_duration(params.m_min_dvd_chapter_duration * AV_TIME_BASE).c_str(),
            params.m_win_smb_fix ? "inactive" : "SMB Lockup Fix Active");
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
        std::printf("%s V%s\n", PACKAGE_NAME, PACKAGE_VERSION);
        std::printf("Copyright (C) 2006-2008 David Collett\n"
                    "Copyright (C) 2008-2012 K. Henriksson\n"
                    "Copyright (C) 2017-2019 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)\n\n");
    }

    init_fuse_ops();

    // Configure FFmpeg
#if !LAVC_DEP_AV_CODEC_REGISTER
    // register all the codecs
    avcodec_register_all();
#endif // !LAVC_DEP_AV_CODEC_REGISTER
#if !LAVF_DEP_AV_REGISTER
    av_register_all();
#endif // !LAVF_DEP_AV_REGISTER
#if !LAVC_DEP_AV_FILTER_REGISTER
    avfilter_register_all();
#endif // LAVC_DEP_AV_FILTER_REGISTER
#ifndef USING_LIBAV
    // Redirect FFmpeg logs
    av_log_set_callback(ffmpeg_log);
#endif

    // Set default
    params.m_max_threads = static_cast<unsigned int>(get_nprocs() * 16);

    if (fuse_opt_parse(&args, &params, ffmpegfs_opts, ffmpegfs_opt_proc))
    {
        std::fprintf(stderr, "INVALID PARAMETER: Parsing options.\n\n");
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
        // av_log_set_level(AV_LOG_DEBUG);
        av_log_set_level(AV_LOG_INFO);
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

    if (params.m_basepath.front() != '/')
    {
        std::fprintf(stderr, "INVALID PARAMETER: basepath must be an absolute path.\n\n");
        return 1;
    }

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
