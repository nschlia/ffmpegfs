/*
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2013 K. Henriksson
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
 * @file transcode.cc
 * @brief File transcoder interface implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2026 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "transcode.h"
#include "ffmpegfs.h"
#include "ffmpeg_transcoder.h"
#include "buffer.h"
#include "cache.h"
#include "logging.h"
#include "cache_entry.h"
#include "thread_pool.h"

#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstdio>

const int GRANULARITY = 250;                                /**< @brief Image frame conversion: ms between checks if a picture frame is available */
const int FRAME_TIMEOUT = 60;                               /**< @brief Image frame conversion: timout seconds to wait if a picture frame is available */
const int TOTAL_RETRIES = FRAME_TIMEOUT*1000/GRANULARITY;   /**< @brief Number of retries */
/**
  * @brief THREAD_DATA struct to pass data from parent to child thread
  */
typedef struct THREAD_DATA
{
    std::mutex              m_thread_running_mutex;         /**< @brief Mutex when thread is running */
    std::condition_variable m_thread_running_cond;          /**< @brief Condition when thread is running */
    std::atomic_bool        m_thread_running_lock_guard;    /**< @brief Lock guard to avoid spurious or missed unlocks */
    bool                    m_initialised;                  /**< @brief True when this object is completely initialised */
    Cache_Entry *           m_cache_entry;                  /**< @brief Cache entry object. Will not be freed by child thread. */
} THREAD_DATA;

static std::unique_ptr<Cache>   cache;                      /**< @brief Global cache manager object */
static std::atomic_bool         thread_exit;                /**< @brief Used for shutdown: if true, forcibly exit all threads */

static bool transcode(std::shared_ptr<THREAD_DATA> thread_data, Cache_Entry *cache_entry, FFmpeg_Transcoder & transcoder, bool *timeout);
static int  transcoder_thread(std::shared_ptr<THREAD_DATA> thread_data);
static int  start_transcoder_thread(Cache_Entry* cache_entry);
static bool transcode_until(Cache_Entry* cache_entry, size_t offset, size_t len, uint32_t segment_no);
static int  transcode_finish(Cache_Entry* cache_entry, FFmpeg_Transcoder & transcoder);
static void reopen_finished_incomplete_cache(Cache_Entry* cache_entry, uint32_t item_no, const char* item_name);
static bool cached_item_available(Cache_Entry* cache_entry, size_t offset, size_t len, uint32_t segment_no);
static bool invalidate_stale_cache_file(Cache_Entry* cache_entry, uint32_t segment_no, uint32_t item_no, const char* item_name);
static void wait_for_active_transcoder(Cache_Entry* cache_entry, uint32_t item_no, const char* item_name);

/**
 * @brief Transcode the buffer until the buffer has enough or until an error occurs.
 * The buffer needs at least 'end' bytes before transcoding stops. Returns true
 * if no errors and false otherwise.
 *  @param[in] cache_entry - corresponding cache entry
 *  @param[in] offset - byte offset to start reading at
 *  @param[in] len - length of data chunk to be read.
 *  @param[in] segment_no - HLS segment file number.
 * @return On success, returns true. Returns false if an error occurred.
 */
static bool transcode_until(Cache_Entry* cache_entry, size_t offset, size_t len, uint32_t segment_no)
{
    bool success = false;

    if (cached_item_available(cache_entry, offset, len, segment_no))
    {
        return true;
    }

    try
    {
        // Wait until decoder thread has reached the desired position
        if (cache_entry->m_is_decoding)
        {
            bool reported = false;
            while (!cached_item_available(cache_entry, offset, len, segment_no) && !cache_entry->m_cache_info.m_error)
            {
                if (fuse_interrupted())
                {
                    Logging::info(cache_entry->virtname(), "The client has gone away.");
                    errno = 0; // No error
                    break;
                }

                if (thread_exit)
                {
                    Logging::warning(cache_entry->virtname(), "Thread exit was received.");
                    errno = EINTR;
                    throw false;
                }

                if (!reported)
                {
                    if (!segment_no)
                    {
                        Logging::trace(cache_entry->virtname(), "Cache miss at offset %1 with length %2.", offset, len);
                    }
                    else
                    {
                        Logging::trace(cache_entry->virtname(), "Cache miss at offset %1 with length %2 for segment no. %3.", offset, len, segment_no);
                    }
                    reported = true;
                }

                if (segment_no && !cache_entry->m_is_decoding)
                {
                    // The worker may have finished just after this read entered
                    // the wait loop.  If the requested HLS segment is still not
                    // physically available, do not wait forever: reopen the
                    // incomplete HLS cache and start an on-demand repair worker.
                    cache_entry->m_seek_to_no = segment_no;
                    reopen_finished_incomplete_cache(cache_entry, segment_no, "HLS segment");

                    int ret = start_transcoder_thread(cache_entry);
                    if (ret)
                    {
                        errno = ret;
                        throw false;
                    }
                }

                mssleep(250);
            }

            if (reported)
            {
                if (!segment_no)
                {
                    Logging::trace(cache_entry->virtname(), "Cache hit at offset %1 with length %2.", offset, len);
                }
                else
                {
                    Logging::trace(cache_entry->virtname(), "Cache hit at offset %1 with length %2 for segment no. %3.", offset, len, segment_no);
                }
            }
            success = !cache_entry->m_cache_info.m_error && cached_item_available(cache_entry, offset, len, segment_no);
        }
    }
    catch (bool _success)
    {
        success = _success;
    }

    return success;
}

/**
 * @brief Close the input file and free everything but the initial buffer.
 * @param[in] cache_entry - corresponding cache entry
 * @param[in] transcoder - Current FFmpeg_Transcoder object.
 * @return On success, returns 0; on error, a negative AVERROR value.
 */
static int transcode_finish(Cache_Entry* cache_entry, FFmpeg_Transcoder & transcoder)
{
    int res = transcoder.encode_finish();
    if (res < 0)
    {
        return res;
    }

    // Check encoded buffer size. Does not affect HLS segments.
    cache_entry->m_cache_info.m_duration            = transcoder.duration();
    cache_entry->m_cache_info.m_encoded_filesize    = cache_entry->m_buffer->buffer_watermark();
    cache_entry->m_cache_info.m_video_frame_count   = transcoder.video_frame_count();
    cache_entry->m_cache_info.m_segment_count       = transcoder.segment_count();
    cache_entry->m_cache_info.m_result              = !transcoder.have_seeked() ? RESULTCODE::FINISHED_SUCCESS : RESULTCODE::FINISHED_INCOMPLETE;
    cache_entry->m_is_decoding                      = false;
    cache_entry->m_cache_info.m_errno               = 0;
    cache_entry->m_cache_info.m_averror             = 0;

    Logging::debug(transcoder.virtname(), "Finishing file.");

    if (!cache_entry->m_buffer->reserve(cache_entry->m_cache_info.m_encoded_filesize))
    {
        Logging::debug(transcoder.virtname(), "Unable to truncate the buffer.");
    }

    if (!transcoder.is_multiformat())
    {
        Logging::debug(transcoder.virtname(), "Predicted size: %1 Final: %2 Diff: %3 (%4%).",
                       format_size_ex(cache_entry->m_cache_info.m_predicted_filesize).c_str(),
                       format_size_ex(cache_entry->m_cache_info.m_encoded_filesize).c_str(),
                       format_result_size_ex(cache_entry->m_cache_info.m_encoded_filesize, cache_entry->m_cache_info.m_predicted_filesize).c_str(),
                       ((static_cast<double>(cache_entry->m_cache_info.m_encoded_filesize) * 1000 / (static_cast<double>(cache_entry->m_cache_info.m_predicted_filesize) + 1)) + 5) / 10);
    }

    cache_entry->flush();

    return 0;
}

