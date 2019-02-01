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
    char riff_header[4];    // Contains "RIFF"
    int wav_size;           // Size of the wav portion of the file, which follows the first 8 bytes. File size - 8
    char wave_header[4];    // Contains "WAVE"

    // Format Header
    char fmt_header[4];     // Contains "fmt " (includes trailing space)
    int fmt_chunk_size;     // Should be 16 for PCM
    short audio_format;     // Should be 1 for PCM. 3 for IEEE Float
    short num_channels;
    int sample_rate;
    int byte_rate;          // Number of bytes per second. sample_rate * num_channels * Bytes Per Sample
    short sample_alignment; // num_channels * Bytes Per Sample
    short bit_depth;        // Number of bits per sample
} WAV_HEADER;

typedef struct WAV_LIST_HEADER
{
    // Data
    char list_header[4];    // Contains "list" (0x6C696E74)
    int data_bytes;         // Number of bytes in list.
    char list_type[4];      // Contains "adtl" (0x6164746C)
} WAV_LIST_HEADER;

typedef struct WAV_DATA_HEADER
{
    // Data
    char data_header[4];    // Contains "data"
    int data_bytes;         // Number of bytes in data. Number of samples * num_channels * sample byte size
    // uint8_t bytes[];     // Remainder of wave file
} WAV_DATA_HEADER;

#endif // WAVE_H
