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
    size_t end = offset + len;
    bool success = true;

    if (cache_entry->m_cache_info.m_finished || cache_entry->m_buffer->tell() >= end)
    {
        return true;
    }

    // Wait until decoder thread has reached the desired position
    if (cache_entry->m_is_decoding)
    {
        while (!cache_entry->m_cache_info.m_finished && !cache_entry->m_cache_info.m_error && cache_entry->m_buffer->tell() < end)
        {
            sleep(0);
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

    ffmpegfs_debug(transcoder->destname(), "Finishing file.");

    if (!cache_entry->m_buffer->reserve(cache_entry->m_cache_info.m_encoded_filesize))
    {
        ffmpegfs_debug(transcoder->destname(), "Unable to truncate buffer.");
    }

    ffmpegfs_debug(transcoder->destname(), "Predicted/final size: %zu/%zu bytes, diff: %zi (%.1f%%).", cache_entry->m_cache_info.m_predicted_filesize, cache_entry->m_cache_info.m_encoded_filesize, cache_entry->m_cache_info.m_encoded_filesize - cache_entry->m_cache_info.m_predicted_filesize, (double)(long)((cache_entry->m_cache_info.m_encoded_filesize * 1000 / (cache_entry->m_cache_info.m_predicted_filesize + 1)) + 5) / 10);

    cache_entry->flush();

    return 0;
}

// Use "C" linkage to allow access from C code.
extern "C" {

void transcoder_cache_path(char *dir, size_t size)
{
    if (params.m_cachepath != NULL)
    {
        *dir = 0;
        strncat(dir, params.m_cachepath, size - 1);
    }
    else
    {
        tempdir(dir, size);
    }

    if (*dir && *(dir + strlen(dir)) != '/')
    {
        strncat(dir, "/", size - 1);
    }
    strncat(dir, PACKAGE, size - 1);
}

int transcoder_init(void)
{
    if (cache == NULL)
    {
        ffmpegfs_debug(NULL, "Creating media file cache.");
        cache = new Cache;
        if (cache == NULL)
        {
            ffmpegfs_error(NULL, "Unable to create media file cache. Out of memory.");
            fprintf(stderr, "ERROR: creating media file cache. Out of memory.\n");
            return -1;
        }

        if (!cache->load_index())
        {
            fprintf(stderr, "ERROR: creating media file cache.\n");
            return -1;
        }
    }
    return 0;
}

}

void transcoder_free(void)
{
    Cache *p1 = cache;
    cache = NULL;

    if (p1 != NULL)
    {
        ffmpegfs_debug(NULL, "Deleting media file cache.");
        delete p1;
    }
}

int transcoder_cached_filesize(const string & filename, struct stat *stbuf)
{
    ffmpegfs_trace(filename.c_str(), "Retrieving encoded size.");

    Cache_Entry* cache_entry = cache->open(filename);
    if (!cache_entry)
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
        stbuf->st_size = encoded_filesize;
        stbuf->st_blocks = (stbuf->st_size + 512 - 1) / 512;
        return true;
    }
    else
    {
        return false;
    }
}

// Allocate and initialize the transcoder

