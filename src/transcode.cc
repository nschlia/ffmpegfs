/*
 * FileTranscoder interface for ffmpegfs
 *
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2013 K. Henriksson
 * Copyright (C) 2017-2018 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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

#include "transcode.h"
#include "ffmpegfs.h"
#include "ffmpeg_transcoder.h"
#include "buffer.h"
#include "cache.h"
#include "logging.h"
#include "cache_entry.h"

#include <unistd.h>

typedef struct tagThread_Data
{
    pthread_mutex_t  m_mutex;
    pthread_cond_t   m_cond;
    bool             m_initialised;
    void *           m_arg;
} Thread_Data;

static Cache *cache;
static volatile bool thread_exit;
static volatile unsigned int thread_count;

extern "C" {
static void *decoder_thread(void *arg);
}

static int transcode_finish(Cache_Entry* cache_entry, FFMPEG_Transcoder *transcoder);

// Transcode the buffer until the buffer has enough or until an error occurs.
// The buffer needs at least 'end' bytes before transcoding stops. Returns true
// if no errors and false otherwise.
static bool transcode_until(Cache_Entry* cache_entry, off_t offset, size_t len)
{
    size_t end = static_cast<size_t>(offset) + len;
    bool success = true;

    if (cache_entry->m_cache_info.m_finished || cache_entry->m_buffer->tell() >= end)
    {
        return true;
    }

    // Wait until decoder thread has reached the desired position
    if (cache_entry->m_is_decoding)
    {
        bool reported = false;
        while (!cache_entry->m_cache_info.m_finished && !cache_entry->m_cache_info.m_error && cache_entry->m_buffer->tell() < end)
        {
            if (fuse_interrupted())
        	{
                Logging::info(cache_entry->filename(), "Client has gone away.");
            	return false;
        	}

            if (thread_exit)
            {
                Logging::warning(cache_entry->filename(), "Received thread exit.");
                return false;
            }

            if (!reported)
            {
                Logging::trace(cache_entry->filename(), "Cache miss at offset %<%11u>1 (length %<%6u>2), remaining %<%11i>3.", offset, len, static_cast<ssize_t>(cache_entry->m_buffer->size()) - end);
                reported = true;
            }
            sleep(0);
        }

        if (reported)
        {
            Logging::trace(cache_entry->filename(), "Cache hit  at offset %<%11u>1 (length %<%6u>2), remaining %<%11i>3.", offset, len, static_cast<ssize_t>(cache_entry->m_buffer->size()) - end);
        }
        success = !cache_entry->m_cache_info.m_error;
    }
    return success;
}

// Close the input file and free everything but the initial buffer.

static int transcode_finish(Cache_Entry* cache_entry, FFMPEG_Transcoder *transcoder)
{
    int res = transcoder->encode_finish();
    if (res < 0)
    {
        return res;
    }

    // Check encoded buffer size.
    cache_entry->m_cache_info.m_encoded_filesize = cache_entry->m_buffer->buffer_watermark();
    cache_entry->m_cache_info.m_finished = true;
    cache_entry->m_is_decoding = false;
    cache_entry->m_cache_info.m_errno = 0;
    cache_entry->m_cache_info.m_averror = 0;

    Logging::debug(transcoder->destname(), "Finishing file.");

    if (!cache_entry->m_buffer->reserve(cache_entry->m_cache_info.m_encoded_filesize))
    {
        Logging::debug(transcoder->destname(), "Unable to truncate buffer.");
    }

    Logging::debug(transcoder->destname(), "Predicted/final size: %1/%2 bytes, diff: %3 (%4%).", cache_entry->m_cache_info.m_predicted_filesize, cache_entry->m_cache_info.m_encoded_filesize, static_cast<int64_t>(cache_entry->m_cache_info.m_encoded_filesize) - static_cast<int64_t>(cache_entry->m_cache_info.m_predicted_filesize), static_cast<double>((cache_entry->m_cache_info.m_encoded_filesize * 1000 / (cache_entry->m_cache_info.m_predicted_filesize + 1)) + 5) / 10);

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
}

int transcoder_init(void)
{
    if (cache == nullptr)
    {
        Logging::debug(nullptr, "Creating media file cache.");
        cache = new Cache;
        if (cache == nullptr)
        {
            Logging::error(nullptr, "Unable to create media file cache. Out of memory.");
            std::fprintf(stderr, "ERROR: Creating media file cache. Out of memory.\n");
            return -1;
        }

        if (!cache->load_index())
        {
            std::fprintf(stderr, "ERROR: Creating media file cache.\n");
            return -1;
        }
    }
    return 0;
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

int transcoder_cached_filesize(LPVIRTUALFILE virtualfile, struct stat *stbuf)
{
    Logging::trace(virtualfile->m_origfile, "Retrieving encoded size.");

    Cache_Entry* cache_entry = cache->open(virtualfile);
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
        stbuf->st_size = static_cast<off_t>(encoded_filesize);
        stbuf->st_blocks = (stbuf->st_size + 512 - 1) / 512;
        return true;
    }
    else
    {
        return false;
    }
}


// Set the file size
bool transcoder_set_filesize(LPVIRTUALFILE virtualfile, double duration, BITRATE audio_bit_rate, int channels, int sample_rate, BITRATE video_bit_rate, int width, int height, int interleaved, double frame_rate)
{
    Cache_Entry * cache_entry = cache->open(virtualfile);
    if (cache_entry == nullptr)
    {
        Logging::error(virtualfile->m_origfile, "Out of memory getting file size.");
        return false;
    }

    ffmpegfs_format *current_format = params.current_format(virtualfile);
    if (current_format == nullptr)
    {
        Logging::error(virtualfile->m_origfile, "Internal error getting file size.");
        return false;
    }

    size_t filesize = 0;

    if (!FFMPEG_Transcoder::audio_size(&filesize, current_format->m_audio_codec_id, audio_bit_rate, duration, channels, sample_rate))
    {
        Logging::warning(virtualfile->m_origfile, "Internal error - unsupported audio codec '%1' for format %2.", get_codec_name(current_format->m_audio_codec_id, 0), current_format->m_desttype);
    }

    if (!FFMPEG_Transcoder::video_size(&filesize, current_format->m_video_codec_id, video_bit_rate, duration, width, height, interleaved, frame_rate))
    {
        Logging::warning(virtualfile->m_origfile, "Internal error - unsupported video codec '%1' for format %2.", get_codec_name(current_format->m_video_codec_id, 0), current_format->m_desttype);
    }

    cache_entry->m_cache_info.m_predicted_filesize = filesize;

    Logging::debug(virtualfile->m_origfile, "Predicted transcoded size of %1 bytes.", cache_entry->m_cache_info.m_predicted_filesize);

    return true;
}

// Predict file size
bool transcoder_predict_filesize(LPVIRTUALFILE virtualfile, Cache_Entry* cache_entry)
{
    FFMPEG_Transcoder *transcoder = new FFMPEG_Transcoder;
    bool success = false;

    if (transcoder == nullptr)
    {
        Logging::error(virtualfile->m_origfile, "Out of memory getting file size.");
        return false;
    }

    if (transcoder->open_input_file(virtualfile) >= 0)
    {
        cache_entry->m_cache_info.m_predicted_filesize = transcoder->predicted_filesize();

        transcoder->close();

        Logging::debug(virtualfile->m_origfile, "Predicted transcoded size of %1 bytes.", cache_entry->m_cache_info.m_predicted_filesize);

        success = true;
    }

    delete transcoder;

    return success;
}

// Allocate and initialize the transcoder
Cache_Entry* transcoder_new(LPVIRTUALFILE virtualfile, bool begin_transcode)
{
    int _errno = 0;

    // Allocate transcoder structure
    Logging::trace(virtualfile->m_origfile, "Creating transcoder object.");

    Cache_Entry* cache_entry = cache->open(virtualfile);
    if (cache_entry == nullptr)
    {
        return nullptr;
    }

    try
    {
        cache_entry->lock();

        if (!cache_entry->open(begin_transcode))
        {
            _errno = errno;
            throw false;
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

        if (!cache_entry->m_is_decoding && !cache_entry->m_cache_info.m_finished)
        {
            if (begin_transcode)
            {
                if (params.m_max_threads && thread_count >= params.m_max_threads)
                {
                    Logging::warning(virtualfile->m_origfile, "Too many active threads. Deferring transcoder start until threads become available.");

                    while (!thread_exit && thread_count >= params.m_max_threads)
                    {
                        sleep(0);
                    }

                    if (thread_count >= params.m_max_threads)
                    {
                        Logging::error(virtualfile->m_origfile, "Unable to start new thread. Cancelling transcode.");
                        _errno = EBUSY; // Report resource busy
                        throw false;
                    }

                    Logging::info(virtualfile->m_origfile, "Threads available again. Continuing now.");
                }

                pthread_attr_t attr;
                size_t stack_size = 0;
                int ret;

                Logging::debug(virtualfile->m_origfile, "Starting decoder thread.");

                if (cache_entry->m_cache_info.m_error)
                {
                    // If error occurred last time, clear cache
                    cache_entry->clear();
                }

                // Must decode the file, otherwise simply use cache
                cache_entry->m_is_decoding = true;

                // Initialise thread creation attributes
                ret = pthread_attr_init(&attr);
                if (ret != 0)
                {
                    _errno = ret;
                    Logging::error(virtualfile->m_origfile, "Error creating thread attributes: %1", strerror(ret));
                    throw false;
                }

                if (stack_size > 0)
                {
                    ret = pthread_attr_setstacksize(&attr, stack_size);
                    if (ret != 0)
                    {
                        _errno = ret;
                        Logging::error(virtualfile->m_origfile, "Error setting stack size: %1", strerror(ret));
                        pthread_attr_destroy(&attr);
                        throw false;
                    }
                }

                // Make thread joinable
                ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
                if (ret != 0)
                {
                    _errno = ret;
                    Logging::error(virtualfile->m_origfile, "Cannot make thread joinable: %1", strerror(ret));
                    pthread_attr_destroy(&attr);
                    throw false;
                }

                //ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
                //if (ret != 0)
                //{
                //  _errno = ret;
                //  Logging::error(virtualfile->m_origfile, "Error setting thread detached state: %1", strerror(ret));
                //  throw false;
                //}

                Thread_Data* thread_data = static_cast<Thread_Data*>(malloc(sizeof(Thread_Data)));  // TODO: replace with new/delete

                thread_data->m_initialised = false;
                thread_data->m_arg = cache_entry;

                pthread_mutex_init(&thread_data->m_mutex, nullptr);
                pthread_cond_init (&thread_data->m_cond, nullptr);

                pthread_mutex_lock(&thread_data->m_mutex);
                ret = pthread_create(&cache_entry->m_thread_id, &attr, &decoder_thread, thread_data);
                if (ret == 0)
                {
                    pthread_cond_wait(&thread_data->m_cond, &thread_data->m_mutex);
                }
                pthread_mutex_unlock(&thread_data->m_mutex);

                Logging::debug(virtualfile->m_origfile, "Decoder thread is running.");

                free(thread_data); // can safely be done here, will not be used in thread from now on

                if (ret != 0)
                {
                    _errno = ret;
                    Logging::error(virtualfile->m_origfile, "Error creating thread: %1", strerror(ret));
                    pthread_attr_destroy(&attr);
                    throw false;
                }

                // Destroy the thread attributes object, since it is no longer needed

                ret = pthread_attr_destroy(&attr);
                if (ret != 0)
                {
                    Logging::warning(virtualfile->m_origfile, "Error destroying thread attributes: %1", strerror(ret));
                }

                if (cache_entry->m_cache_info.m_error)
                {
                    Logging::debug(virtualfile->m_origfile, "Decoder error!");
                    _errno = cache_entry->m_cache_info.m_errno;
                    if (!_errno)
                    {
                        _errno = EIO; // Must return anything...
                        averr2errno(&_errno, cache_entry->m_cache_info.m_averror);
                    }
                    throw false;
                }
            }
            else if (!cache_entry->m_cache_info.m_predicted_filesize)
            {
                if (!transcoder_predict_filesize(virtualfile, cache_entry))
                {
                    _errno = errno;
                    throw false;
                }
            }
        }
        else if (begin_transcode)
        {
            std::string destname;
            Logging::debug(get_destname(&destname, cache_entry->filename()), "Reading file from cache.");
        }

        cache_entry->unlock();
    }
    catch (bool /*_bSuccess*/)
    {
        cache_entry->m_is_decoding = false;
        cache_entry->unlock();
        cache->close(&cache_entry, CLOSE_CACHE_DELETE);
        cache_entry = nullptr;  // Make sure to return NULL here even if the cache could not be deleted now (still in use)
        errno = _errno;         // Restore last errno
    }

    return cache_entry;
}

