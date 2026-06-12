/*
 * Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2026 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file transcode.h
 * @brief File transcoder interface (for use with by FUSE)
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2026 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef TRANSCODE_H
#define TRANSCODE_H

#pragma once

#include "ffmpegfs.h"
#include "fileio.h"

/**
 * @brief Fill a stat buffer with the cached or predicted transcoded file size.
 *
 * Uses the encoded size when the file has already been transcoded. If no
 * encoded size is known yet, the predicted transcoded size is used instead.
 *
 * @param[in] virtualfile Virtual file whose cached size should be queried.
 * @param[in,out] stbuf Stat buffer whose size field is updated on success.
 * @return Returns @c true when a cached or predicted size was available;
 *         otherwise @c false and @p stbuf is left unchanged.
 */
bool            transcoder_cached_filesize(LPVIRTUALFILE virtualfile, struct stat *stbuf);
/**
 * @brief Calculate and store the predicted transcoded file size.
 *
 * Estimates the output size from stream duration, selected output format,
 * audio parameters, video parameters, and container overhead. The result is
 * stored both in the cache entry and in the virtual file.
 *
 * @param[in,out] virtualfile Virtual file whose predicted size should be set.
 * @param[in] duration Media duration in FFmpeg time base units.
 * @param[in] audio_bit_rate Target audio bit rate in bits per second.
 * @param[in] channels Number of audio channels.
 * @param[in] sample_rate Audio sample rate in Hz.
 * @param[in] sample_format Audio sample format.
 * @param[in] video_bit_rate Target video bit rate in bits per second.
 * @param[in] width Video width in pixels.
 * @param[in] height Video height in pixels.
 * @param[in] interleaved Set to @c true for interleaved video output.
 * @param[in] framerate Video frame rate.
 * @return Returns @c true after updating the prediction; otherwise @c false.
 */
bool            transcoder_set_filesize(LPVIRTUALFILE virtualfile, int64_t duration, BITRATE audio_bit_rate, int channels, int sample_rate, AVSampleFormat sample_format, BITRATE video_bit_rate, int width, int height, bool interleaved, const AVRational & framerate);
/**
 * @brief Probe an input file and update cached media properties.
 *
 * Opens the source with FFmpeg, reads the predicted transcoded size, duration,
 * video frame count, and HLS/frame-set segment count, then stores those values
 * in the cache entry when one was supplied.
 *
 * @param[in,out] virtualfile Virtual file to inspect.
 * @param[in,out] cache_entry Optional cache entry to update with probed values.
 * @return Returns @c true when probing succeeded; otherwise @c false.
 */
bool            transcoder_predict_filesize(LPVIRTUALFILE virtualfile, Cache_Entry* cache_entry = nullptr);

// Functions for doing transcoding, called by main program body
/**
 * @brief Open or create a cache entry for a virtual file.
 *
 * Opens the cache entry, refreshes cached metadata, clears outdated cache data,
 * optionally starts transcoding immediately, or otherwise probes enough source
 * information to provide predicted size and segment metadata.
 *
 * @param[in,out] virtualfile Virtual file represented by the cache entry.
 * @param[in] begin_transcode Start the transcoder worker immediately when @c true.
 * @return Pointer to the opened cache entry, or @c nullptr on error with @c errno set.
 */
Cache_Entry*    transcoder_new(LPVIRTUALFILE virtualfile, bool begin_transcode);
/**
 * @brief Read bytes from the cache buffer, starting transcoding if needed.
 *
 * Handles normal file reads and HLS segment reads. Missing HLS segments,
 * stale cache files, and incomplete partial HLS caches are repaired on demand.
 * The function waits until the requested byte range is available, then copies
 * as much data as possible into the caller-provided buffer.
 *
 * @param[in,out] cache_entry Cache entry to read from.
 * @param[out] buff Destination buffer. Must be large enough for @p len bytes.
 * @param[in] offset Byte offset within the requested cache item.
 * @param[in] len Maximum number of bytes to read.
 * @param[out] bytes_read Receives the number of bytes copied to @p buff.
 * @param[in] segment_no HLS segment number, or 0 for a normal single-output file.
 * @return Returns @c true on success; otherwise @c false with @c errno set.
 */
bool            transcoder_read(Cache_Entry* cache_entry, char* buff, size_t offset, size_t len, int *bytes_read, uint32_t segment_no);
/**
 * @brief Read one frame from a frame-set cache.
 *
 * Reads the requested frame directly from the frame-set cache when available.
 * If the frame is missing, the requested frame number is stacked and the
 * transcoder is started or restarted to generate it. The virtual file stat
 * size is updated to the materialised frame size.
 *
 * @param[in,out] cache_entry Cache entry containing the frame set.
 * @param[out] buff Destination buffer. Must be large enough for @p len bytes.
 * @param[in] offset Byte offset within the requested frame.
 * @param[in] len Maximum number of bytes to read.
 * @param[in] frame_no Frame number to read.
 * @param[out] bytes_read Receives the number of bytes copied to @p buff.
 * @param[in,out] virtualfile Virtual frame file whose stat size is updated.
 * @return Returns @c true on success; otherwise @c false with @c errno set.
 */
bool            transcoder_read_frame(Cache_Entry* cache_entry, char* buff, size_t offset, size_t len, uint32_t frame_no, int * bytes_read, LPVIRTUALFILE virtualfile);
/**
 * @brief Close a cache entry previously returned by transcoder_new().
 *
 * The structure is reference counted. After calling this function, the pointer
 * may no longer be valid if no other thread keeps the cache entry open.
 *
 * @param[in,out] cache_entry Cache entry to close.
 */
void            transcoder_delete(Cache_Entry* cache_entry);
/**
 * @brief Return the logical size of a transcoded cache entry.
 *
 * Returns the file size, either the predicted size, which may be inaccurate,
 * or the real size, which is only available once the file was completely recoded.
 *
 * @param[in] cache_entry Cache entry to query.
 * @return Logical cached or predicted transcoded size in bytes. Function never fails.
 */
size_t          transcoder_get_size(Cache_Entry* cache_entry);
/**
 * @brief Return the number of bytes materialised for a cache item.
 *
 * While transcoding, this value reflects the current size of the transcoded file
 * or HLS segment. This is the maximum byte offset that can be read so far.
 *
 * @param[in] cache_entry Cache entry to query.
 * @param[in] segment_no HLS segment number, or 0 for the normal cache file.
 * @return Current cache watermark in bytes.
 */
size_t          transcoder_buffer_watermark(Cache_Entry* cache_entry, uint32_t segment_no);
/**
 * @brief Return the current write position for a cache item.
 * @param[in] cache_entry Cache entry to query.
 * @param[in] segment_no HLS segment number, or 0 for the normal cache file.
 * @return Current cache write position in bytes.
 */
size_t          transcoder_buffer_tell(Cache_Entry* cache_entry, uint32_t segment_no);
/**
 * @brief Exit transcoding
 *
 * Send signal to exit transcoding (ending all transcoder threads).
 */
void            transcoder_exit();

#endif
