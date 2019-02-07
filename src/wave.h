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

// WAVE header structure

typedef struct WAV_HEADER
{
    // RIFF Header
    char    m_riff_header[4];
    int     m_wav_size;
    char    m_wave_header[4];

    // Format Header
    char    m_fmt_header[4];
    int     m_fmt_chunk_size;
    short   m_audio_format;
    short   m_num_channels;
    int     m_sample_rate;
    int     m_byte_rate;
    short   m_sample_alignment;
    short   m_bit_depth;
} WAV_HEADER;

typedef struct WAV_LIST_HEADER
{
    // Data
    char    m_list_header[4];
    int     m_data_bytes;
    char    m_list_type[4];
} WAV_LIST_HEADER;

typedef struct WAV_DATA_HEADER
{
    // Data
    char    m_data_header[4];
    int     m_data_bytes;
    // uint8_t m_bytes[];     // Remainder of wave file
} WAV_DATA_HEADER;

#endif // WAVE_H