/**
 * @brief Re-open an incomplete multi-format cache for an on-demand repair run.
 *
 * HLS and frame-set caches may intentionally be marked FINISHED_INCOMPLETE
 * after a seek/direct access reached EOF.  That state means that existing
 * segments/frames are usable, but missing items may still be generated later.
 * Before starting such a repair worker, clear only the finished/error state;
 * do not clear the buffer, because the already valid segments/frames must be
 * kept.
 *
 * @param[inout] cache_entry Cache entry to re-open.
 * @param[in] item_no Requested HLS segment or frame number.
 * @param[in] item_name Human-readable item name for logging.
 */
static void reopen_finished_incomplete_cache(Cache_Entry* cache_entry, uint32_t item_no, const char* item_name)
{
    if (cache_entry == nullptr || !cache_entry->is_finished_incomplete())
    {
        return;
    }

    Logging::warning(cache_entry->virtname(),
                     "Re-opening incomplete cache to generate missing %1 no. %2.",
                     item_name,
                     item_no);

    cache_entry->m_cache_info.m_result  = RESULTCODE::NONE;
    cache_entry->m_cache_info.m_error   = false;
    cache_entry->m_cache_info.m_errno   = 0;
    cache_entry->m_cache_info.m_averror = 0;
}

/**
 * @brief Invalidate a stale physical cache file and make the entry eligible for recoding.
 *
 * The cache database/in-memory state may still say that a file or segment is
 * available even though the physical cache file has been deleted or truncated.
 * In that case the on-disk state wins: the affected cache item is invalidated
 * and the cache entry is re-opened for transcoding/repair.
 *
 * @param[inout] cache_entry Cache entry containing the stale cache file.
 * @param[in] segment_no HLS segment number, or 0 for the single cache file used by normal files and frame sets.
 * @param[in] item_no Human-readable item number used in the log message.
 * @param[in] item_name Human-readable item name used in the log message.
 * @return Always returns false, so the caller can directly use it as a cache-miss result.
 */
static bool invalidate_stale_cache_file(Cache_Entry* cache_entry, uint32_t segment_no, uint32_t item_no, const char* item_name)
{
    if (cache_entry == nullptr || cache_entry->m_buffer == nullptr)
    {
        errno = EINVAL;
        return false;
    }

    const std::string cachefile = cache_entry->m_buffer->cachefile(segment_no);

    if (item_no)
    {
        Logging::warning(cache_entry->virtname(),
                         "Cached %1 no. %2 is marked available, but cache file '%3' is missing or empty. Recreating it.",
                         item_name,
                         item_no,
                         cachefile.c_str());
    }
    else
    {
        Logging::warning(cache_entry->virtname(),
                         "Cached file is marked available, but cache file '%1' is missing or empty. Recreating it.",
                         cachefile.c_str());
    }

    cache_entry->m_buffer->invalidate_segment(segment_no);

    cache_entry->m_cache_info.m_result        = RESULTCODE::NONE;
    cache_entry->m_cache_info.m_error         = false;
    cache_entry->m_cache_info.m_errno         = 0;
    cache_entry->m_cache_info.m_averror       = 0;
    cache_entry->m_suspend_timeout            = false;

    if (!segment_no)
    {
        cache_entry->m_cache_info.m_encoded_filesize = 0;
    }

    const FFmpegfs_Format *current_format = params.current_format(cache_entry->virtualfile());
    if (item_no && current_format != nullptr && current_format->is_multiformat())
    {
        cache_entry->m_seek_to_no = item_no;
    }

    return false;
}

/**
 * @brief Wait until an already active transcoder worker has left the cache entry.
 *
 * Stale physical HLS files are sometimes detected in a very small race window:
 * the worker has just finished writing a seeked/incomplete HLS run, but still
 * owns the cache entry while the client immediately requests an older segment.
 * Starting a repair worker in that window is intentionally rejected by
 * start_transcoder_thread(), because a real worker is still active.  If we then
 * continue waiting on the missing segment, no later worker is started and the
 * request can stall.
 *
 * For stale-cache repair we therefore wait until the active worker has really
 * left the cache entry, and only then invalidate and start the repair run.
 * Normal sequential reads do not use this path.
 *
 * @param[in] cache_entry Cache entry whose worker should become idle.
 * @param[in] item_no Human-readable item number used in the log message.
 * @param[in] item_name Human-readable item name used in the log message.
 */
static void wait_for_active_transcoder(Cache_Entry* cache_entry, uint32_t item_no, const char* item_name)
{
    if (cache_entry == nullptr || !cache_entry->m_is_decoding)
    {
        return;
    }

    if (item_no)
    {
        Logging::debug(cache_entry->virtname(),
                       "Waiting for active transcoder to finish before repairing stale %1 no. %2.",
                       item_name,
                       item_no);
    }
    else
    {
        Logging::debug(cache_entry->virtname(),
                       "Waiting for active transcoder to finish before repairing stale %1.",
                       item_name);
    }

    std::unique_lock<std::recursive_mutex> lock_active_mutex(cache_entry->m_active_mutex);
}

/**
 * @brief Check whether the requested cache data is available and physically readable.
 *
 * This combines the logical cache state (finished cache, finished HLS segment,
 * or enough bytes already written) with a physical cache-file check.  A stale
 * database entry must not result in an empty file being served to the client.
 *
 * @param[inout] cache_entry Cache entry to check.
 * @param[in] offset Requested byte offset.
 * @param[in] len Requested byte count.
 * @param[in] segment_no HLS segment number, or 0 for normal files/frame sets.
 * @return Returns true if the requested data can be read from the cache.
 */
static bool cached_item_available(Cache_Entry* cache_entry, size_t offset, size_t len, uint32_t segment_no)
{
    if (cache_entry == nullptr || cache_entry->m_buffer == nullptr)
    {
        errno = EINVAL;
        return false;
    }

    const size_t end = offset + len;

    // For HLS each segment is materialised independently.  A
    // FINISHED_INCOMPLETE cache only means that a seeked/direct-access run
    // reached EOF; it must not imply that earlier skipped segments exist.
    // Only a fully successful, non-seeked HLS transcode may make all segment
    // files logically available through the cache entry result.  Otherwise the
    // per-segment finished flag or the current write position is authoritative.
    const bool logically_available = segment_no
            ? (cache_entry->is_finished_success() ||
               cache_entry->m_buffer->is_segment_finished(segment_no) ||
               cache_entry->m_buffer->tell(segment_no) >= end)
            : (cache_entry->is_finished() ||
               cache_entry->m_buffer->tell(segment_no) >= end);

    if (!logically_available)
    {
        return false;
    }

    if (cache_entry->m_buffer->cachefile_valid(segment_no))
    {
        return true;
    }

    return invalidate_stale_cache_file(cache_entry,
                                       segment_no,
                                       segment_no,
                                       segment_no ? "segment" : "file");
}