// Read some bytes into the internal buffer and into the given buffer.

ssize_t transcoder_read(Cache_Entry* cache_entry, char* buff, off_t offset, size_t len)
{
    Logging::trace(cache_entry->filename(), "Reading %1 bytes from offset %2.", len, static_cast<intmax_t>(offset));

    // Store access time
    cache_entry->update_access();

    try
    {
        if (!cache_entry->m_cache_info.m_finished)
        {
            switch (params.current_format(cache_entry->virtualfile())->m_filetype)
            {
            case FILETYPE_MP3:
            {
                // If we are encoding to MP3 and the requested data overlaps the ID3v1 tag
                // at the end of the file, do not encode data first up to that position.
                // This optimises the case where applications read the end of the file
                // first to read the ID3v1 tag.
                if ((static_cast<size_t>(offset) > cache_entry->m_buffer->tell()) &&
                        ((offset + len) > (cache_entry->size() - ID3V1_TAG_LENGTH)))
                {

                    memcpy(buff, &cache_entry->m_id3v1, ID3V1_TAG_LENGTH);

                    errno = 0;

                    throw static_cast<ssize_t>(len);
                }
                break;
            }
            default:
            {
                break;
            }
            }
        }

        // Set last access time
        cache_entry->m_cache_info.m_access_time = time(nullptr);

        bool success = transcode_until(cache_entry, offset, len);

        if (!success)
        {
            errno = cache_entry->m_cache_info.m_errno ? cache_entry->m_cache_info.m_errno : EIO;
            throw static_cast<ssize_t>(-1);
        }

        // truncate if we didn't actually get len
        if (cache_entry->m_buffer->buffer_watermark() < static_cast<size_t>(offset))
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
            // throw static_cast<ssize_t>(-1);
        }

        if (cache_entry->m_cache_info.m_error)
        {
            errno = cache_entry->m_cache_info.m_errno ? cache_entry->m_cache_info.m_errno : EIO;
            throw static_cast<ssize_t>(-1);
        }

        errno = 0;
    }
    catch (ssize_t _len)
    {
        len = _len;
    }

    return len;
}

