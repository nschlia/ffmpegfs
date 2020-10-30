/*
 * Copyright (C) 2017-2020 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @brief Data cache management
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2020 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef CACHE_H
#define CACHE_H

#pragma once

#include "buffer.h"

#include <map>
#include <sqlite3.h>

#define     DB_BASE_VERSION_MAJOR   1           /**< @brief The oldest database version major (Release < 1.95) */
#define     DB_BASE_VERSION_MINOR   0           /**< @brief The oldest database version minor (Release < 1.95) */

#define     DB_VERSION_MAJOR        1           /**< @brief Current database version major */
#define     DB_VERSION_MINOR        97          /**< @brief Current database version minor */

#define     DB_MIN_VERSION_MAJOR    1           /**< @brief Required database version major (required 1.95) */
#define     DB_MIN_VERSION_MINOR    97          /**< @brief Required database version minor (required 1.95) */

/**
  * @brief RESULTCODE of transcoding operation
  */
typedef enum RESULTCODE
{
    RESULTCODE_NONE,                            /**< @brief No result code available */
    RESULTCODE_INCOMPLETE,                      /**< @brief Transcode finished, but incomplete */
    RESULTCODE_FINISHED,                        /**< @brief Transcode finished successfully */
    RESULTCODE_ERROR,                           /**< @brief Transcode finished with error */
} RESULTCODE;
typedef RESULTCODE const *LPCRESULTCODE;        /**< @brief Pointer version of RESULTCODE */
typedef RESULTCODE *LPRESULTCODE;               /**< @brief Pointer to const version of RESULTCODE */

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
    int64_t         m_duration;                 /**< @brief File  duration, in AV_TIME_BASE fractional seconds. */
    size_t          m_predicted_filesize;       /**< @brief Predicted file size */
    size_t          m_encoded_filesize;         /**< @brief Actual file size after encode */
    uint32_t        m_video_frame_count;        /**< @brief Number of frames in video or 0 if not a video */
    uint32_t        m_segment_count;            /**< @brief Number of video segments for HLS */
    RESULTCODE      m_finished;                 /**< @brief Result code: */
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
public:
    /**
      * @brief Definition of sql table
      */
    typedef struct
    {
        const char *    name;                       /**< @brief Table name */
        const char *    primary_key;                /**< @brief Primary key of table */
    } TABLE_DEF;
    typedef TABLE_DEF const *LPCTABLE_DEF;          /**< @brief Pointer version of TABLE_DEF */
    typedef TABLE_DEF *LPTABLE_DEF;                 /**< @brief Pointer to const version of TABLE_DEF */

    /**
      * @brief Column definition of sql table
      */
    typedef struct
    {
        const char *    name;                       /**< @brief Column name */
        const char *    type;                       /**< @brief Column type (INT, CHAR etc) */
    } TABLE_COLUMNS;
    typedef TABLE_COLUMNS const *LPCTABLE_COLUMNS;  /**< @brief Pointer version of TABLE_COLUMNS */
    typedef TABLE_COLUMNS *LPTABLE_COLUMNS;         /**< @brief Pointer to const version of TABLE_COLUMNS */

    friend class Cache_Entry;

public:
    /**
     * @brief Construct #Cache object.
     */
    explicit Cache();
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
     * @param[in] flags - One of the CACHE_CLOSE_* flags.
     * @return Returns true if the object was deleted; false if not.
     */
    bool                    close(Cache_Entry **cache_entry, int flags = CACHE_CLOSE_NOOPT);
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
     * @param[in] fileext - File extension of target file.
     * @return Returns true on success; false on error.
     */
    bool                    remove_cachefile(const std::string & filename, const std::string &fileext);

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
     * @param[in] flags - One of the CACHE_CLOSE_* flags.
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
    /**
     * @brief Prepare all SQL statements
     * @return Returns true on success, false on error.
     */
    bool                    prepare_stmts();
    /**
     * @brief Check if SQL table exists in database.
     * @param[in] table - name of table
     * @return Returns true if table exists, false if not.
     */
    bool                    table_exists(const char *table);
    /**
     * @brief Check if column exists in SQL table.
     * @param[in] table - name of table
     * @param[in] column - name of column
     * @return Returns true if table exists, false if not.
     */
    bool                    column_exists(const char *table, const char *column);
    /**
     * @brief Check the db version if upgrade needed.
     * @param[out] db_version_major - Upon return, contains the major database version.
     * @param[out] db_version_minor - Upon return, contains the minor database version.
     * @return Returns true of version is OK, false if upgrade is needed.
     */
    bool                    check_min_version(int *db_version_major, int *db_version_minor);
    /**
     * @brief Compare two versions.
     * @param[in] version_major_l - Left major version
     * @param[in] version_minor_l - Left minor version
     * @param[in] version_major_r - Right major version
     * @param[in] version_minor_r - Right minor version
     * @return Returns +1 if left version is larger then right, 0 if versions are the same, -1 if right version is larger than left.
     */
    int                     cmp_version(int version_major_l, int version_minor_l, int version_major_r, int version_minor_r);
    /**
     * @brief Begin a database transactio.n
     * @return Returns true on success; false on error.
     */
    bool                    begin_transaction();
    /**
     * @brief End a database transaction.
     * @return Returns true on success; false on error.
     */
    bool                    end_transaction();
    /**
     * @brief Rollback a database transaction.
     * @return Returns true on success; false on error.
     */
    bool                    rollback_transaction();
    /**
     * @brief Create cache_entry table
     * @return
     * @return Returns true on success; false on error.
     */
    bool                    create_table_cache_entry(LPCTABLE_DEF table, const TABLE_COLUMNS columns[]);
    /*
     */
    /**
     * @brief Upgrade database from version below 1.95
     * @param[out] db_version_major - Upon return, contains the new major database version.
     * @param[out] db_version_minor - Upon return, contains the new minor database version.
     * @return Returns true on success; false on error.
     */
    bool                    upgrade_db(int *db_version_major, int *db_version_minor);

private:
    static const
    TABLE_DEF               m_table_cache_entry;            /**< @brief Definition and indexes of table "cache_entry" */
    static const
    TABLE_COLUMNS           m_columns_cache_entry[];        /**< @brief Columns of table "cache_entry" */
    static const
    TABLE_DEF               m_table_version;                /**< @brief Definition and indexes of table "version" */
    static const
    TABLE_COLUMNS           m_columns_version[];            /**< @brief Columns of table "version" */

    std::recursive_mutex    m_mutex;                        /**< @brief Access mutex */
    std::string             m_cacheidx_file;                /**< @brief Name of SQLite cache index database */
    sqlite3*                m_cacheidx_db;                  /**< @brief SQLite handle of cache index database */
    sqlite3_stmt *          m_cacheidx_select_stmt;         /**< @brief Prepared select statement */
    sqlite3_stmt *          m_cacheidx_insert_stmt;         /**< @brief Prepared insert statement */
    sqlite3_stmt *          m_cacheidx_delete_stmt;         /**< @brief Prepared delete statement */
    cache_t                 m_cache;                        /**< @brief Cache file (memory mapped file) */
};

#endif
