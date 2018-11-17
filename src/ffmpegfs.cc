/*
 * FFMPEGFS: A read-only FUSE filesystem which transcodes audio formats
 * (currently FLAC and Ogg Vorbis) to MP3 on the fly when opened and read.
 * See README for more details.
 *
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2012 K. Henriksson
 * Copyright (C) 2017-2018 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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

#include "ffmpegfs.h"
#include "logging.h"

#include <sys/sysinfo.h>
#include <sqlite3.h>
#include <unistd.h>

#ifdef USE_LIBBLURAY
#include <libbluray/bluray-version.h>
#endif

// TODO: Move this elsewehere, so this file can be library agnostic
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

ffmpegfs_params     params;

ffmpegfs_params::ffmpegfs_params()
    : m_basepath("")                            // required parameter
    , m_mountpath("")                           // required parameter

    , m_profile(PROFILE_NONE)                   // default: no profile

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
    , m_cachepath("")                           // default: /tmp
    , m_disable_cache(0)                        // default: enabled
    , m_cache_maintenance((60*60))              // default: prune every 60 minutes
    , m_prune_cache(0)                          // default: Do not prune cache immediately
    , m_clear_cache(0)                          // default: Do not clear cache on startup
    , m_max_threads(0)                          // default: 16 * CPU cores (this value here is overwritten later)
    , m_decoding_errors(0)                      // default: ignore errors
{
}

bool ffmpegfs_params::smart_transcode(void) const
{
    return (params.m_format[1].m_filetype != FILETYPE_UNKNOWN && params.m_format[0].m_filetype != params.m_format[1].m_filetype);
}

int ffmpegfs_params::guess_format_idx(const std::string & filepath) const
{
    AVOutputFormat* format = av_guess_format(nullptr, filepath.c_str(), nullptr);

    if (format != nullptr)
    {
        if (!params.smart_transcode())
        {
            // Not smart encoding: use first format (video file)
            return 0;
        }
        else
        {
            // Smart transcoding
            if (params.m_format[0].m_video_codecid != AV_CODEC_ID_NONE && format->video_codec != AV_CODEC_ID_NONE && !is_album_art(format->video_codec))
            {
                // Is a video: use first format (video file)
                return 0;
            }
            else if (params.m_format[1].m_audio_codecid != AV_CODEC_ID_NONE && format->audio_codec != AV_CODEC_ID_NONE)
            {
                // For audio only, use second format (audio only file)
                return 1;
            }
        }
    }

    return 0;
}


ffmpegfs_format * ffmpegfs_params::current_format(const std::string & filepath)
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

ffmpegfs_format *ffmpegfs_params::current_format(LPCVIRTUALFILE virtualfile)
{
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
    KEY_PROFILE,
    KEY_LOG_MAXLEVEL,
    KEY_LOGFILE
};

#define FFMPEGFS_OPT(templ, param, value) { templ, offsetof(struct ffmpegfs_params, param), value }

static struct fuse_opt ffmpegfs_opts[] =
{
    // Output type
    FUSE_OPT_KEY("--desttype=%s",               KEY_DESTTYPE),
    FUSE_OPT_KEY("desttype=%s",                 KEY_DESTTYPE),
    FUSE_OPT_KEY("--profile=%s",                KEY_PROFILE),
    FUSE_OPT_KEY("profile=%s",                  KEY_PROFILE),

    // Audio
    FUSE_OPT_KEY("--audiobitrate=%s",           KEY_AUDIO_BITRATE),
    FUSE_OPT_KEY("audiobitrate=%s",             KEY_AUDIO_BITRATE),
    FUSE_OPT_KEY("--audiosamplerate=%s",        KEY_AUDIO_SAMPLERATE),
    FUSE_OPT_KEY("audiosamplerate=%s",          KEY_AUDIO_SAMPLERATE),

    // Video
    FUSE_OPT_KEY("--videobitrate=%s",           KEY_VIDEO_BITRATE),
    FUSE_OPT_KEY("videobitrate=%s",             KEY_VIDEO_BITRATE),
    FFMPEGFS_OPT("--videoheight=%u",            m_videoheight, 0),
    FFMPEGFS_OPT("videoheight=%u",              m_videoheight, 0),
    FFMPEGFS_OPT("--videowidth=%u",             m_videowidth, 0),
    FFMPEGFS_OPT("videowidth=%u",               m_videowidth, 0),
#ifndef USING_LIBAV
    FFMPEGFS_OPT("--deinterlace",               m_deinterlace, 1),
    FFMPEGFS_OPT("deinterlace",                 m_deinterlace, 1),
#endif  // !USING_LIBAV
    // Album arts
    FFMPEGFS_OPT("--noalbumarts",               m_noalbumarts, 1),
    FFMPEGFS_OPT("noalbumarts",                 m_noalbumarts, 1),
    // Virtual script
    FFMPEGFS_OPT("--enablescript",              m_enablescript, 1),
    FFMPEGFS_OPT("enablescript",                m_enablescript, 1),
    FUSE_OPT_KEY("--scriptfile=%s",             KEY_SCRIPTFILE),
    FUSE_OPT_KEY("scriptfile=%s",               KEY_SCRIPTFILE),
    FUSE_OPT_KEY("--scriptsource=%s",           KEY_SCRIPTSOURCE),
    FUSE_OPT_KEY("scriptsource=%s",             KEY_SCRIPTSOURCE),
    // Background recoding/caching
    // Cache
    FUSE_OPT_KEY("--expiry_time=%s",            KEY_EXPIRY_TIME),
    FUSE_OPT_KEY("expiry_time=%s",              KEY_EXPIRY_TIME),
    FUSE_OPT_KEY("--max_inactive_suspend=%s",   KEY_MAX_INACTIVE_SUSPEND_TIME),
    FUSE_OPT_KEY("max_inactive_suspend=%s",     KEY_MAX_INACTIVE_SUSPEND_TIME),
    FUSE_OPT_KEY("--max_inactive_abort=%s",     KEY_MAX_INACTIVE_ABORT_TIME),
    FUSE_OPT_KEY("max_inactive_abort=%s",       KEY_MAX_INACTIVE_ABORT_TIME),
    FUSE_OPT_KEY("--prebuffer_size=%s",         KEY_PREBUFFER_SIZE),
    FUSE_OPT_KEY("prebuffer_size=%s",           KEY_PREBUFFER_SIZE),
    FUSE_OPT_KEY("--max_cache_size=%s",         KEY_MAX_CACHE_SIZE),
    FUSE_OPT_KEY("max_cache_size=%s",           KEY_MAX_CACHE_SIZE),
    FUSE_OPT_KEY("--min_diskspace=%s",          KEY_MIN_DISKSPACE_SIZE),
    FUSE_OPT_KEY("min_diskspace=%s",            KEY_MIN_DISKSPACE_SIZE),
    FUSE_OPT_KEY("--cachepath=%s",              KEY_CACHEPATH),
    FUSE_OPT_KEY("cachepath=%s",                KEY_CACHEPATH),
    FFMPEGFS_OPT("--disable_cache",             m_disable_cache, 1),
    FFMPEGFS_OPT("disable_cache",               m_disable_cache, 1),
    FUSE_OPT_KEY("--cache_maintenance=%s",      KEY_CACHE_MAINTENANCE),
    FUSE_OPT_KEY("cache_maintenance=%s",        KEY_CACHE_MAINTENANCE),
    FFMPEGFS_OPT("--prune_cache",               m_prune_cache, 1),
    FFMPEGFS_OPT("--clear_cache",               m_clear_cache, 1),
    FFMPEGFS_OPT("clear_cache",                 m_clear_cache, 1),

    // Other
    FFMPEGFS_OPT("--max_threads=%u",            m_max_threads, 0),
    FFMPEGFS_OPT("max_threads=%u",              m_max_threads, 0),
    FFMPEGFS_OPT("--decoding_errors=%u",        m_decoding_errors, 0),
    FFMPEGFS_OPT("decoding_errors=%u",          m_decoding_errors, 0),
    // ffmpegfs options
    FFMPEGFS_OPT("-d",                          m_debug, 1),
    FFMPEGFS_OPT("debug",                       m_debug, 1),
    FUSE_OPT_KEY("--log_maxlevel=%s",           KEY_LOG_MAXLEVEL),
    FUSE_OPT_KEY("log_maxlevel=%s",             KEY_LOG_MAXLEVEL),
    FFMPEGFS_OPT("--log_stderr",                m_log_stderr, 1),
    FFMPEGFS_OPT("log_stderr",                  m_log_stderr, 1),
    FFMPEGFS_OPT("--log_syslog",                m_log_syslog, 1),
    FFMPEGFS_OPT("log_syslog",                  m_log_syslog, 1),
    FUSE_OPT_KEY("--logfile=%s",                KEY_LOGFILE),
    FUSE_OPT_KEY("logfile=%s",                  KEY_LOGFILE),

    FUSE_OPT_KEY("-h",                          KEY_HELP),
    FUSE_OPT_KEY("--help",                      KEY_HELP),
    FUSE_OPT_KEY("-V",                          KEY_VERSION),
    FUSE_OPT_KEY("--version",                   KEY_VERSION),
    FUSE_OPT_KEY("-d",                          KEY_KEEP_OPT),
    FUSE_OPT_KEY("debug",                       KEY_KEEP_OPT),
    FUSE_OPT_END
};

typedef std::map<std::string, PROFILE, comp> PROFILE_MAP;

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

template <typename T>
static typename std::map<std::string, T>::const_iterator search_by_value(const std::map<std::string, T> & mapOfWords, T value);

static int get_bitrate(const std::string & arg, BITRATE *bitrate);
static int get_samplerate(const std::string & arg, unsigned int *samplerate);
static int get_time(const std::string & arg, time_t *time);
static int get_size(const std::string & arg, size_t *size);
static int get_desttype(const std::string & arg, ffmpegfs_format *video_format, ffmpegfs_format *audio_format);
static int get_profile(const std::string & arg, PROFILE *profile);
static std::string get_profile_text(PROFILE profile);
static int get_value(const std::string & arg, std::string *value);

static int ffmpegfs_opt_proc(void* data, const char* arg, int key, struct fuse_args *outargs);
static void print_params(void);
static void usage(char *name);

static void usage(char *name)
{
    std::printf("Usage: %s [OPTION]... IN_DIR OUT_DIR\n\n", name);
    fputs("Mount IN_DIR on OUT_DIR, converting audio/video files to MP4, MP3, OGG, WEBM, MOV, AIFF, OPUS or WAV upon access.\n"
          "\n"
          "Encoding options:\n"
          "\n"
          "    --desttype=TYPE, -o desttype=TYPE\n"
          "                           Select destination format. 'TYPE' can currently be\n"
          "                           MP4, MP3, OGG, WEBM, MOV, AIFF, OPUS or WAV. To stream videos,\n"
          "                           MP4, OGG, WEBM or MOV must be selected.\n"
          "                           To use the smart transcoding feature, specify a video and audio file\n"
          "                           type, separated by a \"+\" sign. For example, --desttype=mov+aiff will\n"
          "                           convert video files to Apple Quicktime MOV and audio only files to\n"
          "                           AIFF.\n"
          "                           Default: mp4\n"
          "    --profile=NAME, -o profile=NAME\n"
          "                           Set profile for target audience. Currently selectable:\n"
          "                           NONE     no profile\n"
          "                           FF       Firefox\n"
          "                           EDGE     Microsoft Edge or Internet Explorer > 11\n"
          "                           IE       Microsoft Internet Explorer <= 11\n"
          "                           CHROME   Google Chrome\n"
          "                           SAFARI   Apple Safari\n"
          "                           OPERA    Opera\n"
          "                           MAXTHON  Maxthon\n"
          "                           Default: NONE\n"
          "\n", stdout);
    fputs("Audio Options:\n"
          "\n"
          "    --audiobitrate=BITRATE, -o audiobitrate=BITRATE\n"
          "                           Audio encoding bitrate.\n"
          "                           Default: 128 kbit\n"
          "    --audiosamplerate=SAMPLERATE, -o audiosamplerate=SAMPLERATE\n"
          "                           Limits the output sample rate to SAMPLERATE. If the source file\n"
          "                           sample rate is more it will be downsampled automatically.\n"
          "                           Typical values are 8000, 11025, 22050, 44100,\n"
          "                           48000, 96000, 192000. Set to 0 to keep source rate.\n"
          "                           Default: 44.1 kHz\n"
          "\n"
          "BITRATE can be defined as...\n"
          " * n bit/s:  #  or #bps\n"
          " * n kbit/s: #M or #Mbps\n"
          " * n Mbit/s: #M or #Mbps\n"
          "\n"
          "SAMPLERATE can be defined as...n"
          " * In Hz:  #  or #Hzn"
          " * In kHz: #K or #KHzn"
          "\n", stdout);
    fputs("Video Options:\n"
          "\n"
          "    --videobitrate=BITRATE, -o videobitrate=BITRATE\n"
          "                           Video encoding bit rate. Setting this too high or low may\n"
          "                           cause transcoding to fail.\n"
          "                           Default: 2 Mbit\n"
          "    --videoheight=HEIGHT, -o videoheight=HEIGHT\n"
          "                           Sets the height of the transcoded video.\n"
          "                           When the video is rescaled the aspect ratio is\n"
          "                           preserved if --width is not set at the same time.\n"
          "                           Default: keep source video height\n"
          "    --videowidth=WIDTH, -o videowidth=WIDTH\n"
          "                           Sets the width of the transcoded video.\n"
          "                           When the video is rescaled the aspect ratio is\n"
          "                           preserved if --height is not set at the same time.\n"
          "                           Default: keep source video width\n"
          "    --deinterlace, -o deinterlace\n"
          "                           Deinterlace video if necessary while transcoding.\n"
          "                           May need higher bit rate, but will increase picture quality\n"
          "                           when streaming via HTML5.\n"
          "                           Default: no deinterlace\n"
          "\n"
          "BITRATE can be defined as...\n"
          " * n bit/s:  #  or #bps\n"
          " * n kbit/s: #M or #Mbps\n"
          " * n Mbit/s: #M or #Mbps\n"
          "\n", stdout);
    fputs("Album Arts:\n"
          "\n"
          "    --noalbumarts, -o noalbumarts\n"
          "                           Do not copy album arts into output file.\n"
          "                           This will reduce the file size, may be useful when streaming via\n"
          "                           HTML5 when album arts are not used anyway.\n"
          "                           Default: add album arts\n"
          "\n"
          "Virtual Script:\n"
          "\n"
          "     --enablescript, -o enablescript\n"
          "                           Added --enablescript option that will add virtual index.php to every\n"
          "                           directory. It reads scripts/videotag.php from the ffmpegs binary directory.\n"
          "                           This can be very handy to test video playback. Of course, feel free to\n"
          "                           replace videotag.php with your own script.\n"
          "                           Default: Do not generate script file\n"
          "     --scriptfile, -o scriptfile\n"
          "                           Set the name of the virtual script created in each directory.\n"
          "                           Default: index.php\n"
          "     --scriptsource, -o scriptsource\n"
          "                           Take a different source file.\n"
          "                           Default: scripts/videotag.php\n"
          "\n", stdout);
    fputs("Cache Options:\n"
          "\n"
          "     --expiry_time=TIME, -o expiry_time=TIME\n"
          "                           Cache entries expire after TIME and will be deleted\n"
          "                           to save disk space.\n"
          "                           Default: 1 week\n"
          "     --max_inactive_suspend=TIME, -o max_inactive_suspend=TIME\n"
          "                           While being accessed the file is transcoded to the target format\n"
          "                           in the background. When the client quits transcoding will continue\n"
          "                           until this time out. Transcoding is suspended until it is\n"
          "                           accessed again, and transcoding will continue.\n"
          "                           Default: 2 minutes\n"
          "     --max_inactive_abort=TIME, -o max_inactive_abort=TIME\n"
          "                           While being accessed the file is transcoded to the target format\n"
          "                           in the background. When the client quits transcoding will continue\n"
          "                           until this time out, and the transcoder thread quits\n"
          "                           Default: 5 minutes\n"
          "     --prebuffer_size=SIZE, -o prebuffer_size*=SIZE\n"
          "                           Files will be decoded until the buffer contains this much bytes\n"
          "                           allowing playback to start smoothly without lags.\n"
          "                           Set to 0 to disable pre-buffering.\n"
          "                           Default: 100 KB\n"
          "     --max_cache_size=SIZE, -o max_cache_size=SIZE\n"
          "                           Set the maximum diskspace used by the cache. If the cache would grow\n"
          "                           beyond this limit when a file is transcoded, old entries will be deleted\n"
          "                           to keep the cache within the size limit.\n"
          "                           Default: unlimited\n"
          "     --min_diskspace=SIZE, -o min_diskspace=SIZE\n"
          "                           Set the required diskspace on the cachepath mount. If the remaining\n"
          "                           space would fall below SIZE when a file is transcoded, old entries will\n"
          "                           be deleted to keep the diskspace within the limit.\n"
          "                           Default: 0 (no minimum space)\n"
          "     --cachepath=DIR, -o cachepath=DIR\n"
          "                           Sets the disk cache directory to DIR. Will be created if not existing.\n"
          "                           The user running ffmpegfs must have write access to the location.\n"
          "                           Default: temp directory, e.g. /tmp\n"
          "     --disable_cache, -o disable_cache\n"
          "                           Disable the cache functionality.\n"
          "                           Default: enabled\n"
          "     --cache_maintenance=TIME, -o cache_maintenance=TIME\n"
          "                           Starts cache maintenance in TIME intervals. This will enforce the expery_time,\n"
          "                           max_cache_size and min_diskspace settings. Do not set too low as this will slow\n"
          "                           down transcoding.\n"
          "                           Only one ffmpegfs process will do the maintenance.\n"
          "                           Default: 1 hour\n"
          "     --prune_cache\n"
          "                           Prune cache immediately according to the above settings.\n"
          "     --clear-cache, -o clear-cache\n"
          "                           Clear cache on startup. All previously recoded files will be deleted.\n"
          "\n"
          "TIME can be defined as...\n"
          " * Seconds: #\n"
          " * Minutes: #m\n"
          " * Hours:   #h\n"
          " * Days:    #d\n"
          " * Weeks:   #w\n"
          "\n"
          "SIZE can be defined as...\n"
          " * n bytes:  # or #B\n"
          " * n KBytes: #K or #KB\n"
          " * n MBytes: #B or #MB\n"
          " * n GBytes: #G or #GB\n"
          " * n TBytes: #T or #TB\n"
          "\n", stdout);
    fputs("Other:\n"
          "\n"
          "     --max_threads=COUNT, -o max_threads=COUNT\n"
          "                           Limit concurrent transcoder threads. Set to 0 for unlimited threads.\n"
          "                           Recommended values are up to 16 times number of CPU cores.\n"
          "                           Default: 16 times number of detected cpu cores\n"
          "     --decoding_errors, -o decoding_errors\n"
          "                           Decoding errors are normally ignored, leaving bloopers and hiccups in\n"
          "                           encoded audio or video but yet creating a valid file. When this option\n"
          "                           is set, transcoding will stop with an error.\n"
          "                           Default: Ignore errors\n"
          "\n"
          "Logging:\n"
          "\n"
          "    --log_maxlevel=LEVEL, -o log_maxlevel=LEVEL\n"
          "                           Maximum level of messages to log, either ERROR, WARNING, INFO, DEBUG\n"
          "                           or TRACE. Defaults to INFO, and always set to DEBUG in debug mode.\n"
          "                           Note that the other log flags must also be set to enable logging.\n"
          "    --log_stderr, -o log_stderr\n"
          "                           Enable outputting logging messages to stderr.\n"
          "                           Enabled in debug mode.\n"
          "    --log_syslog, -o log_syslog\n"
          "                           Enable outputting logging messages to syslog.\n"
          "    --logfile=FILE, -o logfile=FILE\n"
          "                           File to output log messages to. By default, no\n"
          "                           file will be written.\n"
          "\n"
          "General/FUSE options:\n"
          "    -h, --help             display this help and exit\n"
          "    -V, --version          output version information and exit\n"
          "\n", stdout);
}

template <typename T>
static typename std::map<std::string, T, comp>::const_iterator search_by_value(const std::map<std::string, T, comp> & mapOfWords, T value)
{
    // Iterate through all elements in map and search for the passed element
    typename std::map<std::string, T, comp>::const_iterator it = mapOfWords.begin();
    while (it != mapOfWords.end())
    {
        if(it->second == value)
        {
            return it;
        }
        it++;
    }
    return mapOfWords.end();
}

// Get bitrate:
// In bit/s:  #  or #bps
// In kbit/s: #M or #Mbps
// In Mbit/s: #M or #Mbps
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

// Get sample rate:
// In Hz:  #  or #Hz
// In kHz: #K or #KHz
static int get_samplerate(const std::string & arg, unsigned int * samplerate)
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
            *samplerate = static_cast<unsigned int>(atol(data.c_str()));
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
            *samplerate = static_cast<unsigned int>(atof(data.c_str()) * 1000);
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

// Get time in format
// Seconds: #
// Minutes: #m
// Hours:   #h
// Days:    #d
// Weeks:   #w
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

// Read size:
// In bytes:  # or #B
// In KBytes: #K or #KB
// In MBytes: #B or #MB
// In GBytes: #G or #GB
// In TBytes: #T or #TB
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

// Read destination type
int get_desttype(const std::string & arg, ffmpegfs_format *video_format, ffmpegfs_format * audio_format)
{
    // TODO: evaluate
    size_t pos = arg.find('=');

    if (pos != std::string::npos)
    {
        std::vector<std::string> results =  split(arg.substr(pos + 1), "\\+");

        if (results.size() > 0 && results.size() < 3)
        {
            // Check for valid destination type and obtain codecs and file type.
            if (!get_codecs(results[0], video_format))
            {
                std::fprintf(stderr, "INVALID PARAMETER: No codecs available for desttype: %s\n", results[0].c_str());
                return 1;
            }

            if (results.size() == 2)
            {
                if (!get_codecs(results[1], audio_format))
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

// Read profile:
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

// Get profile text
static std::string get_profile_text(PROFILE profile)
{
    PROFILE_MAP::const_iterator it = search_by_value(profile_map, profile);
    if (it != profile_map.end())
    {
        return it->first;
    }
    return "INVALID";
}
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
            n++;
            return 0;
        }
        else if (n == 1 && params.m_mountpath.empty())
        {
            expand_path(&params.m_mountpath, arg);

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
        usage(outargs->argv[0]);
        fuse_opt_add_arg(outargs, "-ho");
        fuse_main(outargs->argc, outargs->argv, &ffmpegfs_ops, nullptr);
        exit(1);
    }
    case KEY_VERSION:
    {
        // TODO: Also output this information in debug mode
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
    case KEY_PROFILE:
    {
        return get_profile(arg, &params.m_profile);
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

static void print_params(void)
{
    std::string cachepath;

    transcoder_cache_path(cachepath);

    Logging::trace(nullptr, PACKAGE_NAME " options:\n\n"
                                         "Base Path         : %1\n"
                                         "Mount Path        : %2\n\n"
                                         "Smart Transcode   : %3\n"
                                         "Audio File Type   : %4\n"
                                         "Video File Type   : %5\n"
                                         "Profile           : %6\n"
                                         "\nAudio\n\n"
                                         "Audio Codecs      : %7+%8\n"
                                         "Audio Bitrate     : %9\n"
                                         "Audio Sample Rate : %10\n"
                                         "\nVideo\n\n"
                                         "Video Size/Pixels : width=%11 height=%12\n"
               #ifndef USING_LIBAV
                                         "Deinterlace       : %13\n"
               #endif  // !USING_LIBAV
                                         "Remove Album Arts : %14\n"
                                         "Video Codec       : %15\n"
                                         "Video Bitrate     : %16\n"
                                         "\nVirtual Script\n\n"
                                         "Create script     : %17\n"
                                         "Script file name  : %18\n"
                                         "Input file        : %19\n"
                                         "\nLogging\n\n"
                                         "Max. Log Level    : %20\n"
                                         "Log to stderr     : %21\n"
                                         "Log to syslog     : %22\n"
                                         "Logfile           : %23\n"
                                         "\nCache Settings\n\n"
                                         "Expiry Time       : %24\n"
                                         "Inactivity Suspend: %25\n"
                                         "Inactivity Abort  : %26\n"
                                         "Pre-buffer size   : %27\n"
                                         "Max. Cache Size   : %28\n"
                                         "Min. Disk Space   : %29\n"
                                         "Cache Path        : %30\n"
                                         "Disable Cache     : %31\n"
                                         "Maintenance Timer : %32\n"
                                         "Clear Cache       : %33\n"
                                         "\nVarious Options\n\n"
                                         "Max. Threads      : %34\n"
                                         "Decoding Errors   : %35\n",
                   params.m_basepath,
                   params.m_mountpath,
                   params.smart_transcode() ? "yes" : "no",
                   params.m_format[1].m_desttype,
            params.m_format[0].m_desttype,
            get_profile_text(params.m_profile),
            get_codec_name(params.m_format[0].m_audio_codecid, true),
            get_codec_name(params.m_format[1].m_audio_codecid, true),
            format_bitrate(params.m_audiobitrate),
            format_samplerate(params.m_audiosamplerate),
            format_number(params.m_videowidth),
            format_number(params.m_videoheight),
        #ifndef USING_LIBAV
            params.m_deinterlace ? "yes" : "no",
        #endif  // !USING_LIBAV
            params.m_noalbumarts ? "yes" : "no",
            get_codec_name(params.m_format[0].m_video_codecid, true),
            format_bitrate(params.m_videobitrate),
            params.m_enablescript ? "yes" : "no",
            params.m_scriptfile,
            params.m_scriptsource,
            params.m_log_maxlevel,
            params.m_log_stderr ? "yes" : "no",
            params.m_log_syslog ? "yes" : "no",
            !params.m_logfile.empty() ? params.m_logfile : "none",
            format_time(params.m_expiry_time),
            format_time(params.m_max_inactive_suspend),
            format_time(params.m_max_inactive_abort),
            format_size(params.m_prebuffer_size),
            format_size(params.m_max_cache_size),
            format_size(params.m_min_diskspace),
            cachepath,
            params.m_disable_cache ? "yes" : "no",
            params.m_cache_maintenance ? format_time(params.m_cache_maintenance) : "inactive",
            params.m_clear_cache ? "yes" : "no",
            format_number(params.m_max_threads),
            params.m_decoding_errors ? "break transcode" : "ignore");
}

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
                    "Copyright (C) 2017-2018 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)\n\n");
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
    params.m_max_threads = static_cast<unsigned>(get_nprocs() * 16);

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

    if (!init_logging(params.m_logfile, params.m_log_maxlevel, params.m_log_stderr, params.m_log_syslog))
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
        if (transcoder_init())
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

    struct stat st;
    if (stat(params.m_basepath.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
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

    if (stat(params.m_mountpath.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
    {
        std::fprintf(stderr, "INVALID PARAMETER: mountpath is not a valid directory: %s\n\n", params.m_mountpath.c_str());
        return 1;
    }

    if (transcoder_init())
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
