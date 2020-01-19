/*
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
 */

/**
 * @file
 * @brief S/VCD VcdEntries class
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2013-2020 Norbert Schlia (nschlia@oblivion-software.de) @n
 * From BullysPLayer Copyright (C) 1984-2020 by Oblivion Software/Norbert Schlia
 */

#ifndef VCDENTRIES_H
#define VCDENTRIES_H

#pragma once

#include "vcdchapter.h"

#include <vector>

/** @brief Video CD entries
 */
class VcdEntries
{
public:
    /**
      * Seek results
      */
    typedef enum SEEKRES
    {
        SEEKRES_NOTFOUND,                                           /**< @brief Sync not found */
        SEEKRES_FOUND,                                              /**< @brief Sync found */
        SEEKRES_ERROR                                               /**< @brief Seek error */
    } SEEKRES, *LPSEEKRES;                                          /**< @brief Pointer to SEEKRES */
    typedef const SEEKRES * LPCSEEKRES;                             /**< @brief Const pointer to SEEKRES */

public:
    /**
     * @brief Construct #VcdEntries object
     */
    explicit VcdEntries();
    /**
     * @brief Destroy VcdEntries object
     */
    virtual ~VcdEntries();

    /**
     * @brief Reset this object
     */
    void                        clear();
    /**
     * @brief Load VCD from path
     * @param[in] path - path to locate VCD in
     * @return On success, returns 0; in case of error returns errno
     */
    int                         load_file(const std::string & path);

    /**
     * @brief Get date of disk (takenf rom INFO.VCD or SVD).
     * @return Returns date of disk.
     */
    time_t                      get_file_date() const;
    /**
     * @brief Get disk ID.
     * @return Returns disk ID.
     */
    const std::string   &       get_id() const;
    /**
     * @brief Get disk type.
     * @return Returns disk type.
     */
    VCDTYPE                     get_type() const;
    /**
     * @brief Get disk type as string.
     * @return Returns disk type as string.
     */
    std::string                 get_type_str() const;
    /**
     * @brief Get disk profile tag.
     * @return Returns disk profile tag.
     */
    VCDPROFILETAG               get_profile_tag() const;
    /**
     * @brief Get disk profile tag as string.
     * @return Returns disk profile tag as string.
     */
    std::string                 get_profile_tag_str() const;
    /**
     * @brief Get number of chapters on this disk.
     * @return Returns number of chapters on this disk.
     */
    int                         get_number_of_chapters() const;
    /**
     * @brief Get chapter object.
     * @note At least 1 chapter is guaranteed to exist if a disk was sucessfully read.
     * @param[in] chapter_idx - 0..number of chapters - 1
     * @return VcdChapter object with this chapter, nullptr if chapter_idx is invalid
     */
    const VcdChapter  *         get_chapter(int chapter_idx) const;
    /**
     * @brief Get total disk duration, in AV_TIME_BASE fractional seconds.
     * @return Returns total disk duration.
     */
    int64_t                     get_duration() const;
    /**
     * @brief Get disk size (DAT/MPEG only).
     * @return Returns disk size.
     */
    uint64_t                    get_size() const;
    /** @brief Get disk directory.
     * @return Returns disk directory.
     */
    const std::string &         get_disk_path() const;

protected:
    /**
     * @brief Scan disk for chapters.
     * @return On success, returns 0; on error, returns errno.
     */
    int                         scan_chapters();
    /**
     * @brief Seek for sync bytes.
     * @param[in] fpi - file pointer of open file
     * @param[in] sync - sync bytes
     * @param[in] len - length of sync bytes
     * @return Returns SEEKRES result code.
     */
    SEEKRES                     seek_sync(FILE *fpi, const char * sync, int len) const;

protected:
    // Common data
    time_t                      m_file_date;                        /**< @brief File date */
    std::string                 m_id;                               /**< @brief ID of CD. */
    VCDTYPE                     m_type;                             /**< @brief Type of CD. */
    VCDPROFILETAG               m_profile_tag;                      /**< @brief System profile tag. */
    // ENTRIES.XXX data
    std::vector<VcdChapter>     m_chapters;                         /**< @brief VCD chapters */
    int64_t                     m_duration;                         /**< @brief Total disk duration, in AV_TIME_BASE fractional seconds. */

    // misc.
    std::string                 m_disk_path;                        /**< @brief Path to this disk */
};

#endif // VCDENTRIES_H
