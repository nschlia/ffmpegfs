/*
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 * 
 * On Debian systems, the complete text of the GNU General Public License
 * Version 3 can be found in `/usr/share/common-licenses/GPL-3'.
 */

/**
 * @file
 * @brief File transcoder interface implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2019 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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

/** @brief 1 millisecond = 1,000,000 Nanoseconds */
#define MS  *1000000L

/**
  * @brief THREAD_DATA struct to pass data from parent to child thread
  */
typedef struct THREAD_DATA
{
    std::mutex              m_mutex;            /**< @brief Mutex when thread is running */
    std::condition_variable m_cond;             /**< @brief Condition when thread is running */
    std::atomic_bool        m_lock_guard;       /**< @brief Lock guard to avoid spurious or missed unlocks */
    bool                    m_initialised;      /**< @brief True when this object is completely initialised */
    void *                  m_arg;              /**< @brief Opaque argument pointer. Will not be freed by child thread. */
} THREAD_DATA;

static Cache *cache;                            /**< @brief Global cache manager object */
static volatile bool thread_exit;               /**< @brief Used for shutdown: if true, exit all thread */

static void transcoder_thread(void *arg);
static bool transcode_until(Cache_Entry* cache_entry, size_t offset, size_t len);
static int transcode_finish(Cache_Entry* cache_entry, FFmpeg_Transcoder *transcoder);

/**
 * @brief Transcode the buffer until the buffer has enough or until an error occurs.
 * The buffer needs at least 'end' bytes before transcoding stops. Returns true
 * if no errors and false otherwise.
 *  @param[in] cache_entry - corresponding cache entry
 *  @param[in] offset - byte offset to start reading at
 *  @param[in] len - length of data chunk to be read.
 * @return On success, returns true. Returns false if an error occurred.
 */
