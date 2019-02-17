/*
 * WAV file header structure
 *
 * Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef WAVE_H
#define WAVE_H

#pragma once

#include <stdint.h>

// WAVE header structure

typedef struct WAV_HEADER
{
    // RIFF Header
    uint8_t     m_riff_header[4];
    uint32_t    m_wav_size;
    uint8_t     m_wave_header[4];

    // Format Header
    uint8_t     m_fmt_header[4];
    uint32_t    m_fmt_chunk_size;
    uint16_t    m_audio_format;
    uint16_t    m_num_channels;
    uint32_t    m_sample_rate;
    uint32_t    m_byte_rate;
    uint16_t    m_sample_alignment;
    uint16_t    m_bit_depth;
} WAV_HEADER;

typedef struct WAV_LIST_HEADER
{
    // Data
    uint8_t    m_list_header[4];
    uint32_t    m_data_bytes;
    uint8_t    m_list_type[4];
} WAV_LIST_HEADER;

typedef struct WAV_DATA_HEADER
{
    // Data
    uint8_t    m_data_header[4];
    uint32_t    m_data_bytes;
    // uint8_t m_bytes[];     // Remainder of wave file
} WAV_DATA_HEADER;

#endif // WAVE_H
