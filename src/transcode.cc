/*
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2013 K. Henriksson
 * Copyright (C) 2017-2023 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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
 * Copyright (C) 2017-2023 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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
    void *                  m_arg;                          /**< @brief Opaque argument pointer. Will not be freed by child thread. */
} THREAD_DATA;

static Cache *              cache;                          /**< @brief Global cache manager object */
static std::atomic_bool     thread_exit;                    /**< @brief Used for shutdown: if true, forcibly exit all threads */

static bool transcode(THREAD_DATA *thread_data, Cache_Entry *cache_entry, FFmpeg_Transcoder & transcoder, bool *timeout);
static int  transcoder_thread(void *arg);
static bool transcode_until(Cache_Entry* cache_entry, size_t offset, size_t len, uint32_t segment_no);
static int  transcode_finish(Cache_Entry* cache_entry, FFmpeg_Transcoder & transcoder);

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
    size_t end = offset + len; // Cast OK: offset will never be < 0.
    bool success = true;

    if (cache_entry->is_finished() || cache_entry->m_buffer->is_segment_finished(segment_no) || cache_entry->m_buffer->tell(segment_no) >= end)
    {
        return true;
    }

    try
    {
        // Wait until decoder thread has reached the desired position
        if (cache_entry->m_is_decoding)
        {
            bool reported = false;
            while (!(cache_entry->is_finished() || cache_entry->m_buffer->is_segment_finished(segment_no) || cache_entry->m_buffer->tell(segment_no) >= end) && !cache_entry->m_cache_info.m_error)
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
            success = !cache_entry->m_cache_info.m_error;
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
static int transcode_finish(Cache_Entry* cache_entry, FFmpeg_Transcoder &transcoder)
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
    cache_entry->m_cache_info.m_result              = !transcoder.have_seeked() ? RESULTCODE_FINISHED_SUCCESS : RESULTCODE_FINISHED_INCOMPLETE;
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

void transcoder_cache_path(std::string & path)
{
    if (params.m_cachepath.size())
    {
        path = params.m_cachepath;
    }
    else
    {
        if (geteuid() == 0)
        {
            // Running as root
            path = "/var/cache";
        }
        else
        {
            // Running as regular user, put cache in home dir
            if (const char* cache_home = std::getenv("XDG_CACHE_HOME"))
            {
                path = cache_home;
            }
            else
            {
                expand_path(&path, "~/.cache");
            }
        }
    }

    append_sep(&path);

    path += PACKAGE;

    append_sep(&path);
}

bool transcoder_init()
{
    if (cache == nullptr)
    {
        Logging::debug(nullptr, "Creating new media file cache.");
        cache = new(std::nothrow) Cache;
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

void transcoder_free()
{
    Cache *p1 = cache;
    cache = nullptr;

    if (p1 != nullptr)
    {
        Logging::debug(nullptr, "Deleting media file cache.");
        delete p1;
    }
}

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
        Logging::warning(cache_entry->virtname(), "Unsupported audio codec '%1' for format %2.", get_codec_name(current_format->audio_codec(), false), current_format->desttype().c_str());
    }

    if (!FFmpeg_Transcoder::video_size(&filesize, current_format->video_codec(), video_bit_rate, duration, width, height, interleaved, framerate))
    {
        Logging::warning(cache_entry->virtname(), "Unsupported video codec '%1' for format %2.", get_codec_name(current_format->video_codec(), false), current_format->desttype().c_str());
    }

    if (!FFmpeg_Transcoder::total_overhead(&filesize, current_format->filetype()))
    {
        Logging::warning(cache_entry->virtname(), "Unsupported file type '%1' for format %2.", get_filetype_text(current_format->filetype()).c_str(), current_format->desttype().c_str());
    }

    cache_entry->m_cache_info.m_predicted_filesize = virtualfile->m_predicted_size = filesize;

    Logging::trace(cache_entry->virtname(), "Predicted transcoded size of %1.", format_size_ex(cache_entry->m_cache_info.m_predicted_filesize).c_str());

    return true;
}

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

        if (!cache_entry->openio(begin_transcode))
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
                Logging::debug(cache_entry->filename(), "Starting transcoder thread.");

                // Clear cache to remove any older remains
                cache_entry->clear();

                // Must decode the file, otherwise simply use cache
                cache_entry->m_is_decoding                  = true;

                THREAD_DATA* thread_data                    = new(std::nothrow) THREAD_DATA;

                thread_data->m_initialised                  = false;
                thread_data->m_arg                          = cache_entry;
                thread_data->m_thread_running_lock_guard    = false;

                {
                    std::unique_lock<std::mutex> lock_thread_running_mutex(thread_data->m_thread_running_mutex);

                    tp->schedule_thread(&transcoder_thread, thread_data);

                    // Let decoder get into gear before returning from open
                    while (!thread_data->m_thread_running_lock_guard)
                    {
                        thread_data->m_thread_running_cond.wait(lock_thread_running_mutex);
                    }
                }

                if (cache_entry->m_cache_info.m_error)
                {
                    int ret;

                    Logging::trace(cache_entry->filename(), "Transcoder error!");

                    ret = cache_entry->m_cache_info.m_errno;
                    if (!ret)
                    {
                        ret = EIO; // Must return something, be it a simple I/O error...
                    }
                    throw ret;
                }

                Logging::debug(cache_entry->filename(), "Transcoder thread is running.");
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
            Logging::trace(cache_entry->virtname(), "Reading file from cache.");
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
        // Try to read requested segment, stack a seek to if if this fails.
        // No seek if not HLS (segment_no) and not required if < MIN_SEGMENT
        if (segment_no && !cache_entry->m_buffer->segment_exists(segment_no))
        {
            cache_entry->m_seek_to_no = segment_no;
        }

        // Open for reading if necessary
        if (!cache_entry->m_buffer->open_file(segment_no, CACHE_FLAG_RO))
        {
            throw false;
        }

        if (!cache_entry->is_finished_success())
        {
            switch (params.current_format(cache_entry->virtualfile())->filetype())
            {
            case FILETYPE_MP3:
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
        // Open for reading if necessary
        if (!cache_entry->m_buffer->open_file(0, CACHE_FLAG_RO))
        {
            throw false;
        }

        std::vector<uint8_t> data;

        // Wait until decoder thread has the requested frame available
        if (cache_entry->m_is_decoding || cache_entry->m_suspend_timeout)
        {
            // Try to read requested frame, stack a seek to if if this fails.
            if (!cache_entry->m_buffer->read_frame(&data, frame_no))
            {
                bool reported = false;

                cache_entry->m_seek_to_no = frame_no;

                int retries = TOTAL_RETRIES;
                while (!cache_entry->m_buffer->read_frame(&data, frame_no) && !thread_exit)
                {
                    if (errno != EAGAIN)
                    {
                        Logging::error(cache_entry->virtname(), "Reading image frame no. %1: (%2) %3", frame_no, errno, strerror(errno));
                        throw false;
                    }

                    if (!cache_entry->m_suspend_timeout)
                    {
                        if (errno == EAGAIN && !--retries)
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
                        Logging::trace(cache_entry->virtname(), "Frame no. %1: Cache miss at offset %<%11zu>2 (length %<%6u>3).", frame_no, offset, len);
                        reported = true;
                    }

                    mssleep(GRANULARITY);
                }
            }

            Logging::trace(cache_entry->virtname(), "Frame no. %1: Cache hit  at offset %<%11zu>2 (length %<%6u>3).", frame_no, offset, len);

            success = !cache_entry->m_cache_info.m_error;
        }
        else
        {
            success = cache_entry->m_buffer->read_frame(&data, frame_no);
            if (!success)
            {
                Logging::error(cache_entry->virtname(), "Reading image frame no. %1: (%2) %3", frame_no, errno, strerror(errno));
                throw false;
            }
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

void transcoder_delete(Cache_Entry* cache_entry)
{
    cache->closeio(&cache_entry);
}

size_t transcoder_get_size(Cache_Entry* cache_entry)
{
    return cache_entry->size();
}

size_t transcoder_buffer_watermark(Cache_Entry* cache_entry, uint32_t segment_no)
{
    return cache_entry->m_buffer->buffer_watermark(segment_no);
}

size_t transcoder_buffer_tell(Cache_Entry* cache_entry, uint32_t segment_no)
{
    return cache_entry->m_buffer->tell(segment_no);
}

void transcoder_exit()
{
    thread_exit = true;
}

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
static bool transcode(THREAD_DATA *thread_data, Cache_Entry *cache_entry, FFmpeg_Transcoder & transcoder, bool *timeout)
{
    int averror = 0;
    int syserror = 0;
    bool success = true;

    // Clear cache to remove any older remains
    cache_entry->clear();

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

        averror = transcoder.open_output_file(cache_entry->m_buffer);
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
            DECODER_STATUS status = DECODER_SUCCESS;

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

            if (status == DECODER_ERROR)
            {
                errno = EIO;
                throw (static_cast<int>(errno));
            }
            else if (status == DECODER_EOF)
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
                Logging::debug(cache_entry->virtname(), "Pre-buffer limit reached.");

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

                Logging::info(cache_entry->virtname(), "Suspend timeout. Transcoding suspended after %1 seconds inactivity.", params.m_max_inactive_suspend);

                while (cache_entry->suspend_timeout() && !(*timeout = cache_entry->decode_timeout()) && !thread_exit)
                {
                    mssleep(GRANULARITY);
                }

                if (timeout)
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
 * @brief Transcoding thread
 * @param[in] arg - Corresponding Cache_Entry object.
 * @returns Returns 0 on success; or errno code on error.
 */
static int transcoder_thread(void *arg)
{
    THREAD_DATA *thread_data = static_cast<THREAD_DATA*>(arg);
    Cache_Entry *cache_entry = static_cast<Cache_Entry *>(thread_data->m_arg);
    FFmpeg_Transcoder transcoder;
    bool timeout = false;
    bool success = true;

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
    }
    while (success && !thread_exit && cache != nullptr && seek_frame);

    cache_entry->m_is_decoding          = false;

    if (timeout || thread_exit || transcoder.have_seeked())
    {
        if (!transcoder.have_seeked())
        {
            cache_entry->m_cache_info.m_error       = true;
            cache_entry->m_cache_info.m_errno       = EIO;      // Report I/O error

            if (timeout)
            {
                Logging::warning(cache_entry->virtname(), "Timeout! Transcoding aborted after %1 seconds inactivity.", params.m_max_inactive_abort);
            }
            else if (thread_exit)
            {
                Logging::info(cache_entry->virtname(), "Thread exit! Transcoding aborted.");
            }
            else
            {
                Logging::info(cache_entry->virtname(), "Transcoding aborted.");
            }
        }
        else
        {
            // Must restart from scratch, but this is not an error.
            cache_entry->m_cache_info.m_error       = false;
            cache_entry->m_cache_info.m_errno       = 0;
            cache_entry->m_cache_info.m_averror     = 0;
        }
    }
    else
    {
        cache_entry->m_cache_info.m_error           = !success;

        if (success)
        {
            Logging::info(cache_entry->virtname(), "Transcoding completed successfully.");

            cache_entry->m_cache_info.m_errno       = 0;
            cache_entry->m_cache_info.m_averror     = 0;
        }
        else
        {
            Logging::error(cache_entry->virtname(), "Transcoding exited with error.");
            if (cache_entry->m_cache_info.m_errno)
            {
                Logging::error(cache_entry->virtname(), "System error: (%1) %2", cache_entry->m_cache_info.m_errno, strerror(cache_entry->m_cache_info.m_errno));
            }

            if (cache_entry->m_cache_info.m_averror)
            {
                Logging::error(cache_entry->virtname(), "FFMpeg error: (%1) %2", cache_entry->m_cache_info.m_averror, ffmpeg_geterror(cache_entry->m_cache_info.m_averror).c_str());
            }
        }
    }

    int _errno = cache_entry->m_cache_info.m_errno;

    if (cache != nullptr)
    {
        cache->closeio(&cache_entry, timeout ? CACHE_CLOSE_DELETE : CACHE_CLOSE_NOOPT);
    }

    delete thread_data;

    errno = _errno;

    return errno;
}