static bool transcode_until(Cache_Entry* cache_entry, size_t offset, size_t len)
{
    size_t end = offset + len; // Cast OK: offset will never be < 0.
    bool success = true;

    if (cache_entry->m_cache_info.m_finished == RESULTCODE_FINISHED || cache_entry->m_buffer->tell() >= end)
    {
        return true;
    }

    try
    {
        // Wait until decoder thread has reached the desired position
        if (cache_entry->m_is_decoding)
        {
            bool reported = false;
            while (cache_entry->m_cache_info.m_finished != RESULTCODE_FINISHED && !cache_entry->m_cache_info.m_error && cache_entry->m_buffer->tell() < end)
            {
                if (fuse_interrupted())
                {
                    Logging::info(cache_entry->destname(), "Client has gone away.");
                    throw false;
                }

                if (thread_exit)
                {
                    Logging::warning(cache_entry->destname(), "Received thread exit.");
                    throw false;
                }

                if (!reported)
                {
                    Logging::trace(cache_entry->destname(), "Cache miss at offset %<%11zu>1 (length %<%6u>2), remaining %3.", offset, len, format_size_ex(cache_entry->m_buffer->size() - end).c_str());
                    reported = true;
                }
                sleep(0);
            }

            if (reported)
            {
                Logging::trace(cache_entry->destname(), "Cache hit  at offset %<%11zu>1 (length %<%6u>2), remaining %3.", offset, len, format_size_ex(cache_entry->m_buffer->size() - end).c_str());
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
 * @return On success returns 0; on error negative AVERROR.
 */
static int transcode_finish(Cache_Entry* cache_entry, FFmpeg_Transcoder *transcoder)
{
    int res = transcoder->encode_finish();
    if (res < 0)
    {
        return res;
    }

    // Check encoded buffer size.
    cache_entry->m_cache_info.m_encoded_filesize    = cache_entry->m_buffer->buffer_watermark();
    cache_entry->m_cache_info.m_finished            = RESULTCODE_FINISHED;
    cache_entry->m_is_decoding                      = false;
    cache_entry->m_cache_info.m_errno               = 0;
    cache_entry->m_cache_info.m_averror             = 0;

    Logging::debug(transcoder->destname(), "Finishing file.");

    if (!cache_entry->m_buffer->reserve(cache_entry->m_cache_info.m_encoded_filesize))
    {
        Logging::debug(transcoder->destname(), "Unable to truncate buffer.");
    }

    if (!transcoder->export_frameset())
    {
        Logging::debug(transcoder->destname(), "Predicted size: %1 Final: %2 Diff: %3 (%4%).",
                       format_size_ex(cache_entry->m_cache_info.m_predicted_filesize).c_str(),
                       format_size_ex(cache_entry->m_cache_info.m_encoded_filesize).c_str(),
                       format_result_size_ex(cache_entry->m_cache_info.m_encoded_filesize, cache_entry->m_cache_info.m_predicted_filesize).c_str(),
                       static_cast<double>((cache_entry->m_cache_info.m_encoded_filesize * 1000 / (cache_entry->m_cache_info.m_predicted_filesize + 1)) + 5) / 10);
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
        tempdir(path);
    }

    append_sep(&path);

    path += PACKAGE;

    append_sep(&path);
}

bool transcoder_init(void)
{
    if (cache == nullptr)
    {
        Logging::debug(nullptr, "Creating media file cache.");
        cache = new(std::nothrow) Cache;
        if (cache == nullptr)
        {
            Logging::error(nullptr, "Unable to create media file cache. Out of memory.");
            std::fprintf(stderr, "ERROR: Creating media file cache. Out of memory.\n");
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

void transcoder_free(void)
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
    Cache_Entry* cache_entry = cache->open(virtualfile);
    if (cache_entry == nullptr)
    {
        return false;
    }

    Logging::trace(cache_entry->destname(), "Retrieving encoded size.");

    size_t encoded_filesize = cache_entry->m_cache_info.m_encoded_filesize;

    if (!encoded_filesize)
    {
        // If not yet encoded, return predicted file size
        encoded_filesize = cache_entry->m_cache_info.m_predicted_filesize;
    }

    if (encoded_filesize)
    {
        stbuf->st_size = static_cast<off_t>(encoded_filesize);
        stbuf->st_blocks = (stbuf->st_size + 512 - 1) / 512;
        return true;
    }
    else
    {
        return false;
    }
}

bool transcoder_set_filesize(LPVIRTUALFILE virtualfile, int64_t duration, BITRATE audio_bit_rate, int channels, int sample_rate, BITRATE video_bit_rate, int width, int height, int interleaved, const AVRational &framerate)
{
    Cache_Entry* cache_entry = cache->open(virtualfile);
    if (cache_entry == nullptr)
    {
        Logging::error(cache_entry->filename(), "Out of memory getting file size.");
        return false;
    }

    FFmpegfs_Format *current_format = params.current_format(virtualfile);
    if (current_format == nullptr)
    {
        Logging::error(cache_entry->filename(), "Internal error getting file size.");
        return false;
    }

    size_t filesize = 0;

    if (!FFmpeg_Transcoder::audio_size(&filesize, current_format->audio_codec_id(), audio_bit_rate, duration, channels, sample_rate))
    {
        Logging::warning(cache_entry->filename(), "Unsupported audio codec '%1' for format %2.", get_codec_name(current_format->audio_codec_id(), 0), current_format->desttype().c_str());
    }

    if (!FFmpeg_Transcoder::video_size(&filesize, current_format->video_codec_id(), video_bit_rate, duration, width, height, interleaved, framerate))
    {
        Logging::warning(cache_entry->filename(), "Unsupported video codec '%1' for format %2.", get_codec_name(current_format->video_codec_id(), 0), current_format->desttype().c_str());
    }

    cache_entry->m_cache_info.m_predicted_filesize = filesize;

    Logging::debug(cache_entry->filename(), "Predicted transcoded size of %1.", format_size_ex(cache_entry->m_cache_info.m_predicted_filesize).c_str());

    return true;
}

bool transcoder_predict_filesize(LPVIRTUALFILE virtualfile, Cache_Entry* cache_entry)
{
    FFmpeg_Transcoder *transcoder = new(std::nothrow) FFmpeg_Transcoder;
    bool success = false;

    if (transcoder == nullptr)
    {
        Logging::error(cache_entry->filename(), "Out of memory getting file size.");
        return false;
    }

    if (transcoder->open_input_file(virtualfile) >= 0)
    {
        cache_entry->m_cache_info.m_predicted_filesize  = transcoder->predicted_filesize();
        cache_entry->m_cache_info.m_video_frame_count   = transcoder->video_frame_count();

        transcoder->close();

        Logging::debug(cache_entry->filename(), "Predicted transcoded size of %1.", format_size_ex(cache_entry->m_cache_info.m_predicted_filesize).c_str());

        success = true;
    }

    delete transcoder;

    return success;
}

Cache_Entry* transcoder_new(LPVIRTUALFILE virtualfile, bool begin_transcode)
{
    // Allocate transcoder structure
    Cache_Entry* cache_entry = cache->open(virtualfile);
    if (cache_entry == nullptr)
    {
        return nullptr;
    }

    Logging::trace(cache_entry->filename(), "Creating transcoder object.");

    try
    {
        cache_entry->lock();

        if (!cache_entry->open(begin_transcode))
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

        virtualfile->m_video_frame_count = cache_entry->m_cache_info.m_video_frame_count; /***< @todo: duration? */

        if (!cache_entry->m_is_decoding && cache_entry->m_cache_info.m_finished != RESULTCODE_FINISHED)
        {
            if (begin_transcode)
            {
                int ret;

                Logging::debug(cache_entry->filename(), "Starting decoder thread.");

                if (cache_entry->m_cache_info.m_error)
                {
                    // If error occurred last time, clear cache
                    cache_entry->clear();
                }

                // Must decode the file, otherwise simply use cache
                cache_entry->m_is_decoding = true;

                THREAD_DATA* thread_data = new(std::nothrow) THREAD_DATA;

                thread_data->m_initialised  = false;
                thread_data->m_arg          = cache_entry;
                thread_data->m_lock_guard    = false;

                {
                    std::unique_lock<std::mutex> lock(thread_data->m_mutex);

                    tp->schedule_thread(&transcoder_thread, thread_data);

                    while (!thread_data->m_lock_guard)
                    {
                        thread_data->m_cond.wait(lock);
                    }
                }

                Logging::debug(cache_entry->filename(), "Decoder thread is running.");

                if (cache_entry->m_cache_info.m_error)
                {
                    Logging::trace(cache_entry->filename(), "Decoder error!");
                    ret = cache_entry->m_cache_info.m_errno;
                    if (!ret)
                    {
                        ret = EIO; // Must return anything...
                    }
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
            Logging::trace(cache_entry->destname(), "Reading file from cache.");
        }

        cache_entry->unlock();
    }
    catch (int _errno)
    {
        cache_entry->m_is_decoding = false;
        cache_entry->unlock();
        cache->close(&cache_entry, CLOSE_CACHE_DELETE);
        cache_entry = nullptr;  // Make sure to return NULL here even if the cache could not be deleted now (still in use)
        errno = _errno;         // Restore last errno
    }

    return cache_entry;
}

bool transcoder_read(Cache_Entry* cache_entry, char* buff, size_t offset, size_t len, int * bytes_read)
{
    bool success = true;

    Logging::trace(cache_entry->destname(), "Reading %1 bytes from offset %2.", len, offset);

    // Store access time
    cache_entry->update_access();

    // Update read counter
    cache_entry->update_read_count();

    try
    {
        if (cache_entry->m_cache_info.m_finished != RESULTCODE_FINISHED)
        {
            switch (params.current_format(cache_entry->virtualfile())->filetype())
            {
            case FILETYPE_MP3:
            {
                // If we are encoding to MP3 and the requested data overlaps the ID3v1 tag
                // at the end of the file, do not encode data first up to that position.
                // This optimises the case where applications read the end of the file
                // first to read the ID3v1 tag.
                if ((offset > cache_entry->m_buffer->tell()) &&
                        ((offset + len) > (cache_entry->size() - ID3V1_TAG_LENGTH)))
                {

                    memcpy(buff, &cache_entry->m_id3v1, ID3V1_TAG_LENGTH);

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
                if ((offset > cache_entry->m_buffer->tell()) &&
                        (len <= 65536) &&
                        check_ignore(cache_entry->size(), offset) &&
                        ((offset + len) > (cache_entry->size())))
                {
                    Logging::warning(cache_entry->destname(), "EXPERIMENTAL FEATURE: Ignoring Windows' groundless access at last 8K boundary of file.");

                    errno = 0;
                    *bytes_read = 0;  // We've read nothing
                    len = 0;

                    throw true; // OK
                }
            }
        }

        // Set last access time
        cache_entry->m_cache_info.m_access_time = time(nullptr);

        bool success = transcode_until(cache_entry, offset, len);

        if (!success)
        {
            errno = cache_entry->m_cache_info.m_errno ? cache_entry->m_cache_info.m_errno : EIO;
            throw false;
        }

        // truncate if we didn't actually get len
        if (cache_entry->m_buffer->buffer_watermark() < offset)
        {
            len = 0;
        }
        else if (cache_entry->m_buffer->buffer_watermark() < offset + len)
        {
            len = cache_entry->m_buffer->buffer_watermark() - offset;
        }

        if (!cache_entry->m_buffer->copy(reinterpret_cast<uint8_t*>(buff), offset, len))
        {
            len = 0;
        }

        if (cache_entry->m_cache_info.m_error)
        {
            errno = cache_entry->m_cache_info.m_errno ? cache_entry->m_cache_info.m_errno : EIO;
            throw false;
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

    Logging::trace(cache_entry->destname(), "Reading %1 bytes from offset %2 for frame no. %3.", len, offset, frame_no);

    // Store access time
    cache_entry->update_access();

    // Update read counter
    cache_entry->update_read_count();

    try
    {
        // Set last access time
        cache_entry->m_cache_info.m_access_time = time(nullptr);

        std::vector<uint8_t> data;

        // Wait until decoder thread has the requested frame available
        if (cache_entry->m_is_decoding)
        {
            bool reported = false;

            if (!cache_entry->m_buffer->read_frame(&data, frame_no))
            {
                cache_entry->m_seek_frame_no = frame_no;
            }

            int n = 300;
            while (!cache_entry->m_buffer->read_frame(&data, frame_no) && !thread_exit)
            {
                if (errno != EAGAIN)
                {
                    Logging::error(cache_entry->destname(), "Reading image frame no. %1: (%2) %3", frame_no, errno, strerror(errno));
                    throw false;
                }

                if (errno == EAGAIN && !--n)
                {
                    errno = ETIMEDOUT;
                    Logging::error(cache_entry->destname(), "Timeout reading image frame no. %1: (%2) %3", frame_no, errno, strerror(errno));
                    throw false;
                }

                if (fuse_interrupted())
                {
                    Logging::info(cache_entry->destname(), "Client has gone away.");
                    throw false;
                }

                if (thread_exit)
                {
                    Logging::warning(cache_entry->destname(), "Received thread exit.");
                    throw false;
                }

                if (!reported)
                {
                    Logging::trace(cache_entry->destname(), "Cache miss at offset %<%11zu>1 (length %<%6u>2).", offset, len);
#ifdef DEBUG_FRAME_SET
                    if (!offset)
                    {
                        Logging::FRAME_SET_WARNING(cache_entry->filename(), "FRAME READ MISS    | Frame: %<%10u>1", frame_no);
                    }
#endif // DEBUG_FRAME_SET
                    reported = true;
                }

                const struct timespec ts = { 0, 100 MS };
                nanosleep(&ts, nullptr);
            }

            if (reported)
            {
                Logging::trace(cache_entry->destname(), "Cache hit  at offset %<%11zu>1 (length %<%6u>2).", offset, len);
            }
#ifdef DEBUG_FRAME_SET
            if (!offset/* && !reported*/)
            {
                Logging::FRAME_SET(cache_entry->filename(), "FRAME READ HIT     | Frame: %<%10u>1", frame_no);
            }
#endif // DEBUG_FRAME_SET
            success = !cache_entry->m_cache_info.m_error;
        }
        else
        {
            success = cache_entry->m_buffer->read_frame(&data, frame_no);
            if (!success)
            {
                Logging::error(cache_entry->destname(), "Reading image frame no. %1: (%2) %3", frame_no, errno, strerror(errno));
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
                memcpy(buff, data.data() + offset, len);
            }

            virtualfile->m_st.st_size   = static_cast<off_t>(data.size());
            virtualfile->m_st.st_blocks = (virtualfile->m_st.st_size + 512 - 1) / 512;

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
    cache->close(&cache_entry);
}

size_t transcoder_get_size(Cache_Entry* cache_entry)
{
    return cache_entry->size();
}

size_t transcoder_buffer_watermark(Cache_Entry* cache_entry)
{
    return cache_entry->m_buffer->buffer_watermark();
}

size_t transcoder_buffer_tell(Cache_Entry* cache_entry)
{
    return cache_entry->m_buffer->tell();
}

void transcoder_exit(void)
{
    thread_exit = true;
}

bool transcoder_cache_maintenance(void)
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

bool transcoder_cache_clear(void)
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
 * @brief Transcoding thread
 * @param[in] arg - Corresponding Cache_Entry object.
 */
static void transcoder_thread(void *arg)
{
    THREAD_DATA *thread_data = static_cast<THREAD_DATA*>(arg);
    Cache_Entry *cache_entry = static_cast<Cache_Entry *>(thread_data->m_arg);
    FFmpeg_Transcoder *transcoder = new(std::nothrow) FFmpeg_Transcoder;
    int averror = 0;
    int syserror = 0;
    bool timeout = false;
    bool success = true;

    std::unique_lock<std::recursive_mutex> lock(cache_entry->m_active_mutex);

    try
    {
        if (transcoder == nullptr)
        {
            Logging::error(cache_entry->filename(), "Internal error: Trancoder object should not be NULL.");
            throw (static_cast<int>(ENOMEM));
        }

        Logging::info(cache_entry->filename(), "Transcoding to %1.", params.current_format(cache_entry->virtualfile())->desttype().c_str());

        if (!cache_entry->open())
        {
            throw (static_cast<int>(errno));
        }

        averror = transcoder->open_input_file(cache_entry->virtualfile());
        if (averror < 0)
        {
            throw (static_cast<int>(errno));
        }

        if (!cache_entry->m_cache_info.m_predicted_filesize)
        {
            cache_entry->m_cache_info.m_predicted_filesize  = transcoder->predicted_filesize();
        }

        if (!cache_entry->m_cache_info.m_video_frame_count)
        {
            cache_entry->m_cache_info.m_video_frame_count   = transcoder->video_frame_count();
        }

        if (!cache->maintenance(transcoder->predicted_filesize()))
        {
            throw (static_cast<int>(errno));
        }

        averror = transcoder->open_output_file(cache_entry->m_buffer);
        if (averror < 0)
        {
            throw (static_cast<int>(errno));
        }

        memcpy(&cache_entry->m_id3v1, transcoder->id3v1tag(), sizeof(ID3v1));

        thread_data->m_initialised = true;

        bool unlocked = false;
        if (!params.m_prebuffer_size || transcoder->export_frameset())
        {
            // Unlock frame set from beginning
            unlocked = true;
            thread_data->m_lock_guard = true;
            thread_data->m_cond.notify_all();       // signal that we are running
        }
        else
        {
            Logging::debug(cache_entry->destname(), "Pre-buffering up to %1 bytes.", params.m_prebuffer_size);
        }

        while ((cache_entry->m_cache_info.m_finished != RESULTCODE_FINISHED) && !(timeout = cache_entry->decode_timeout()) && !thread_exit)
        {
            int status = 0;

            if (cache_entry->ref_count() > 1)
            {
                // Set last access time
                cache_entry->update_access(false);
            }

            if (transcoder->export_frameset())
            {
                uint32_t frame_no = cache_entry->m_seek_frame_no;
                if (frame_no)
                {
                    cache_entry->m_seek_frame_no = 0;

                    averror = transcoder->seek_frame(frame_no);
                    if (averror < 0)
                    {
                        throw (static_cast<int>(errno));
                    }
                }
            }

            averror = transcoder->process_single_fr(status);
            if (status < 0)
            {
                syserror = EIO;
                success = false;
                break;
            }

            if (status == 1 && ((averror = transcode_finish(cache_entry, transcoder)) < 0))
            {
                syserror = EIO;
                success = false;
                break;
            }

            if (!unlocked && cache_entry->m_buffer->buffer_watermark() > params.m_prebuffer_size)
            {
                unlocked = true;
                Logging::debug(cache_entry->destname(), "Pre-buffer limit reached.");
                thread_data->m_lock_guard = true;
                thread_data->m_cond.notify_all();       // signal that we are running
            }

            if (cache_entry->ref_count() <= 1 && cache_entry->suspend_timeout())
            {
                if (!unlocked && params.m_prebuffer_size)
                {
                    unlocked = true;
                    thread_data->m_lock_guard = true;
                    thread_data->m_cond.notify_all();  // signal that we are running
                }

                Logging::info(cache_entry->destname(), "Suspend timeout. Transcoding suspended after %1 seconds inactivity.", params.m_max_inactive_suspend);

                while (cache_entry->suspend_timeout() && !(timeout = cache_entry->decode_timeout()) && !thread_exit)
                {
                    sleep(1);
                }

                if (timeout)
                {
                    break;
                }

                Logging::info(cache_entry->destname(), "Transcoding resumed.");
            }
        }

        if (!unlocked && params.m_prebuffer_size)
        {
            Logging::debug(cache_entry->destname(), "File transcode complete, releasing buffer early: Size %1.", cache_entry->m_buffer->buffer_watermark());
            thread_data->m_lock_guard = true;
            thread_data->m_cond.notify_all();       // signal that we are running
        }
    }
    catch (int _syserror)
    {
        success = false;
        syserror = _syserror;

        cache_entry->m_is_decoding              = false;
        cache_entry->m_cache_info.m_error       = !success;
        cache_entry->m_cache_info.m_errno       = success ? 0 : (syserror ? syserror : EIO);    // Preserve errno
        cache_entry->m_cache_info.m_averror     = success ? 0 : averror;                        // Preserve averror

        thread_data->m_lock_guard = true;
        thread_data->m_cond.notify_all();           // unlock main thread
    }

    transcoder->close();

    delete transcoder;

    if (timeout || thread_exit || transcoder->have_seeked())
    {
        cache_entry->m_is_decoding              = false;

        if (!transcoder->have_seeked())
        {
            cache_entry->m_cache_info.m_finished    = RESULTCODE_ERROR;
            cache_entry->m_cache_info.m_error       = true;
            cache_entry->m_cache_info.m_errno       = EIO;      // Report I/O error
            cache_entry->m_cache_info.m_averror     = averror;  // Preserve averror

            if (timeout)
            {
                Logging::warning(cache_entry->destname(), "Timeout! Transcoding aborted after %1 seconds inactivity.", params.m_max_inactive_abort);
            }
            else
            {
                Logging::info(cache_entry->destname(), "Thread exit! Transcoding aborted.");
            }
        }
        else
        {
            // Must restart from scratch, but this is not an error.
            cache_entry->m_cache_info.m_finished    = RESULTCODE_INCOMPLETE;
            cache_entry->m_cache_info.m_error       = false;
            cache_entry->m_cache_info.m_errno       = 0;
            cache_entry->m_cache_info.m_averror     = 0;
        }
    }
    else
    {
        cache_entry->m_is_decoding                  = false;
        cache_entry->m_cache_info.m_error           = !success;
        cache_entry->m_cache_info.m_errno           = success ? 0 : (syserror ? syserror : EIO);    // Preserve errno
        cache_entry->m_cache_info.m_averror         = success ? 0 : averror;                        // Preserve averror

        if (success)
        {
            Logging::info(cache_entry->destname(), "Transcoding completed successfully.");
        }
        else
        {
            Logging::error(cache_entry->destname(), "Transcoding exited with error.");
            if (cache_entry->m_cache_info.m_errno)
            {
                Logging::error(cache_entry->destname(), "System error: (%1) %2", cache_entry->m_cache_info.m_errno, strerror(cache_entry->m_cache_info.m_errno));
            }
            if (cache_entry->m_cache_info.m_averror)
            {
                Logging::error(cache_entry->destname(), "FFMpeg error: (%1) %2", cache_entry->m_cache_info.m_averror, ffmpeg_geterror(cache_entry->m_cache_info.m_averror).c_str());
            }
        }
    }

    cache->close(&cache_entry, timeout ? CLOSE_CACHE_DELETE : CLOSE_CACHE_NOOPT);

    delete thread_data;

    errno = syserror;
}

#ifndef USING_LIBAV
void ffmpeg_log(void *ptr, int level, const char *fmt, va_list vl)
{
    va_list vl2;
    Logging::level ffmpegfs_level = ERROR;
    static int print_prefix = 1;

#if (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 23, 0))
    char * line;
    int line_size;

    va_copy(vl2, vl);
    av_log_default_callback(ptr, level, fmt, vl);
    line_size = av_log_format_line2(ptr, level, fmt, vl2, nullptr, 0, &print_prefix);
    if (line_size < 0)
    {
        return;
    }
    line = static_cast<char *>(av_malloc(static_cast<size_t>(line_size)));
    if (line == nullptr)
    {
        return;
    }
    av_log_format_line2(ptr, level, fmt, vl2, line, line_size, &print_prefix);
    va_end(vl2);
#else
    char line[1024];

    va_copy(vl2, vl);
    av_log_default_callback(ptr, level, fmt, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);
#endif

    // Map log level
    // AV_LOG_PANIC     0
    // AV_LOG_FATAL     8
    // AV_LOG_ERROR    16
    if (level <= AV_LOG_ERROR)
    {
        ffmpegfs_level = ERROR;
    }
    // AV_LOG_WARNING  24
    else if (level <= AV_LOG_WARNING)
    {
        ffmpegfs_level = WARNING;
    }
#ifdef AV_LOG_TRACE
    // AV_LOG_INFO     32
    else if (level <= AV_LOG_INFO)
    {
        ffmpegfs_level = DEBUG;
    }
    // AV_LOG_VERBOSE  40
    // AV_LOG_DEBUG    48
    // AV_LOG_TRACE    56
    else   // if (level <= AV_LOG_TRACE)
    {
        ffmpegfs_level = TRACE;
    }
#else
    // AV_LOG_INFO     32
    else   // if (level <= AV_LOG_INFO)
    {
        ffmpegfs_level = DEBUG;
    }
#endif

    Logging::log_with_level(ffmpegfs_level, "", line);

#if (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 23, 0))
    av_free(line);
#endif
}
#endif

bool init_logging(const std::string &logfile, const std::string & max_level, bool to_stderr, bool to_syslog)
{
    static const std::map<std::string, Logging::level, comp> level_map =
    {
        { "ERROR",      ERROR },
        { "WARNING",    WARNING },
        { "INFO",       INFO },
        { "DEBUG",      DEBUG },
        { "TRACE",      TRACE },
    };

    auto it = level_map.find(max_level);

    if (it == level_map.end())
    {
        std::fprintf(stderr, "Invalid logging level string: %s\n", max_level.c_str());
        return false;
    }

    return Logging::init_logging(logfile, it->second, to_stderr, to_syslog);
}
