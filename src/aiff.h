/*
 * Copyright (C) 2017-2022 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file
 * @brief AIFF file structures
 * http://paulbourke.net/dataformats/audio/
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef AIFF_H
#define AIFF_H

#pragma once

#include <stdint.h>

#pragma pack(push, 1)

typedef uint8_t AIFF_ID[4];         /**< @brief AIFF fourcc ID */

#define AIFF_FORMID         "FORM"  /**< @brief ckID for Form Chunk */
/**
  * AIFF format chunk
  */
typedef struct
{
    AIFF_ID     m_ckID;             /**< @brief Chunk ID, always "FORM" */
    uint32_t    m_ckSize;           /**< @brief Total file size - 8 */
    AIFF_ID     formType;           /**< @brief  */
    //uint8_t   chunks[];
} AIFF_FORMCHUNK;

/**
  * AIFF chunk (anything else than the special chunks like form, common etc.)
  */
typedef struct
{
    AIFF_ID     m_ckID;             /**< @brief Chunk ID */
    uint32_t    m_ckSize;           /**< @brief Size of this chunk - 8 */
    //uint8_t   data[];
} AIFF_CHUNK;

#define AIFF_COMMONID       "COMM"  /**< @brief ckID for Common Chunk */
/**
  * AIFF Common chunk
  */
typedef struct
{
    AIFF_ID     m_ckID;             /**< @brief Chunk ID, always "COMM" */
    uint32_t    m_ckSize;           /**< @brief Size of this chunk - 8 */
    uint8_t     m_numChannels;      /**< @brief Number of audio channels for the sound. */
    uint32_t    m_numSampleFrames;  /**< @brief Number of sample frames in the sound data chunk. */
    uint8_t     m_sampleSize;       /**< @brief Number of bits in each sample point. */
    //extended sampleRate;
} AIFF_COMMONCHUNK;

#define AIFF_SOUNDATAID     "SSND"  /**< @brief ckID for Sound Data Chunk */
/**
  * AIFF sound data chunk
  */
typedef struct
{
    AIFF_ID     m_ckID;             /**< @brief Chunk ID, always "SSND" */
    uint32_t    m_ckSize;           /**< @brief Total size of sound data chunk - 8 */
    uint32_t    m_offset;           /**< @brief Determines where the first sample frame in the soundData starts. */
    uint32_t    m_blockSize;        /**< @brief Contains the size in bytes of the blocks that sound data is aligned to. */
    //uint8_t   soundData[];
} AIFF_SOUNDDATACHUNK;

#define AIFF_NAMEID         "NAME"  /**< @brief ckID for Name Chunk. */
#define AIFF_AUTHORID       "AUTH"  /**< @brief ckID for Author Chunk. */
#define AIFF_COPYRIGHTID    "(c) "  /**< @brief ckID for Copyright Chunk. */
#define AIFF_ANNOTATIONID   "ANNO"  /**< @brief ckID for Annotation Chunk. */
/**
  * AIFF name chunk
  */
typedef struct
{
    AIFF_ID     m_ckID;             /**< @brief Chunk ID, one of "NAME", "AUTH", "(c) ", "ANNO" */
    uint32_t    m_ckSize;           /**< @brief Size of this chunk - 8 */
    //uint8_t   text[];
} AIFF_TEXTCHUNK;

#pragma pack(pop)

#endif // AIFF_H
