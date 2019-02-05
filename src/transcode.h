/*
 * FileTranscoder interface for FFmpegfs
 *
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2013 K. Henriksson
 * Copyright (C) 2017-2019 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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

#ifndef TRANSCODE_H
#define TRANSCODE_H

#pragma once

#include "ffmpegfs.h"
#include "fileio.h"

// Simply get encoded file size (do not create the whole encoder/decoder objects)
bool            transcoder_cached_filesize(LPVIRTUALFILE virtualfile, struct stat *stbuf);
// Set the file size
bool            transcoder_set_filesize(LPVIRTUALFILE virtualfile, double duration, BITRATE audio_bit_rate, int channels, int sample_rate, BITRATE video_bit_rate, int width, int height, int interleaved, double frame_rate);
// Predict file size
bool            transcoder_predict_filesize(LPVIRTUALFILE virtualfile, Cache_Entry* cache_entry);

// Functions for doing transcoding, called by main program body
// Allocate and initialize the transcoder
Cache_Entry*    transcoder_new(LPVIRTUALFILE virtualfile, bool begin_transcode);
// Read some bytes into the internal buffer and into the given buffer.
ssize_t         transcoder_read(Cache_Entry* cache_entry, char* buff, off_t offset, size_t len);
// Free the transcoder structure.
void            transcoder_delete(Cache_Entry* cache_entry);
// Return size of output file, as computed by Encoder.
size_t          transcoder_get_size(Cache_Entry* cache_entry);
size_t          transcoder_buffer_watermark(Cache_Entry* cache_entry);
size_t          transcoder_buffer_tell(Cache_Entry* cache_entry);
void            transcoder_exit(void);

#endif
