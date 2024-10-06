/*
 * Copyright (C) 2017-2024 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file cache.cc
 * @brief Cache class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2024 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "cache.h"
#include "cache_entry.h"
#include "ffmpegfs.h"
#include "logging.h"

#include <vector>
#include <cassert>
#include <sqlite3.h>

#ifndef HAVE_SQLITE_ERRSTR
#define sqlite3_errstr(rc)  ""              /**< @brief If our version of SQLite hasn't go this function */
#endif // HAVE_SQLITE_ERRSTR

#define STRINGIFY(x) #x                                                             /**< @brief Stringification helper for STRINGIFY. Not to be used separately. */
#define TOSTRING(x) STRINGIFY(x)                                                    /**< @brief Convert a macro argument into a string constant */

Cache::sqlite_t::sqlite_t(const std::string & filename, int flags, const char *zVfs)
    : m_ret(SQLITE_OK)
    , m_filename(filename)
    , m_select_stmt(nullptr)
    , m_insert_stmt(nullptr)
    , m_delete_stmt(nullptr)
{
    m_ret = sqlite3_open_v2(m_filename.c_str(), &m_db_handle, flags, zVfs);
}

Cache::sqlite_t::~sqlite_t()
{
    if (m_db_handle != nullptr)
    {
#ifdef HAVE_SQLITE_CACHEFLUSH
        flush_index();
#endif // HAVE_SQLITE_CACHEFLUSH

        sqlite3_finalize(m_select_stmt);
        sqlite3_finalize(m_insert_stmt);
        sqlite3_finalize(m_delete_stmt);

        sqlite3_close(m_db_handle);
    }
    sqlite3_shutdown();
}

const Cache::TABLE_DEF Cache::m_table_cache_entry =
{
    //
    // Table name
    //
    "cache_entry",
    //
    // Primary key
    //
    "PRIMARY KEY(`filename`,`desttype`)"
};

const Cache::TABLECOLUMNS_VEC Cache::m_columns_cache_entry =
{
    //
    // Primary key: filename + desttype
    //
    { "filename",           "TEXT NOT NULL" },
    { "desttype",           "CHAR ( 10 ) NOT NULL" },
    //
    // Encoding parameters
    //
    { "enable_ismv",        "BOOLEAN NOT NULL" },
    { "audiobitrate",       "UNSIGNED INT NOT NULL" },
    { "audiosamplerate",    "UNSIGNED INT NOT NULL" },
    { "videobitrate",       "UNSIGNED INT NOT NULL" },
    { "videowidth",         "UNSIGNED INT NOT NULL" },
    { "videoheight",        "UNSIGNED INT NOT NULL" },
    { "deinterlace",        "BOOLEAN NOT NULL" },
    //
    // Encoding results
    //
    { "duration",           "UNSIGNED BIG INT NOT NULL" },
    { "predicted_filesize", "UNSIGNED BIG INT NOT NULL" },
    { "encoded_filesize",   "UNSIGNED BIG INT NOT NULL" },
    { "video_frame_count",  "UNSIGNED BIG INT NOT NULL" },
    { "segment_count",      "UNSIGNED BIG INT NOT NULL" },
    { "finished",           "INT NOT NULL" },
    { "error",              "BOOLEAN NOT NULL" },
    { "errno",              "INT NOT NULL" },
    { "averror",            "INT NOT NULL" },
    { "creation_time",      "DATETIME NOT NULL" },
    { "access_time",        "DATETIME NOT NULL" },
    { "file_time",          "DATETIME NOT NULL" },
    { "file_size",          "UNSIGNED BIG INT NOT NULL" }
};

const Cache::TABLE_DEF Cache::m_table_version =
{
    //
    // Table name
    //
    "version",
    //
    nullptr
};

const Cache::TABLECOLUMNS_VEC Cache::m_columns_version =
{
    { "db_version_major",   "INTEGER NOT NULL" },
    { "db_version_minor",   "INTEGER NOT NULL" }
};

Cache::Cache()
{
}

Cache::~Cache()
{
    // Clean up memory
    for (auto& [key, value] : m_cache)
    {
        value->destroy();
    }

    m_cache.clear();

    close_index();
}

bool Cache::prepare_stmts()
{
    int ret;
    const char * sql;

    sql =   "INSERT OR REPLACE INTO cache_entry\n"
            "(filename, desttype, enable_ismv, audiobitrate, audiosamplerate, videobitrate, videowidth, videoheight, deinterlace, duration, predicted_filesize, encoded_filesize, video_frame_count, segment_count, finished, error, errno, averror, creation_time, access_time, file_time, file_size) VALUES\n"
            "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), ?);\n";

    if (SQLITE_OK != (ret = sqlite3_prepare_v2(*m_cacheidx_db, sql, -1, &m_cacheidx_db->m_insert_stmt, nullptr)))
    {
        Logging::error(m_cacheidx_db->filename(), "Failed to prepare insert: (%1) %2\n%3", ret, sqlite3_errmsg(*m_cacheidx_db), sql);
        return false;
    }

    sql =   "SELECT desttype, enable_ismv, audiobitrate, audiosamplerate, videobitrate, videowidth, videoheight, deinterlace, duration, predicted_filesize, encoded_filesize, video_frame_count, segment_count, finished, error, errno, averror, strftime('%s', creation_time), strftime('%s', access_time), strftime('%s', file_time), file_size FROM cache_entry WHERE filename = ? AND desttype = ?;\n";

    if (SQLITE_OK != (ret = sqlite3_prepare_v2(*m_cacheidx_db, sql, -1, &m_cacheidx_db->m_select_stmt, nullptr)))
    {
        Logging::error(m_cacheidx_db->filename(), "Failed to prepare select: (%1) %2\n%3", ret, sqlite3_errmsg(*m_cacheidx_db), sql);
        return false;
    }

    sql =   "DELETE FROM cache_entry WHERE filename = ? AND desttype = ?;\n";

    if (SQLITE_OK != (ret = sqlite3_prepare_v2(*m_cacheidx_db, sql, -1, &m_cacheidx_db->m_delete_stmt, nullptr)))
    {
        Logging::error(m_cacheidx_db->filename(), "Failed to prepare delete: (%1) %2\n%3", ret, sqlite3_errmsg(*m_cacheidx_db), sql);
        return false;
    }

    return true;
}

