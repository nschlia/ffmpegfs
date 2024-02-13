/*
 * Copyright (C) 2017 Original author K. Henriksson @n
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
 * @file logging.h
 * @brief Provide various log facilities to stderr, disk or syslog
 *
 * @ingroup ffmpegfs
 *
 * @author K. Henriksson, Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017 Original author K. Henriksson @n
 * Copyright (C) 2017-2024 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef LOGGING_H
#define LOGGING_H

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <map>
#include <fstream>
#include <sstream>
#include <regex>
#include <mutex>

/**
 * @brief #Logging facility
 *
 * @anchor format Within the log message text, escape sequences are supported for dynamically formatting the message.
 *
 * Escape sequences have either the form %n or %\<format specifier\>n, where the index n starts at 1.
 *
 * The index specifies the position of the argument in the list. They can appear in any given order.
 * An index can be used more than once, and not all indexes have to be present (in which case the
 * argument will not be printed). The input value is automatically converted to string format and
 * can be among std::string, char *, int, uint64_t and more.
 *
 * The format specifier is the same as used in printf and sprintf.
 *
 * @code
 * int channels = 2;
 * int sample_rate = 44100:
 * Logging::debug(filename, "Audio %1 channels %2 KHz", channels, sample_rate);
 * @endcode
 *
 * Prints "Audio 2 channels 44100 KHz".
 *
 * @code
 * int channels = 2;
 * double sample_rate = 44.1:
 * Logging::debug(filename, "Audio %1 %<%.3f>2 KHz", channels == 2 ? "stereo" : "mono", sample_rate);
 * @endcode
 *
 * Prints "Audio stereo 44.100 KHz".
 */
class Logging
{
public:
    /** @brief Logging level types enum
     */
    enum class LOGLEVEL
    {
        LOGERROR = 1,       /**< @brief Error level */
        LOGWARN = 2,        /**< @brief Warning level */
        LOGINFO = 3,        /**< @brief Info level */
        LOGDEBUG = 4,       /**< @brief Debug level */
        LOGTRACE = 5        /**< @brief Error level */
    };

    /**
     * Construct Logging object.
     *
     * @param[in] logfile - The name of a file to write logging output to. If empty, no output will be written.
     * @param[in] max_level - The maximum level of log output to write.
     * @param[in] to_stderr - Whether to write log output to stderr.
     * @param[in] to_syslog - Whether to write log output to syslog.
     */
    explicit Logging(const std::string & logfile, LOGLEVEL max_level, bool to_stderr, bool to_syslog);

    /**
     * @brief Check whether either failbit or badbit is set.
     * @return Returns true if either (or both) the failbit or the badbit error state flags are set for the stream.
     */
    bool GetFail() const;

private:
    /**
     * @brief Logging helper class.
     */
    class Logger : public std::ostringstream
    {
    public:
        /**
         * @brief Construct Logger object.
         * @param[in] loglevel - The maximum level of log output to write.
         * @param[in] filename - Name of file for which this log entry was written. May be empty.
         */
        Logger(LOGLEVEL loglevel, const std::string & filename) :
            m_loglevel(loglevel),
            m_filename(filename) {}
        /**
         * @brief Construct Logger object
         */
        explicit Logger() :
            m_loglevel(LOGLEVEL::LOGDEBUG) {}
        /**
         * @brief Destroy Logger object
         */
        virtual ~Logger();

    private:
        const LOGLEVEL      m_loglevel;                                     /**< @brief Log level required to write log entry */

        const std::string   m_filename;                                     /**< @brief Name of file for which this log entry was written. May be empty. */

        static const std::map<LOGLEVEL, int>           m_syslog_level_map;  /**< @brief Map our log levels to syslog levels */
        static const std::map<LOGLEVEL, std::string>   m_level_name_map;    /**< @brief Map log level enums to strings */
        static const std::map<LOGLEVEL, std::string>   m_level_colour_map;  /**< @brief Map log level enums to colours (logging to stderr only) */
    };

public:
    /**
     * @brief Initialise the logging facility.
     * @param[in] logfile - The name of a file to write logging output to. If empty, no output will be written.
     * @param[in] max_level - The maximum level of log output to write.
     * @param[in] to_stderr - Whether to write log output to stderr.
     * @param[in] to_syslog - Whether to write log output to syslog.
     * @return On success, returns true. On error, returns false.
     * @note It will only fail if the file cannot be opened. Writing to stderr or syslog will never fail. errno is not set.
     */
    static bool init_logging(const std::string & logfile, LOGLEVEL max_level, bool to_stderr, bool to_syslog);