// Free the transcoder structure.

void transcoder_delete(Cache_Entry* cache_entry)
{
    cache->close(&cache_entry);
}

// Return size of output file, as computed by Encoder.
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

int transcoder_cache_maintenance(void)
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

int transcoder_cache_clear(void)
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

static void *decoder_thread(void *arg)
{
    Thread_Data *thread_data = static_cast<Thread_Data*>(arg);
    Cache_Entry *cache_entry = static_cast<Cache_Entry *>(thread_data->m_arg);
    FFMPEG_Transcoder *transcoder = new FFMPEG_Transcoder;
    int averror = 0;
    int syserror = 0;
    bool timeout = false;
    bool success = true;

    thread_count++;

    try
    {
        Logging::info(cache_entry->filename(), "Transcoding to %1.", params.current_format(cache_entry->virtualfile())->m_desttype);

        if (transcoder == nullptr)
        {
            Logging::error(cache_entry->filename(), "Out of memory creating transcoder.");
            throw (static_cast<int>(ENOMEM));
        }

        if (!cache_entry->open())
        {
            throw (static_cast<int>(errno));
        }

        averror = transcoder->open_input_file(cache_entry->virtualfile());
        if (averror < 0)
        {
            throw (static_cast<int>(errno));
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
        if (!params.m_prebuffer_size)
        {
            pthread_cond_signal(&thread_data->m_cond);  // signal that we are running
        }
        else
        {
            Logging::debug(cache_entry->filename(), "Pre-buffering up to %1 bytes.", params.m_prebuffer_size);
        }

        bool unlocked = false;

        while (!cache_entry->m_cache_info.m_finished && !(timeout = cache_entry->decode_timeout()) && !thread_exit)
        {
            int status = 0;

            if (cache_entry->ref_count() > 1)
            {
                // Set last access time
                cache_entry->update_access(false);
            }

            averror = transcoder->process_single_fr(status);
            if (status < 0)
            {
                syserror = EIO;
                averr2errno(&syserror, averror);
                success = false;
                break;
            }

            if (status == 1 && ((averror = transcode_finish(cache_entry, transcoder)) < 0))
            {
                syserror = EIO;
                averr2errno(&syserror, averror);
                success = false;
                break;
            }

            if (!unlocked && cache_entry->m_buffer->buffer_watermark() > params.m_prebuffer_size)
            {
                unlocked = true;
                Logging::debug(cache_entry->filename(), "Pre-buffer limit reached.");
                pthread_cond_signal(&thread_data->m_cond);  // signal that we are running
            }

            if (cache_entry->ref_count() <= 1 && cache_entry->suspend_timeout())
            {
                if (!unlocked && params.m_prebuffer_size)
                {
                    unlocked = true;
                    pthread_cond_signal(&thread_data->m_cond);  // signal that we are running
                }

                Logging::info(cache_entry->filename(), "Suspend timeout. Transcoding suspended after %1 seconds inactivity.", params.m_max_inactive_suspend);

                while (cache_entry->suspend_timeout() && !(timeout = cache_entry->decode_timeout()) && !thread_exit)
                {
                    sleep(1);
                }

                if (timeout)
                {
                    break;
                }

                Logging::info(cache_entry->filename(), "Transcoding resumed.");
            }
        }

        if (!unlocked && params.m_prebuffer_size)
        {
            Logging::debug(cache_entry->filename(), "File transcode complete, releasing buffer early: Size %1.", cache_entry->m_buffer->buffer_watermark());
            pthread_cond_signal(&thread_data->m_cond);  // signal that we are running
        }
    }
    catch (int _syserror)
    {
        success = false;
        syserror = _syserror;
        if (!syserror)
        {
            averr2errno(&syserror, averror);
        }
        cache_entry->m_is_decoding              = false;
        cache_entry->m_cache_info.m_error       = !success;
        cache_entry->m_cache_info.m_errno       = success ? 0 : (syserror ? syserror : EIO);    // Preserve errno
        cache_entry->m_cache_info.m_averror     = success ? 0 : averror;                        // Preserve averror

        pthread_cond_signal(&thread_data->m_cond);  // unlock main thread
    }

    transcoder->close();

    delete transcoder;

    if (timeout || thread_exit)
    {
        cache_entry->m_is_decoding              = false;
        cache_entry->m_cache_info.m_finished    = false;
        cache_entry->m_cache_info.m_error       = true;
        cache_entry->m_cache_info.m_errno       = EIO;      // Report I/O error
        cache_entry->m_cache_info.m_averror     = averror;  // Preserve averror

        if (timeout)
        {
            Logging::warning(cache_entry->filename(), "Timeout! Transcoding aborted after %1 seconds inactivity.", params.m_max_inactive_abort);
        }
        else
        {
            Logging::info(cache_entry->filename(), "Thread exit! Transcoding aborted.");
        }
    }
    else
    {
        if (!success && !syserror)
        {
            // If syserror is 0 check if averror is a valid system errno value
            averr2errno(&syserror, averror);
        }

        cache_entry->m_is_decoding = false;
        cache_entry->m_cache_info.m_error   = !success;
        cache_entry->m_cache_info.m_errno   = success ? 0 : (syserror ? syserror : EIO);    // Preserve errno
        cache_entry->m_cache_info.m_averror = success ? 0 : averror;                        // Preserve averror

        if (success)
        {
            Logging::info(cache_entry->filename(), "Transcoding completed successfully.");
        }
        else
        {
            Logging::error(cache_entry->filename(), "Transcoding exited with error.");
            Logging::error(cache_entry->filename(), "System error: %1 (%2)", strerror(cache_entry->m_cache_info.m_errno), cache_entry->m_cache_info.m_errno);
            Logging::error(cache_entry->filename(), "FFMpeg error: %1 (%2)", ffmpeg_geterror(cache_entry->m_cache_info.m_averror), cache_entry->m_cache_info.m_averror);
        }
    }

    cache_entry->m_thread_id = 0;

    cache->close(&cache_entry, timeout ? CLOSE_CACHE_DELETE : CLOSE_CACHE_NOOPT);

    thread_count--;

    errno = syserror;

    return nullptr;
}

#ifndef USING_LIBAV
void ffmpeg_log(void *ptr, int level, const char *fmt, va_list vl)
{
    va_list vl2;
    char line[1024];
    Logging::level ffmpegfs_level = ERROR;
    static int print_prefix = 1;

    va_copy(vl2, vl);
    av_log_default_callback(ptr, level, fmt, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);

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
}
#endif

int init_logging(const std::string &logfile, const std::string & max_level, int to_stderr, int to_syslog)
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
