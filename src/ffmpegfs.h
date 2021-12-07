/*
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2013 K. Henriksson
 * Copyright (C) 2017-2021 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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

#ifndef FFMPEGFS_H
#define FFMPEGFS_H

#pragma once

/**
 * @file
 * @ingroup ffmpegfs
 * @brief Main include for FFmpegfs project
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2021 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
 */

/** @mainpage FFmpegfs FUSE Filesystem
 *
 * @section ffmpeg_introduction Introduction to FFmpegfs
 *
 * FFmpegfs: A read-only FUSE filesystem which transcodes various audio and video formats
 * to MP4, WebM and many more on the fly when opened and read.
 *
 * FFmpegfs can be download [here](https://github.com/nschlia/ffmpegfs).
 *
 * @section ffmpeg_documentation Documentation
 *
 * @li [Read me!](README.md)
 * @li [Man Pages](manpages.html)
 * @li [History](HISTORY.md)
 * @li [Installation instructions](INSTALL.md)
 * @li @ref todo
 * @li @ref bug
 * @li @ref ffmpegfs_NEWS
 * @li @ref ffmpegfs_TODO
 * @li @ref ffmpegfs_COPYING
 * @li @ref ffmpegfs_COPYING.DOC
 * @li @ref ffmpegfs_COPYING.CC0
 *
 * @section ffmpegfs_external External links
 *
 * @li [FFmpeg Homepages](https://www.ffmpeg.org/) @n
 */

/**
 * @page ffmpegfs_NEWS Online NEWS list
 *
 * \include NEWS
 *
 * @page ffmpegfs_TODO Online TODO List
 *
 * \include TODO
 *
 * @page ffmpegfs_COPYING General License
 *
 * \include COPYING
 *
 * @page ffmpegfs_COPYING.DOC Documentation License
 *
 * \include COPYING.DOC
 *
 * @page ffmpegfs_COPYING.CC0 Demo Files License
 *
 * \include COPYING.CC0
 */

#define FUSE_USE_VERSION 26             /**< @brief Requested minimum FUSE version */

#include <fuse.h>
#include <stdarg.h>

#include "ffmpeg_utils.h"
#include "fileio.h"

/**
  *
  * @brief Hardware acceleration types.
  */
typedef enum HWACCELAPI
{
    HWACCELAPI_NONE,
    HWACCELAPI_VAAPI,               /**< Intel: VAAPI */

    HWACCELAPI_MMAL,                /**< Raspberry: MMAL */
    HWACCELAPI_OMX,                 /**< Raspberry: OpenMAX */

    // Additional formats
#if 0
    HWACCELAPI_CUDA,                /**< Nividia: CUDA    to be added */
    HWACCELAPI_V4L2M2M,             /**< v4l2 mem to mem  to be added */
    HWACCELAPI_VDPAU,               /**< VDPAU            to be added */
    HWACCELAPI_QSV,                 /**< QSV              to be added */
    HWACCELAPI_OPENCL,              /**< OPENCL           to be added */
#if HAVE_VULKAN_HWACCEL
    HWACCELAPI_VULKAN,              /**< VULKAN           to be added */
#endif // HAVE_VULKAN_HWACCEL
#if __APPLE__
    // MacOS acceleration APIs not supported
    HWACCELAPI_VIDEOTOOLBOX,        /**< VIDEOTOOLBOX     not supported */
#endif
#if __ANDROID__
    // Android acceleration APIs not supported
    HWACCELAPI_MEDIACODEC,          /**< MediaCodec API   not supported */
#endif
#if _WIN32
// Windows acceleration APIs not supported
    HWACCELAPI_DRM,                 /**< DRM              not supported */
    HWACCELAPI_DXVA2,               /**< DXVA2            not supported */
    HWACCELAPI_D3D11VA,             /**< D3D11VA          not supported */
#endif
#endif
} HWACCELAPI;