    /**
     * @brief Write trace level log entry.
     * @param[in] filename - Name of the file for which this log entry was written. May be empty.
     * @param[in] format_string - Format a string in FFmpegfs logger format.
     * @param[in] args - 0 or more format arguments. See @ref format.
     */
    template <typename T, typename... Args>
    static void trace(const T filename, const std::string &format_string, Args &&...args)
    {
        LOGLEVEL loglevel = LOGLEVEL::LOGTRACE;

        if (!show(loglevel))
        {
            return;
        }

        log_with_level(loglevel, filename, format_helper(format_string, 1, std::forward<Args>(args)...));
    }

    /**
     * @brief Write debug level log entry.
     * @param[in] filename - Name of the file for which this log entry was written. May be empty.
     * @param[in] format_string - Format a string in FFmpegfs logger format.
     * @param[in] args - 0 or more format arguments. See @ref format.
     */
    template <typename T, typename... Args>
    static void debug(const T filename, const std::string &format_string, Args &&...args)
    {
        LOGLEVEL loglevel = LOGLEVEL::LOGDEBUG;

        if (!show(loglevel))
        {
            return;
        }

        log_with_level(loglevel, filename, format_helper(format_string, 1, std::forward<Args>(args)...));
    }

    /**
     * @brief Write info level log entry.
     * @param[in] filename - Name of the file for which this log entry was written. May be empty.
     * @param[in] format_string - Format a string in FFmpegfs logger format.
     * @param[in] args - 0 or more format arguments. See @ref format.
     */
    template <typename T, typename... Args>
    static void info(const T filename, const std::string &format_string, Args &&...args)
    {
        LOGLEVEL loglevel = LOGLEVEL::LOGINFO;

        if (!show(loglevel))
        {
            return;
        }

        log_with_level(loglevel, filename, format_helper(format_string, 1, std::forward<Args>(args)...));
    }

    /**
     * @brief Write warning level log entry.
     * @param[in] filename - Name of the file for which this log entry was written. May be empty.
     * @param[in] format_string - Format a string in FFmpegfs logger format.
     * @param[in] args - 0 or more format arguments. See @ref format.
     */
    template <typename T, typename... Args>
    static void warning(const T filename, const std::string &format_string, Args &&...args)
    {
        LOGLEVEL loglevel = LOGLEVEL::LOGWARN;

        if (!show(loglevel))
        {
            return;
        }

        log_with_level(loglevel, filename, format_helper(format_string, 1, std::forward<Args>(args)...));
    }

    /**
     * @brief Write error level log entry.
     * @param[in] filename - Name of the file for which this log entry was written. May be empty.
     * @param[in] format_string - Format a string in FFmpegfs logger format.
     * @param[in] args - 0 or more format arguments. See @ref format.
     */
    template <typename T, typename... Args>
    static void error(const T filename, const std::string &format_string, Args &&...args)
    {
        LOGLEVEL loglevel = LOGLEVEL::LOGERROR;

        if (!show(loglevel))
        {
            return;
        }

        log_with_level(loglevel, filename, format_helper(format_string, 1, std::forward<Args>(args)...));
    }

    /**
     * @brief Write log entry
     * @param[in] loglevel - The level of log this message is for.
     * @param[in] filename - Name of the file for which this log entry was written. May be nullptr.
     * @param[in] message - Message to log.
     */
    static void log_with_level(LOGLEVEL loglevel, const char *filename, const std::string & message);

    /**
     * @brief Write log entry
     * @param[in] loglevel - The level of log this message is for.
     * @param[in] filename - Name of the file for which this log entry was written. May be empty.
     * @param[in] message - Message to log.
     */
    static void log_with_level(LOGLEVEL loglevel, const std::string & filename, const std::string & message);

    /**
     * @brief Check if log entry should be displayed at the current log level.
     * @param[in] loglevel - Log level of log entry.
     * @return True, if entry should be be shown; false if not.
     */
    static bool show(LOGLEVEL loglevel)
    {
        return (m_logging && loglevel <= m_logging->m_max_level);
    }

private:
    /**
     * @brief Standard format_helper without parameters.
     * @param[in] string_to_update - Original string.
     * @param[in] index_to_replace - unused
     * @return Returns original string.
     */
    static std::string format_helper(
            const std::string &string_to_update,
            const size_t __attribute__((unused)) index_to_replace);

