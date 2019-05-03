/*
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

/**
 * @file
 * @brief %Cache entry
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
 */
 
#ifndef CACHE_ENTRY_H
#define CACHE_ENTRY_H

#pragma once

#include "cache.h"

#include "id3v1tag.h"

class Buffer;

/**
 * @brief The #Cache_Entry class
 */
class Cache_Entry
{
private:
    /**
     * @brief Create Cache_Entry object.
     * @param[in] owner - Cache object of owner.
     * @param[in] virtualfile - Requesting virtual file.
     */
    explicit Cache_Entry(Cache *owner, LPVIRTUALFILE virtualfile);
    /**
     * @brief Destroy Cache_Entry object.
     */
    virtual ~Cache_Entry();

public:
    /**
     * @brief Create a new Cache_Entry object.
     * @param[in] owner - Cache object of owner.
     * @param[in] virtualfile - Requesting virtual file.
     * @return On success, returns a Cache_Entry object; on error (out of memory) returns a nullptr
     */
    static Cache_Entry *    create(Cache *owner, LPVIRTUALFILE virtualfile);
    /**
     * @brief Destroy this Cache_Entry object.
     * @return true if object was destroyed right away; false if it will be destroyed later (NOT IMPLEMENTED, WILL BE DESTROYED AT ONCE).
     */
    bool                    destroy();

    /**
     * @brief Open the cache file.
     * @param[in] create_cache - If true, the cache will be created if it does not yet exist.
     * @return On success returns true; on error returns false and errno contains the error code.
     */
    bool                    open(bool create_cache = true);
    /**
     * @brief Flush current memory cache to disk.
     * @return On success returns true; on error returns false and errno contains the error code.
     */
    bool                    flush();
    /**
     * @brief Clear the cache entry
     * @param[in] fetch_file_time - If true, the entry file time will be filled in from the source file.
     */
    void                    clear(bool fetch_file_time = true);
    /** @brief Return size of output file, as computed by encoder.
     *
     * Returns the file size, either the predicted size (which may be inaccurate) or
     * the real size (which is only available once the file was completely recoded).
     *
     *  @return The size of the file. Function never fails.
     */
    size_t                  size() const;
    /**
     * @brief Get the age of the cache entry.
     * @return Returns the age of the cache entry in seconds since epoch.
     */
    time_t                  age() const;
    /**
     * @brief Get last access time.
     * @return Returns last access time in seconds since epoch.
     */
    time_t                  last_access() const;
    /**
     * @brief Check if cache entry expired.
     *
     * Checks if entry is older or larger than the limit.
     *
     * @return If entry is expired, returns true.
     */
    bool                    expired() const;
    /**
     * @brief Check for decode suspend timeout.
     * @return Returns true if decoding was suspended.
     */
    bool                    suspend_timeout() const;
    /**
     * @brief Check for decode timeout.
     * @return Returns true if decoding timed out.
     */
    bool                    decode_timeout() const;
    /**
     * @brief Get the name of the source file.
     * @return Returns the name of the source file.
     */
    const std::string &     filename() const;
    /**
     * @brief Get the name of the transcoded file.
     * @return Returns the name of the transcoded file.
     */
    const std::string &     destname() const;
    /**
     * @brief Update last access time.
     * @param[in] update_database - If true, also persist in SQL database.
     * @return If update was successful, returns true; returns false on error.
     */
    bool                    update_access(bool update_database = false);
    /**
     * @brief Lock the access mutex.
     */
    void                    lock();
    /**
     * @brief Unlock the access mutex.
     */
    void                    unlock();
    /**
     * @brief Get the current reference counter.
     * @return Returns the current reference counter.
     */
    int                     ref_count() const;

    /**
     * @brief Check if cache entry needs to be recoded
     */
    bool                    outdated() const;

    /**
     * @brief Get the underlying VIRTUALFILE object.
     * @return Return the underlying VIRTUALFILE object.
     */
    LPVIRTUALFILE           virtualfile();

    /** @brief Close the cache entry
     *  @param[in] flags - one of the CLOSE_CACHE_* flags
     *  @return Returns true if entry may be deleted, false if still in use.
     */
    bool                    close(int flags);
    /**
     * @brief Update read counter.
     */
    void                    update_read_count();
    /**
     * @brief Get read counter.
     *
     * This is the number of read accesses to the cache entry.
     *
     * @return Returns current read counter
     */
    unsigned int            read_count() const;

protected:
    /**
     * @brief Close buffer object.
     *  @param[in] flags - one of the CLOSE_CACHE_* flags
     */
    void                    close_buffer(int flags);
    /**
     * @brief Read cache info.
     * @return On success, returns true; returns false on error.
     */
    bool                    read_info();
    /**
     * @brief Write cache info.
     * @return On success, returns true; returns false on error.
     */
    bool                    write_info();
    /**
     * @brief Delete cache info.
     * @return On success, returns true; returns false on error.
     */
    bool                    delete_info();

protected:
    Cache *                 m_owner;                        /**< @brief Owner cache object */
    std::recursive_mutex    m_mutex;                        /**< @brief Access mutex */

    int                     m_ref_count;                    /**< @brief Reference counter */

    LPVIRTUALFILE           m_virtualfile;                  /**< @brief Underlying virtual file object */

public:
    Buffer *                m_buffer;                       /**< @brief Buffer object */
    bool                    m_is_decoding;                  /**< @brief true while file is decoding */
    pthread_t               m_thread_id;                    /**< @brief ID if decoder thread if currently decoding */

    CACHE_INFO              m_cache_info;                   /**< @brief Info about cached object */

    ID3v1                   m_id3v1;                        /**< @brief ID3v1 structure which is used to send to clients */
};

#endif // CACHE_ENTRY_H
