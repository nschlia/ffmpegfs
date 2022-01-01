/*
 * Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2022 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @brief File transcoder interface (for use with by FUSE)
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2022 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef TRANSCODE_H
#define TRANSCODE_H

#pragma once

#include "ffmpegfs.h"
#include "fileio.h"

/**
 * @brief Simply get encoded file size (do not create the whole encoder/decoder objects)
 * @param[in] virtualfile - virtual file object to open
 * @param[out] stbuf - stat struct filled in with the size of the cached file
 * @return Returns true if file was found in cache, false if not (stbuf will be unchanged)
 */
bool            transcoder_cached_filesize(LPVIRTUALFILE virtualfile, struct stat *stbuf);
/**
 * @brief Set the file size
 * @param[in] virtualfile - virtual file object to open.
 * @param[in] duration - duration of the file, in AV_TIME_BASE fractional seconds.
 * @param[in] audio_bit_rate - average bitrate of audio data (in bits per second).
 * @param[in] channels - number of channels (1: mono, 2: stereo, or more).
 * @param[in] sample_rate - number of audio samples per second.
 * @param[in] sample_format - Selected sample format
 * @param[in] video_bit_rate - average bitrate of video data (in bits per second).
 * @param[in] width - video width in pixels.
 * @param[in] height - video height in pixels.
 * @param[in] interleaved - 1 if video is interleaved, 0 if not.
 * @param[in] framerate - frame rate per second (e.g. 24, 25, 30...).
 * @return On error, returns false (size could not be set) or true on success.
 */
bool            transcoder_set_filesize(LPVIRTUALFILE virtualfile, int64_t duration, BITRATE audio_bit_rate, int channels, int sample_rate, AVSampleFormat sample_format, BITRATE video_bit_rate, int width, int height, int interleaved, const AVRational & framerate);
/**
 * @brief Predict file size
 * @param[in] virtualfile - virtual file object to open
 * @param[in] cache_entry - corresponding cache entry
 * @return On error, returns false (size could not be predicted) or true on success
 */
bool            transcoder_predict_filesize(LPVIRTUALFILE virtualfile, Cache_Entry* cache_entry = nullptr);

// Functions for doing transcoding, called by main program body
/**
 * @brief Allocate and initialise the transcoder
 *
 * Opens a file and starts the decoding thread if begin_transcode is true.
 * File will be scanned to detect bit rate, duration etc. only if begin_transcode is false.
 *
 * @param[in] virtualfile - virtual file object to open
 * @param[in] begin_transcode - if true, transcoding starts, if false file will be scanned only
 * @return On success, returns cache entry object. On error, returns nullptr and sets errno accordingly.
 */
Cache_Entry*    transcoder_new(LPVIRTUALFILE virtualfile, bool begin_transcode);
/**
 * @brief Read some bytes from the internal buffer and into the given buffer.
 * @note buff must be large enough to hold len number of bytes.
 * @note Returns number of bytes read, may be less than len bytes.
 * @param[in] cache_entry - corresponding cache entry
 * @param[out] buff - will be filled in with data read
 * @param[in] offset - byte offset to start reading at
 * @param[in] len - length of data chunk to be read.
 * @param[out] bytes_read - Bytes read from transcoder.
 * @param[in] segment_no - HLS segment file number.
 * @return On success, returns true. On error, returns false and sets errno accordingly.
 */
bool            transcoder_read(Cache_Entry* cache_entry, char* buff, size_t offset, size_t len, int *bytes_read, uint32_t segment_no);
/**
 * @brief Read one image frame from the internal buffer and into the given buffer.
 * @note buff must be large enough to hold len number of bytes.
 * @note Returns number of bytes read, may be less than len bytes.
 * @param[in] cache_entry - corresponding cache entry
 * @param[out] buff - will be filled in with data read
 * @param[in] offset - byte offset to start reading at
 * @param[in] len - length of data chunk to be read.
 * @param[in] frame_no - Number of frame to return.
 * @param[out] bytes_read - Bytes read from transcoder.
 * @param[in,out] virtualfile - Birtual file object of image, may be modified.
 * @return On success, returns true. On error, returns false and sets errno accordingly.
 */
bool            transcoder_read_frame(Cache_Entry* cache_entry, char* buff, size_t offset, size_t len, uint32_t frame_no, int * bytes_read, LPVIRTUALFILE virtualfile);
/**
 * @brief Free the cache entry structure.
 *
 * Call this to free the cache entry structure. @n
 * The structure is reference counted, after calling this function, if the cache entry is not
 * use by another thread, the cache entry may no longer be valid.
 *
 * @param[in] cache_entry - corresponding cache entry
 */
void            transcoder_delete(Cache_Entry* cache_entry);
/**
 * @brief Return size of output file, as computed by encoder.
 *
 * Returns the file size, either the predicted size (which may be inaccurate) or
 * the real size (which is only available once the file was completely recoded).
 *
 * @param[in] cache_entry - corresponding cache entry
 * @return The size of the file. Function never fails.
 */
size_t          transcoder_get_size(Cache_Entry* cache_entry);
/**
 * @brief Return the current watermark of the file while transcoding
 *
 * While transcoding, this value reflects the current size of the transcoded file.
 * This is the maximum byte offset until the file can be read so far.
 *
 * @param[in] cache_entry - corresponding cache entry
 * @param[in] segment_no - HLS segment file number.
 * @return Returns the current watermark
 */
size_t          transcoder_buffer_watermark(Cache_Entry* cache_entry, uint32_t segment_no);
/**
 * @brief Return the current file position in the file
 * @param[in] cache_entry - corresponding cache entry
 * @param[in] segment_no - HLS segment file number.
 * @return Returns the current file position
 */
size_t          transcoder_buffer_tell(Cache_Entry* cache_entry, uint32_t segment_no);
/**
 * @brief Exit transcoding
 *
 * Send signal to exit transcoding (ending all transcoder threads).
 */
void            transcoder_exit(void);

#endif