    // std::string kann man eigentlich nicht an %s übergeben. So geht es doch...

    template<class T, typename std::enable_if<!std::is_same<T, std::string>::value, bool>::type = true>
    static T & fix_std_string(T & val)
    {
        return val;
    }

    static const char * fix_std_string(const std::string & val)
    {
        return val.c_str();
    }

    /**
     * @brief format_helper with variadic parameters.
     *
     * Calls itself recursively until all tokens are replaced.
     *
     * @param[in] string_to_search - format string to be searched.
     * @param[in] index_to_replace - index number (%n) to be replaced. May be present 0...x times.
     * @param[in] val - Replacement value to fill in tokens.
     * @param[in] args - Further arguments.
     * @return Contents of string_to_search with all tokens replaced.
     */
    template <typename T, typename... Args>
    static std::string format_helper(
            const std::string &string_to_search,
            const size_t index_to_replace,
            T &&val,
            Args &&...args)
    {
        // Match %# exactly (e.g. %12 and %123 literally)

        std::regex exp("%(<([^>]+)>)*" + std::to_string(index_to_replace) + "(?=[^0-9]|$)");
        std::smatch res;
        std::string string_to_update(string_to_search);
        std::string::const_iterator searchStart(string_to_search.cbegin());
        size_t offset = 0;

        while (std::regex_search(searchStart, string_to_search.cend(), res, exp))
        {
            std::ostringstream ostr;

            if (res[2].length())
            {
                // Found match with printf format in res[2]
                size_t size = static_cast<size_t>(std::snprintf(nullptr, 0, res[2].str().c_str(), fix_std_string(val))) + 1;
                std::vector<char> buffer;
                buffer.resize(size);
                std::snprintf(buffer.data(), size, res[2].str().c_str(), fix_std_string(val));
                ostr << buffer.data();
            }
            else
            {
                // No printf format, replace literally
                ostr << val;
            }

            string_to_update.replace(static_cast<size_t>(res.position()) + offset, static_cast<size_t>(res[0].length()), ostr.str());

            offset += static_cast<size_t>(res.position()) + ostr.str().length();

            searchStart = res.suffix().first;
        }

        return format_helper(
                    string_to_update,
                    index_to_replace + 1,
                    std::forward<Args>(args)...);
    }

    /**
     * @brief format string with single token
     *
     * @param[in] format_string - Format string to be searched.
     * @param[in] args - arguments
     * @return Contents of format_string with all tokens replaced.
     */
    template <typename... Args>
    static std::string format(const std::string &format_string, Args &&...args)
    {
        return format_helper(format_string, 1, std::forward<Args>(args)...);
    }

protected:
    /**
     * @brief Make logger class our friend for our constructor
     * @param[in] loglevel - The level of log this message is for.
     * @param[in] filename - Name of the file for which this log entry was written. May be empty.
     */
    friend Logger Log(LOGLEVEL loglevel, const std::string & filename);
    friend Logger;                                                          /**< @brief Make logger class our friend */

    static Logging*                 m_logging;                              /**< @brief Reference to self, Logging is a singleton */
    static std::recursive_mutex     m_mutex;                                /**< @brief Access mutex */
    std::ofstream                   m_logfile;                              /**< @brief Log file object for writing to disk */
    const LOGLEVEL                  m_max_level;                            /**< @brief The maximum level of log output to write. */
    const bool                      m_to_stderr;                            /**< @brief Whether to write log output to stderr. */
    const bool                      m_to_syslog;                            /**< @brief Whether to write log output to syslog. */
};

constexpr Logging::LOGLEVEL LOGERROR    = Logging::LOGLEVEL::LOGERROR;      /**< @brief Shorthand for log level ERROR */
constexpr Logging::LOGLEVEL LOGWARN     = Logging::LOGLEVEL::LOGWARN;       /**< @brief Shorthand for log level WARNING */
constexpr Logging::LOGLEVEL LOGINFO     = Logging::LOGLEVEL::LOGINFO;       /**< @brief Shorthand for log level INFO */
constexpr Logging::LOGLEVEL LOGDEBUG    = Logging::LOGLEVEL::LOGDEBUG;      /**< @brief Shorthand for log level DEBUG */
constexpr Logging::LOGLEVEL LOGTRACE    = Logging::LOGLEVEL::LOGTRACE;      /**< @brief Shorthand for log level TRACE */

#endif