Cache_Entry* transcoder_new(LPCVIRTUALFILE virtualfile, bool begin_transcode)
{
    int _errno = 0;

    // Allocate transcoder structure
    ffmpegfs_trace(virtualfile->m_origfile.c_str(), "Creating transcoder object.");

    Cache_Entry* cache_entry = cache->open(virtualfile);
    if (!cache_entry)
    {
        return NULL;
    }

    cache_entry->lock();

    try
    {
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
                    ffmpegfs_warning(virtualfile->m_origfile.c_str(), "Too many active threads. Deferring transcoder start until threads become available.");

                    while (!thread_exit && thread_count >= params.m_max_threads)
                    {
                        sleep(0);
                    }

                    if (thread_count >= params.m_max_threads)
                    {
                        ffmpegfs_error(virtualfile->m_origfile.c_str(), "Unable to start new thread. Cancelling transcode.");
                        _errno = EBUSY; // Report resource busy
                        throw false;
                    }

                    ffmpegfs_info(virtualfile->m_origfile.c_str(), "Threads available again. Continuing now.");
                }

                pthread_attr_t attr;
                int stack_size = 0;
                int s;

                ffmpegfs_debug(virtualfile->m_origfile.c_str(), "Starting decoder thread.");

                if (cache_entry->m_cache_info.m_error)
                {
                    // If error occurred last time, clear cache
                    cache_entry->clear();
                }

                // Must decode the file, otherwise simply use cache
                cache_entry->m_is_decoding = true;

                // Initialise thread creation attributes
                s = pthread_attr_init(&attr);
                if (s != 0)
                {
                    _errno = s;
                    ffmpegfs_error(virtualfile->m_origfile.c_str(), "Error creating thread attributes: %s", strerror(s));
                    throw false;
                }

                if (stack_size > 0)
                {
                    s = pthread_attr_setstacksize(&attr, stack_size);
                    if (s != 0)
                    {
                        _errno = s;
                        ffmpegfs_error(virtualfile->m_origfile.c_str(), "Error setting stack size: %s", strerror(s));
                        pthread_attr_destroy(&attr);
                        throw false;
                    }
                }

                Thread_Data* thread_data = (Thread_Data*)malloc(sizeof(Thread_Data));

                thread_data->m_initialised = false;
                thread_data->m_arg = cache_entry;

                pthread_mutex_init(&thread_data->m_mutex, 0);
                pthread_cond_init (&thread_data->m_cond, 0);

                pthread_mutex_lock(&thread_data->m_mutex);
                s = pthread_create(&cache_entry->m_thread_id, &attr, &decoder_thread, thread_data);
                if (s == 0)
                {
                    pthread_cond_wait(&thread_data->m_cond, &thread_data->m_mutex);
                }
                pthread_mutex_unlock(&thread_data->m_mutex);

                ffmpegfs_debug(virtualfile->m_origfile.c_str(), "Decoder thread is running.");

                free(thread_data); // can safely be done here, will not be used in thread from now on

                if (s != 0)
                {
                    _errno = s;
                    ffmpegfs_error(virtualfile->m_origfile.c_str(), "Error creating thread: %s", strerror(s));
                    pthread_attr_destroy(&attr);
                    throw false;
                }

                // Destroy the thread attributes object, since it is no longer needed

                s = pthread_attr_destroy(&attr);
                if (s != 0)
                {
                    ffmpegfs_warning(virtualfile->m_origfile.c_str(), "Error destroying thread attributes: %s", strerror(s));
                }

                if (cache_entry->m_cache_info.m_error)
                {
                    ffmpegfs_debug(virtualfile->m_origfile.c_str(), "Decoder error!");
                    _errno = cache_entry->m_cache_info.m_errno;
                    if (!_errno)
                    {
                        _errno = EIO; // Must return anything...
                    }
                    throw false;
                }
            }
            else if (!cache_entry->m_cache_info.m_predicted_filesize)
            {
                FFMPEG_Transcoder *transcoder = new FFMPEG_Transcoder;

                if (transcoder->open_input_file(virtualfile) < 0)
                {
                    _errno = errno;
                    delete transcoder;
                    throw false;
                }

                cache_entry->m_cache_info.m_predicted_filesize = transcoder->predict_filesize();

                transcoder->close();

                ffmpegfs_debug(virtualfile->m_origfile.c_str(), "Predicted transcoded size of %zu bytes.", cache_entry->m_cache_info.m_predicted_filesize);

                delete transcoder;
            }
        }
        else if (begin_transcode)
        {
            string destname;
            ffmpegfs_debug(get_destname(&destname, cache_entry->filename()).c_str(), "Reading file from cache.");
        }

        cache_entry->unlock();
    }
    catch (bool /*_bSuccess*/)
    {
        cache_entry->m_is_decoding = false;
        cache_entry->unlock();
        cache->close(&cache_entry, CLOSE_CACHE_DELETE);
        errno = _errno; // Restore last errno
    }

    return cache_entry;
}

// Read some bytes into the internal buffer and into the given buffer.