bool Cache::table_exists(const char *table)
{
    std::string sql;
    sqlite3_stmt *  stmt = nullptr;
    int results = 0;
    int ret;

    sql = "SELECT Count(*) FROM sqlite_master WHERE type='table' AND name='";
    sql += table;
    sql += "'";

    if (SQLITE_OK != (ret = sqlite3_prepare_v2(*m_cacheidx_db, sql.c_str(), -1, &stmt, nullptr)))
    {
        Logging::error(m_cacheidx_db->filename(), "Failed to prepare statement for table_exists: (%1) %2\n%3", ret, sqlite3_errmsg(*m_cacheidx_db), sql.c_str());
        return false;
    }

    ret = sqlite3_step(stmt);

    if (ret == SQLITE_ROW)
    {
        results = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    return (results == 1);
}

bool Cache::column_exists(const char *table, const char *column)
{
    std::string sql;
    sqlite3_stmt *  stmt = nullptr;
    int results = 0;
    int ret;

    sql = "SELECT COUNT(*) AS CNTREC FROM pragma_table_info('";
    sql += table;
    sql += "') WHERE name='";
    sql += column;
    sql += "';";

    if (SQLITE_OK != (ret = sqlite3_prepare_v2(*m_cacheidx_db, sql.c_str(), -1, &stmt, nullptr)))
    {
        Logging::error(m_cacheidx_db->filename(), "Failed to prepare statement for table_exists: (%1) %2\n%3", ret, sqlite3_errmsg(*m_cacheidx_db), sql.c_str());
        return false;
    }

    ret = sqlite3_step(stmt);

    if (ret == SQLITE_ROW)
    {
        results = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    return (results == 1);
}

bool Cache::check_min_version(int *db_version_major, int *db_version_minor)
{
    std::string sql;
    sqlite3_stmt *  stmt = nullptr;
    int ret;

    sql = "SELECT db_version_major, db_version_minor FROM version;";

    if (SQLITE_OK != (ret = sqlite3_prepare_v2(*m_cacheidx_db, sql.c_str(), -1, &stmt, nullptr)))
    {
        Logging::error(m_cacheidx_db->filename(), "Failed to prepare statement for check_min_version: (%1) %2\n%3", ret, sqlite3_errmsg(*m_cacheidx_db), sql.c_str());
        return false;
    }

    ret = sqlite3_step(stmt);

    if (ret == SQLITE_ROW)
    {
        *db_version_major = sqlite3_column_int(stmt, 0);
        *db_version_minor = sqlite3_column_int(stmt, 1);
    }

    sqlite3_finalize(stmt);

    return (cmp_version(*db_version_major, *db_version_minor, DB_MIN_VERSION_MAJOR, DB_MIN_VERSION_MINOR) >= 0);
}

int Cache::cmp_version(int version_major_l, int version_minor_l, int version_major_r, int version_minor_r)
{
    if (version_major_l > version_major_r)
    {
        return 1;
    }
    if (version_major_l < version_major_r)
    {
        return -1;
    }

    // version_major_l == version_major_r

    if (version_minor_l > version_minor_r)
    {
        return 1;
    }
    if (version_minor_l < version_minor_r)
    {
        return -1;
    }

    // version_minor_l == version_minor_r

    return 0;
}

bool Cache::begin_transaction()
{
    char *errmsg = nullptr;
    const char * sql;
    int ret;

    sql = "BEGIN TRANSACTION;";
    if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql, nullptr, nullptr, &errmsg)))
    {
        Logging::error(m_cacheidx_db->filename(), "SQLite3 begin transaction failed: (%1) %2\n%3", ret, errmsg, sql);
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

bool Cache::end_transaction()
{
    char *errmsg = nullptr;
    const char * sql;
    int ret;

    sql = "END TRANSACTION;";
    if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql, nullptr, nullptr, &errmsg)))
    {
        Logging::error(m_cacheidx_db->filename(), "SQLite3 end transaction failed: (%1) %2\n%3", ret, errmsg, sql);
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

bool Cache::rollback_transaction()
{
    char *errmsg = nullptr;
    const char * sql;
    int ret;

    sql = "ROLLBACK;";
    if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql, nullptr, nullptr, &errmsg)))
    {
        Logging::error(m_cacheidx_db->filename(), "SQLite3 rollback transaction failed: (%1) %2\n%3", ret, errmsg, sql);
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

bool Cache::create_table_cache_entry(LPCTABLE_DEF table, const TABLECOLUMNS_VEC & columns)
{
    char *errmsg = nullptr;
    std::string sql;
    int ret;

    sql = "CREATE TABLE `";
    sql += table->name;
    sql += "` (\n";

    int i = 0;
    for (const TABLE_COLUMNS & col : columns)
    {
        if (i++)
        {
            sql += ",\n";
        }
        sql += "`";
        sql += col.name;
        sql += "` ";
        sql += col.type;
    }

    if (table->primary_key != nullptr)
    {
        sql += ",\n";
        sql += table->primary_key;
    }
    sql += "\n";
    sql += ");\n";

    if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql.c_str(), nullptr, nullptr, &errmsg)))
    {
        Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error: (%1) %2\n%3", ret, errmsg, sql.c_str());
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

bool Cache::upgrade_db(int *db_version_major, int *db_version_minor)
{
    if (!column_exists("cache_entry", "video_frame_count"))
    {
        // If video_frame_count is missing, this db is definetly old

        {
            char *errmsg = nullptr;
            std::string sql;
            int ret;

            Logging::debug(m_cacheidx_db->filename(), "Adding `video_frame_count` column.");

            // Add `video_frame_count` UNSIGNED BIG INT NOT NULL DEFAULT 0
            sql = "ALTER TABLE `";
            sql += m_table_cache_entry.name;
            sql += "` ADD COLUMN `video_frame_count` UNSIGNED BIG INT NOT NULL DEFAULT 0;\n";
            if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql.c_str(), nullptr, nullptr, &errmsg)))
            {
                Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error adding column `video_frame_count`: (%1) %2\n%3", ret, errmsg, sql.c_str());
                sqlite3_free(errmsg);
                return false;
            }

            Logging::debug(m_cacheidx_db->filename(), "Altering `finished` from BOOLEAN to INT.");
        }

        {
            char *errmsg = nullptr;
            std::string sql;
            int ret;

            //ALTER `finished` from BOOLEAN to INT NOT NULL
            // sqlite can't do that for us, we must...
            //
            // 1. Rename `cache_entry` to `cache_entry_old`
            // 2. Create new table `cache_entry` with new structure
            // 3. Copy all data from `cache_entry_old` to `cache_entry`, converting old to new column
            // 4. Delete `cache_entry_old`

            sql = "PRAGMA foreign_keys=off;\n";
            if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql.c_str(), nullptr, nullptr, &errmsg)))
            {
                Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error: (%1) %2\n%3", ret, errmsg, sql.c_str());
                sqlite3_free(errmsg);
                return false;
            }
        }

        // Step 1
        {
            char *errmsg = nullptr;
            std::string sql;
            int ret;

            sql = "ALTER TABLE `";
            sql += m_table_cache_entry.name;
            sql += "` RENAME TO `";
            sql += m_table_cache_entry.name;
            sql += "_old`;\n";
            if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql.c_str(), nullptr, nullptr, &errmsg)))
            {
                Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error: (%1) %2\n%3", ret, errmsg, sql.c_str());
                sqlite3_free(errmsg);
                return false;
            }
        }

        // Step 2
        if (!create_table_cache_entry(&m_table_cache_entry, m_columns_cache_entry))
        {
            Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error creating 'cache_entry' table.");
            return false;
        }

        // Step 3
        {
            char *errmsg = nullptr;
            std::string sql;
            std::string columns;
            int ret;

            for (const TABLE_COLUMNS & col : m_columns_cache_entry)
            {
                if (!columns.empty())
                {
                    columns += ",";
                }
                columns += "`";
                columns += col.name;
                columns += "`";
            }

            sql = "INSERT INTO `";
            sql += m_table_cache_entry.name;
            sql += "` (";
            sql += columns;
            sql += ")\nSELECT ";
            sql += columns;
            sql += " FROM `";
            sql += m_table_cache_entry.name;
            sql += "_old`;";

            if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql.c_str(), nullptr, nullptr, &errmsg)))
            {
                Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error: (%1) %2\n%3", ret, errmsg, sql.c_str());
                sqlite3_free(errmsg);
                return false;
            }
        }

        {
            char *errmsg = nullptr;
            std::string sql;
            int ret;

            //    Old 0 is RESULTCODE::NONE (0)
            //    Old 1 is RESULTCODE::FINISHED (2)
            sql = "UPDATE `";
            sql += m_table_cache_entry.name;
            sql += "`\n";
            sql += "SET `finished` = 3\n";
            sql += "WHERE `finished` = 1\n";
            if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql.c_str(), nullptr, nullptr, &errmsg)))
            {
                Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error updating column `finished`: (%1) %2\n%3", ret, errmsg, sql.c_str());
                sqlite3_free(errmsg);
                return false;
            }
        }

        // Step 4
        {
            char *errmsg = nullptr;
            std::string sql;
            int ret;

            sql = "DROP TABLE `cache_entry_old`";
            if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql.c_str(), nullptr, nullptr, &errmsg)))
            {
                Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error adding column `video_frame_count`: (%1) %2\n%3", ret, errmsg, sql.c_str());
                sqlite3_free(errmsg);
                return false;
            }

            sql = "PRAGMA foreign_keys=on;\n";
            if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql.c_str(), nullptr, nullptr, &errmsg)))
            {
                Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error: (%1) %2\n%3", ret, errmsg, sql.c_str());
                sqlite3_free(errmsg);
                return false;
            }
        }
    }

    if (!column_exists("cache_entry", "duration"))
    {
        char *errmsg = nullptr;
        std::string sql;
        int ret;

        Logging::debug(m_cacheidx_db->filename(), "Adding `duration` column.");

        // Add `duration` UNSIGNED BIG INT NOT NULL DEFAULT 0
        sql = "ALTER TABLE `";
        sql += m_table_cache_entry.name;
        sql += "` ADD COLUMN `duration` UNSIGNED BIG INT NOT NULL DEFAULT 0;\n";
        if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql.c_str(), nullptr, nullptr, &errmsg)))
        {
            Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error adding column `duration`: (%1) %2\n%3", ret, errmsg, sql.c_str());
            sqlite3_free(errmsg);
            return false;
        }
    }

    if (!column_exists("cache_entry", "segment_count"))
    {
        char *errmsg = nullptr;
        std::string sql;
        int ret;

        Logging::debug(m_cacheidx_db->filename(), "Adding `segment_count` column.");

        // Add `segment_count` UNSIGNED BIG INT NOT NULL DEFAULT 0
        sql = "ALTER TABLE `";
        sql += m_table_cache_entry.name;
        sql += "` ADD COLUMN `segment_count` UNSIGNED BIG INT NOT NULL DEFAULT 0;\n";
        if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql.c_str(), nullptr, nullptr, &errmsg)))
        {
            Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error adding column `segment_count`: (%1) %2\n%3", ret, errmsg, sql.c_str());
            sqlite3_free(errmsg);
            return false;
        }
    }

    // Update DB version
    Logging::debug(m_cacheidx_db->filename(), "Updating version table to V%1.%2.", DB_VERSION_MAJOR, DB_VERSION_MINOR);

    {
        char *errmsg = nullptr;
        const char * sql;
        int ret;

        sql = "UPDATE `version` SET db_version_major = " TOSTRING(DB_VERSION_MAJOR) ", db_version_minor = " TOSTRING(DB_VERSION_MINOR) ";\n";
        if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql, nullptr, nullptr, &errmsg)))
        {
            Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error: (%1) %2\n%3", ret, errmsg, sql);
            sqlite3_free(errmsg);
            return false;
        }

        *db_version_major = DB_VERSION_MAJOR;
        *db_version_minor = DB_VERSION_MINOR;
    }

    Logging::info(m_cacheidx_db->filename(), "Database successfully upgraded to V%1.%2.", *db_version_major, *db_version_minor);

    return true;
}

