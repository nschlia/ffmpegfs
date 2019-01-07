/*
 * Fuse interface for ffmpegfs
 *
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2013 K. Henriksson
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef FFMPEGFS_H
#define FFMPEGFS_H

#pragma once

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdarg.h>

#include "ffmpeg_utils.h"
#include "fileio.h"

// Global program parameters
extern struct ffmpegfs_params
{
    ffmpegfs_params();

    bool                smart_transcode(void) const;
    int                 guess_format_idx(const std::string & filepath) const;
    ffmpegfs_format *   current_format(const std::string & filepath);
    ffmpegfs_format *   current_format(LPCVIRTUALFILE virtualfile);

    // Paths
    std::string         m_basepath;
    std::string         m_mountpath;
    // Output type
    PROFILE             m_profile;					// Target profile: Firefox, MS Edge/IE or other
    LEVEL               m_level;                    // Level, currently proxy/hq/lt/HQ (ProRes only)
    // Format
    ffmpegfs_format     m_format[2];
    // Audio
    BITRATE             m_audiobitrate;
    unsigned int        m_audiosamplerate;
    // Video
    BITRATE             m_videobitrate;
    unsigned int        m_videowidth;               // set video width
    unsigned int        m_videoheight;              // set video height
#ifndef USING_LIBAV
    int                 m_deinterlace;              // deinterlace video
#endif // !USING_LIBAV
    // Album arts
    int                 m_noalbumarts;              // skip album arts
    // Virtual script
    int                 m_enablescript;             // Enable virtual script
    std::string         m_scriptfile;               // Script name
    std::string         m_scriptsource;             // Source script
    // ffmpegfs options
    int                 m_debug;
    std::string         m_log_maxlevel;
    int                 m_log_stderr;
    int                 m_log_syslog;
    std::string         m_logfile;
    // Background recoding/caching
    time_t              m_expiry_time;              // Time (seconds) after which an cache entry is deleted
    time_t              m_max_inactive_suspend;     // Time (seconds) that must elapse without access until transcoding is suspended
    time_t              m_max_inactive_abort;       // Time (seconds) that must elapse without access until transcoding is aborted
    size_t              m_prebuffer_size;           // Number of bytes that will be decoded before it can be accessed
    size_t              m_max_cache_size;           // Max. cache size in MB. When exceeded, oldest entries will be pruned
    size_t              m_min_diskspace;            // Min. diskspace required for cache
    std::string         m_cachepath;                // Disk cache path, defaults to /tmp
    int                 m_disable_cache;            // Disable cache
    time_t              m_cache_maintenance;        // Prune timer interval
    int                 m_prune_cache;              // Prune cache immediately
    int                 m_clear_cache;              // Clear cache on start up
    unsigned int        m_max_threads;              // Max. number of recoder threads
    int                 m_decoding_errors;          // Break transcoding on decoding error
} params;

struct Cache_Entry;

// Fuse operations struct
extern fuse_operations ffmpegfs_ops;

void        init_fuse_ops(void);

#ifndef USING_LIBAV
void        ffmpeg_log(void *ptr, int level, const char *fmt, va_list vl);
#endif

int         init_logging(const std::string &logfile, const std::string & max_level, int to_stderr, int to_syslog);

void        transcoder_cache_path(std::string & path);
int         transcoder_init(void);
void        transcoder_free(void);
int         transcoder_cache_maintenance(void);
int         transcoder_cache_clear(void);

LPVIRTUALFILE insert_file(VIRTUALTYPE type, const std::string &filepath, const std::string & origfile, const struct stat *st);
LPVIRTUALFILE find_file(const std::string &filepath);
LPVIRTUALFILE find_original(std::string *filepath);

#endif // FFMPEGFS_H