typedef std::multimap<AVCodecID, int> HWACCEL_BLOCKED_MAP;      /**< @brief Map command line option to AVCodecID */

// Format
extern FFmpegfs_Format     ffmpeg_format[2];                    /**< @brief Two FFmpegfs_Format infos, 0: video file, 1: audio file */

/**
 * @brief Global program parameters
 */
extern struct FFMPEGFS_PARAMS
{
    FFMPEGFS_PARAMS();
    ~FFMPEGFS_PARAMS();

    /**
     * @brief Check for smart transcode mode
     * @return true if smart transcode is active, false if not
     */
    bool                smart_transcode(void) const;
    /**
     * @brief Try to guess the format index (audio or video) for a file.
     * @param[in] filepath - Name of the file, path my be included, but not required.
     * @return Index 0 or 1
     */
    int                 guess_format_idx(const std::string & filepath) const;
    /**
     * @brief Get FFmpegfs_Format for the file.
     * @param[in] filepath - Name of the file, path my be included, but not required.
     * @return On success, returns pointer to format. On error, returns nullptr.
     */
    FFmpegfs_Format *   current_format(const std::string & filepath);
    /**
     * @brief Get FFmpegfs_Format for a virtual file.
     * @param[in] virtualfile - virtualfile struct of a file.
     * @return On success, returns pointer to format. On error, returns nullptr.
     */
    FFmpegfs_Format *   current_format(LPCVIRTUALFILE virtualfile);

    // Paths
    std::string         m_basepath;                 /**< @brief Base path: Files from this directory (including all sub directories) will be mapped to m_mountpath. */
    std::string         m_mountpath;                /**< @brief Mount path: Files from m_mountpath will be mapped to this directory. */

    // Output type
    AVCodecID           m_audio_codec;              /**< @brief Either AV_CODEC_ID_NONE for default, or a user selected codec */
    AVCodecID           m_video_codec;              /**< @brief Either AV_CODEC_ID_NONE for default, or a user selected codec */
    AUTOCOPY            m_autocopy;                 /**< @brief Copy streams if codec matches */
    RECODESAME          m_recodesame;               /**< @brief Recode to same format options */
    PROFILE             m_profile;					/**< @brief Target profile: Firefox, MS Edge/IE or other */
    PRORESLEVEL         m_level;                    /**< @brief Level, currently proxy/hq/lt/HQ (ProRes only) */
    // Audio
    BITRATE             m_audiobitrate;             /**< @brief Output audio bit rate (bits per second) */
    int                 m_audiosamplerate;          /**< @brief Output audio sample rate (in Hz) */
    int                 m_audiochannels;            /**< @brief Max. number of audio channels */
    SAMPLE_FMT          m_sample_fmt;               /**< @brief Sample format */
    // Video
    BITRATE             m_videobitrate;             /**< @brief Output video bit rate (bits per second) */
    int                 m_videowidth;               /**< @brief Output video width */
    int                 m_videoheight;              /**< @brief Output video height */
    int                 m_deinterlace;              /**< @brief 1: deinterlace video, 0: no deinterlace */
    // HLS Options
    int64_t             m_segment_duration;         /**< @brief Duration of one HLS segment file, in AV_TIME_BASE fractional seconds. */
    int64_t             m_min_seek_time_diff;       /**< @brief Minimum time diff from current to next requested segment to perform a seek, in AV_TIME_BASE fractional seconds. */
    // Hardware acceleration
    HWACCELAPI          m_hwaccel_enc_API;          /**< @brief Encoder API */
    AVHWDeviceType      m_hwaccel_enc_device_type;  /**< @brief Enable hardware acceleration buffering for encoder */
    std::string         m_hwaccel_enc_device;       /**< @brief Encoder device. May be AUTO to auto detect or empty */
    HWACCELAPI          m_hwaccel_dec_API;          /**< @brief Decoder API */
    AVHWDeviceType      m_hwaccel_dec_device_type;  /**< @brief Enable hardware acceleration buffering for decoder */
    std::string         m_hwaccel_dec_device;       /**< @brief Decoder device. May be AUTO to auto detect or empty */
    HWACCEL_BLOCKED_MAP*m_hwaccel_dec_blocked;      /**< @brief List of blocked decoders and optional profiles */
    // Album arts
    int                 m_noalbumarts;              /**< @brief skip album arts */
    // Virtual script
    int                 m_enablescript;             /**< @brief Enable virtual script */
    std::string         m_scriptfile;               /**< @brief Script name */
    std::string         m_scriptsource;             /**< @brief Source script */
    // FFmpegfs options
    int                 m_debug;                    /**< @brief Debug mode (stay in foreground */
    std::string         m_log_maxlevel;             /**< @brief Max. log level */
    int                 m_log_stderr;               /**< @brief Log output to standard error */
    int                 m_log_syslog;               /**< @brief Log output to system log */
    std::string         m_logfile;                  /**< @brief Output filename if logging to file */
    // Background recoding/caching
    time_t              m_expiry_time;              /**< @brief Time (seconds) after which an cache entry is deleted */
    time_t              m_max_inactive_suspend;     /**< @brief Time (seconds) that must elapse without access until transcoding is suspended */
    time_t              m_max_inactive_abort;       /**< @brief Time (seconds) that must elapse without access until transcoding is aborted */
    size_t              m_prebuffer_size;           /**< @brief Number of bytes that will be decoded before it can be accessed */
    size_t              m_max_cache_size;           /**< @brief Max. cache size in MB. When exceeded, oldest entries will be pruned */
    size_t              m_min_diskspace;            /**< @brief Min. diskspace required for cache */
    std::string         m_cachepath;                /**< @brief Disk cache path, defaults to $XDG_CACHE_HOME */
    int                 m_disable_cache;            /**< @brief Disable cache */
    time_t              m_cache_maintenance;        /**< @brief Prune timer interval */
    int                 m_prune_cache;              /**< @brief Prune cache immediately */
    int                 m_clear_cache;              /**< @brief Clear cache on start up */
    unsigned int        m_max_threads;              /**< @brief Max. number of recoder threads */
    // Miscellanous options
    int                 m_decoding_errors;          /**< @brief Break transcoding on decoding error */
    int                 m_min_dvd_chapter_duration; /**< @brief Min. DVD chapter duration. Shorter chapters will be ignored. */
    int                 m_oldnamescheme;            /**< @brief Use old output name scheme, can create duplicate filenames */
    // Experimental options
    int                 m_win_smb_fix;              /**< @brief Experimental Windows fix for access to EOF at file open */
} params;                                           /**< @brief Command line parameters */

