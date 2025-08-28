/*
 * Copyright (C) 2017-2025 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file fileio.h
 * @brief FileIO class
 *
 * This class allows transparent access to files from DVD, Blu-ray, Video CD or
 * to regular disk files.
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2025 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef FILEIO_H
#define FILEIO_H

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/stat.h>

#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <memory>
#include <array>

// Disable annoying warnings outside our code
#ifdef __cplusplus
extern "C" {
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <libavutil/avutil.h>
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

#pragma pack(push, 1)

#define IMAGE_FRAME_TAG         "IMGFRAME"      /**< @brief Tag of an image frame header for the frame images buffer. */
/**
  * @brief Image frame header
  *
  * This image frame header will always start at an 8K boundary of the cache.
  *
  * It can be used to find the next image by seeking an 8K block starting with the tag.
  */
typedef struct IMAGE_FRAME
{
    std::array<char, 8>     m_tag;              /**< @brief Start tag, always ascii "IMGFRAME". */
    uint32_t                m_frame_no;         /**< @brief Number of the frame image. 0 if not yet decoded. */
    uint64_t                m_offset;           /**< @brief Offset in index file. */
    uint32_t                m_size;             /**< @brief Image size in bytes. */
    std::array<uint8_t, 8>  m_reserved;         /**< @brief Reserved. Pad structure to 32 bytes. */
    // ...data
} IMAGE_FRAME;
#pragma pack(pop)
typedef IMAGE_FRAME const *LPCIMAGE_FRAME;      /**< @brief Pointer version of IMAGE_FRAME */
typedef IMAGE_FRAME *LPIMAGE_FRAME;             /**< @brief Pointer to const version of IMAGE_FRAME */

/** @brief Virtual file types enum
 */
enum class VIRTUALTYPE
{
    PASSTHROUGH,                                        /**< @brief passthrough file, not used */
    DISK,                                               /**< @brief Regular disk file to transcode */
    SCRIPT,                                             /**< @brief Virtual script */
#ifdef USE_LIBVCD
    VCD,                                                /**< @brief Video CD file */
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
    DVD,                                                /**< @brief DVD file */
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
    BLURAY,                                             /**< @brief Blu-ray disk file */
#endif // USE_LIBBLURAY

    BUFFER,                                             /**< @brief Buffer file */
};
typedef VIRTUALTYPE const *LPCVIRTUALTYPE;                          /**< @brief Pointer version of VIRTUALTYPE */
typedef VIRTUALTYPE LPVIRTUALTYPE;                                  /**< @brief Pointer to const version of VIRTUALTYPE */

#define VIRTUALFLAG_NONE            0x00000000                      /**< @brief No flags */
#define VIRTUALFLAG_PASSTHROUGH     0x00000001                      /**< @brief passthrough file, not used */
#define VIRTUALFLAG_DIRECTORY       0x00000002                      /**< @brief File is a virtual directory */
#define VIRTUALFLAG_FILESET         0x00000004                      /**< @brief File is file set (images, HLS) */
#define VIRTUALFLAG_FRAME           0x00000008                      /**< @brief File is part of a set of frames */
#define VIRTUALFLAG_HLS             0x00000010                      /**< @brief File is part of a set of HLS transport stream (ts) files */
#define VIRTUALFLAG_CUESHEET        0x00000020                      /**< @brief File is part of a set of cue sheet tracks or the directory */
#define VIRTUALFLAG_HIDDEN          0x00000040                      /**< @brief File is not transcodable or should otherwise show in listings */

/** @brief Virtual file definition
 */
