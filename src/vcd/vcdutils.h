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
 * @brief S/VCD utility functions
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2013-2021 Norbert Schlia (nschlia@oblivion-software.de) @n
 * From BullysPLayer Copyright (C) 1984-2021 by Oblivion Software/Norbert Schlia
 */

#ifndef VCDUTILS_H
#define VCDUTILS_H

#pragma once

#include <string>

/** Convert BCD value to hex
 */
#define     BCD2DEC(hex)        (((hex & 0xF0) >> 4) * 10 + (hex & 0x0F))

/** Max. number of chapters on video CD
 */
#define     VCD_MAX_CHAPTERS    500

#pragma pack(push)
#pragma pack(1)

/** @brief Video CD MSF time format
 *
 * MSF in minutes, seconds, and fractional seconds (frames). @n
 *
 * @note The fields in this structure are BCD encoded
 */
typedef struct VCDMSF
{
    uint8_t m_min;                          /**< @brief Minute in BCD code */
    uint8_t m_sec;                          /**< @brief Second in BCD code */
    uint8_t m_frame;                        /**< @brief Number of frames, for a 25 frames per second movie 0...24. */

} VCDMSF, *LPVCDMSF;                        /**< @brief Pointer version of VCDMSF */
typedef const VCDMSF * LPCVCDMSF;           /**< @brief Pointer to const version of VCDMSF */

/** @brief Video CD chapter
 */
typedef struct VCDCHAPTER
{
    uint8_t m_track_no;                     /**< @brief Track number */
    VCDMSF  m_msf;                          /**< @brief MSF position of chapter in file */

} VCDCHAPTER, *LPVCDCHAPTER;                /**< @brief Pointer version of VCDCHAPTER */
typedef const VCDCHAPTER * LPCVCDCHAPTER;   /**< @brief Pointer to const version of VCDCHAPTER */

/** @brief Video CD entry
 */
typedef struct VCDENTRIES
{
    char        m_ID[8];                    /**< @brief 8 Bytes: ID "ENTRYVCD" or "ENTRYSVD" */
    /** @brief 1 Byte: CD type
     *
     * 1 for VCD 1.0, VCD 1.1, SVCD 1.0 and HQVCD @n
     * 2 for VCD 2.0 @n
     * Identical with value in INFO.VCD/SVD @n
     */
    uint8_t     m_type;
    /** @brief 1 Byte: System Profile Tag.
     *
     * 0 for VCD 1.0, VCD 2.0, SVCD und HQVCD
     * 1 for VCD 1.1
     * Identical with value in INFO.VCD/SVD
     */
    uint8_t     m_profile_tag;
    /** @brief 2 Bytes: 1 <= tracks <= 500
     *
     * There must be at least 1 chapter
     */
    uint16_t    m_num_entries;
    /** @brief Chapters
     *
     * Number of chapters as in m_profile_tag @n
     * @n
     * 1 Byte: Tracknummer @n
     * 3 Byte: Adresse MSF
     */
    VCDCHAPTER  m_chapter[VCD_MAX_CHAPTERS];

    uint8_t     reserved[36];               /**< @brief RESERVED, must be 0x00 */

} VCDENTRY, *LPVCDENTRIES;                  /**< @brief Pointer version of VCDENTRY */
typedef const VCDENTRY * LPCVCDENTRIES;     /**< @brief Pointer to const version of VCDENTRY */
#pragma pack(pop)

extern const char SYNC[12];                 /**< @brief Chapter synchronisation in S/VCD mpeg/dat files (12 byte: 0x00FFFFFFFFFFFFFFFFFFFF00) */

/** @namespace VCDUTILS
 *  @brief Video CD utility functions
 */
namespace VCDUTILS
{
/**
 * @brief Convert non zero terminated text to std::string
 * @param[in] txt - Text for conversion (not zero terminated)
 * @param[in] size - Length of string
 * @param[in] trimmed - If true, trim trailing white spaces
 * @return Converted text
 */
std::string convert_txt2string(const char * txt, int size, bool trimmed = true);
/**
 * @brief Check if path is a S/VCD
 * @param[in] path - Path to check
 * @param[in] filename - file name to check, can per ENTRIES or INFO (extension .SVD or .VCD will be added automatically).
 * @param[out] fullname - path and filename or ENTRIES.SVC/VCD or INFO.SVC/VCD if found
 * @param[out] is_vcd - true if directory contains a Super Video CD, false if it's a Video CD
 * @return true if path contains a S/VCD, false if not
 */
bool        locate_file(const std::string & path, const std::string & filename, std::string & fullname, bool & is_vcd);
/**
 * @brief Locate AVSEQ*DAT/MPEG video file for track_no
 * @param[in] path - path to search in
 * @param[in] track_no - track number (1...n)
 * @param[out] fullname - name and path of file if found
 * @return
 */
int         locate_video(const std::string & path, int track_no, std::string & fullname);
/**
 * @brief Return disk type as string
 * @param[in] type - 1: VCD 1.0, VCD 1.1, SVCD 1.0, HQVCD, 2: VCD 2.0
 * @return Disk type as human readable string
 */
std::string get_type_str(int type);
/**
 * @brief get_profile_tag_str
 * @param[in] tag - 1: VCD 1.0, VCD 2.0, SVCD, HQVCD, 2: VCD 1.1
 * @return Profile as human readable string
 */
std::string get_profile_tag_str(int tag);
/**
 * @brief Check if fullname is a directory. Remove filename if necessary.
 * @note Really checks if fullname is a path even if the trailing slash is missing.
 * @param[in] fullname - Path and optional filename
 * @param[out] directory - Directory without file name
 */
void        get_directory(const std::string & fullname, std::string * directory);
}

#endif // VCDUTILS_H