class Cache_Entry;

extern bool             docker_client;              /**< @brief True if running inside a Docker container */

/**
 * @brief Fuse operations struct
 */
extern fuse_operations  ffmpegfs_ops;

class thread_pool;
/**
 * @brief Thread pool object
 */
extern thread_pool*     tp;

/**
 * @brief Initialise FUSE operation structure.
 */
void            init_fuse_ops(void);

/**
 * @brief Custom FFmpeg log function. Used with av_log_set_callback().
 * @param[in] ptr - See av_log_set_callback() in FFmpeg API.
 * @param[in] level - See av_log_set_callback() in FFmpeg API.
 * @param[in] fmt - See av_log_set_callback() in FFmpeg API.
 * @param[in] vl - See av_log_set_callback() in FFmpeg API.
 */
void            ffmpeg_log(void *ptr, int level, const char *fmt, va_list vl);
#endif

/**
 * @brief Inititalise logging facility
 * @param[in] logfile - Name of log file if file writing is selected.
 * @param[in] max_level - Maximum level to log.
 * @param[in] to_stderr - If true, log to stderr.
 * @param[in] to_syslog - If true, log to syslog.
 * @return Returns true on success; false on error.
 */
bool            init_logging(const std::string &logfile, const std::string & max_level, bool to_stderr, bool to_syslog);
/**
 * @brief Get transcoder cache path.
 * @param[out] path - Path to transcoder cache.
 */
