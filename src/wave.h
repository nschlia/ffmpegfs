/*
 * Copyright (C) 2017-2024 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file wave.h
 * @brief WAVE file structures
 * https://wavefilegem.com/how_wave_files_work.html
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2024 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef WAVE_H
#define WAVE_H

#pragma once

#include <stdint.h>

#include <array>

#pragma pack(push, 1)

/**
 * @brief WAVE header structure
 *
 * @note All numeric values are in big-endian format.
 */
typedef struct WAV_HEADER
{
    /**@{*/
    std::array<uint8_t, 4>  m_riff_header;      /**< @brief RIFF Header: Contains the letters "RIFF" in ASCII form (0x52494646 big-endian form). */
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
    uint32_t                m_wav_size;
    std::array<uint8_t, 4>  m_wave_header;      /**< @brief Contains the letters "WAVE" (0x57415645 big-endian form). */
    /**@}*/

    /**@{*/
    std::array<uint8_t, 4>  m_fmt_header;       /**< @brief RIFF Format Header: Contains "fmt " including trailing space (0x666d7420 big-endian form). */
    uint32_t                m_fmt_chunk_size;   /**< @brief Should be 16 for PCM This is the size of the rest of the chunk size which follows this number. */
    uint16_t                m_audio_format;     /**< @brief Should be 1 for PCM. 3 for IEEE Float */
    uint16_t                m_num_channels;     /**< @brief Number of channels 1...n (1: mono, 2: stereo/dual channel;...) */
    uint32_t                m_sample_rate;      /**< @brief 8000, 44100, etc. */
    uint32_t                m_byte_rate;        /**< @brief Number of bytes per second. sample_rate * num_channels * bit_depth / 8 */
    uint16_t                m_sample_alignment; /**< @brief num_channels * bit_depth / 8 */
    uint16_t                m_bit_depth;        /**< @brief Number of bits per sample: 8 bits = 8, 16 bits = 16, etc. */
    /**@}*/
} WAV_HEADER;

static_assert(sizeof(WAV_HEADER) == 36);

/**
 * @brief WAVE extended header structure
 *
 * @note All numeric values are in big-endian format.
 */
typedef struct WAV_HEADER_EX
{
    uint16_t                m_extension_size;           /**< @brief Extension Size           2 	16-bit unsigned integer (value 22) */
    uint16_t                m_valid_bits_per_sample;    /**< @brief Valid Bits Per Sample    2 	16-bit unsigned integer */
    uint32_t                m_channel_mask;             /**< @brief Channel Mask             4 	32-bit unsigned integer */
    std::array<uint8_t, 16> m_sub_format_guid;          /**< @brief Sub Format GUID          16 16-byte GUID */
} WAV_HEADER_EX;

static_assert(sizeof(WAV_HEADER_EX) == 24);

/**
 * @brief WAVE "fact" header structure
 *
 * @note All numeric values are in big-endian format.
 */
typedef struct WAV_FACT
{
    std::array<uint8_t, 4>  m_chunk_id;                 /**< @brief Chunk ID                4 	0x66 0x61 0x63 0x74 (i.e. "fact") */
    uint32_t                m_body_size;                /**< @brief Chunk Body Size         4 	32-bit unsigned integer */
    uint32_t                m_number_of_sample_frames;  /**< @brief Number of sample frames 4 	32-bit unsigned integer */
} WAV_FACT;

static_assert(sizeof(WAV_FACT) == 12);

/**
 *  @brief WAVE list header structure
 *
 *  @note All numeric values are in big-endian format.
 */
typedef struct WAV_LIST_HEADER
{
    std::array<uint8_t, 4>  m_list_header;              /**< @brief Contains "list" (0x6C696E74) */
    uint32_t                m_data_bytes;               /**< @brief Number of bytes in list. */
    std::array<uint8_t, 4>  m_list_type;                /**< @brief Contains "adtl" (0x6164746C) */
} WAV_LIST_HEADER;

static_assert(sizeof(WAV_LIST_HEADER) == 12);

/** @brief WAVE data header structure
 *
 *  @note All numeric values are in big-endian format.
 */
typedef struct WAV_DATA_HEADER
{
    std::array<uint8_t, 4>  m_data_header;              /**< @brief Contains "data" (0x64617461 big-endian form). */
    uint32_t                m_data_bytes;               /**< @brief Number of bytes in data. Number of samples * num_channels * bit_depth / 8 */
    // Remainder of wave file: actual sound data
    // uint8_t m_bytes[];
} WAV_DATA_HEADER;

static_assert(sizeof(WAV_DATA_HEADER) == 8);

#pragma pack(pop)

#endif // WAVE_H