bool Cache::load_index()
{
    bool success = true;

    try
    {
        std::string filename;
        char *errmsg = nullptr;
        int ret;
        bool new_database = false;
        bool need_upate = false;

        transcoder_cache_path(&filename);

        if (mktree(filename, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) && errno != EEXIST)
        {
            Logging::error(filename, "Error creating cache directory: (%1) %2\n%3", errno, strerror(errno), m_cacheidx_db->filename().c_str());
            throw false;
        }

        append_filename(&filename, "cacheidx.sqlite");

        // initialise engine
        if (SQLITE_OK != (ret = sqlite3_initialize()))
        {
            Logging::error(filename, "Failed to initialise SQLite3 library: (%1) %2", ret, sqlite3_errstr(ret));
            throw false;
        }

        // open connection to a DB
        m_cacheidx_db = std::make_unique<sqlite_t>(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_SHAREDCACHE);

        if (m_cacheidx_db == nullptr)
        {
            Logging::error(filename, "Out of memory.");
            throw false;
        }

        if (SQLITE_OK != (ret = m_cacheidx_db->ret()))
        {
            Logging::error(m_cacheidx_db->filename(), "Failed to initialise SQLite3 connection: (%1) %2", ret, sqlite3_errmsg(*m_cacheidx_db));
            throw false;
        }

        if (SQLITE_OK != (ret = sqlite3_busy_timeout(*m_cacheidx_db, 1000)))
        {
            Logging::error(m_cacheidx_db->filename(), "Failed to set SQLite3 busy timeout: (%1) %2", ret, sqlite3_errmsg(*m_cacheidx_db));
            throw false;
        }

        // Beginning with version 3.7.0 (2010-07-21), a new "Write-Ahead Log" option
        // We support Sqlite from 3.7.13 anyway
        if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, "pragma journal_mode = WAL", nullptr, nullptr, nullptr)))
        {
            Logging::error(m_cacheidx_db->filename(), "Failed to set SQLite3 WAL mode: (%1) %2", ret, sqlite3_errmsg(*m_cacheidx_db));
            throw false;
        }

        // Very strange: Compare operations with =, > etc. are case sensitive, while LIKE by default ignores upper/lowercase.
        // Produces strange results when reading from a Samba drive and different cases are used...
        if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, "PRAGMA case_sensitive_like = 1;", nullptr, nullptr, nullptr)))
        {
            Logging::error(m_cacheidx_db->filename(), "Failed to set SQLite3 case_sensitive_like = 1: (%1) %2", ret, sqlite3_errmsg(*m_cacheidx_db));
            throw false;
        }

        // Make sure the next changes are either all successfull or rolled back
        if (!begin_transaction())
        {
            throw false;
        }

        // Check  if we got a new, empty database and create necessary tables

        // Create cache_entry table if not already existing
        if (!table_exists("cache_entry"))
        {
            Logging::debug(m_cacheidx_db->filename(), "Creating 'cache_entry' table in database.");

            if (!create_table_cache_entry(&m_table_cache_entry, m_columns_cache_entry))
            {
                Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error creating 'cache_entry' table: (%1) %2", ret, errmsg);
                throw false;
            }

            new_database = true;    //  Created a new database
        }

        // If version table does not exist add it
        if (!table_exists("version"))
        {
            const char * sql;

            Logging::debug(m_cacheidx_db->filename(), "Creating 'version' table in database.");

            if (!create_table_cache_entry(&m_table_version, m_columns_version))
            {
                Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error creating 'cache_entry' table: (%1) %2", ret, errmsg);
                throw false;
            }

            sql = "INSERT INTO `version` (db_version_major, db_version_minor) VALUES (" TOSTRING(DB_VERSION_MAJOR) ", " TOSTRING(DB_VERSION_MINOR) ");\n";
            if (SQLITE_OK != (ret = sqlite3_exec(*m_cacheidx_db, sql, nullptr, nullptr, &errmsg)))
            {
                Logging::error(m_cacheidx_db->filename(), "SQLite3 exec error: (%1) %2\n%3", ret, errmsg, sql);
                sqlite3_free(errmsg);
                throw false;
            }

            if (!new_database)
            {
                // Added version only, old database, need upgrade
                need_upate = true;
            }
        }

        // Check if database needs a structure upgrade
        int db_version_major = DB_BASE_VERSION_MAJOR;   // Old database contains no version table. This is the version of this database.
        int db_version_minor = DB_BASE_VERSION_MINOR;
        if (need_upate || !check_min_version(&db_version_major, &db_version_minor))
        {
            // No version table found, or minimum version too low, do an upgrade.
            Logging::warning(m_cacheidx_db->filename(), "Database version is %1.%2, but a least %3.%4 required. Upgrading database now.", db_version_major, db_version_minor, DB_MIN_VERSION_MAJOR, DB_MIN_VERSION_MINOR);

            if (!upgrade_db(&db_version_major, &db_version_minor))
            {
                throw false;
            }
        }

        if (!end_transaction())
        {
            throw false;
        }

