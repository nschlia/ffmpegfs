/*
 * Copyright (C) 2017-2021 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @copyright Copyright (C) 2017-2021 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef AIFF_H
#define AIFF_H

#pragma once

#include <stdint.h>

#pragma pack(push, 1)

typedef uint8_t AIFF_ID[4];

#define AIFF_FORMID "FORM" /* ckID for Form Chunk */
typedef struct
{
    AIFF_ID     m_ckID;
    uint32_t    m_ckSize;
    AIFF_ID     formType;
    //uint8_t   chunks[];
} AIFF_FORMCHUNK;

typedef struct
{
    AIFF_ID     m_ckID;
    uint32_t    m_ckSize;
    //uint8_t   data[];
} AIFF_CHUNK;

#define AIFF_COMMONID "COMM" /* ckID for Common Chunk */
typedef struct
{
    AIFF_ID     m_ckID;
    uint32_t    m_ckSize;
    uint8_t     m_numChannels;
    uint32_t    m_numSampleFrames;
    uint8_t     m_sampleSize;
    //extended sampleRate;
} AIFF_COMMONCHUNK;

#define AIFF_SOUNDATAID "SSND" /* ckID for Sound Data Chunk */
typedef struct
{
    AIFF_ID     m_ckID;
    uint32_t    m_ckSize;
    uint32_t    m_offset;
    uint32_t    m_blockSize;
    //uint8_t   soundData[];
} AIFF_SOUNDDATACHUNK;

#define AIFF_NAMEID "NAME" /* ckID for Name Chunk. */
#define AIFF_AUTHORID "AUTH" /* ckID for Author Chunk. */
#define AIFF_COPYRIGHTID "(c) " /* ckID for Copyright Chunk. */
#define AIFF_ANNOTATIONID "ANNO" /* ckID for Annotation Chunk. */
typedef struct
{
    AIFF_ID     m_ckID;
    uint32_t    m_ckSize;
    //uint8_t   text[];
} AIFF_TEXTCHUNK;

#pragma pack(pop)

#endif // AIFF_H