ssize_t transcoder_read(Cache_Entry* cache_entry, char* buff, off_t offset, size_t len)
{
    ffmpegfs_trace(cache_entry->filename().c_str(), "Reading %zu bytes from offset %jd.", len, (intmax_t)offset);

    cache_entry->lock();

    // Store access time
    cache_entry->update_access();

    try
    {
        // If we are encoding to MP3 and the requested data overlaps the ID3v1 tag
        // at the end of the file, do not encode data first up to that position.
        // This optimizes the case where applications read the end of the file
        // first to read the ID3v1 tag.
        if (!cache_entry->m_cache_info.m_finished &&
                (size_t)offset > cache_entry->m_buffer->tell() &&
                offset + len > (cache_entry->size() - ID3V1_TAG_LENGTH) &&
                !strcmp(params.m_desttype, "mp3"))
        {

            memcpy(buff, &cache_entry->m_id3v1, ID3V1_TAG_LENGTH);

            errno = 0;

            throw (ssize_t)len;
        }

        // Set last access time
        cache_entry->m_cache_info.m_access_time = time(NULL);

        bool success = transcode_until(cache_entry, offset, len);

        if (!success)
        {
            errno = cache_entry->m_cache_info.m_errno ? cache_entry->m_cache_info.m_errno : EIO;
            throw (ssize_t)-1;
        }

        // truncate if we didn't actually get len
        if (cache_entry->m_buffer->buffer_watermark() < (size_t) offset)
        {
            len = 0;
        }
        else if (cache_entry->m_buffer->buffer_watermark() < offset + len)
        {
            len = cache_entry->m_buffer->buffer_watermark() - offset;
        }

        if (!cache_entry->m_buffer->copy((uint8_t*)buff, offset, len))
        {
            len = 0;
            // throw (ssize_t)-1;
        }

        if (cache_entry->m_cache_info.m_error)
        {
            errno = cache_entry->m_cache_info.m_errno ? cache_entry->m_cache_info.m_errno : EIO;
            throw (ssize_t)-1;
        }

        errno = 0;
    }
    catch (ssize_t _len)
    {
        len = _len;
    }

    cache_entry->unlock();

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
    if (cache != NULL)
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
    if (cache != NULL)
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
    Thread_Data *thread_data = (Thread_Data*)arg;
    Cache_Entry *cache_entry = (Cache_Entry *)thread_data->m_arg;
    FFMPEG_Transcoder *transcoder = new FFMPEG_Transcoder;
    int averror = 0;
    int syserror = 0;
    bool timeout = false;
    bool success = true;

    thread_count++;

    try
    {
        ffmpegfs_info(cache_entry->filename().c_str(), "Transcoding to %s.", params.m_desttype);

        if (transcoder == NULL)
        {
            ffmpegfs_error(cache_entry->filename().c_str(), "Out of memory creating transcoder.");
            throw ((int)ENOMEM);
        }

        if (!cache_entry->open())
        {
            throw ((int)errno);
        }

        averror = transcoder->open_input_file(cache_entry->virtualfile());
        if (averror < 0)
        {
            throw ((int)errno);
        }

        if (!cache->maintenance(transcoder->predict_filesize()))
        {
            throw ((int)errno);
        }

        averror = transcoder->open_output_file(cache_entry->m_buffer);
        if (averror < 0)
        {
            throw ((int)errno);
        }

        memcpy(&cache_entry->m_id3v1, transcoder->id3v1tag(), sizeof(ID3v1));

        thread_data->m_initialised = true;
        if (!params.m_prebuffer_size)
        {
            pthread_cond_signal(&thread_data->m_cond);  // signal that we are running
        }
        else
        {
            ffmpegfs_debug(cache_entry->filename().c_str(), "Pre-buffering up to %zu bytes.", params.m_prebuffer_size);
        }

        bool unlocked = false;

        while (!cache_entry->m_cache_info.m_finished && !(timeout = cache_entry->decode_timeout()) && !thread_exit)
        {
            int status = 0;
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
                ffmpegfs_debug(cache_entry->filename().c_str(), "Pre-buffer limit reached.");
                pthread_cond_signal(&thread_data->m_cond);  // signal that we are running
            }

            if (cache_entry->ref_count() <= 1 && cache_entry->suspend_timeout())
            {
                if (!unlocked && params.m_prebuffer_size)
                {
                    unlocked = true;
                    pthread_cond_signal(&thread_data->m_cond);  // signal that we are running
                }

                ffmpegfs_info(cache_entry->filename().c_str(), "Suspend timeout. Transcoding suspended after %zu seconds inactivity.", params.m_max_inactive_suspend);

                while (cache_entry->suspend_timeout() && !(timeout = cache_entry->decode_timeout()) && !thread_exit)
                {
                    sleep(1);
                }

                if (timeout)
                {
                    break;
                }

                ffmpegfs_info(cache_entry->filename().c_str(), "Transcoding resumed.");
            }
        }

        if (!unlocked && params.m_prebuffer_size)
        {
            ffmpegfs_debug(cache_entry->filename().c_str(), "File transcode complete, releasing buffer early: Size %zu.", cache_entry->m_buffer->buffer_watermark());
            pthread_cond_signal(&thread_data->m_cond);  // signal that we are running
        }
    }
    catch (int _syserror)
    {
        success = false;
        syserror = _syserror;
        cache_entry->m_is_decoding = false;
        cache_entry->m_cache_info.m_error = !success;
        cache_entry->m_cache_info.m_errno = success ? 0 : (syserror ? syserror : EIO);  // Preserve errno
        cache_entry->m_cache_info.m_averror = success ? 0 : averror;                    // Preserve averror

        pthread_cond_signal(&thread_data->m_cond);  // unlock main thread
        cache_entry->unlock();
    }

    transcoder->close();

    delete transcoder;

    if (timeout || thread_exit)
    {
        cache_entry->m_is_decoding = false;
        cache_entry->m_cache_info.m_finished = false;
        cache_entry->m_cache_info.m_error = true;
        cache_entry->m_cache_info.m_errno = EIO;        // Report I/O error
        cache_entry->m_cache_info.m_averror = averror;  // Preserve averror

        if (timeout)
        {
            ffmpegfs_error(cache_entry->filename().c_str(), "Timeout! Transcoding aborted after %zu seconds inactivity.", params.m_max_inactive_abort);
        }
        else
        {
            ffmpegfs_error(cache_entry->filename().c_str(), "Thread exit! Transcoding aborted.");
        }
    }
    else
    {
        cache_entry->m_is_decoding = false;
        cache_entry->m_cache_info.m_error = !success;
        cache_entry->m_cache_info.m_errno = success ? 0 : (syserror ? syserror : EIO);  // Preserve errno
        cache_entry->m_cache_info.m_averror = success ? 0 : averror;                    // Preserve averror

        if (success)
        {
            ffmpegfs_info(cache_entry->filename().c_str(), "Transcoding completed successfully.");
        }
        else
        {
            ffmpegfs_error(cache_entry->filename().c_str(), "Transcoding exited with error.\nSystem error: %s (%i)\nFFMpeg error: %s (%i)", strerror(syserror), syserror, ffmpeg_geterror(averror).c_str(), averror);
        }
    }

    cache_entry->m_thread_id = 0;

    cache->close(&cache_entry, timeout ? CLOSE_CACHE_DELETE : CLOSE_CACHE_NOOPT);

    thread_count--;

    return NULL;
}