/**
 * @brief Build the base directory used for the transcoder cache.
 *
 * Uses the configured cache path when one was supplied.  Otherwise root uses
 * `/var/cache`, while regular users use `$XDG_CACHE_HOME` or `~/.cache`.
 * The package name is appended and the returned path always ends with a path
 * separator.
 *
 * @param[out] path Receives the complete transcoder cache directory path.
 */
void transcoder_cache_path(std::string * path)
{
    if (params.m_cachepath.size())
    {
        *path = params.m_cachepath;
    }
    else
    {
        if (geteuid() == 0)
        {
            // Running as root
            *path = "/var/cache";
        }
        else
        {
            // Running as regular user, put cache in home dir
            if (const char* cache_home = std::getenv("XDG_CACHE_HOME"))
            {
                *path = cache_home;
            }
            else
            {
                expand_path(path, "~/.cache");
            }
        }
    }

    append_sep(path);

    *path += PACKAGE;

    append_sep(path);
}

/**
 * @brief Initialise the global transcoder cache.
 *
 * Creates the cache manager on first use and loads the persistent cache index.
 * Calling this function more than once is harmless.
 *
 * @return Returns @c true when the cache is available; otherwise @c false.
 */
bool transcoder_init()
{
    if (cache == nullptr)
    {
        Logging::debug(nullptr, "Creating new media file cache.");
        cache = std::make_unique<Cache>();
        if (cache == nullptr)
        {
            Logging::error(nullptr, "Unable to create new media file cache. Out of memory.");
            std::fprintf(stderr, "ERROR: Creating new media file cache. Out of memory.\n");
            return false;
        }

        if (!cache->load_index())
        {
            std::fprintf(stderr, "ERROR: Creating media file cache failed.\n");
            return false;
        }
    }
    return true;
}

/**
 * @brief Release the global transcoder cache manager.
 *
 * Flushes and destroys the cache object if it has been created.  Calling this
 * function with no active cache is harmless.
 */
void transcoder_free()
{
    if (cache != nullptr)
    {
        cache.reset();

        Logging::debug(nullptr, "Deleting media file cache.");
    }
}

/**
 * @brief Fill a stat buffer with the cached or predicted transcoded file size.
 *
 * Uses the encoded size when the file has already been transcoded.  If no
 * encoded size is known yet, the predicted transcoded size is used instead.
 *
 * @param[in] virtualfile Virtual file whose cached size should be queried.
 * @param[inout] stbuf Stat buffer whose size field is updated on success.
 * @return Returns @c true when a cached or predicted size was available;
 *         otherwise @c false.
 */
