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
 * @brief S/VCD VcdInfo class
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2013-2019 Norbert Schlia (nschlia@oblivion-software.de) @n
 * From BullysPLayer Copyright (C) 1984-2017 by Oblivion Software/Norbert Schlia
 */

#pragma once

#ifndef VCDINFO_H
#define VCDINFO_H

#include "vcdchapter.h"

/**
 * @brief The #VcdInfo class
 */
class VcdInfo
{
public:
    /**
     * @brief Construct VcdInfo object
     */
    explicit VcdInfo();
    /**
     * @brief Destruct VcdInfo object
     */
    virtual ~VcdInfo();

    /**
     * @brief Reset this object
     */
    void                    clear();
    /**
     * @brief Load VCD from path
     * @param path - path to locate VCD in
     * @return On success, returns 0; in case of error returns errno
     */
    int                     load_file(const std::string & path);

    const time_t  &         get_file_date() const;          /**< @brief Date of disk (of INFO.VCD or SVD) */
    const std::string   &   get_id() const;                 /**< @brief Get disk ID */
    VCDTYPE                 get_type() const;               /**< @brief Get disk type */
    std::string             get_type_str() const;           /**< @brief Get disk type as string */
    VCDPROFILETAG           get_profile_tag() const;        /**< @brief Get disk profile tag */
    std::string             get_profile_tag_str() const;    /**< @brief Get disk profile tag as string */
    const std::string   &   get_album_id() const;           /**< @brief Get album ID */
    int                     get_number_of_cds() const;      /**< @brief Get number of CDs in set */
    int                     get_cd_number() const;          /**< @brief Get CD number in set */

protected:
    // Common data
    std::string             m_disk_path;                    /**< @brief Path to disk */
    time_t                  m_file_date;                    /**< @brief File date */
    std::string             m_id;                           /**< @brief ID of this CD. */
    VCDTYPE                 m_type;                         /**< @brief Type of CD. */
    VCDPROFILETAG           m_profile_tag;                  /**< @brief System profile tag. */
    // INFO.XXX data
    std::string             m_album_id;                     /**< @brief Album ID */
    int                     m_number_of_cds;                /**< @brief Number of CDs in set */
    int                     m_cd_number;                    /**< @brief Number of this CD in set */
};

#endif // VCDINFO_H