void            transcoder_cache_path(std::string & path);
/**
 * @brief Initialise transcoder, create cache.
 * @return Returns true on success; false on error. Check errno for details.
 */
bool            transcoder_init(void);
/**
 * @brief Free transcoder.
 */
void            transcoder_free(void);
/**
 * @brief Run cache maintenance.
 * @return Returns true on success; false on error. Check errno for details.
 */
bool            transcoder_cache_maintenance(void);
/**
 * @brief Clear transcoder cache.
 * @return Returns true on success; false on error. Check errno for details.
 */
bool            transcoder_cache_clear(void);
/**
 * @brief Add new virtual file to internal list.
 *
 * For Bluray/DVD/VCD actually no physical input file exists, so virtual file and origfile are the same.
 * The input file is handled by the BlurayIO or VcdIO classes.
 * For cue sheets, the original (huge) input file is used. Start positions are sought, end positions
 * reported as EOF.
 *
 * @param[in] type - Type of virtual file.
 * @param[in] virtfile - Name of virtual file.
 * @param[in] stbuf - stat buffer with file size, time etc.
 * @param[in] flags - One of the VIRTUALFLAG_* flags to control the detailed behaviour.
 * @return Returns constant pointer to VIRTUALFILE object of file, nullptr if not found
 */
LPVIRTUALFILE   insert_file(VIRTUALTYPE type, const std::string & virtfile, const struct stat *stbuf, int flags = VIRTUALFLAG_NONE);
/**
 * @brief Add new virtual file to internal list. If the file already exists, it will be updated.
 * @param[in] type - Type of virtual file.
 * @param[in] virtfile - Name of virtual file.
 * @param[in] origfile - Original file name.
 * @param[in] stbuf - stat buffer with file size, time etc.
 * @param[in] flags - One of the VIRTUALFLAG_* flags to control the detailed behaviour.
 * @return Returns constant pointer to VIRTUALFILE object of file, nullptr if not found
 */
LPVIRTUALFILE   insert_file(VIRTUALTYPE type, const std::string &virtfile, const std::string & origfile, const struct stat *stbuf, int flags = VIRTUALFLAG_NONE);
/**
 * @brief Add new virtual directory to internal list. If the file already exists, it will be updated.
 * @param[in] type - Type of virtual file.
 * @param[in] virtdir - Name of virtual directory.
 * @param[in] stbuf - stat buffer with file size, time etc.
 * @param[in] flags - One of the VIRTUALFLAG_* flags to control the detailed behaviour.
 * @return Returns constant pointer to VIRTUALFILE object of file, nullptr if not found
 */
LPVIRTUALFILE   insert_dir(VIRTUALTYPE type, const std::string & virtdir, const struct stat * stbuf, int flags = VIRTUALFLAG_NONE);
/**
 * @brief Find file in cache.
 * @param[in] virtfile - Virtual filename and path of file to find.
 * @return If found, returns VIRTUALFILE object, if not found returns nullptr.
 */
LPVIRTUALFILE   find_file(const std::string &virtfile);
/**
 * @brief Check if path has already been parsed.
 * Only useful if for DVD, Bluray or VCD where it is guaranteed that all files have been parsed whenever the directory is in the hash.
 * @param[in] path - Path to parse.
 * @return Returns true if path was found; false if not.
 */
bool            check_path(const std::string & path);
/**
 * @brief Load a path with virtual files for FUSE.
 * @param[in] path - Physical path to load.
 * @param[in] statbuf - stat buffer to load.
 * @param[in] buf - FUSE buffer to fill.
 * @param[in] filler - Filler function.
 * @return Returns number of files found.
 */
int             load_path(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);
/**
 * @brief Given the destination (post-transcode) file name, determine the parent of the file to be transcoded.
 * @param[in] origpath - The original file
 * @return Returns contstant pointer to VIRTUALFILE object of file, nullptr if not found
 */