bool transcoder_cached_filesize(LPVIRTUALFILE virtualfile, struct stat *stbuf)
{
    Cache_Entry* cache_entry = cache->openio(virtualfile);
    if (cache_entry == nullptr)
    {
        return false;
    }

    size_t encoded_filesize = cache_entry->m_cache_info.m_encoded_filesize;

    if (!encoded_filesize)
    {
        // If not yet encoded, return predicted file size
        encoded_filesize = cache_entry->m_cache_info.m_predicted_filesize;
    }

    if (encoded_filesize)
    {
        stat_set_size(stbuf, encoded_filesize);
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * @brief Calculate and store the predicted transcoded file size.
 *
 * Estimates the output size from stream duration, selected output format,
 * audio parameters, video parameters, and container overhead.  The result is
 * stored both in the cache entry and in the virtual file.
 *
 * @param[inout] virtualfile Virtual file whose predicted size should be set.
 * @param[in] duration Media duration in FFmpeg time base units.
 * @param[in] audio_bit_rate Target audio bit rate.
 * @param[in] channels Number of audio channels.
 * @param[in] sample_rate Audio sample rate in Hz.
 * @param[in] sample_format Audio sample format.
 * @param[in] video_bit_rate Target video bit rate.
 * @param[in] width Video width in pixels.
 * @param[in] height Video height in pixels.
 * @param[in] interleaved Set to @c true for interleaved video output.
 * @param[in] framerate Video frame rate.
 * @return Returns @c true after updating the prediction; otherwise @c false.
 */
bool transcoder_set_filesize(LPVIRTUALFILE virtualfile, int64_t duration, BITRATE audio_bit_rate, int channels, int sample_rate, AVSampleFormat sample_format, BITRATE video_bit_rate, int width, int height, bool interleaved, const AVRational &framerate)
{
    Cache_Entry* cache_entry = cache->openio(virtualfile);
    if (cache_entry == nullptr)
    {
        Logging::error(nullptr, "Out of memory getting file size.");
        return false;
    }

    const FFmpegfs_Format *current_format = params.current_format(virtualfile);
    if (current_format == nullptr)
    {
        Logging::error(cache_entry->virtname(), "Internal error getting file size.");
        return false;
    }

    size_t filesize = 0;

    if (!FFmpeg_Transcoder::audio_size(&filesize, current_format->audio_codec(), audio_bit_rate, duration, channels, sample_rate, sample_format))
    {
        Logging::warning(cache_entry->virtname(), "Unsupported audio codec '%1' for format %2.", get_codec_name(current_format->audio_codec()), current_format->desttype().c_str());
    }

    if (!FFmpeg_Transcoder::video_size(&filesize, current_format->video_codec(), video_bit_rate, duration, width, height, interleaved, framerate))
    {
        Logging::warning(cache_entry->virtname(), "Unsupported video codec '%1' for format %2.", get_codec_name(current_format->video_codec()), current_format->desttype().c_str());
    }

    if (!FFmpeg_Transcoder::total_overhead(&filesize, current_format->filetype()))
    {
        Logging::warning(cache_entry->virtname(), "Unsupported file type '%1' for format %2.", get_filetype_text(current_format->filetype()).c_str(), current_format->desttype().c_str());
    }

    cache_entry->m_cache_info.m_predicted_filesize = virtualfile->m_predicted_size = filesize;

    Logging::trace(cache_entry->virtname(), "Predicted transcoded size of %1.", format_size_ex(cache_entry->m_cache_info.m_predicted_filesize).c_str());

    return true;
}

/**
 * @brief Probe an input file and update cached media properties.
 *
 * Opens the source with FFmpeg, reads the predicted transcoded size, duration,
 * video frame count, and HLS/frame-set segment count, then stores those values
 * in the cache entry when one was supplied.
 *
 * @param[inout] virtualfile Virtual file to inspect.
 * @param[inout] cache_entry Optional cache entry to update with probed values.
 * @return Returns @c true when probing succeeded; otherwise @c false.
 */
bool transcoder_predict_filesize(LPVIRTUALFILE virtualfile, Cache_Entry* cache_entry)
{
    FFmpeg_Transcoder transcoder;
    bool success = false;

    if (transcoder.open_input_file(virtualfile) >= 0)
    {
        if (cache_entry != nullptr)
        {
            cache_entry->m_cache_info.m_predicted_filesize  = transcoder.predicted_filesize();
            cache_entry->m_cache_info.m_video_frame_count   = transcoder.video_frame_count();
            cache_entry->m_cache_info.m_segment_count       = transcoder.segment_count();
            cache_entry->m_cache_info.m_duration            = transcoder.duration();
        }

        Logging::trace(transcoder.filename(), "Predicted transcoded size of %1.", format_size_ex(transcoder.predicted_filesize()).c_str());

        transcoder.closeio();

        success = true;
    }

    return success;
}

/**
 * @brief Start the transcoder thread for an existing cache entry.
 *
 * For HLS/frame-set partial cache repair, the caller may already have placed
 * the requested segment/frame number in m_seek_to_no. In that case transcode()
 * keeps existing segment files and starts directly at the requested item.
 *
 * @param[inout] cache_entry Cache entry to transcode.
 * @return 0 on success, otherwise errno-compatible error code.
 */
static int start_transcoder_thread(Cache_Entry* cache_entry)
{
    if (cache_entry == nullptr)
    {
        return EINVAL;
    }

    bool expected_is_decoding = false;
    if (!cache_entry->m_is_decoding.compare_exchange_strong(expected_is_decoding, true))
    {
        std::unique_lock<std::recursive_mutex> lock_active_mutex(cache_entry->m_active_mutex, std::try_to_lock);
        if (!lock_active_mutex.owns_lock())
        {
            Logging::warning(cache_entry->virtname(), "Transcoder thread already running.");
            return 0;
        }

        Logging::warning(cache_entry->virtname(),
                         "Transcoder thread was marked as running, but no active worker owns the cache entry. Resetting decoding state and starting a new worker.");

        cache_entry->m_is_decoding = false;
        expected_is_decoding = false;
        if (!cache_entry->m_is_decoding.compare_exchange_strong(expected_is_decoding, true))
        {
            Logging::warning(cache_entry->virtname(), "Transcoder thread already running.");
            return 0;
        }
    }

    const FFmpegfs_Format *current_format = params.current_format(cache_entry->virtualfile());
    if (current_format != nullptr &&
            current_format->is_multiformat() &&
            !cache_entry->virtualfile()->get_segment_count() &&
            !cache_entry->m_cache_info.m_segment_count)
    {
        // HLS/frame-set buffers need a known segment/frame count before
        // Buffer::init() can create the cache file table.  With deferred
        // transcoding this may not have happened during open(), so predict
        // the source properties now, before the worker calls openio(true).
        if (!transcoder_predict_filesize(cache_entry->virtualfile(), cache_entry))
        {
            int ret = errno;
            if (!ret)
            {
                ret = EIO;
            }

            cache_entry->m_is_decoding = false;
            return ret;
        }
    }

    Logging::debug(cache_entry->filename(), "Starting transcoder thread.");

    std::shared_ptr<THREAD_DATA> thread_data = std::make_shared<THREAD_DATA>();

    thread_data->m_initialised                  = false;
    thread_data->m_cache_entry                  = cache_entry;
    thread_data->m_thread_running_lock_guard    = false;

    {
        std::unique_lock<std::mutex> lock_thread_running_mutex(thread_data->m_thread_running_mutex);

        tp->schedule_thread(std::bind(&transcoder_thread, thread_data));

        // Let decoder get into gear before returning from open/read.
        while (!thread_data->m_thread_running_lock_guard)
        {
            thread_data->m_thread_running_cond.wait(lock_thread_running_mutex);
        }
    }

    if (cache_entry->m_cache_info.m_error)
    {
        int ret = cache_entry->m_cache_info.m_errno;
        if (!ret)
        {
            ret = EIO;
        }
        return ret;
    }

    Logging::debug(cache_entry->filename(), "Transcoder thread is running.");
    return 0;
}

/**
 * @brief Open or create a cache entry for a virtual file.
 *
 * Opens the cache entry, refreshes cached metadata, clears outdated cache data,
 * optionally starts transcoding immediately, or otherwise probes enough source
 * information to provide predicted size and segment metadata.
 *
 * @param[inout] virtualfile Virtual file represented by the cache entry.
 * @param[in] begin_transcode Start the transcoder worker immediately when @c true.
 * @return Pointer to the opened cache entry, or @c nullptr on error with @c errno set.
 */
Cache_Entry* transcoder_new(LPVIRTUALFILE virtualfile, bool begin_transcode)
{
    // Allocate transcoder structure
    Cache_Entry* cache_entry = cache->openio(virtualfile);
    if (cache_entry == nullptr)
    {
        return nullptr;
    }

    Logging::trace(cache_entry->filename(), "Creating transcoder object.");

    try
    {
        cache_entry->lock();

        const FFmpegfs_Format *current_format = params.current_format(virtualfile);
        const bool create_cache = begin_transcode ||
                (current_format != nullptr &&
                 current_format->is_multiformat() &&
                 virtualfile->get_segment_count() != 0);

        if (!cache_entry->openio(create_cache))
        {
            throw static_cast<int>(errno);
        }

        if (params.m_disable_cache)
        {
            // Disable cache
            cache_entry->clear();
        }
        else if (!cache_entry->m_is_decoding && cache_entry->outdated())
        {
            cache_entry->clear();
        }

        if (cache_entry->m_cache_info.m_duration)
        {
            virtualfile->m_duration             = cache_entry->m_cache_info.m_duration;
        }
        if (cache_entry->m_cache_info.m_predicted_filesize)
        {
            virtualfile->m_predicted_size       = cache_entry->m_cache_info.m_predicted_filesize;
        }
        if (cache_entry->m_cache_info.m_video_frame_count)
        {
            virtualfile->m_video_frame_count    = cache_entry->m_cache_info.m_video_frame_count;
        }

        if (!cache_entry->m_is_decoding && !cache_entry->is_finished_success())
        {
            if (begin_transcode)
            {
                int ret = start_transcoder_thread(cache_entry);
                if (ret)
                {
                    Logging::trace(cache_entry->filename(), "Transcoder error!");
                    throw ret;
                }
            }
            else if (!cache_entry->m_cache_info.m_predicted_filesize)
            {
                if (!transcoder_predict_filesize(virtualfile, cache_entry))
                {
                    throw static_cast<int>(errno);
                }
            }
        }
        else if (begin_transcode)
        {
            Logging::info(cache_entry->virtname(), "Reading file from cache.");
        }

        cache_entry->unlock();
    }
    catch (int orgerrno)
    {
        cache_entry->m_is_decoding = false;
        cache_entry->unlock();
        cache->closeio(&cache_entry, CACHE_CLOSE_DELETE);
        cache_entry = nullptr;      // Make sure to return NULL here even if the cache could not be deleted now (still in use)
        errno = orgerrno;           // Restore last errno
    }

    return cache_entry;
}

/**
 * @brief Read transcoded data from the cache, starting or repairing transcoding if needed.
 *
 * Handles normal file reads and HLS segment reads.  Missing HLS segments,
 * stale cache files, and incomplete partial HLS caches are repaired on demand.
 * The function waits until the requested byte range is available, then copies
 * as much data as possible into the caller-provided buffer.
 *
 * @param[inout] cache_entry Cache entry to read from.
 * @param[out] buff Destination buffer.
 * @param[in] offset Byte offset within the requested cache item.
 * @param[in] len Maximum number of bytes to read.
 * @param[out] bytes_read Receives the number of bytes copied to @p buff.
 * @param[in] segment_no HLS segment number, or 0 for a normal single-output file.
 * @return Returns @c true on success; otherwise @c false with @c errno set.
 */
bool transcoder_read(Cache_Entry* cache_entry, char* buff, size_t offset, size_t len, int * bytes_read, uint32_t segment_no)
{
    bool success = true;

    if (!segment_no)
    {
        Logging::trace(cache_entry->virtname(), "Reading %1 bytes from offset %2 to %3.", len, offset, len + offset);
    }
    else
    {
        Logging::trace(cache_entry->virtname(), "Reading %1 bytes from offset %2 to %3 for segment no. %4.", len, offset, len + offset, segment_no);
    }

    // Store access time
    cache_entry->update_access();

    // Update read counter
    cache_entry->update_read_count();

    try
    {
        // For HLS partial/seeked caches, FINISHED_INCOMPLETE must not mark
        // skipped segments as complete.  Only FINISHED_SUCCESS covers all
        // segments; otherwise the per-segment finished flag decides.
        const bool segment_logically_complete = segment_no &&
                (cache_entry->m_buffer->is_segment_finished(segment_no) || cache_entry->is_finished_success());
        bool segment_complete = segment_logically_complete;
        bool repair_requested = false;

        if (segment_logically_complete && !cache_entry->m_buffer->cachefile_valid(segment_no))
        {
            wait_for_active_transcoder(cache_entry, segment_no, "segment");

            if (!cache_entry->m_buffer->cachefile_valid(segment_no))
            {
                invalidate_stale_cache_file(cache_entry, segment_no, segment_no, "segment");
                segment_complete = false;
                repair_requested = true;
            }
        }

        if (!segment_no && cache_entry->is_finished() && !cache_entry->m_buffer->cachefile_valid(0))
        {
            wait_for_active_transcoder(cache_entry, 0, "file");

            if (!cache_entry->m_buffer->cachefile_valid(0))
            {
                invalidate_stale_cache_file(cache_entry, 0, 0, "file");
                repair_requested = true;
            }
        }

        const bool segment_missing  = segment_no && !segment_complete;

        // For HLS partial caches, an incomplete cache is still usable.  Only
        // start/restart the transcoder when the requested segment is not
        // finished yet.  Physical cache files are checked as well: if a segment
        // is marked as finished in memory/the cache index but the actual file
        // has been deleted or truncated, it is treated as missing and repaired.
        if (segment_missing)
        {
            if (!cache_entry->m_is_decoding)
            {
                // Segment 1 naturally starts at the beginning.  For later HLS
                // segments, stack the target before the worker starts so a
                // direct request does not first encode segment 1 and only seek
                // afterwards.
                cache_entry->m_seek_to_no = segment_no;

                // A FINISHED_INCOMPLETE HLS cache may still contain useful
                // segments, but a missing requested segment needs a fresh
                // repair worker.  Only re-open it when no worker is active; an
                // active worker must keep its current cache state.
                reopen_finished_incomplete_cache(cache_entry, segment_no, "HLS segment");
            }
            else
            {
                // A running HLS transcoder can satisfy large jumps by
                // consuming m_seek_to_no in transcode().  However, during
                // normal linear playback the current or immediately following
                // segment is often requested before it has been finalised.  Do
                // not queue a seek for those normal sequential reads: doing so
                // can make the encoder reopen the same segment and corrupt the
                // cache file.
                //
                // Only queue a seek while the worker is active if the request
                // is farther away than the configured minimum seek distance.
                // The FFmpeg_Transcoder still performs its own final check and
                // logs discarded short seeks.
                const uint32_t current_segment = cache_entry->m_buffer->current_segment_no();
                if (current_segment && segment_no > current_segment)
                {
                    uint32_t min_seek_segments = 0;
                    if (params.m_segment_duration > 0)
                    {
                        min_seek_segments = static_cast<uint32_t>(params.m_min_seek_time_diff / params.m_segment_duration);
                    }

                    if (segment_no > current_segment + min_seek_segments)
                    {
                        cache_entry->m_seek_to_no = segment_no;
                    }
                }
            }
        }

        if (repair_requested ||
                (!cache_entry->m_is_decoding &&
                 (segment_missing ||
                  (!cache_entry->is_finished_success() &&
                   !(cache_entry->is_finished_incomplete() && !segment_missing)))))
        {
            int ret = start_transcoder_thread(cache_entry);
            if (ret)
            {
                errno = ret;
                throw false;
            }
        }

        if (!cache_entry->is_finished_success() && !cache_entry->is_finished_incomplete())
        {
            switch (params.current_format(cache_entry->virtualfile())->filetype())
            {
            case FILETYPE::MP3:
            {
                // If we are encoding to MP3 and the requested data overlaps the ID3v1 tag
                // at the end of the file, do not encode data first up to that position.
                // This optimises the case where applications read the end of the file
                // first to read the ID3v1 tag.
                if ((offset > cache_entry->m_buffer->tell(segment_no)) &&
                        (offset + len >= (cache_entry->size() - ID3V1_TAG_LENGTH)))
                {
                    // Stuff buffer with garbage, apps won't try to play that chunk anyway.
                    std::memset(buff, 0xFF, len);
                    if (cache_entry->size() - offset == ID3V1_TAG_LENGTH)
                    {
                        std::memcpy(buff, &cache_entry->m_id3v1, std::min(len, ID3V1_TAG_LENGTH));
                    }

                    errno = 0;

                    throw true; // OK
                }
                break;
            }
            default:
            {
                break;
            }
            }

            // Windows seems to access the files on Samba drives starting at the last 64K segment simply when
            // the file is opened. Setting win_smb_fix=1 will ignore these attempts (not decode the file up
            // to this point).
            // Access will only be ignored if occurring at the second access.
            if (params.m_win_smb_fix && cache_entry->read_count() == 2)
            {
                if ((offset > cache_entry->m_buffer->tell(segment_no)) &&
                        (len <= 65536) &&
                        check_ignore(cache_entry->size(), offset) &&
                        ((offset + len) > (cache_entry->size())))
                {
                    Logging::warning(cache_entry->virtname(), "Ignoring Windows' groundless access to the last 8K boundary of the file.");

                    errno = 0;
                    *bytes_read = 0;  // We've read nothing
                    len = 0;

                    throw true; // OK
                }
            }
        }

        success = transcode_until(cache_entry, offset, len, segment_no);

        if (!success)
        {
            errno = cache_entry->m_cache_info.m_errno ? cache_entry->m_cache_info.m_errno : EIO;
            throw false;
        }

        // Open for reading if necessary.  This intentionally happens after
        // transcode_until(), so a missing HLS segment is created by the
        // transcoder, not by a read-only cache probe.
        if (!cache_entry->m_buffer->open_file(segment_no, CACHE_FLAG_RO))
        {
            throw false;
        }

        // truncate if we didn't actually get len
        if (cache_entry->m_buffer->buffer_watermark(segment_no) < offset)
        {
            len = 0;
        }
        else if (cache_entry->m_buffer->buffer_watermark(segment_no) < offset + len)
        {
            len = cache_entry->m_buffer->buffer_watermark(segment_no) - offset;
        }

        if (len)
        {
            if (!cache_entry->m_buffer->copy(reinterpret_cast<uint8_t*>(buff), offset, len, segment_no))
            {
                len = 0;
                // We already capped len to not overread the buffer, so it is an error if we end here.
                throw false;
            }

            if (cache_entry->m_cache_info.m_error)
            {
                errno = cache_entry->m_cache_info.m_errno ? cache_entry->m_cache_info.m_errno : EIO;
                throw false;
            }
        }

        errno = 0;
    }
    catch (bool _success)
    {
        success = _success;
    }

    *bytes_read = static_cast<int>(len);

    return success;
}

/**
 * @brief Read one frame from a frame-set cache.
 *
 * Reads the requested frame directly from the frame-set cache when available.
 * If the frame is missing, the requested frame number is stacked and the
 * transcoder is started or restarted to generate it.  The virtual file stat
 * size is updated to the materialised frame size.
 *
 * @param[inout] cache_entry Cache entry containing the frame set.
 * @param[out] buff Destination buffer.
 * @param[in] offset Byte offset within the requested frame.
 * @param[in] len Maximum number of bytes to read.
 * @param[in] frame_no Frame number to read.
 * @param[out] bytes_read Receives the number of bytes copied to @p buff.
 * @param[inout] virtualfile Virtual frame file whose stat size is updated.
 * @return Returns @c true on success; otherwise @c false with @c errno set.
 */
bool transcoder_read_frame(Cache_Entry* cache_entry, char* buff, size_t offset, size_t len, uint32_t frame_no, int * bytes_read, LPVIRTUALFILE virtualfile)
{
    bool success = false;

    Logging::trace(cache_entry->virtname(), "Reading %1 bytes from offset %2 to %3 for frame no. %4.", len, offset, len + offset, frame_no);

    // Store access time
    cache_entry->update_access();

    // Update read counter
    cache_entry->update_read_count();

    try
    {
        if (cache_entry->is_finished() && !cache_entry->m_buffer->cachefile_valid(0))
        {
            invalidate_stale_cache_file(cache_entry, 0, frame_no, "frame-set cache");
            cache_entry->m_seek_to_no = frame_no;
        }

        // Open for reading if necessary.  For frame sets this only opens the
        // frame index/cache for probing; a missing frame still has to start the
        // transcoder below.
        if (!cache_entry->m_buffer->open_file(0, CACHE_FLAG_RO))
        {
            throw false;
        }

        std::vector<uint8_t> data;

        // Try to read the requested frame first.  Frame-set files are opened
        // through their parent object without starting the transcoder, because
        // the exact frame number is only known here.  Therefore a cache miss
        // must start or wake the transcoder from this read path.
        success = cache_entry->m_buffer->read_frame(&data, frame_no);
        if (!success)
        {
            if (errno != EAGAIN)
            {
                Logging::error(cache_entry->virtname(), "Reading image frame no. %1: (%2) %3", frame_no, errno, strerror(errno));
                throw false;
            }

            bool reported = false;

            // Stack the requested frame before starting the worker so a direct
            // frame-set access does not begin at frame 1 first.
            cache_entry->m_seek_to_no = frame_no;

            // A FINISHED_INCOMPLETE frame-set cache means that some frames are
            // valid, but the requested missing frame still has to be generated.
            // Clear the finished state before starting the repair worker,
            // otherwise transcode() exits immediately.
            reopen_finished_incomplete_cache(cache_entry, frame_no, "frame");

            if (!cache_entry->m_is_decoding)
            {
                int ret = start_transcoder_thread(cache_entry);
                if (ret)
                {
                    errno = ret;
                    throw false;
                }
            }

            int retries = TOTAL_RETRIES;
            while (!cache_entry->m_buffer->read_frame(&data, frame_no) && !thread_exit)
            {
                if (errno != EAGAIN)
                {
                    Logging::error(cache_entry->virtname(), "Reading image frame no. %1: (%2) %3", frame_no, errno, strerror(errno));
                    throw false;
                }

                // A previous frame-set worker may have reached EOF just after
                // this request stacked m_seek_to_no.  In that case the worker
                // exits, the requested frame is still missing, and simply
                // waiting would time out.  Start a fresh repair worker for the
                // same frame as soon as the old worker is no longer decoding.
                if (!cache_entry->m_is_decoding)
                {
                    cache_entry->m_seek_to_no = frame_no;
            		reopen_finished_incomplete_cache(cache_entry, frame_no, "frame");

                    int ret = start_transcoder_thread(cache_entry);
                    if (ret)
                    {
                        errno = ret;
                        throw false;
                    }

                    // The new worker is now responsible for producing the
                    // requested frame, so give it a full retry budget.
                    retries = TOTAL_RETRIES;
                }

                if (!cache_entry->m_suspend_timeout)
                {
                    if (!--retries)
                    {
                        errno = ETIMEDOUT;
                        Logging::error(cache_entry->virtname(), "Timeout reading image frame no. %1: (%2) %3", frame_no, errno, strerror(errno));
                        throw false;
                    }
                }
                else
                {
                    retries = TOTAL_RETRIES;
                }

                if (thread_exit)
                {
                    Logging::warning(cache_entry->virtname(), "Thread exit was received.");
                    errno = EINTR;
                    throw false;
                }

                if (!reported)
                {
                    Logging::trace(cache_entry->virtname(), "Frame no. %1: Cache miss at offset %<11zu>2 (length %<6u>3).", frame_no, offset, len);
                    reported = true;
                }

                mssleep(GRANULARITY);
            }

            if (thread_exit)
            {
                Logging::warning(cache_entry->virtname(), "Thread exit was received.");
                errno = EINTR;
                throw false;
            }

            Logging::trace(cache_entry->virtname(), "Frame no. %1: Cache hit  at offset %<11zu>2 (length %<6u>3).", frame_no, offset, len);

            success = !cache_entry->m_cache_info.m_error;
        }

        if (success)
        {
            if (data.size() < offset)
            {
                len = 0;
            }
            else if (data.size() < offset + len)
            {
                len = data.size() - offset;
            }

            if (len)
            {
                std::memcpy(buff, data.data() + offset, len);
            }

            stat_set_size(&virtualfile->m_st, data.size());

            *bytes_read = static_cast<int>(len);
        }
    }
    catch (bool _success)
    {
        success = _success;
    }

    return success;
}

/**
 * @brief Close a cache entry previously returned by transcoder_new().
 *
 * @param[inout] cache_entry Cache entry to close.
 */
void transcoder_delete(Cache_Entry* cache_entry)
{
    cache->closeio(&cache_entry);
}

/**
 * @brief Return the logical size of a transcoded cache entry.
 *
 * @param[in] cache_entry Cache entry to query.
 * @return Logical cached or predicted transcoded size in bytes.
 */
size_t transcoder_get_size(Cache_Entry* cache_entry)
{
    return cache_entry->size();
}

/**
 * @brief Return the number of bytes materialised for a cache item.
 *
 * @param[in] cache_entry Cache entry to query.
 * @param[in] segment_no HLS segment number, or 0 for the normal cache file.
 * @return Current cache watermark in bytes.
 */
size_t transcoder_buffer_watermark(Cache_Entry* cache_entry, uint32_t segment_no)
{
    return cache_entry->m_buffer->buffer_watermark(segment_no);
}

/**
 * @brief Return the current write position for a cache item.
 *
 * @param[in] cache_entry Cache entry to query.
 * @param[in] segment_no HLS segment number, or 0 for the normal cache file.
 * @return Current cache write position in bytes.
 */
size_t transcoder_buffer_tell(Cache_Entry* cache_entry, uint32_t segment_no)
{
    return cache_entry->m_buffer->tell(segment_no);
}

/**
 * @brief Request all transcoder workers to terminate.
 *
 * Sets the global shutdown flag checked by worker and wait loops.
 */
void transcoder_exit()
{
    thread_exit = true;
}

/**
 * @brief Run cache maintenance immediately.
 *
 * @return Returns @c true when maintenance completed successfully or no cache
 *         work was needed; otherwise @c false.
 */
bool transcoder_cache_maintenance()
{
    if (cache != nullptr)
    {
        return cache->maintenance();
    }
    else
    {
        return false;
    }
}

/**
 * @brief Clear all entries from the transcoder cache.
 *
 * @return Returns @c true when the cache was cleared; otherwise @c false.
 */
bool transcoder_cache_clear()
{
    if (cache != nullptr)
    {
        return cache->clear();
    }
    else
    {
        return false;
    }
}

/**
 * @brief Actually transcode file
 * @param[inout] thread_data - Thread data with lock objects
 * @param[inout] cache_entry - Underlying thread entry
 * @param[in] transcoder - Transcoder object for transcoding
 * @param[out] timeout - True if transcoding timed out, false if not
 * @return On success, returns true; on error, returns false
 */
static bool transcode(std::shared_ptr<THREAD_DATA> thread_data, Cache_Entry *cache_entry, FFmpeg_Transcoder & transcoder, bool *timeout)
{
    int averror = 0;
    int syserror = 0;
    bool success = true;

    const bool partial_multiformat_recode =
            cache_entry->m_seek_to_no != 0 &&
            params.current_format(cache_entry->virtualfile())->is_multiformat();

    if (partial_multiformat_recode)
    {
        // HLS/frame-set repair run: keep existing segments/frames and start
        // directly at the requested item. Existing later items may be
        // overwritten by the encoder; missing earlier gaps intentionally stay
        // missing.
        cache_entry->m_cache_info.m_result  = RESULTCODE::NONE;
        cache_entry->m_cache_info.m_error   = false;
        cache_entry->m_cache_info.m_errno   = 0;
        cache_entry->m_cache_info.m_averror = 0;
    }
    else
    {
        // Full transcode run: remove any older remains.
        cache_entry->clear();
    }

    // Must decode the file, otherwise simply use cache
    cache_entry->m_is_decoding  = true;

    try
    {
        bool unlocked = false;

        Logging::info(cache_entry->filename(), "Transcoding to %1.", params.current_format(cache_entry->virtualfile())->desttype().c_str());

        if (!cache_entry->openio())
        {
            throw (static_cast<int>(errno));
        }

        averror = transcoder.open_input_file(cache_entry->virtualfile());
        if (averror < 0)
        {
            throw (static_cast<int>(errno));
        }

        if (!cache_entry->m_cache_info.m_duration)
        {
            cache_entry->m_cache_info.m_duration            = transcoder.duration();
        }

        if (!cache_entry->m_cache_info.m_predicted_filesize)
        {
            cache_entry->m_cache_info.m_predicted_filesize  = transcoder.predicted_filesize();
        }

        if (!cache_entry->m_cache_info.m_video_frame_count)
        {
            cache_entry->m_cache_info.m_video_frame_count   = transcoder.video_frame_count();
        }

        if (!cache_entry->m_cache_info.m_segment_count)
        {
            cache_entry->m_cache_info.m_segment_count       = transcoder.segment_count();
        }

        if (cache != nullptr && !cache->maintenance(transcoder.predicted_filesize()))
        {
            throw (static_cast<int>(errno));
        }

        if (transcoder.is_hls())
        {
            const uint32_t segment_no = cache_entry->m_seek_to_no;
            if (segment_no)
            {
                // Initial direct HLS access: stack the requested segment before
                // opening the output.  open_output() consumes this immediately
                // and seeks the input before segment 1 is opened/written.
                cache_entry->m_seek_to_no = 0;

                averror = transcoder.stack_seek_segment(segment_no);
                if (averror < 0)
                {
                    throw (static_cast<int>(errno));
                }
            }
        }

        averror = transcoder.open_output_file(cache_entry->m_buffer.get());
        if (averror < 0)
        {
            throw (static_cast<int>(errno));
        }

        std::memcpy(&cache_entry->m_id3v1, transcoder.id3v1tag(), sizeof(ID3v1));

        thread_data->m_initialised = true;

        unlocked = false;
        if ((!params.m_prebuffer_size && !params.m_prebuffer_time) || transcoder.is_frameset())
        {
            // Unlock frame set from beginning
            unlocked = true;
            thread_data->m_thread_running_lock_guard = true;
            thread_data->m_thread_running_cond.notify_all();       // signal that we are running
        }
        else
        {
            if (params.m_prebuffer_time)
            {
                Logging::debug(cache_entry->virtname(), "Pre-buffering up to %1.", format_time(params.m_prebuffer_time).c_str());
            }
            if (params.m_prebuffer_size)
            {
                Logging::debug(cache_entry->virtname(), "Pre-buffering up to %1.", format_size(params.m_prebuffer_size).c_str());
            }
        }

        cache_entry->m_suspend_timeout = false;

        while (!cache_entry->is_finished() && !(*timeout = cache_entry->decode_timeout()) && !thread_exit)
        {
            DECODER_STATUS status = DECODER_STATUS::DEC_SUCCESS;

            if (cache_entry->ref_count() > 1)
            {
                // Set last access time
                cache_entry->update_access(false);
            }

            if (transcoder.is_frameset())
            {
                uint32_t frame_no = cache_entry->m_seek_to_no;
                if (frame_no)
                {
                    cache_entry->m_seek_to_no = 0;

                    averror = transcoder.stack_seek_frame(frame_no);
                    if (averror < 0)
                    {
                        throw (static_cast<int>(errno));
                    }
                }
            }
            else if (transcoder.is_hls())
            {
                uint32_t segment_no = cache_entry->m_seek_to_no;
                if (segment_no)
                {
                    cache_entry->m_seek_to_no = 0;

                    averror = transcoder.stack_seek_segment(segment_no);
                    if (averror < 0)
                    {
                        throw (static_cast<int>(errno));
                    }
                }
            }

            averror = transcoder.process_single_fr(&status);

            if (status == DECODER_STATUS::DEC_ERROR)
            {
                errno = EIO;
                throw (static_cast<int>(errno));
            }
            else if (status == DECODER_STATUS::DEC_EOF)
            {
                cache_entry->m_suspend_timeout = true; // Suspend read_frame time out until transcoder is reopened.

                averror = transcode_finish(cache_entry, transcoder);

                if (averror < 0)
                {
                    errno = EIO;
                    throw (static_cast<int>(errno));
                }
            }

            if (!unlocked && cache_entry->m_buffer->buffer_watermark() > params.m_prebuffer_size && transcoder.pts() > static_cast<int64_t>(params.m_prebuffer_time) * AV_TIME_BASE)
            {
                unlocked = true;
                Logging::info(cache_entry->virtname(), "Pre-buffer limit reached.");

                thread_data->m_thread_running_lock_guard = true;
                thread_data->m_thread_running_cond.notify_all();       // signal that we are running
            }

            if (cache_entry->ref_count() <= 1 && cache_entry->suspend_timeout())
            {
                if (!unlocked && (params.m_prebuffer_size || params.m_prebuffer_time))
                {
                    unlocked = true;
                    thread_data->m_thread_running_lock_guard = true;
                    thread_data->m_thread_running_cond.notify_all();  // signal that we are running
                }

                Logging::info(cache_entry->virtname(), "Timeout! Transcoding suspended after %1 seconds inactivity.", params.m_max_inactive_suspend);

                while (cache_entry->suspend_timeout() && !(*timeout = cache_entry->decode_timeout()) && !thread_exit)
                {
                    mssleep(GRANULARITY);
                }

                if (*timeout)
                {
                    break;
                }

                Logging::info(cache_entry->virtname(), "Transcoding resumed.");
            }
        }

        if (!unlocked && (params.m_prebuffer_size || params.m_prebuffer_time))
        {
            Logging::debug(cache_entry->virtname(), "File transcode complete, releasing buffer early: Size %1.", cache_entry->m_buffer->buffer_watermark());
            thread_data->m_thread_running_lock_guard = true;
            thread_data->m_thread_running_cond.notify_all();       // signal that we are running
        }
    }
    catch (int _syserror)
    {
        success = false;
        syserror = _syserror;

        if (!syserror && averror > -512)
        {
            // If no system error reported explicitly, and averror is a POSIX error
            // (we simply assume that if averror < 512, I think averrors are all higher values)
            syserror = AVUNERROR(averror);
        }

        cache_entry->m_cache_info.m_error   = true;
        if (!syserror)
        {
            // If system error is still zero, set to EIO to return at least anything else than success.
            syserror = EIO;
        }

        thread_data->m_thread_running_lock_guard = true;
        thread_data->m_thread_running_cond.notify_all();           // unlock main thread
    }

    cache_entry->m_suspend_timeout = false; // Should end that suspension; otherwise, read may hang.

    cache_entry->m_cache_info.m_errno       = syserror;                         // Preserve errno
    cache_entry->m_cache_info.m_averror     = averror;                          // Preserve averror

    transcoder.closeio();

    return success;
}

/**
 * @brief Log the final result of a transcoder worker run.
 *
 * Emits exactly one final status message for a transcoder thread after the
 * worker has finished, timed out, been asked to exit, or failed.
 *
 * Successful transcodes include the elapsed runtime in milliseconds. Timeout,
 * thread-exit, and error cases keep their specific diagnostic messages, with
 * system and FFmpeg error details emitted when available in the cache entry.
 *
 * @param cache_entry Cache entry associated with the transcoder worker. If this
 *                    is @c nullptr, no message is emitted.
 * @param transcoder  Transcoder instance used to decide whether this was a
 *                    seeked partial multi-format run.
 * @param timeout     Set to @c true if transcoding was aborted because the
 *                    inactivity timeout expired.
 * @param success     Set to @c true if transcoding completed successfully.
 * @param start_time  Monotonic start time captured when the transcoder worker
 *                    started; used to calculate elapsed runtime for successful
 *                    transcodes.
 */
static void log_transcoding_result(Cache_Entry* cache_entry,
                                   const FFmpeg_Transcoder& transcoder,
                                   bool timeout,
                                   bool success,
                                   const std::chrono::steady_clock::time_point& start_time)
{
    if (cache_entry == nullptr)
    {
        return;
    }

    if (timeout)
    {
        Logging::warning(cache_entry->virtname(),
                         "Timeout! Transcoding aborted after %1 seconds inactivity.",
                         params.m_max_inactive_abort);
        return;
    }

    if (thread_exit)
    {
        Logging::info(cache_entry->virtname(), "Thread exit! Transcoding aborted.");
        return;
    }

    if (success)
    {
        const auto end_time = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        char elapsed_ms_text[32];

        std::snprintf(elapsed_ms_text, sizeof(elapsed_ms_text), "%.1f", elapsed_ms);

        Logging::info(cache_entry->virtname(),
                      "Transcoding completed successfully after %1 ms.",
                      elapsed_ms_text);
        return;
    }

    Logging::error(cache_entry->virtname(), "Transcoding exited with error.");

    if (cache_entry->m_cache_info.m_errno)
    {
        Logging::error(cache_entry->virtname(),
                       "System error: (%1) %2",
                       cache_entry->m_cache_info.m_errno,
                       strerror(cache_entry->m_cache_info.m_errno));
    }

    if (cache_entry->m_cache_info.m_averror)
    {
        Logging::error(cache_entry->virtname(),
                       "FFMpeg error: (%1) %2",
                       cache_entry->m_cache_info.m_averror,
                       ffmpeg_geterror(cache_entry->m_cache_info.m_averror).c_str());
    }
}

/**
 * @brief Transcoding thread
 * @param[in] thread_data - Corresponding thread data object.
 * @returns Returns 0 on success; or errno code on error.
 */
static int transcoder_thread(std::shared_ptr<THREAD_DATA> thread_data)
{
    Cache_Entry * cache_entry = thread_data->m_cache_entry;
    FFmpeg_Transcoder transcoder;
    bool timeout = false;
    bool success = true;
    const auto start_time = std::chrono::steady_clock::now();

    std::unique_lock<std::recursive_mutex> lock_active_mutex(cache_entry->m_active_mutex);
    std::unique_lock<std::recursive_mutex> lock_restart_mutex(cache_entry->m_restart_mutex);

    uint32_t seek_frame = 0;

    do
    {
        if (seek_frame)
        {
            Logging::error(transcoder.virtname(), "Transcoder completed with last seek frame to %1. Transcoder is being restarted.", seek_frame);
        }

        success = transcode(thread_data, cache_entry, transcoder, &timeout);

        seek_frame = cache_entry->m_seek_to_no != 0 ? cache_entry->m_seek_to_no.load() : transcoder.last_seek_frame_no();

        if (transcoder.is_multiformat() && transcoder.have_seeked())
        {
            // HLS/frame-set caches are intentionally allowed to remain
            // incomplete after a seek.  Missing segments/frames are repaired
            // on demand; restarting from segment/frame 1 here would recreate
            // the old loop: transcode, invalidate/restart, seek, repeat.
            seek_frame = 0;
        }
    }
    while (success && !thread_exit && cache != nullptr && seek_frame);

cache_entry->m_is_decoding = false;

	if (timeout || thread_exit || transcoder.have_seeked())
	{
	    if (!transcoder.have_seeked())
	    {
	        cache_entry->m_cache_info.m_error   = true;
	        cache_entry->m_cache_info.m_errno   = EIO;      // Report I/O error
	    }
	    else
	    {
	        // Must restart from scratch, but this is not an error.
	        cache_entry->m_cache_info.m_error   = false;
	        cache_entry->m_cache_info.m_errno   = 0;
	        cache_entry->m_cache_info.m_averror = 0;
	    }
	}
	else
	{
	    cache_entry->m_cache_info.m_error = !success;

	    if (success)
	    {
	        cache_entry->m_cache_info.m_errno   = 0;
	        cache_entry->m_cache_info.m_averror = 0;
	    }
	}

	log_transcoding_result(cache_entry, transcoder, timeout, success, start_time);

    int _errno = cache_entry->m_cache_info.m_errno;

    if (cache != nullptr)
    {
        cache->closeio(&cache_entry, timeout ? CACHE_CLOSE_DELETE : CACHE_CLOSE_NOOPT);
    }

    thread_data.reset();

    errno = _errno;

    return errno;
}
