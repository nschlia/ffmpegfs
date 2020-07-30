/*
 * Copyright (C) 2017 Original author K. Henriksson @n
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
 * @brief Provide various log facilities to stderr, disk or syslog
 *
 * @ingroup ffmpegfs
 *
 * @author K. Henriksson, Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017 Original author K. Henriksson @n
 * Copyright (C) 2017-2020 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef LOGGING_H
#define LOGGING_H

#pragma once

#include <map>
#include <fstream>
#include <sstream>
#include <regex>
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
 * The format specifier is the same as used in printf/sprintf.
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
    enum class level
    {
        ERROR = 1,      /**< @brief Error level */
        WARNING = 2,    /**< @brief Warning level */
        INFO = 3,       /**< @brief Info level */
        DEBUG = 4,      /**< @brief Debug level */
        TRACE = 5       /**< @brief Error level */
    };

    /**
     * Construct Logging object
	 *
     * @param[in] logfile - The name of a file to write logging output to. If empty, no output will be written.
     * @param[in] max_level - The maximum level of log output to write.
     * @param[in] to_stderr - Whether to write log output to stderr.
     * @param[in] to_syslog - Whether to write log output to syslog.
     *
     * @return Returns 0 if successful; if <0 on this is an FFmpeg AVERROR value
     */
    explicit Logging(const std::string & logfile, level max_level, bool to_stderr, bool to_syslog);

    /**
     * @brief Check whether either failbit or badbit is set
     * @return Returns true if either (or both) the failbit or the badbit error state flags is set for the stream.
     */
    bool GetFail() const;

private:
    /**
     * @brief Logging helper class
     */
    class Logger : public std::ostringstream
    {
    public:
        /**
         * @brief Construct Logger object
         * @param[in] loglevel - The maximum level of log output to write.
         * @param[in] filename - Name of file for which this log entry was written. May be empty.
         * @param[in] logging - Corresponding Logging object
         */
        Logger(level loglevel, const std::string & filename, Logging* logging) :
            m_loglevel(loglevel),
            m_filename(filename),
            m_logging(logging) {}
        /**
         * @brief Construct Logger object
         */
        explicit Logger() :
            m_loglevel(level::DEBUG) {}
        /**
         * @brief Destroy Logger object
         */
        virtual ~Logger();

    private:
        const level         m_loglevel;                                     /**< @brief Log level required to write log entry */

        const std::string   m_filename;                                     /**< @brief Name of file for which this log entry was written. May be empty. */
        Logging*            m_logging;                                      /**< @brief Corresponding Logging object */

        static const std::map<level, int>           m_syslog_level_map;     /**< @brief Map our log levels to syslog levels */
        static const std::map<level, std::string>   m_level_name_map;       /**< @brief Map log level enums to strings */
        static const std::map<level, std::string>   m_level_colour_map;     /**< @brief Map log level enums to colours (logging to stderr only) */
    };