typedef struct VIRTUALFILE
{
    VIRTUALFILE()
        : m_type(VIRTUALTYPE::DISK)
        , m_flags(VIRTUALFLAG_NONE)
        , m_format_idx(0)
        , m_full_title(false)
        , m_duration(0)
        , m_predicted_size(0)
        , m_video_frame_count(0)
        , m_has_audio(false)
        , m_has_video(false)
        , m_has_subtitle(false)
        , m_channels(0)
        , m_sample_rate(0)
        , m_width(0)
        , m_height(0)
        , m_framerate{ 0, 0 }
    {
        std::memset(&m_st, 0, sizeof(m_st));
    }

    uint32_t get_segment_count() const;                             /**< @brief Number of HLS segments in set */

    VIRTUALTYPE             m_type;                                 /**< @brief Type of this virtual file */
    int                     m_flags;                                /**< @brief One of the VIRTUALFLAG_* flags */

    size_t                  m_format_idx;                           /**< @brief Index into params.format[] array */
    std::string             m_destfile;                             /**< @brief Name and path of destination file */
    std::string             m_virtfile;                             /**< @brief Name and path of virtual file */
    std::string             m_origfile;                             /**< @brief Sanitised name and path of original file */
    struct stat             m_st;                                   /**< @brief stat structure with size etc. */

    bool                    m_full_title;                           /**< @brief If true, ignore m_chapter_no and provide full track */
    int64_t                 m_duration;                             /**< @brief Track/chapter duration, in AV_TIME_BASE fractional seconds. */
    size_t                  m_predicted_size;                       /**< @brief Use this as the size instead of computing it over and over. */
    uint32_t                m_video_frame_count;                    /**< @brief Number of frames in video or 0 if not a video */

    bool                    m_has_audio;                            /**< @brief True if file has an audio track */
    bool                    m_has_video;                            /**< @brief True if file has a video track */
    bool                    m_has_subtitle;                         /**< @brief True if file has a subtitle track */

    std::vector<char>       m_file_contents;                        /**< @brief Buffer for virtual files */

#ifdef USE_LIBVCD
    /** @brief Extra value structure for Video CDs.
     *  @note Only available if compiled with -DUSE_LIBVCD.
     */
    struct VCD_CHAPTER
    {
        VCD_CHAPTER()
            : m_track_no(0)
            , m_chapter_no(0)
            , m_start_pos(0)
            , m_end_pos(0)
        {}
        int                 m_track_no;                             /**< @brief Track number (1..) */
        int                 m_chapter_no;                           /**< @brief Chapter number (1..) */
        uint64_t            m_start_pos;                            /**< @brief Start offset in bytes */
        uint64_t            m_end_pos;                              /**< @brief End offset in bytes (not including this byte) */
    }                       m_vcd;                                  /**< @brief S/VCD track/chapter info */
#endif //USE_LIBVCD
#ifdef USE_LIBDVD
    /** @brief Extra value structure for DVDs.
     *  @note Only available if compiled with -DUSE_LIBDVD.
     */
    struct DVD_CHAPTER
    {
        DVD_CHAPTER()
            : m_title_no(0)
            , m_chapter_no(0)
            , m_angle_no(0)
        {}
        int                 m_title_no;                             /**< @brief Track number (1...n) */
        int                 m_chapter_no;                           /**< @brief Chapter number (1...n) */
        int                 m_angle_no;                             /**< @brief Selected angle number (1...n) */
    }                       m_dvd;                                  /**< @brief DVD title/chapter info */
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
    /** @brief Extra value structure for Blu-ray disks.
     *  @note Only available if compiled with -DUSE_LIBBLURAY.
     */
    struct BLURAY_CHAPTER
    {
        BLURAY_CHAPTER()
            : m_title_no(0)
            , m_playlist_no(0)
            , m_chapter_no(0)
            , m_angle_no(0)
        {}
        uint32_t            m_title_no;                             /**< @brief Track number (1...n) */
        uint32_t            m_playlist_no;                          /**< @brief Playlist number (1...n) */
        unsigned            m_chapter_no;                           /**< @brief Chapter number (1...n) */
        unsigned            m_angle_no;                             /**< @brief Selected angle number (1...n) */
    }                       m_bluray;                               /**< @brief Blu-ray title/chapter info */
#endif // USE_LIBBLURAY
    /** @brief Extra value structure for cue sheets.
     */
    struct CUESHEET_TRACK
    {
        CUESHEET_TRACK()
            : m_tracktotal(0)
            , m_trackno(0)
            , m_start(0)
            , m_duration(0)
            , m_nextfile(nullptr)
        {}

        int                 m_tracktotal;                           /**< @brief Total number of tracks in cue sheet. */
        int                 m_trackno;                              /**< @brief Track number */
        std::string         m_artist;                               /**< @brief Track artist */
        std::string         m_title;                                /**< @brief Track title */
        std::string         m_album;                                /**< @brief Album title */
        std::string         m_genre;                                /**< @brief Album genre */
        std::string         m_date;                                 /**< @brief Publishing date */

        int64_t             m_start;                                /**< @brief Track start time, in AV_TIME_BASE fractional seconds. */
        int64_t             m_duration;                             /**< @brief Track/chapter duration, in AV_TIME_BASE fractional seconds. */

        VIRTUALFILE*        m_nextfile;                             /**< @brief Next (probable) file to be played. Used for cuesheet lists. */
    }                       m_cuesheet_track;                       /**< @brief Cue sheet data for track. */
    std::string             m_cuesheet;                             /**< @brief Cue sheet file contents for physical file. */

    // These may be filled in for DVD/Blu-ray
    int                     m_channels;                             /**< @brief Audio channels - Filled in for the DVD/Blu-ray directory. */
    int                     m_sample_rate;                          /**< @brief Audio sample rate - Filled in for the DVD/Blu-ray directory. */

    int                     m_width;                                /**< @brief Video width - Filled in for the DVD/Blu-ray directory. */
    int                     m_height;                               /**< @brief Video height - Filled in for the DVD/Blu-ray directory. */
    AVRational              m_framerate;                            /**< @brief Video frame rate - Filled in for the DVD/Blu-ray directory. */

} VIRTUALFILE;
typedef VIRTUALFILE const *LPCVIRTUALFILE;                          /**< @brief Pointer to const version of VIRTUALFILE. */
typedef VIRTUALFILE *LPVIRTUALFILE;                                 /**< @brief Pointer version of VIRTUALFILE. */

