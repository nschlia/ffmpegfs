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
 * @brief Data cache management
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef CACHE_H
#define CACHE_H

#pragma once

#include "buffer.h"

#include <map>
#include <sqlite3.h>
/**
  * @brief Cache information block
  */
typedef struct CACHE_INFO
{
    std::string     m_origfile;                 /**< @brief Original filename before transcode */
    std::string     m_destfile;                 /**< @brief Destination filename after transcode */
    char            m_desttype[11];             /**< @brief Destination type */
    int64_t         m_audiobitrate;             /**< @brief Audio bitrate in bit/s */
    int             m_audiosamplerate;          /**< @brief Audio sample rate in Hz */
    int64_t         m_videobitrate;             /**< @brief Video bitrate in bit/s */
    int             m_videowidth;               /**< @brief Video width */
    int             m_videoheight;              /**< @brief Video height */
    bool            m_deinterlace;              /**< @brief true if video was deinterlaced */
    size_t          m_predicted_filesize;       /**< @brief Predicted file size */
    size_t          m_encoded_filesize;         /**< @brief Actual file size after encode */
    bool            m_finished;                 /**< @brief true if decode has finished */
    bool            m_error;                    /**< @brief true if encode failed */
    int             m_errno;                    /**< @brief errno if encode failed */
    int             m_averror;                  /**< @brief FFmpeg error code if encode failed */
    time_t          m_creation_time;            /**< @brief Source file creation time */
    time_t          m_access_time;              /**< @brief Source file last access time */
    time_t          m_file_time;                /**< @brief Source file file time */
    size_t          m_file_size;                /**< @brief Source file file size */
    unsigned int    m_access_count;             /**< @brief Read access counter */
} CACHE_INFO;
typedef CACHE_INFO const *LPCCACHE_INFO;        /**< @brief Pointer version of CACHE_INFO */
typedef CACHE_INFO *LPCACHE_INFO;               /**< @brief Pointer to const version of CACHE_INFO */

class Cache_Entry;

/**
 * @brief The #Cache class
 */
class Cache
{
    typedef std::pair<std::string, std::string> cache_key_t;
    typedef std::map<cache_key_t, Cache_Entry *> cache_t;

    friend class Cache_Entry;

public:
    /**
     * @brief Construct #Cache object.
     */
    Cache();
    /**
     * @brief Destruct #Cache object.
     */
    virtual ~Cache();
    /**
     * @brief Open cache entry.
     *
     * Opens a cache entry and opens the cache file.
     *
     * @param[in] virtualfile - virtualfile struct of a file.
     * @return On success, returns pointer to a Cache_Entry. On error, returns nullptr.
     */
    Cache_Entry *           open(LPVIRTUALFILE virtualfile);
    /**
     * @brief Close a cache entry.
     *
     * If the cache entry is in use will not be deleted.
     *
     * @param[in, out] cache_entry - Cache entry object to be closed.
     * @param[in] flags - One of the CLOSE_CACHE_* flags.
     * @return Returns true if the object was deleted; false if not.
     */
    bool                    close(Cache_Entry **cache_entry, int flags = CLOSE_CACHE_NOOPT);
    /**
     * @brief Load cache index from disk.
     * @return Returns true on success; false on error.
     */
    bool                    load_index();
#ifdef HAVE_SQLITE_CACHEFLUSH
    /**
     * @brief Flush cache index to disk.
     * @return Returns true on success; false on error.
     */
    bool                    flush_index();
#endif // HAVE_SQLITE_CACHEFLUSH
    /**
     * @brief Run disk maintenance.
     *
     * Can be done before a new file is added. Set predicted_filesize to make sure disk space
     * or cache size will be kept within limits.
     *
     * @param[in] predicted_filesize - Size of new file
     * @return Returns true on success; false on error.
     */
    bool                    maintenance(size_t predicted_filesize = 0);
    /**
     * @brief Clear cache: deletes all entries.
     * @return Returns true on success; false on error.
     */
    bool                    clear();
    /**
     * @brief Prune expired cache entries.
     * @return Returns true on success; false on error.
     */
    bool                    prune_expired();
    /**
     * @brief Prune cache entries to keep cache size within limit.
     * @return Returns true on success; false on error.
     */
    bool                    prune_cache_size();
    /**
     * @brief Prune cache entries to ensure disk space.
     * @return Returns true on success; false on error.
     */
    bool                    prune_disk_space(size_t predicted_filesize);
    /**
     * @brief Remove a cache file from disk.
     * @param[in] filename - Source file name.
     * @param[in] desttype - Destination type (MP4, WEBM etc.).
     * @return Returns true on success; false on error.
     */
    bool                    remove_cachefile(const std::string & filename, const std::string &desttype);
    
protected:
    /**
     * @brief Read cache file info.
     * @param[in] cache_info - Structure with cache info data.
     * @return Returns true on success; false on error.
     */
    bool                    read_info(LPCACHE_INFO cache_info);
    /**
     * @brief Write cache file info.
     * @param[in] cache_info - Structure with cache info data.
     * @return Returns true on success; false on error.
     */
    bool                    write_info(LPCCACHE_INFO cache_info);
    /**
     * @brief Delete cache file info.
     * @param[in] filename - Source file name.
     * @param[in] desttype - Destination type (MP4, WEBM etc.).
     * @return Returns true on success; false on error.
     */
    bool                    delete_info(const std::string & filename, const std::string & desttype);
    /**
     * @brief Create cache entry object for a VIRTUALFILE.
     * @param[in] virtualfile - virtualfile struct of a file.
     * @param[in] desttype - Destination type (MP4, WEBM etc.).
     * @return On success, returns pointer to a Cache_Entry. On error, returns nullptr.
     */
    Cache_Entry*            create_entry(LPVIRTUALFILE virtualfile, const std::string & desttype);
    /**
     * @brief Delete cache entry object.
     * @param[in, out] cache_entry - Cache entry object to be closed.
     * @param[in] flags - One of the CLOSE_CACHE_* flags.
     * @return Returns true if the object was deleted; false if not.
     */
    bool                    delete_entry(Cache_Entry **cache_entry, int flags);
    /**
     * @brief Close cache index.
     */
    void                    close_index();
    /**
     * @brief Get expanded SQL string for a statement.
     * @param[in] pStmt - SQLite statement handle.
     * @return Returns the SQL string bound to the statement handle.
     */
    std::string             expanded_sql(sqlite3_stmt *pStmt);

private:
    std::recursive_mutex    m_mutex;                        /**< @brief Access mutex */
    std::string             m_cacheidx_file;                /**< @brief Name of SQLite cache index database */
    sqlite3*                m_cacheidx_db;                  /**< @brief SQLite handle of cache index database */
    sqlite3_stmt *          m_cacheidx_select_stmt;         /**< @brief Prepared select statement */
    sqlite3_stmt *          m_cacheidx_insert_stmt;         /**< @brief Prepared insert statement */
    sqlite3_stmt *          m_cacheidx_delete_stmt;         /**< @brief Prepared delete statement */
    cache_t                 m_cache;                        /**< @brief Cache file (memory mapped file) */
};

#endif