public:
    /**
     * @brief Initialise the logging facility.
     * @param[in] logfile - The name of a file to write logging output to. If empty, no output will be written.
     * @param[in] max_level - The maximum level of log output to write.
     * @param[in] to_stderr - Whether to write log output to stderr.
     * @param[in] to_syslog - Whether to write log output to syslog.
     * @return On success, returns true. On error, returns false.
     * @note Will only fail if the file could not be opened. Writing to stderr or syslog will never fail. errno is not set.
     */
    static bool init_logging(const std::string & logfile, Logging::level max_level, bool to_stderr, bool to_syslog);

    /**
     * @brief Write trace level log entry
     * @param[in] filename - Name of file for which this log entry was written. May be nullptr.
     * @param[in] format_string - Format string in ffmpegfs logger format.
     * @param[in] args - 0 or more argments for format. See @ref format.
     */
    template <typename... Args>
    static void trace(const char *filename, const std::string &format_string, Args &&...args)
    {
        log_with_level(Logging::level::TRACE, filename != nullptr ? filename : "", format_helper(format_string, 1, std::forward<Args>(args)...));
    }
    /**
     * @brief Write trace level log entry
     * @param[in] filename - Name of file for which this log entry was written. May be empty.
     * @param[in] format_string - Format string in ffmpegfs logger format.
     * @param[in] args - 0 or more argments for format. See @ref format.
     */
    template <typename... Args>
    static void trace(const std::string &filename, const std::string &format_string, Args &&...args)
    {
        log_with_level(Logging::level::TRACE, filename, format_helper(format_string, 1, std::forward<Args>(args)...));
    }

    /**
     * @brief Write debug level log entry
     * @param[in] filename - Name of file for which this log entry was written. May be nullptr.
     * @param[in] format_string - Format string in ffmpegfs logger format.
     * @param[in] args - 0 or more argments for format. See @ref format.
     */
    template <typename... Args>
    static void debug(const char * filename, const std::string &format_string, Args &&...args)
    {
        log_with_level(Logging::level::DEBUG, filename != nullptr ? filename : "", format_helper(format_string, 1, std::forward<Args>(args)...));
    }
    /**
     * @brief Write debug level log entry
     * @param[in] filename - Name of file for which this log entry was written. May be empty.
     * @param[in] format_string - Format string in ffmpegfs logger format.
     * @param[in] args - 0 or more argments for format. See @ref format.
     */
    template <typename... Args>
    static void debug(const std::string & filename, const std::string &format_string, Args &&...args)
    {
        log_with_level(Logging::level::DEBUG, filename, format_helper(format_string, 1, std::forward<Args>(args)...));
    }

    /**
     * @brief Write info level log entry
     * @param[in] filename - Name of file for which this log entry was written. May be nullptr.
     * @param[in] format_string - Format string in ffmpegfs logger format.
     * @param[in] args - 0 or more argments for format. See @ref format.
     */
    template <typename... Args>
    static void info(const char *filename, const std::string &format_string, Args &&...args)
    {
        log_with_level(Logging::level::INFO, filename != nullptr ? filename : "", format_helper(format_string, 1, std::forward<Args>(args)...));
    }
    /**
     * @brief Write info level log entry
     * @param[in] filename - Name of file for which this log entry was written. May be empty.
     * @param[in] format_string - Format string in ffmpegfs logger format.
     * @param[in] args - 0 or more argments for format. See @ref format.
     */
    template <typename... Args>
    static void info(const std::string &filename, const std::string &format_string, Args &&...args)
    {
        log_with_level(Logging::level::INFO, filename, format_helper(format_string, 1, std::forward<Args>(args)...));
    }

    /**
     * @brief Write warning level log entry
     * @param[in] filename - Name of file for which this log entry was written. May be nullptr.
     * @param[in] format_string - Format string in ffmpegfs logger format.
     * @param[in] args - 0 or more argments for format. See @ref format.
     */
    template <typename... Args>
    static void warning(const char *filename, const std::string &format_string, Args &&...args)
    {
        log_with_level(Logging::level::WARNING, filename != nullptr ? filename : "", format_helper(format_string, 1, std::forward<Args>(args)...));
    }
    /**
     * @brief Write warning level log entry
     * @param[in] filename - Name of file for which this log entry was written. May be empty.
     * @param[in] format_string - Format string in ffmpegfs logger format.
     * @param[in] args - 0 or more argments for format. See @ref format.
     */
    template <typename... Args>
    static void warning(const std::string &filename, const std::string &format_string, Args &&...args)
    {
        log_with_level(Logging::level::WARNING, filename, format_helper(format_string, 1, std::forward<Args>(args)...));
    }

    /**
     * @brief Write error level log entry
     * @param[in] filename - Name of file for which this log entry was written. May be nullptr.
     * @param[in] format_string - Format string in ffmpegfs logger format.
     * @param[in] args - 0 or more argments for format. See @ref format.
     */
    template <typename... Args>
    static void error(const char *filename, const std::string &format_string, Args &&...args)
    {
        log_with_level(Logging::level::ERROR, filename != nullptr ? filename : "", format_helper(format_string, 1, std::forward<Args>(args)...));
    }
    /**
     * @brief Write error level log entry
     * @param[in] filename - Name of file for which this log entry was written. May be empty.
     * @param[in] format_string - Format string in ffmpegfs logger format.
     * @param[in] args - 0 or more argments for format. See @ref format.
     */
    template <typename... Args>
    static void error(const std::string &filename, const std::string &format_string, Args &&...args)
    {
        log_with_level(Logging::level::ERROR, filename, format_helper(format_string, 1, std::forward<Args>(args)...));
    }

    /**
     * @brief Write log entry
     * @param[in] loglevel - The maximum level of log output to write.
     * @param[in] filename - Name of file for which this log entry was written. May be empty.
     * @param[in] message - Message to log
     */
    static void log_with_level(Logging::level loglevel, const std::string & filename, const std::string & message);

protected:
    /**
     * @brief Standard format_helper without parameters
     * @param[in] string_to_update - original string
     * @return Original string
     */
    static std::string format_helper(
            const std::string &string_to_update,
            const size_t);

    /**
     * @brief format_helper with variadic parameters
     *
     * Calls itself recurisively until all tokens are replaced.
     *
     * @param[in] string_to_search - format string to be searched
     * @param[in] index_to_replace - index numer (%n) to be replaced. May be present 0...x times.
     * @param[in] val - replace value to fill into tokens
     * @param[in] args - further arguments
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
                size_t size = static_cast<size_t>(std::snprintf(nullptr, 0, res[2].str().c_str(), val)) + 1;
                std::vector<char> buffer;
                buffer.resize(size);
                std::snprintf(buffer.data(), size, res[2].str().c_str(), val);
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
     * @param[in] format_string - format string to be searched
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
     * @param[in] loglevel - Selected loglevel.
     * @param[in] filename - If logging to file, use this file name.
     */
    friend Logger Log(level loglevel, const std::string & filename);
    friend Logger;                                                      /**< @brief Make logger class our friend */

    std::ofstream   m_logfile;                      /**< @brief Log file object for writing to disk */
    const level     m_max_level;                    /**< @brief The maximum level of log output to write. */
    const bool      m_to_stderr;                    /**< @brief Whether to write log output to stderr. */
    const bool      m_to_syslog;                    /**< @brief Whether to write log output to syslog. */
};

constexpr auto ERROR    = Logging::level::ERROR;    /**< @brief Shorthand for log level ERROR */
constexpr auto WARNING  = Logging::level::WARNING;  /**< @brief Shorthand for log level WARNING */
constexpr auto INFO     = Logging::level::INFO;     /**< @brief Shorthand for log level INFO */
constexpr auto DEBUG    = Logging::level::DEBUG;    /**< @brief Shorthand for log level DEBUG */
constexpr auto TRACE    = Logging::level::TRACE;    /**< @brief Shorthand for log level TRACE */

#endif
