/*
 * FFmpeg utilities for ffmpegfs
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

#include "ffmpeg_utils.h"
#include "id3v1tag.h"
#include "ffmpegfs.h"

#include <libgen.h>
#include <unistd.h>
#include <algorithm>
#include <regex.h>

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
#include <libswscale/swscale.h>
#if LAVR_DEPRECATE
#include <libswresample/swresample.h>
#else
#include <libavresample/avresample.h>
#endif
#ifndef USING_LIBAV
// Does not exist in Libav
#include "libavutil/ffversion.h"
#else
#include "libavutil/avstring.h"
#include "libavutil/opt.h"
#endif
#ifdef __cplusplus
}
#endif
#pragma GCC diagnostic pop

static int is_device(const AVClass *avclass);

#ifndef AV_ERROR_MAX_STRING_SIZE
#define AV_ERROR_MAX_STRING_SIZE 128
#endif // AV_ERROR_MAX_STRING_SIZE

// Add / to the path if required
// Returns: Constant reference to path
const string & append_sep(string * path)
{
    if (path->back() != '/')
    {
        *path += '/';
    }

    return *path;
}

// Add filename to path, adding a / to the path if required
// Returns: Constant reference to path
const string & append_filename(string * path, const string & filename)
{
    append_sep(path);

    *path += filename;

    return *path;
}

// Remove filename from path. Handy dirname alternative.
const string & remove_filename(string * path)
{
    char *p = strdup(path->c_str());
    *path = dirname(p);
    free(p);
    append_sep(path);
    return *path;
}

// Remove path from filename. Handy basename alternative.
const string & remove_path(string *path)
{
    char *p = strdup(path->c_str());
    *path = basename(p);
    free(p);
    return *path;
}

// Find extension in filename, if any
// Returns: Constant true if extension was found, false if there was none
bool find_ext(string * ext, const string & filename)
{
    size_t found;

    found = filename.rfind('.');

    if (found == string::npos)
    {
        // No extension
        ext->clear();
        return false;
    }
    else
    {
        // Have extension
        *ext = filename.substr(found + 1);
        return true;
    }
}

// Replace extension in filename. Take into account that there might
// not be an extension already.
// Returns: Constant reference to filename
const string & replace_ext(string * filename, const string & ext)
{
    size_t found;

    found = filename->rfind('.');

    if (found == string::npos)
    {
        // No extension, just add
        *filename += '.';
    }
    else
    {
        // Have extension, so replace
        *filename = filename->substr(0, found + 1);
    }

    *filename += ext;

    return *filename;
}

const string & get_destname(string *destname, const string & filename)
{
    size_t len = strlen(params.m_basepath);

    *destname = params.m_mountpath;
    *destname += filename.substr(len);

    replace_ext(destname, params.m_desttype);

    return *destname;
}

string ffmpeg_geterror(int errnum)
{
    char error[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, error, AV_ERROR_MAX_STRING_SIZE);
    return error;
}

double ffmpeg_cvttime(int64_t ts, const AVRational & time_base)
{
    if (ts != 0 && ts != (int64_t)AV_NOPTS_VALUE)
    {
        return ((double)ts * ::av_q2d(time_base));
    }
    else
    {
        return 0;
    }
}

#if !HAVE_MEDIA_TYPE_STRING
const char *get_media_type_string(enum AVMediaType media_type)
{
    switch (media_type)
    {
    case AVMEDIA_TYPE_VIDEO:
        return "video";
    case AVMEDIA_TYPE_AUDIO:
        return "audio";
    case AVMEDIA_TYPE_DATA:
        return "data";
    case AVMEDIA_TYPE_SUBTITLE:
        return "subtitle";
    case AVMEDIA_TYPE_ATTACHMENT:
        return "attachment";
    default:
        return "unknown";
    }
}
#endif

static string ffmpeg_libinfo(
        bool bLibExists,
        unsigned int /*nVersion*/,
        const char * /*pszCfg*/,
        int nVersionMinor,
        int nVersionMajor,
        int nVersionMicro,
        const char * pszLibname)
{
    string info;

    if (bLibExists)
    {
        char buffer[1024];

        sprintf(buffer,
                "lib%-17s: %d.%d.%d\n",
                pszLibname,
                nVersionMinor,
                nVersionMajor,
                nVersionMicro);
        info = buffer;
    }

    return info;
}