void ffmpegfs_trace(const char *filename, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_with_level(TRACE, filename, format, args);
    va_end(args);
}

void ffmpegfs_debug(const char * filename, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_with_level(DEBUG, filename, format, args);
    va_end(args);
}

void ffmpegfs_info(const char *filename, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_with_level(INFO, filename, format, args);
    va_end(args);
}

void ffmpegfs_warning(const char *filename, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_with_level(WARNING, filename, format, args);
    va_end(args);
}

void ffmpegfs_error(const char *filename, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_with_level(ERROR, filename, format, args);
    va_end(args);
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

    log_with_level(ffmpegfs_level, "", NULL, line);
}
#endif

int init_logging(const char* logfile, const char* max_level, int to_stderr, int to_syslog)
{
    static const map<string, Logging::level> level_map =
    {
        { "ERROR", ERROR },
        { "WARNING", WARNING },
        { "INFO", INFO },
        { "DEBUG", DEBUG },
        { "TRACE", TRACE },
    };

    auto it = level_map.find(max_level);

    if (it == level_map.end())
    {
        fprintf(stderr, "Invalid logging level string: %s\n", max_level);
        return false;
    }

    return InitLogging(logfile, it->second, to_stderr, to_syslog);
}

static const map<string, PROFILE> profile_map =
{
    { "NONE", PROFILE_NONE },
	
	// MP4
	
    { "FF", PROFILE_MP4_FF },
    { "EDGE", PROFILE_MP4_EDGE },
    { "IE", PROFILE_MP4_IE },
    { "CHROME", PROFILE_MP4_CHROME },
    { "SAFARI", PROFILE_MP4_SAFARI },
    { "OPERA", PROFILE_MP4_OPERA },
    { "MAXTHON", PROFILE_MP4_MAXTHON }
};

int get_profile(const char * arg, PROFILE *value)
{
    const char * ptr = strchr(arg, '=');

    if (ptr)
    {
        ptr++;

        auto it = profile_map.find(ptr);

        if (it == profile_map.end())
        {
            fprintf(stderr, "Invalid profile: %s\n", ptr);
            return -1;
        }

        *value = it->second;

        return 0;
    }

    fprintf(stderr, "Missing profile string\n");

    return -1;
}

static map<string, PROFILE>::const_iterator search_by_value(const map<string, PROFILE> & mapOfWords, PROFILE value)
{
    // Iterate through all elements in map and search for the passed element
    map<string, PROFILE>::const_iterator it = mapOfWords.begin();
    while (it != mapOfWords.end())
    {
        if(it->second == value)
        {
            return it;
        }
        it++;
    }
    return mapOfWords.end();
}

const char * get_profile_text(PROFILE value)
{
    map<string, PROFILE>::const_iterator it = search_by_value(profile_map, value);
    if (it != profile_map.end())
    {
        return it->first.c_str();
    }
    return "INVALID";
}