LPVIRTUALFILE   find_original(const std::string & origpath);
/**
 * @brief Given the destination (post-transcode) file name, determine the name of the original file to be transcoded.
 * @param[inout] filepath - Input the original file, output name of transcoded file.
 * @return Returns contstant pointer to VIRTUALFILE object of file, nullptr if not found
 */
LPVIRTUALFILE   find_original(std::string *filepath);
/**
 * @brief Given the destination (post-transcode) file name, determine the parent of the file to be transcoded.
 * @param[in] origpath - The original file
 * @return Returns contstant pointer to VIRTUALFILE object of file, nullptr if not found
 */
LPVIRTUALFILE   find_parent(const std::string & origpath);

/**
 * @brief Convert SAMPLE_FMT enum to human readable text.
 * @return SAMPLE_FMT enum as text or "INVALID" if not known.
 */
std::string         get_sampleformat_text(SAMPLE_FMT sample_fmt);
/**
 * @brief Convert AUTOCOPY enum to human readable text.
 * @param[in] autocopy - AUTOCOPY enum value to convert.
 * @return AUTOCOPY enum as text or "INVALID" if not known.
 */
std::string 		get_autocopy_text(AUTOCOPY autocopy);
/**
 * @brief Convert RECODESAME enum to human readable text.
 * @param[in] recode - RECODESAME enum value to convert.
 * @return RECODESAME enum as text or "INVALID" if not known.
 */
std::string 		get_recodesame_text(RECODESAME recode);
/**
 * @brief Convert PROFILE enum to human readable text.
 * @param[in] profile - PROFILE enum value to convert.
 * @return PROFILE enum as text or "INVALID" if not known.
 */
std::string 		get_profile_text(PROFILE profile);
/**
 * @brief Convert PRORESLEVEL enum to human readable text.
 * @param[in] level - PRORESLEVEL enum value to convert.
 * @return PRORESLEVEL enum as text or "INVALID" if not known.
 */
std::string 		get_level_text(PRORESLEVEL level);
/**
 * @brief Get the selected hardware acceleration as text.
 * @param[in] hwaccel_API - Hardware acceleration buffering API.
 * @return Hardware acceleration API as string.
 */
std::string  		get_hwaccel_API_text(HWACCELAPI hwaccel_API);

/**
 * @brief Check if codec_id and optional profile is in block list.
 * @param codec_id - Codec ID of rto check
 * @param profile - Profile to check. Set to FF_PROFILE_UNKOWN to ignore.
 * @return Returns true if codec is in block list, false if not.
 */
bool                check_hwaccel_dec_blocked(AVCodecID codec_id, int profile);

/**
 * @brief Wrapper to the Fuse filler function.
 * @param[inout] buf - The buffer passed to the readdir() operation. May be nullptr.
 * @param[in] filler - Function pointer to the Fuse update function.  May be nullptr.
 * @param[in] name - The file name of the directory entry. Do not include the path!
 * @param[in] stbuf - File attributes, can be nullptr.
 * @param[in] off - Offset of the next entry or zero.
 * @return 1 if buffer is full, zero otherwise or if buf or filler is nullptr.
 */
int             add_fuse_entry(void *buf, fuse_fill_dir_t filler, const char * name, const struct stat *stbuf, off_t off);

/**
 * @brief Create the ./.. entries for a virtual directory
 * @param[inout] buf - The buffer passed to the readdir() operation. May be nullptr.
 * @param[in] filler - Function pointer to the Fuse update function.  May be nullptr.
 * @param[in] stbuf - File attributes, can be nullptr.
 * @param[in] off - Offset of the next entry or zero.
 * @return 1 if buffer is full, zero otherwise or if buf or filler is nullptr.
 */
int             add_dotdot(void *buf, fuse_fill_dir_t filler, const struct stat *stbuf, off_t off);
