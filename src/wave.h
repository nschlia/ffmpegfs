/*
 * Copyright (C) 2017-2020 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @brief WAVE file structures
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2020 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef WAVE_H
#define WAVE_H

#pragma once

#include <stdint.h>

/**
 * @brief WAVE header structure
 * @note All numeric values are in big-endian format.
 */
typedef struct WAV_HEADER
{
    /**@{*/
    uint8_t     m_riff_header[4];   /**< @brief RIFF Header: Contains the letters "RIFF" in ASCII form (0x52494646 big-endian form). */
    /**
     * Size of the wav portion of the file, which follows the first 8 bytes (File size - 8).@n
     * 36 + fmt_chunk_size, or more precisely:@n
     * 4 + (8 + fmt_chunk_size) + (8 + fmt_chunk_size)
     * This is the size of the rest of the chunk
     * following this number.  This is the size of the
     * entire file in bytes minus 8 bytes for the
     * two fields not included in this count:
     * riff_header and wav_size.
     */
    uint32_t    m_wav_size;
    uint8_t     m_wave_header[4];   /**< @brief Contains the letters "WAVE" (0x57415645 big-endian form). */
    /**@}*/

    /**@{*/
    uint8_t     m_fmt_header[4];    /**< @brief RIFF Format Header: Contains "fmt " including trailing space (0x666d7420 big-endian form). */
    uint32_t    m_fmt_chunk_size;   /**< @brief Should be 16 for PCM This is the size of the rest of the chunk size which follows this number. */
    uint16_t    m_audio_format;     /**< @brief Should be 1 for PCM. 3 for IEEE Float */
    uint16_t    m_num_channels;     /**< @brief Number of channels 1...n (1: mono, 2: stereo/dual channel;...) */
    uint32_t    m_sample_rate;      /**< @brief 8000, 44100, etc. */
    uint32_t    m_byte_rate;        /**< @brief Number of bytes per second. sample_rate * num_channels * bit_depth / 8 */
    uint16_t    m_sample_alignment; /**< @brief num_channels * bit_depth / 8 */
    uint16_t    m_bit_depth;        /**< @brief Number of bits per sample: 8 bits = 8, 16 bits = 16, etc. */
    /**@}*/
} WAV_HEADER;

/** @brief WAVE list header structure
 *
 *  @note All numeric values are in big-endian format.
 */
typedef struct WAV_LIST_HEADER
{
    uint8_t     m_list_header[4];   /**< @brief Contains "list" (0x6C696E74) */
    uint32_t    m_data_bytes;       /**< @brief Number of bytes in list. */
    uint8_t     m_list_type[4];     /**< @brief Contains "adtl" (0x6164746C) */
} WAV_LIST_HEADER;

/** @brief WAVE data header structure
 *
 *  @note All numeric values are in big-endian format.
 */
typedef struct WAV_DATA_HEADER
{
    uint8_t     m_data_header[4];   /**< @brief Contains "data" (0x64617461 big-endian form). */
    uint32_t    m_data_bytes;       /**< @brief Number of bytes in data. Number of samples * num_channels * bit_depth / 8 */
    // Remainder of wave file: actual sound data
    // uint8_t m_bytes[];
} WAV_DATA_HEADER;

#endif // WAVE_H