void ffmpeg_libinfo(char * buffer, size_t maxsize)
{
#define PRINT_LIB_INFO(libname, LIBNAME) \
    ffmpeg_libinfo(true, libname##_version(), libname##_configuration(), \
    LIB##LIBNAME##_VERSION_MAJOR, LIB##LIBNAME##_VERSION_MINOR, LIB##LIBNAME##_VERSION_MICRO, #libname)

    string info;

#ifdef USING_LIBAV
    info = "Libav Version       :\n";
#else
    info = "FFmpeg Version      : " FFMPEG_VERSION "\n";
#endif

    info += PRINT_LIB_INFO(avutil,      AVUTIL);
    info += PRINT_LIB_INFO(avcodec,     AVCODEC);
    info += PRINT_LIB_INFO(avformat,    AVFORMAT);
    // info += PRINT_LIB_INFO(avdevice,    AVDEVICE);
    // info += PRINT_LIB_INFO(avfilter,    AVFILTER);
#if LAVR_DEPRECATE
    info += PRINT_LIB_INFO(swresample,  SWRESAMPLE);
#else
    info += PRINT_LIB_INFO(avresample,  AVRESAMPLE);
#endif
    info += PRINT_LIB_INFO(swscale,     SWSCALE);
    // info += PRINT_LIB_INFO(postproc,    POSTPROC);

    *buffer = '\0';
    strncat(buffer, info.c_str(), maxsize - 1);
}

static int is_device(const AVClass *avclass)
{
    if (!avclass)
        return 0;

    return 0;
    //return AV_IS_INPUT_DEVICE(avclass->category) || AV_IS_OUTPUT_DEVICE(avclass->category);
}

int show_formats_devices(int device_only)
{
    const AVInputFormat *ifmt  = NULL;
    const AVOutputFormat *ofmt = NULL;
    const char *last_name;
    int is_dev;

    printf("%s\n"
           " D. = Demuxing supported\n"
           " .E = Muxing supported\n"
           " --\n", device_only ? "Devices:" : "File formats:");
    last_name = "000";
    for (;;)
    {
        int decode = 0;
        int encode = 0;
        const char *name      = NULL;
        const char *long_name = NULL;
        const char *extensions = NULL;

#if LAVF_DEP_AV_REGISTER
        void *ofmt_opaque = NULL;
        ofmt_opaque = NULL;
        while ((ofmt = av_muxer_iterate(&ofmt_opaque)))
#else
        while ((ofmt = av_oformat_next(ofmt)))
#endif
        {
            is_dev = is_device(ofmt->priv_class);
            if (!is_dev && device_only)
            {
                continue;
            }

            if ((!name || strcmp(ofmt->name, name) < 0) &&
                    strcmp(ofmt->name, last_name) > 0)
            {
                name        = ofmt->name;
                long_name   = ofmt->long_name;
                encode      = 1;
            }
        }
#if LAVF_DEP_AV_REGISTER
        void *ifmt_opaque = NULL;
        ifmt_opaque = NULL;
        while ((ifmt = av_demuxer_iterate(&ifmt_opaque)))
#else
        while ((ifmt = av_iformat_next(ifmt)))
#endif
        {
            is_dev = is_device(ifmt->priv_class);
            if (!is_dev && device_only)
            {
                continue;
            }

            if ((!name || strcmp(ifmt->name, name) < 0) &&
                    strcmp(ifmt->name, last_name) > 0)
            {
                name        = ifmt->name;
                long_name   = ifmt->long_name;
                extensions  = ifmt->extensions;
                encode      = 0;
            }
            if (name && strcmp(ifmt->name, name) == 0)
            {
                decode      = 1;
            }
        }

        if (!name)
        {
            break;
        }

        last_name = name;
        if (!extensions)
        {
            continue;
        }

        printf(" %s%s %-15s %-15s %s\n",
               decode ? "D" : " ",
               encode ? "E" : " ",
               extensions != NULL ? extensions : "-",
               name,
               long_name ? long_name:" ");
    }
    return 0;
}

const char * get_codec_name(enum AVCodecID codec_id, int long_name)
{
    const AVCodecDescriptor * pCodecDescriptor;
    const char * psz = "unknown";

    pCodecDescriptor = avcodec_descriptor_get(codec_id);

    if (pCodecDescriptor != NULL)
    {
        if (pCodecDescriptor->long_name != NULL && long_name)
        {
            psz = pCodecDescriptor->long_name;
        }

        else
        {
            psz = pCodecDescriptor->name;
        }
    }

    return psz;
}

int mktree(const char *path, mode_t mode)
{
    char *_path = strdup(path);

    if (_path == NULL)
    {
        return ENOMEM;
    }

    char dir[PATH_MAX] = "\0";
    char *p = strtok (_path, "/");
    int status = 0;

    while (p != NULL)
    {
        int newstat;

        strcat(dir, "/");
        strcat(dir, p);

        errno = 0;

        newstat = mkdir(dir, mode);

        if (!status && newstat && errno != EEXIST)
        {
            status = -1;
            break;
        }

        status = newstat;

        p = strtok (NULL, "/");
    }

    free(_path);
    return status;
}

void tempdir(char *dir, size_t size)
{
    *dir = '\0';
    const char *temp = getenv("TMPDIR");

    if (temp != NULL)
    {
        strncpy(dir, temp, size);
        return;
    }

    strncpy(dir, P_tmpdir, size);

    if (*dir)
    {
        return;
    }

    strncpy(dir, "/tmp", size);
}

int supports_albumart(FILETYPE filetype)
{
    return (filetype == FILETYPE_MP3 || filetype == FILETYPE_MP4);
}

FILETYPE get_filetype(const char * type)
{
    if (strcasestr(type, "mp3") != NULL)
    {
        return FILETYPE_MP3;
    }
    else if (strcasestr(type, "mp4") != NULL)
    {
        return FILETYPE_MP4;
    }
    else if (strcasestr(type, "wav") != NULL)
    {
        return FILETYPE_WAV;
    }
    else if (strcasestr(type, "ogg") != NULL)
    {
        return FILETYPE_OGG;
    }
    else if (strcasestr(type, "webm") != NULL)
    {
        return FILETYPE_WEBM;
    }
    else if (strcasestr(type, "mov") != NULL)
    {
        return FILETYPE_MOV;
    }
    else if (strcasestr(type, "aiff") != NULL)
    {
        return FILETYPE_AIFF;
    }
    else if (strcasestr(type, "opus") != NULL)
    {
        return FILETYPE_OPUS;
    }
    else
    {
        return FILETYPE_UNKNOWN;
    }
}

const char * get_codecs(const char * type, FILETYPE * output_type, AVCodecID * audio_codecid, AVCodecID * video_codecid)
{
    const char * format = NULL;

    switch (get_filetype(type))
    {
    case FILETYPE_MP3:
    {
        *audio_codecid = AV_CODEC_ID_MP3;
        *video_codecid = AV_CODEC_ID_NONE; //AV_CODEC_ID_MJPEG;
        if (output_type != NULL)
        {
            *output_type = FILETYPE_MP3;
        }
        format = "mp3";
        break;
    }
    case FILETYPE_MP4:
    {
        *audio_codecid = AV_CODEC_ID_AAC;
        *video_codecid = AV_CODEC_ID_H264;
        if (output_type != NULL)
        {
            *output_type = FILETYPE_MP4;
        }
        format = "mp4";
        break;
    }
    case FILETYPE_WAV:
    {
        *audio_codecid = AV_CODEC_ID_PCM_S16LE;
        *video_codecid = AV_CODEC_ID_NONE;
        if (output_type != NULL)
        {
            *output_type = FILETYPE_WAV;
        }
        format = "wav";
        break;
    }
    case FILETYPE_OGG:
    {
        *audio_codecid = AV_CODEC_ID_VORBIS;
        *video_codecid = AV_CODEC_ID_THEORA;
        if (output_type != NULL)
        {
            *output_type = FILETYPE_OGG;
        }
        format = "ogg";
        break;
    }
    case FILETYPE_WEBM:
    {
        *audio_codecid = AV_CODEC_ID_OPUS;
        *video_codecid = AV_CODEC_ID_VP9;
        if (output_type != NULL)
        {
            *output_type = FILETYPE_WEBM;
        }
        format = "webm";
        break;
    }
    case FILETYPE_MOV:
    {
        *audio_codecid = AV_CODEC_ID_AAC;
        *video_codecid = AV_CODEC_ID_H264;
        if (output_type != NULL)
        {
            *output_type = FILETYPE_MOV;
        }
        format = "mov";
        break;
    }
    case FILETYPE_AIFF:
    {
        *audio_codecid = AV_CODEC_ID_PCM_S16BE;
        *video_codecid = AV_CODEC_ID_NONE;
        if (output_type != NULL)
        {
            *output_type = FILETYPE_AIFF;
        }
        format = "aiff";
        break;
    }
    case FILETYPE_OPUS:
    {
        *audio_codecid = AV_CODEC_ID_OPUS;
        *video_codecid = AV_CODEC_ID_NONE;
        if (output_type != NULL)
        {
            *output_type = FILETYPE_OPUS;
        }
        format = "opus";
        break;
    }
    case FILETYPE_UNKNOWN:
    {
        break;
    }
    }

    return format;
}

#ifdef USING_LIBAV
int avformat_alloc_output_context2(AVFormatContext **avctx, AVOutputFormat *oformat, const char *format, const char *filename)
{
    AVFormatContext *s = avformat_alloc_context();
    int ret = 0;

    *avctx = NULL;
    if (!s)
        goto nomem;

    if (!oformat)
    {
        if (format)
        {
            oformat = av_guess_format(format, NULL, NULL);
            if (!oformat)
            {
                av_log(s, AV_LOG_ERROR, "Requested output format '%s' is not a suitable output format\n", format);
                ret = AVERROR(EINVAL);
                goto error;
            }
        }
        else
        {
            oformat = av_guess_format(NULL, filename, NULL);
            if (!oformat)
            {
                ret = AVERROR(EINVAL);
                av_log(s, AV_LOG_ERROR, "%s * Unable to find a suitable output format\n", filename);
                goto error;
            }
        }
    }

    s->oformat = oformat;
    if (s->oformat->priv_data_size > 0)
    {
        s->priv_data = av_mallocz(s->oformat->priv_data_size);
        if (!s->priv_data)
            goto nomem;
        if (s->oformat->priv_class)
        {
            *(const AVClass**)s->priv_data= s->oformat->priv_class;
            av_opt_set_defaults(s->priv_data);
        }
    }
    else
        s->priv_data = NULL;

    if (filename)
        av_strlcpy(s->filename, filename, sizeof(s->filename));
    *avctx = s;
    return 0;
nomem:
    av_log(s, AV_LOG_ERROR, "Out of memory\n");
    ret = AVERROR(ENOMEM);
error:
    avformat_free_context(s);
    return ret;
}

const char *avcodec_get_name(enum AVCodecID id)
{
    const AVCodecDescriptor *cd;
    AVCodec *codec;

    if (id == AV_CODEC_ID_NONE)
        return "none";
    cd = avcodec_descriptor_get(id);
    if (cd)
        return cd->name;
    av_log(NULL, AV_LOG_WARNING, "Codec 0x%x is not in the full list.\n", id);
    codec = avcodec_find_decoder(id);
    if (codec)
        return codec->name;
    codec = avcodec_find_encoder(id);
    if (codec)
        return codec->name;
    return "unknown_codec";
}
#endif

void init_id3v1(ID3v1 *id3v1)
{
    // Initialise ID3v1.1 tag structure
    memset(id3v1, ' ', sizeof(ID3v1));
    memcpy(&id3v1->m_tag, "TAG", 3);
    id3v1->m_padding = '\0';
    id3v1->m_title_no = 0;
    id3v1->m_genre = 0;
}

void format_number(char *output, size_t size, uint64_t value)
{
    if (!value)
    {
        strncpy(output, "unlimited", size);
        return;
    }

    if (value == (uint64_t)AV_NOPTS_VALUE)
    {
        strncpy(output, "unset", size);
        return;
    }

    snprintf(output, size, "%" PRId64, value);
}

void format_bitrate(char *output, size_t size, uint64_t value)
{
    if (value == (uint64_t)AV_NOPTS_VALUE)
    {
        strncpy(output, "unset", size);
        return;
    }

    if (value > 1000000)
    {
        snprintf(output, size, "%.2f Mbps", (double)value / 1000000);
    }
    else if (value > 1000)
    {
        snprintf(output, size, "%.1f kbps", (double)value / 1000);
    }
    else
    {
        snprintf(output, size, "%" PRId64 " bps", value);
    }
}

void format_samplerate(char *output, size_t size, unsigned int value)
{
    if (value == (unsigned int)AV_NOPTS_VALUE)
    {
        strncpy(output, "unset", size);
        return;
    }

    if (value < 1000)
    {
        snprintf(output, size, "%u Hz", value);
    }
    else
    {
        snprintf(output, size, "%.3f kHz", (double)value / 1000);
    }
}

void format_duration(char *output, size_t size, time_t value)
{
    if (value == (time_t)AV_NOPTS_VALUE)
    {
        strncpy(output, "unset", size);
        return;
    }

    int hours;
    int mins;
    int secs;
    int pos;

    hours = (int)(value / (60*60));
    value -= hours * (60*60);
    mins = (int)(value / (60));
    value -= mins * (60);
    secs = (int)(value);

    *output = '0';
    pos = 0;
    if (hours)
    {
        pos += snprintf(output + pos, size - pos, "%02i:", hours);
    }
    snprintf(output + pos, size - pos, "%02i:%02i", mins, secs);
}

void format_time(char *output, size_t size, time_t value)
{
    if (!value)
    {
        strncpy(output, "unlimited", size);
        return;
    }

    if (value == (time_t)AV_NOPTS_VALUE)
    {
        strncpy(output, "unset", size);
        return;
    }

    int weeks;
    int days;
    int hours;
    int mins;
    int secs;
    int pos;

    weeks = (int)(value / (60*60*24*7));
    value -= weeks * (60*60*24*7);
    days = (int)(value / (60*60*24));
    value -= days * (60*60*24);
    hours = (int)(value / (60*60));
    value -= hours * (60*60);
    mins = (int)(value / (60));
    value -= mins * (60);
    secs = (int)(value);

    *output = '0';
    pos = 0;
    if (weeks)
    {
        pos += snprintf(output, size, "%iw ", weeks);
    }
    if (days)
    {
        pos += snprintf(output + pos, size - pos, "%id ", days);
    }
    if (hours)
    {
        pos += snprintf(output + pos, size - pos, "%ih ", hours);
    }
    if (mins)
    {
        pos += snprintf(output + pos, size - pos, "%im ", mins);
    }
    if (secs)
    {
        pos += snprintf(output + pos, size - pos, "%is ", secs);
    }
}

void format_size(char *output, size_t size, size_t value)
{
    if (!value)
    {
        strncpy(output, "unlimited", size);
        return;
    }

    if (value == (size_t)AV_NOPTS_VALUE)
    {
        strncpy(output, "unset", size);
        return;
    }

    if (value > 1024*1024*1024*1024LL)
    {
        snprintf(output, size, "%.3f TB", (double)value / (1024*1024*1024*1024LL));
    }
    else if (value > 1024*1024*1024)
    {
        snprintf(output, size, "%.2f GB", (double)value / (1024*1024*1024));
    }
    else if (value > 1024*1024)
    {
        snprintf(output, size, "%.1f MB", (double)value / (1024*1024));
    }
    else if (value > 1024)
    {
        snprintf(output, size, "%.1f KB", (double)value / (1024));
    }
    else
    {
        snprintf(output, size, "%zu bytes", value);
    }
}

string format_number(int64_t value)
{
    char buffer[100];

    format_number(buffer, sizeof(buffer), value);

    return buffer;
}

string format_bitrate(uint64_t value)
{
    char buffer[100];

    format_bitrate(buffer, sizeof(buffer), value);

    return buffer;
}

string format_samplerate(unsigned int value)
{
    char buffer[100];

    format_samplerate(buffer, sizeof(buffer), value);

    return buffer;
}

string format_duration(time_t value)
{
    char buffer[100];

    format_duration(buffer, sizeof(buffer), value);

    return buffer;
}
string format_time(time_t value)
{
    char buffer[100];

    format_time(buffer, sizeof(buffer), value);

    return buffer;
}

string format_size(size_t value)
{
    char buffer[100];

    format_size(buffer, sizeof(buffer), value);

    return buffer;
}

static void print_fps(double d, const char *postfix)
{
    uint64_t v = lrint(d * 100);
    if (!v)
        printf("%1.4f %s\n", d, postfix);
    else if (v % 100)
        printf("%3.2f %s\n", d, postfix);
    else if (v % (100 * 1000))
        printf("%1.0f %s\n", d, postfix);
    else
        printf("%1.0fk %s\n", d / 1000, postfix);
}

int print_info(AVStream* stream)
{
    int ret = 0;

#if LAVF_DEP_AVSTREAM_CODEC
    AVCodecContext *avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
    {
        return AVERROR(ENOMEM);
    }

    ret = avcodec_parameters_to_context(avctx, stream->codecpar);
    if (ret < 0) {
        avcodec_free_context(&avctx);
        return ret;
    }

    // Fields which are missing from AVCodecParameters need to be taken from the AVCodecContext
    //            avctx->properties   = output_stream->codec->properties;
    //            avctx->codec        = output_stream->codec->codec;
    //            avctx->qmin         = output_stream->codec->qmin;
    //            avctx->qmax         = output_stream->codec->qmax;
    //            avctx->coded_width  = output_stream->codec->coded_width;
    //            avctx->coded_height = output_stream->codec->coded_height;
#else
    AVCodecContext *avctx = stream->codec;
#endif
    int fps = stream->avg_frame_rate.den && stream->avg_frame_rate.num;
#ifndef USING_LIBAV
    int tbr = stream->r_frame_rate.den && stream->r_frame_rate.num;
#endif
    int tbn = stream->time_base.den && stream->time_base.num;
    int tbc = avctx->time_base.den && avctx->time_base.num; // Even the currently latest (lavf 58.10.100) refers to AVStream codec->time_base member... (See dump.c dump_stream_format)

    if (fps)
        print_fps(av_q2d(stream->avg_frame_rate), "avg fps");
#ifndef USING_LIBAV
    if (tbr)
        print_fps(av_q2d(stream->r_frame_rate), "Real base framerate (tbr)");
#endif
    if (tbn)
        print_fps(1 / av_q2d(stream->time_base), "stream timebase (tbn)");
    if (tbc)
        print_fps(1 / av_q2d(avctx->time_base), "codec timebase (tbc)");

#if LAVF_DEP_AVSTREAM_CODEC
    avcodec_free_context(&avctx);
#endif

    return ret;
}

void exepath(string * path)
{
    char result[PATH_MAX + 1] = "";
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1)
    {
        *path = dirname(result);
        append_sep(path);
    }
    else
    {
        path->clear();
    }
}

// trim from start
std::string &ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
std::string &rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}

// Compare value with pattern
// Returns:
// -1 if pattern is no valid regex
// 0 if pattern matches
// 1 if not
int compare(const char *value, const char *pattern)
{
    regex_t regex;
    int reti;

    reti = regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE);
    if (reti)
    {
        fprintf(stderr, "Could not compile regex\n");
        return -1;
    }

    reti = regexec(&regex, value, 0, NULL, 0);

    regfree(&regex);

    return reti;
}