#ifdef HAVE_SQLITE_CACHEFLUSH
        if (!m_cacheidx_db->flush_index())
        {
            throw false;
        }
#endif // HAVE_SQLITE_CACHEFLUSH

        // prepare the statements
        if (!prepare_stmts())
        {
            throw false;
        }
    }
    catch (bool _success)
    {
        success = _success;
    }

    return success;
}

#ifdef HAVE_SQLITE_CACHEFLUSH
bool Cache::sqlite_t::flush_index()
{
    if (m_db_handle != nullptr)
    {
        int ret;

        // Flush cache to disk
        if (SQLITE_OK != (ret = sqlite3_db_cacheflush(m_db_handle)))
        {
            Logging::error(m_filename, "SQLite3 cache flush error: (%1) %2", ret, sqlite3_errstr(ret));
            return false;
        }
    }
    return true;
}
#endif // HAVE_SQLITE_CACHEFLUSH

bool Cache::read_info(LPCACHE_INFO cache_info)
{
    bool success = true;

    //cache_info->m_enable_ismv         = 0;
    cache_info->m_audiobitrate          = 0;
    cache_info->m_audiosamplerate       = 0;
    cache_info->m_videobitrate          = 0;
    cache_info->m_videowidth            = 0;
    cache_info->m_videoheight           = 0;
    cache_info->m_deinterlace           = false;
    cache_info->m_duration              = 0;
    cache_info->m_predicted_filesize    = 0;
    cache_info->m_encoded_filesize      = 0;
    cache_info->m_video_frame_count     = 0;
    cache_info->m_segment_count         = 0;
    cache_info->m_result                = RESULTCODE::NONE;
    cache_info->m_error                 = false;
    cache_info->m_errno                 = 0;
    cache_info->m_averror               = 0;
    cache_info->m_creation_time         = 0;
    cache_info->m_access_time           = 0;
    cache_info->m_file_time             = 0;
    cache_info->m_file_size             = 0;

    if (m_cacheidx_db->m_select_stmt == nullptr)
    {
        Logging::error(m_cacheidx_db->filename(), "SQLite3 select statement not open.");
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    try
    {
        int ret;

        assert(sqlite3_bind_parameter_count(m_cacheidx_db->m_select_stmt) == 2);

        if (SQLITE_OK != (ret = sqlite3_bind_text(m_cacheidx_db->m_select_stmt, 1, cache_info->m_destfile.c_str(), -1, nullptr)))
        {
            Logging::error(m_cacheidx_db->filename(), "SQLite3 select error binding 'filename': (%1) %2", ret, sqlite3_errstr(ret));
            throw false;
        }

        if (SQLITE_OK != (ret = sqlite3_bind_text(m_cacheidx_db->m_select_stmt, 2, cache_info->m_desttype.data(), -1, nullptr)))
        {
            Logging::error(m_cacheidx_db->filename(), "SQLite3 select error binding 'desttype': (%1) %2", ret, sqlite3_errstr(ret));
            throw false;
        }

        ret = sqlite3_step(m_cacheidx_db->m_select_stmt);

        if (ret == SQLITE_ROW)
        {
            const char *text                = reinterpret_cast<const char *>(sqlite3_column_text(m_cacheidx_db->m_select_stmt, 0));
            if (text != nullptr)
            {
                cache_info->m_desttype[0] = '\0';
                strncat(cache_info->m_desttype.data(), text, cache_info->m_desttype.size() - 1);
            }
            //cache_info->m_enable_ismv        = sqlite3_column_int(m_cacheidx_db->m_cacheidx_select_stmt, 1);
            cache_info->m_audiobitrate          = sqlite3_column_int(m_cacheidx_db->m_select_stmt, 2);
            cache_info->m_audiosamplerate       = sqlite3_column_int(m_cacheidx_db->m_select_stmt, 3);
            cache_info->m_videobitrate          = sqlite3_column_int(m_cacheidx_db->m_select_stmt, 4);
            cache_info->m_videowidth            = sqlite3_column_int(m_cacheidx_db->m_select_stmt, 5);
            cache_info->m_videoheight           = sqlite3_column_int(m_cacheidx_db->m_select_stmt, 6);
            cache_info->m_deinterlace           = sqlite3_column_int(m_cacheidx_db->m_select_stmt, 7);
            cache_info->m_duration              = sqlite3_column_int64(m_cacheidx_db->m_select_stmt, 8);
            cache_info->m_predicted_filesize    = static_cast<size_t>(sqlite3_column_int64(m_cacheidx_db->m_select_stmt, 9));
            cache_info->m_encoded_filesize      = static_cast<size_t>(sqlite3_column_int64(m_cacheidx_db->m_select_stmt, 10));
            cache_info->m_video_frame_count     = static_cast<uint32_t>(sqlite3_column_int(m_cacheidx_db->m_select_stmt, 11));
            cache_info->m_segment_count         = static_cast<uint32_t>(sqlite3_column_int(m_cacheidx_db->m_select_stmt, 12));
            cache_info->m_result                = static_cast<RESULTCODE>(sqlite3_column_int(m_cacheidx_db->m_select_stmt, 13));
            cache_info->m_error                 = sqlite3_column_int(m_cacheidx_db->m_select_stmt, 14);
            cache_info->m_errno                 = sqlite3_column_int(m_cacheidx_db->m_select_stmt, 15);
            cache_info->m_averror               = sqlite3_column_int(m_cacheidx_db->m_select_stmt, 16);
            cache_info->m_creation_time         = static_cast<time_t>(sqlite3_column_int64(m_cacheidx_db->m_select_stmt, 17));
            cache_info->m_access_time           = static_cast<time_t>(sqlite3_column_int64(m_cacheidx_db->m_select_stmt, 18));
            cache_info->m_file_time             = static_cast<time_t>(sqlite3_column_int64(m_cacheidx_db->m_select_stmt, 19));
            cache_info->m_file_size             = static_cast<size_t>(sqlite3_column_int64(m_cacheidx_db->m_select_stmt, 20));
        }
        else if (ret != SQLITE_DONE)
        {
            Logging::error(m_cacheidx_db->filename(), "Sqlite 3 could not step (execute) select statement: (%1) %2", ret, sqlite3_errstr(ret));
            throw false;
        }
    }
    catch (bool _success)
    {
        success = _success;
    }

    sqlite3_reset(m_cacheidx_db->m_select_stmt);

    if (success)
    {
        errno = 0; // sqlite3 sometimes sets errno without any reason, better reset any error
    }

    return success;
}

#define SQLBINDTXT(idx, var) \
    if (SQLITE_OK != (ret = sqlite3_bind_text(m_cacheidx_db->m_insert_stmt, idx, var, -1, nullptr))) \
{ \
    Logging::error(m_cacheidx_db->filename(), "SQLite3 select column #%1 error: %2\n%3", idx, ret, sqlite3_errstr(ret)); \
    throw false; \
    }       /**< @brief Bind text column to SQLite statement */

#define SQLBINDNUM(func, idx, var) \
    if (SQLITE_OK != (ret = func(m_cacheidx_db->m_insert_stmt, idx, var))) \
{ \
    Logging::error(m_cacheidx_db->filename(), "SQLite3 select column #%1 error: %2\n%3", idx, ret, sqlite3_errstr(ret)); \
    throw false; \
    }       /**< @brief Bind numeric column to SQLite statement */

bool Cache::write_info(LPCCACHE_INFO cache_info)
{
    bool success = true;

    if (m_cacheidx_db->m_insert_stmt == nullptr)
    {
        Logging::error(m_cacheidx_db->filename(), "SQLite3 select statement not open.");
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    try
    {
        int ret;
        bool enable_ismv_dummy = false;

        assert(sqlite3_bind_parameter_count(m_cacheidx_db->m_insert_stmt) == 22);

        SQLBINDTXT(1, cache_info->m_destfile.c_str());
        SQLBINDTXT(2, cache_info->m_desttype.data());
        //SQLBINDNUM(sqlite3_bind_int,  3,  cache_info->m_enable_ismv);
        SQLBINDNUM(sqlite3_bind_int,    3,  enable_ismv_dummy);
        SQLBINDNUM(sqlite3_bind_int64,  4,  cache_info->m_audiobitrate);
        SQLBINDNUM(sqlite3_bind_int,    5,  cache_info->m_audiosamplerate);
        SQLBINDNUM(sqlite3_bind_int64,  6,  cache_info->m_videobitrate);
        SQLBINDNUM(sqlite3_bind_int,    7,  static_cast<int>(cache_info->m_videowidth));
        SQLBINDNUM(sqlite3_bind_int,    8,  static_cast<int>(cache_info->m_videoheight));
        SQLBINDNUM(sqlite3_bind_int,    9,  cache_info->m_deinterlace);
        SQLBINDNUM(sqlite3_bind_int64,  10, static_cast<sqlite3_int64>(cache_info->m_duration));
        SQLBINDNUM(sqlite3_bind_int64,  11, static_cast<sqlite3_int64>(cache_info->m_predicted_filesize));
        SQLBINDNUM(sqlite3_bind_int64,  12, static_cast<sqlite3_int64>(cache_info->m_encoded_filesize));
        SQLBINDNUM(sqlite3_bind_int,    13, static_cast<int32_t>(cache_info->m_video_frame_count));
        SQLBINDNUM(sqlite3_bind_int,    14, static_cast<int32_t>(cache_info->m_segment_count));
        SQLBINDNUM(sqlite3_bind_int,    15, static_cast<int32_t>(cache_info->m_result));
        SQLBINDNUM(sqlite3_bind_int,    16, cache_info->m_error);
        SQLBINDNUM(sqlite3_bind_int,    17, cache_info->m_errno);
        SQLBINDNUM(sqlite3_bind_int,    18, cache_info->m_averror);
        SQLBINDNUM(sqlite3_bind_int64,  19, cache_info->m_creation_time);
        SQLBINDNUM(sqlite3_bind_int64,  20, cache_info->m_access_time);
        SQLBINDNUM(sqlite3_bind_int64,  21, cache_info->m_file_time);
        SQLBINDNUM(sqlite3_bind_int64,  22, static_cast<sqlite3_int64>(cache_info->m_file_size));

        ret = sqlite3_step(m_cacheidx_db->m_insert_stmt);

        if (ret != SQLITE_DONE)
        {
            Logging::error(m_cacheidx_db->filename(), "Sqlite 3 could not step (execute) insert statement: (%1) %2", ret, sqlite3_errstr(ret));
            throw false;
        }
    }
    catch (bool _success)
    {
        success = _success;
    }

    sqlite3_reset(m_cacheidx_db->m_insert_stmt);

    if (success)
    {
        errno = 0; // sqlite3 sometimes sets errno without any reason, better reset any error
    }

    return success;
}

bool Cache::delete_info(const std::string & filename, const std::string & desttype)
{
    bool success = true;

    if (m_cacheidx_db->m_delete_stmt == nullptr)
    {
        Logging::error(m_cacheidx_db->filename(), "SQLite3 delete statement not open.");
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    try
    {
        int ret;

        assert(sqlite3_bind_parameter_count(m_cacheidx_db->m_delete_stmt) == 2);

        if (SQLITE_OK != (ret = sqlite3_bind_text(m_cacheidx_db->m_delete_stmt, 1, filename.c_str(), -1, nullptr)))
        {
            Logging::error(m_cacheidx_db->filename(), "SQLite3 select error binding 'filename': (%1) %2", ret, sqlite3_errstr(ret));
            throw false;
        }

        if (SQLITE_OK != (ret = sqlite3_bind_text(m_cacheidx_db->m_delete_stmt, 2, desttype.c_str(), -1, nullptr)))
        {
            Logging::error(m_cacheidx_db->filename(), "SQLite3 select error binding 'desttype': (%1) %2", ret, sqlite3_errstr(ret));
            throw false;
        }

        ret = sqlite3_step(m_cacheidx_db->m_delete_stmt);

        if (ret != SQLITE_DONE)
        {
            Logging::error(m_cacheidx_db->filename(), "Sqlite 3 could not step (execute) delete statement: (%1) %2", ret, sqlite3_errstr(ret));
            throw false;
        }
    }
    catch (bool _success)
    {
        success = _success;
    }

    sqlite3_reset(m_cacheidx_db->m_delete_stmt);

    if (success)
    {
        errno = 0; // sqlite3 sometimes sets errno without any reason, better reset any error
    }

    return success;
}

void Cache::close_index()
{
    m_cacheidx_db.reset();
}

Cache_Entry* Cache::create_entry(LPVIRTUALFILE virtualfile, const std::string & desttype)
{
    //Cache_Entry* cache_entry = new (std::nothrow) Cache_Entry(this, filename);
    Cache_Entry* cache_entry = Cache_Entry::create(this, virtualfile);
    if (cache_entry == nullptr)
    {
        Logging::error(m_cacheidx_db->filename(), "Out of memory creating cache entry.");
        return nullptr;
    }

    m_cache.insert(make_pair(make_pair(virtualfile->m_destfile, desttype), cache_entry));

    return cache_entry;
}

bool Cache::delete_entry(Cache_Entry ** cache_entry, int flags)
{
    if (*cache_entry == nullptr)
    {
        return true;
    }

    bool deleted = false;

    if ((*cache_entry)->closeio(flags))
    {
        // If CACHE_CLOSE_FREE is set, also free memory
        if (CACHE_CHECK_BIT(CACHE_CLOSE_FREE, flags))
        {
            m_cache.erase(make_pair((*cache_entry)->m_cache_info.m_destfile, (*cache_entry)->m_cache_info.m_desttype.data()));

            deleted = (*cache_entry)->destroy();
            *cache_entry = nullptr;
        }
    }

    return deleted;
}

Cache_Entry *Cache::openio(LPVIRTUALFILE virtualfile)
{
    Cache_Entry* cache_entry = nullptr;
    cache_t::const_iterator p = m_cache.find(make_pair(virtualfile->m_destfile, params.current_format(virtualfile)->desttype()));
    if (p == m_cache.cend())
    {
        Logging::trace(virtualfile->m_destfile, "Created new transcoder.");
        cache_entry = create_entry(virtualfile, params.current_format(virtualfile)->desttype());
    }
    else
    {
        Logging::trace(virtualfile->m_destfile, "Reusing cached transcoder.");
        cache_entry = p->second;
    }

    return cache_entry;
}

bool Cache::closeio(Cache_Entry **cache_entry, int flags /*= CACHE_CLOSE_NOOPT*/)
{
    if (*cache_entry == nullptr)
    {
        return true;
    }

    bool deleted;

    std::string filename((*cache_entry)->filename());
    if (delete_entry(cache_entry, flags))
    {
        Logging::trace(filename, "Freed cache entry.");
        deleted = true;
    }
    else
    {
        Logging::trace(filename, "Keeping cache entry.");
        deleted = false;
    }

    return deleted;
}

bool Cache::prune_expired()
{
    if (params.m_expiry_time <= 0)
    {
        // There's no limit.
        return true;
    }

    std::vector<cache_key_t> keys;
    sqlite3_stmt * stmt;
    time_t now = time(nullptr);
    std::string sql;

    Logging::trace(m_cacheidx_db->filename(), "Pruning expired cache entries older than %1...", format_time(params.m_expiry_time).c_str());

    strsprintf(&sql, "SELECT filename, desttype, strftime('%%s', access_time) FROM cache_entry WHERE strftime('%%s', access_time) + %" FFMPEGFS_FORMAT_TIME_T " < %" FFMPEGFS_FORMAT_TIME_T ";\n", params.m_expiry_time, now);

    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    sqlite3_prepare(*m_cacheidx_db, sql.c_str(), -1, &stmt, nullptr);

    int ret = 0;
    while ((ret = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const char *filename = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        const char *desttype = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));

        keys.emplace_back(filename, desttype);

        Logging::trace(filename, "Found %1 old entries.", format_time(now - static_cast<time_t>(sqlite3_column_int64(stmt, 2))).c_str());
    }

    Logging::trace(m_cacheidx_db->filename(), "%1 expired cache entries found.", keys.size());

    if (ret == SQLITE_DONE)
    {
        for (const auto& [key, value] : keys)
        {
            Logging::trace(m_cacheidx_db->filename(), "Pruning '%1' - Type: %2", key.c_str(), value.c_str());

            cache_t::iterator p = m_cache.find(make_pair(key, value));
            if (p != m_cache.end())
            {
                delete_entry(&p->second, CACHE_CLOSE_DELETE);
            }

            if (delete_info(key, value))
            {
                remove_cachefile(key, value);
            }
        }
    }
    else
    {
        Logging::error(m_cacheidx_db->filename(), "Failed to execute select. Return code: %1 Error: %2 SQL: %3", ret, sqlite3_errmsg(*m_cacheidx_db), expanded_sql(stmt).c_str());
    }

    sqlite3_finalize(stmt);

    return true;
}

bool Cache::prune_cache_size()
{
    if (!params.m_max_cache_size)
    {
        // There's no limit.
        return true;
    }

    std::vector<cache_key_t> keys;
    std::vector<size_t> filesizes;
    sqlite3_stmt * stmt;
    const char * sql;

    Logging::trace(m_cacheidx_db->filename(), "Pruning oldest cache entries exceeding %1 cache size...", format_size(params.m_max_cache_size).c_str());

    sql = "SELECT filename, desttype, encoded_filesize FROM cache_entry ORDER BY access_time ASC;\n";

    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    sqlite3_prepare(*m_cacheidx_db, sql, -1, &stmt, nullptr);

    int ret = 0;
    size_t total_size = 0;
    while((ret = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const char *filename = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        const char *desttype = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        size_t size = static_cast<size_t>(sqlite3_column_int64(stmt, 2));

        keys.emplace_back(filename, desttype);
        filesizes.push_back(size);
        total_size += size;
    }

    Logging::trace(m_cacheidx_db->filename(), "%1 in cache.", format_size(total_size).c_str());

    if (total_size > params.m_max_cache_size)
    {
        Logging::trace(m_cacheidx_db->filename(), "Pruning %1 of oldest cache entries to limit cache size.", format_size(total_size - params.m_max_cache_size).c_str());
        if (ret == SQLITE_DONE)
        {
            size_t n = 0;
            for (const auto& [key, value] : keys)
            {
                Logging::trace(m_cacheidx_db->filename(), "Pruning: %1 Type: %2", key.c_str(), value.c_str());

                cache_t::iterator p = m_cache.find(make_pair(key, value));
                if (p != m_cache.end())
                {
                    delete_entry(&p->second, CACHE_CLOSE_DELETE);
                }

                if (delete_info(key, value))
                {
                    remove_cachefile(key, value);
                }

                total_size -= filesizes[n++];

                if (total_size <= params.m_max_cache_size)
                {
                    break;
                }
            }

            Logging::trace(m_cacheidx_db->filename(), "%1 left in cache.", format_size(total_size).c_str());
        }
        else
        {
            Logging::error(m_cacheidx_db->filename(), "Failed to execute select. Return code: %1 Error: %2 SQL: %3", ret, sqlite3_errmsg(*m_cacheidx_db), expanded_sql(stmt).c_str());
        }
    }

    sqlite3_finalize(stmt);

    return true;
}

bool Cache::prune_disk_space(size_t predicted_filesize)
{
    std::string cachepath;

    transcoder_cache_path(&cachepath);

    size_t free_bytes = get_disk_free(cachepath);

    if (!free_bytes && errno)
    {
        if (errno == ENOENT)
        {
            // Cache path does not exist. Strange problem, but not error. Ignore silently.
            return true;
        }

        Logging::error(cachepath, "prune_disk_space() cannot determine free disk space: (%1) %2", errno, strerror(errno));
        return false;
    }

    if (free_bytes < predicted_filesize)
    {
        Logging::error(cachepath, "prune_disk_space() : Insufficient disk space %1 on cache drive, at least %2 required.", format_size(free_bytes).c_str(), format_size(predicted_filesize).c_str());
        errno = ENOSPC;
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    Logging::trace(cachepath, "%1 disk space before prune.", format_size(free_bytes).c_str());
    if (free_bytes < params.m_min_diskspace + predicted_filesize)
    {
        std::vector<cache_key_t> keys;
        std::vector<size_t> filesizes;
        sqlite3_stmt * stmt;
        const char * sql;

        sql = "SELECT filename, desttype, encoded_filesize FROM cache_entry ORDER BY access_time ASC;\n";

        sqlite3_prepare(*m_cacheidx_db, sql, -1, &stmt, nullptr);

        int ret = 0;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            const char *filename = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            const char *desttype = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            size_t size = static_cast<size_t>(sqlite3_column_int64(stmt, 2));

            keys.emplace_back(filename, desttype);
            filesizes.push_back(size);
        }

        Logging::trace(cachepath, "Pruning %1 of oldest cache entries to keep disk space above %2 limit...", format_size(params.m_min_diskspace + predicted_filesize - free_bytes).c_str(), format_size(params.m_min_diskspace).c_str());

        if (ret == SQLITE_DONE)
        {
            size_t n = 0;
            for (const auto& [key, value] : keys)
            {
                Logging::trace(cachepath, "Pruning: %1 Type: %2", key.c_str(), value.c_str());

                cache_t::iterator p = m_cache.find(make_pair(key, value));
                if (p != m_cache.end())
                {
                    delete_entry(&p->second, CACHE_CLOSE_DELETE);
                }

                if (delete_info(key, value))
                {
                    remove_cachefile(key, value);
                }

                free_bytes += filesizes[n++];

                if (free_bytes >= params.m_min_diskspace + predicted_filesize)
                {
                    break;
                }
            }
            Logging::trace(cachepath, "Disk space after prune: %1", format_size(free_bytes).c_str());
        }
        else
        {
            Logging::error(cachepath, "Failed to execute select. Return code: %1 Error: %2 SQL: %3", ret, sqlite3_errmsg(*m_cacheidx_db), expanded_sql(stmt).c_str());
        }

        sqlite3_finalize(stmt);
    }

    return true;
}

bool Cache::maintenance(size_t predicted_filesize)
{
    bool success = true;

    // Find and remove expired cache entries
    success &= prune_expired();

    // Check max. cache size
    success &= prune_cache_size();

    // Check min. diskspace required for cache
    success &= prune_disk_space(predicted_filesize);

    return success;
}

bool Cache::clear()
{
    bool success = true;

    std::lock_guard<std::recursive_mutex> lock_mutex(m_mutex);

    std::vector<cache_key_t> keys;
    sqlite3_stmt * stmt;
    const char * sql;

    sql = "SELECT filename, desttype FROM cache_entry;\n";

    sqlite3_prepare(*m_cacheidx_db, sql, -1, &stmt, nullptr);

    int ret = 0;
    while((ret = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const char *filename = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        const char *desttype = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));

        keys.emplace_back(filename, desttype);
    }

    Logging::trace(m_cacheidx_db->filename(), "Clearing all %1 entries from cache...", keys.size());

    if (ret == SQLITE_DONE)
    {
        for (const auto& [key, value] : keys)
        {
            Logging::trace(m_cacheidx_db->filename(), "Pruning: %1 Type: %2", key.c_str(), value.c_str());

            cache_t::iterator p = m_cache.find(make_pair(key, value));
            if (p != m_cache.end())
            {
                delete_entry(&p->second, CACHE_CLOSE_DELETE);
            }

            if (delete_info(key, value))
            {
                remove_cachefile(key, value);
            }
        }
    }
    else
    {
        Logging::error(m_cacheidx_db->filename(), "Failed to execute select. Return code: %1 Error: %2 SQL: %3", ret, sqlite3_errmsg(*m_cacheidx_db), expanded_sql(stmt).c_str());
    }

    sqlite3_finalize(stmt);

    return success;
}

bool Cache::remove_cachefile(const std::string & filename, const std::string & fileext)
{
    std::string cachefile;
    bool success;

    Buffer::make_cachefile_name(&cachefile, filename, fileext, false);

    success = Buffer::remove_file(cachefile);

    Buffer::make_cachefile_name(&cachefile, filename, fileext, true);

    if (!Buffer::remove_file(cachefile) && errno != ENOENT)
    {
        success = false;
    }

    return success;
}

std::string Cache::expanded_sql(sqlite3_stmt *pStmt)
{
    std::string sql;
#ifdef HAVE_SQLITE_EXPANDED_SQL
    char * p = sqlite3_expanded_sql(pStmt);
    sql = p;
    sqlite3_free(p);
#else
    const char *p = sqlite3_sql(pStmt);
    if (p != nullptr)
    {
        sql=p;
    }
    else
    {
        sql="(nullptr)";
    }
#endif
    return sql;
}
