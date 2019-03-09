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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file
 * @brief S/VCD VcdChapter class
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2013-2019 Norbert Schlia (nschlia@oblivion-software.de) @n
 * From BullysPLayer Copyright (C) 1984-2017 by Oblivion Software/Norbert Schlia
 */

#pragma once

#ifndef VCDCHAPTER_H
#define VCDCHAPTER_H

#include <string>

/**
  * S/VCD type
  */
typedef enum VCDTYPE
{
    VCDTYPE_UNKNOWN = -1,                               /**< @brief unknown type */
    VCDTYPE_VCD_10_11_SVCD_10_HQVCD = 1,                /**< @brief VCD 1.0, VCD 1.1, SVCD 1.0 und HQVCD */
    VCDTYPE_VCD_20 = 2                                  /**< @brief VCD 2.0 */
} VCDTYPE;

/**
  * S/VCD profile tag
  */
typedef enum VCDPROFILETAG
{
    VCDPROFILETAG_UNKNOWN = -1,                         /**< @brief unknown file tag */
    VCDPROFILETAG_VCD_10_20_SVCD_HQVCD = 0,             /**< @brief VCD 1.0, VCD 2.0, SVCD und HQVCD */
    VCDPROFILETAG_VCD_11 = 1                            /**< @brief VCD 1.1 */
} VCDPROFILETAG;

struct VCDCHAPTER;                                      /**< @brief Video CD chapter forward declaration */
typedef struct VCDCHAPTER VCDCHAPTER;

/** @brief Video CD chapter
 */
class VcdChapter
{
    friend class VcdEntries;

public:
    /**
     * @brief Construct VcdChapter object
     * @param[in] is_svcd - true for SVCD CD, false for VCD
     */
    explicit VcdChapter(bool is_svcd);
    /**
     * @brief Construct VcdChapter object
     * @param[in] VcdChapter - source object to copy from
     * @param[in] is_svcd - true for SVCD CD, false for VCD
     */
    explicit VcdChapter(const VCDCHAPTER & VcdChapter, bool is_svcd);
    /**
     * @brief Construct VcdChapter object
     * @param[in] track_no - track number 1..
     * @param[in] min - Start minute
     * @param[in] sec - Start second
     * @param[in] frame - Start frame
     * @param[in] is_svcd - true for SVCD CD, false for VCD
     * @param[in] duration - Chapter duration, in AV_TIME_BASE fractional seconds
     */
    explicit VcdChapter(int track_no, int min, int sec, int frame, bool is_svcd, int64_t duration);
    /**
     * @brief Destroy VcdChapter object
     */
    virtual ~VcdChapter();

    bool        get_is_vcd() const;                     /**< @brief true for SVCD, false for VCD */
    int         get_track_no() const;                   /**< @brief Track no */
    int         get_min() const;                        /**< @brief MSF minute */
    int         get_sec() const;                        /**< @brief MSF second */
    int         get_frame() const;                      /**< @brief MSF frame */
    int64_t     get_duration() const;                   /**< @brief Return chapter duration, in AV_TIME_BASE fractional seconds */

    std::string get_filename() const;                   /**< @brief File name and path (e.g. MPEG/AVSEQ##.MPG) */
    uint64_t    get_start_pos() const;                  /**< @brief File position of chapter in bytes */
    uint64_t    get_end_pos() const;                    /**< @brief End position of chapter in bytes */
    int64_t     get_start_time() const;                 /**< @brief Start position of chapter in AV_TIME_BASE units */
    uint64_t    get_size() const;                       /**< @brief Get chapter size */
    int         get_lba() const;                        /**< @brief LBA (large block address) */

    /**
     * @brief operator =
     * @param[in] other
     * @return this
     */
    VcdChapter & operator= (VcdChapter const & other);

    /**
     * @brief operator ==
     * @param[in] other
     * @return Nonzero if equal, 0 if not
     */
    int operator==(const VcdChapter & other) const;
    /**
     * @brief operator <
     * @param[in] other
     * @return Nonzero if this object is smaller, 0 if not
     */
    int operator<(const VcdChapter & other) const;
    /**
     * @brief operator <=
     * @param[in] other
     * @return Nonzero if this object is smaller or equal, 0 if not
     */
    int operator<=(const VcdChapter & other) const;
    /**
     * @brief operator >
     * @param[in] other
     * @return Nonzero if this object is greater, 0 if not
     */
    int operator>(const VcdChapter & other) const;
    /**
     * @brief operator >=
     * @param[in] other
     * @return Nonzero if this object is greater or equal, 0 if not
     */
    int operator>=(const VcdChapter & other) const;
    /**
     * @brief operator !=
     * @param[in] other
     * @return Nonzero if this object is not equal, 0 if equal
     */
    int operator!=(const VcdChapter & other) const;

protected:
    /**
     * @brief Read file from disk
     * @param[in] fpi - Open file object to read from
     * @param[in] track_no - Track number 1...
     * @return
     */
    int         read(FILE *fpi, int track_no);

protected:
    bool        m_is_svcd;                              /**< @brief true for SVCD, false for VCD */
    int         m_track_no;                             /**< @brief Track no */
    int         m_min;                                  /**< @brief MSF minute */
    int         m_sec;                                  /**< @brief MSF second */
    int         m_frame;                                /**< @brief MSF frame */
    int64_t     m_duration;                             /**< @brief Chapter duration, in AV_TIME_BASE fractional seconds */
    uint64_t    m_start_pos;                            /**< @brief Start offset in bytes */
    uint64_t    m_end_pos;                              /**< @brief End offset in bytes (not including this byte) */
};

#endif // VCDCHAPTER_H
