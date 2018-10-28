/*
 * Fuse interface for ffmpegfs
 *
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2013 K. Henriksson
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

#ifndef FFMPEGFS_H
#define FFMPEGFS_H

#pragma once

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdarg.h>

#include "ffmpeg_utils.h"

typedef enum _tagPROFILE
{
    PROFILE_INVALID = -1,

    PROFILE_NONE,               // no specific profile

    // MP4

    PROFILE_MP4_FF,				// Firefox
    PROFILE_MP4_EDGE,			// MS Edge
    PROFILE_MP4_IE,				// MS Internet Explorer
    PROFILE_MP4_CHROME,			// Google Chrome
    PROFILE_MP4_SAFARI,			// Apple Safari
    PROFILE_MP4_OPERA,			// Opera
    PROFILE_MP4_MAXTHON         // Maxthon

    // WEBM

} PROFILE;

// Global program parameters
extern struct ffmpegfs_params
{
    // Paths
    const char *    m_basepath;
    const char *    m_mountpath;
    // Output type
    const char *    m_desttype;
    const char *    m_format;
    FILETYPE        m_filetype;
    PROFILE         m_profile;					// Target type: Firefox, MS Edge/IE or other
    // Audio
    enum AVCodecID  m_audio_codecid;
    unsigned int    m_audiobitrate;
    unsigned int    m_audiosamplerate;
    // Video
    enum AVCodecID  m_video_codecid;
    unsigned int    m_videobitrate;
    unsigned int    m_videowidth;               // set video width
    unsigned int    m_videoheight;              // set video height
#ifndef USING_LIBAV
    int             m_deinterlace;              // deinterlace video
#endif // !USING_LIBAV
    // Album arts
    int             m_noalbumarts;              // skip album arts
    // Virtual script
    int             m_enablescript;             // Enable virtual script
    const char*     m_scriptfile;               // Script name
    const char*     m_scriptsource;             // Source script
    // ffmpegfs options
    int             m_debug;
    const char*     m_log_maxlevel;
    int             m_log_stderr;
    int             m_log_syslog;
    const char*     m_logfile;
    // Background recoding/caching
    time_t          m_expiry_time;              // Time (seconds) after which an cache entry is deleted
    time_t          m_max_inactive_suspend;     // Time (seconds) that must elapse without access until transcoding is suspended
    time_t          m_max_inactive_abort;       // Time (seconds) that must elapse without access until transcoding is aborted
    size_t          m_prebuffer_size;           // Number of bytes that will be decoded before it can be accessed
    size_t          m_max_cache_size;           // Max. cache size in MB. When exceeded, oldest entries will be pruned
    size_t          m_min_diskspace;            // Min. diskspace required for cache
    const char*     m_cachepath;                // Disk cache path, defaults to /tmp
    int             m_disable_cache;            // Disable cache
    time_t          m_cache_maintenance;        // Prune timer interval
    int             m_prune_cache;              // Prune cache immediately
    int             m_clear_cache;              // Clear cache on start up
    unsigned int    m_max_threads;              // Max. number of recoder threads
    int             m_decoding_errors;          // Break transcoding on decoding error
} params;

// Forward declare transcoder struct. Don't actually define it here, to avoid
// including coders.h and turning into C++.

struct Cache_Entry;

#ifdef __cplusplus
extern "C" {
#endif
// Fuse operations struct
extern struct fuse_operations ffmpegfs_ops;

int ffmpegfs_readlink(const char *path, char *buf, size_t size);
int ffmpegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int ffmpegfs_getattr(const char *path, struct stat *stbuf);
int ffmpegfs_fgetattr(const char *path, struct stat * stbuf, struct fuse_file_info *fi);
int ffmpegfs_open(const char *path, struct fuse_file_info *fi);
int ffmpegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int ffmpegfs_statfs(const char *path, struct statvfs *stbuf);
int ffmpegfs_release(const char *path, struct fuse_file_info *fi);
void *ffmpegfs_init(struct fuse_conn_info *conn);
void ffmpegfs_destroy(__attribute__((unused)) void * p);

// Functions to print output until C++ conversion is done.
void ffmpegfs_trace(const char *filename, const char* f, ...) __attribute__ ((format(printf, 2, 3)));
void ffmpegfs_debug(const char *filename, const char* f, ...) __attribute__ ((format(printf, 2, 3)));
void ffmpegfs_warning(const char *filename, const char* f, ...) __attribute__ ((format(printf, 2, 3)));
void ffmpegfs_info(const char *filename, const char* f, ...) __attribute__ ((format(printf, 2, 3)));
void ffmpegfs_error(const char *filename, const char* f, ...) __attribute__ ((format(printf, 2, 3)));
#ifndef USING_LIBAV
void ffmpeg_log(void *ptr, int level, const char *fmt, va_list vl);
#endif

int init_logging(const char* logfile, const char* max_level, int to_stderr, int to_syslog);

int get_profile(const char * arg, PROFILE *value);
const char * get_profile_text(PROFILE value);

void transcoder_cache_path(char *dir, size_t size);
int transcoder_init(void);
void transcoder_free(void);
int transcoder_cache_maintenance(void);
int transcoder_cache_clear(void);

#ifdef __cplusplus
}
#endif

#endif // FFMPEGFS_H