/** @brief Base class for I/O
 */
class FileIO
{
public:
    /**
     * @brief Create #FileIO object
     */
    explicit FileIO();
    /**
     * @brief Free #FileIO object
     */
    virtual ~FileIO() = default;

    /** @brief Allocate the correct object for type().
     *
     * Free with delete if no longer required.
     *
     * @param[in] type - VIRTUALTYPE of new object.
     * @return Upon successful completion, #FileIO of the requested type.
     * On error, (out of memory), it returns a nullptr.
     */
    static std::shared_ptr<FileIO> alloc(VIRTUALTYPE type);

    /**
     * @brief Get type of the virtual file.
     * @return Returns the type of the virtual file.
     */
    virtual VIRTUALTYPE type() const = 0;
    /**
     * @brief Get the ideal buffer size.
     * @return Return the ideal buffer size.
     */
    virtual size_t  	bufsize() const = 0;
    /** @brief Open a virtual file.
     * @param[in] virtualfile - LPCVIRTUALFILE of file to open.
     * @return Upon successful completion, #openio() returns 0. @n
     * On error, a nonzero value is returned and errno is set to indicate the error.
     */
    virtual int         openio(LPVIRTUALFILE virtualfile) = 0;
    /** @brief Read data from a file.
     * @param[out] data - A buffer to store read bytes in. It must be large enough to hold up to size bytes.
     * @param[in] size - The number of bytes to read.
     * @return Upon successful completion, #readio() returns the number of bytes read. @n
     * This may be less than size. @n
     * On error, the value 0 is returned and errno is set to indicate the error. @n
     * If at the end of the file, 0 may be returned but errno is not set. error() will return 0 if at EOF.
     */
    virtual size_t      readio(void *data, size_t size) = 0;
    /**
     * @brief Get last error.
     * @return errno value of last error.
     */
    virtual int         error() const = 0;
    /** @brief Get the duration of the file, in AV_TIME_BASE fractional seconds.
     *
     * This is only possible for file formats that are aware of the play time.
     * May be AV_NOPTS_VALUE if the time is not known.
     */
    virtual int64_t     duration() const = 0;
    /**
     * @brief Get the file size.
     * @return Returns the file size.
     */
    virtual size_t      size() const = 0;
    /**
     * @brief Get current read position.
     * @return Gets the current read position.
     */
    virtual size_t      tell() const = 0;
    /** @brief Seek to position in file
     *
     * Repositions the offset of the open file to the argument offset according to the directive whence.
     *
     * @param[in] offset - offset in bytes
     * @param[in] whence - how to seek: @n
     * SEEK_SET: The offset is set to offset bytes. @n
     * SEEK_CUR: The offset is set to its current location plus offset bytes. @n
     * SEEK_END: The offset is set to the size of the file plus offset bytes.
     * @return Upon successful completion, #seek() returns the resulting offset location as measured in bytes
     * from the beginning of the file.  @n
     * On error, the value -1 is returned and errno is set to indicate the error.
     */
    virtual int         seek(int64_t offset, int whence) = 0;
    /**
     * @brief Check if at end of file.
     * @return Returns true if at end of file.
     */
    virtual bool        eof() const = 0;
    /**
     * @brief Close virtual file.
     */
    virtual void        closeio() = 0;
    /**
     * @brief Get virtual file object
     * @return Current virtual file object or nullptr if unset.
     */
    LPVIRTUALFILE       virtualfile();
    /**
     * @brief Get source filename.
     * @return Returns source filename.
     */
    const std::string & filename() const;
    /**
     * @brief Path to source file (without file name)
     * @return  Returns path to source file.
     */
    const std::string & path() const;

protected:
    /** @brief Set the virtual file object.
     * @param[in] virtualfile - LPCVIRTUALFILE of file to set.
     */
    void                set_virtualfile(LPVIRTUALFILE virtualfile);

private:
    std::string         m_path;                                     /**< @brief Source path (directory without file name) */
    LPVIRTUALFILE       m_virtualfile;                              /**< @brief Virtual file object of current file */
};

#endif // FILEIO_H
