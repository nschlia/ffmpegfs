/*
 * Copyright (C) 2017 Original author K. Henriksson
 * Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file logging.cc
 * @brief Log facilities implementation
 *
 * @ingroup ffmpegfs
 *
 * @author K. Henriksson, Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017 Original author K. Henriksson @n
 * Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "logging.h"
#include "ffmpegfs.h"

#include <cstdarg>
#include <iostream>
#include <syslog.h>
#include <ostream>

#define COLOUR_BLACK        "\033[0;30m"        /**< @brief ANSI ESC for black foreground */
#define COLOUR_DARK_GRAY    "\033[1;30m"        /**< @brief ANSI ESC for dark gray foreground */
#define COLOUR_LIGHT_GRAY   "\033[0;37m"        /**< @brief ANSI ESC for light gray foreground */
#define COLOUR_RED          "\033[0;31m"        /**< @brief ANSI ESC for red foreground */
#define COLOUR_LIGHT_RED    "\033[1;31m"        /**< @brief ANSI ESC for light red foreground */
#define COLOUR_GREEN        "\033[0;32m"        /**< @brief ANSI ESC for green foreground */
#define COLOUR_LIGHT_GREEN  "\033[1;32m"        /**< @brief ANSI ESC for light green foreground */
#define COLOUR_BROWN_ORANGE "\033[0;33m"        /**< @brief ANSI ESC for brown orange foreground */
#define COLOUR_YELLOW       "\033[1;33m"        /**< @brief ANSI ESC for yellow foreground */
#define COLOUR_BLUE         "\033[0;34m"        /**< @brief ANSI ESC for blue foreground */
#define COLOUR_LIGHT_BLUE   "\033[1;34m"        /**< @brief ANSI ESC for light blue foreground */
#define COLOUR_PURPLE       "\033[0;35m"        /**< @brief ANSI ESC for purple foreground */
#define COLOUR_LIGHT_PURPLE "\033[1;35m"        /**< @brief ANSI ESC for light purple foreground */
#define COLOUR_CYAN         "\033[0;36m"        /**< @brief ANSI ESC for cyan foreground */
#define COLOUR_LIGHT_CYAN   "\033[1;36m"        /**< @brief ANSI ESC for light cyan foreground */
#define COLOUR_WHITE        "\033[1;37m"        /**< @brief ANSI ESC for white foreground */
#define COLOUR_RESET        "\033[0m"           /**< @brief ANSI ESC to reset the foreground colour */

Logging* Logging::m_logging;

std::recursive_mutex Logging::m_mutex;

const std::map<Logging::LOGLEVEL, int> Logging::Logger::m_syslog_level_map =
{
    { LOGERROR,     LOG_ERR },
    { LOGWARN,      LOG_WARNING },
    { LOGINFO,      LOG_INFO },
    { LOGDEBUG,     LOG_DEBUG },
    { LOGTRACE,     LOG_DEBUG },
};

const std::map<Logging::LOGLEVEL, std::string> Logging::Logger::m_level_name_map =
{
    { LOGERROR,     "ERROR  " },
    { LOGWARN,      "WARNING" },
    { LOGINFO,      "INFO   " },
    { LOGDEBUG,     "DEBUG  " },
    { LOGTRACE,     "TRACE  " },
};

const std::map<Logging::LOGLEVEL, std::string> Logging::Logger::m_level_colour_map =
{
    { LOGERROR,     COLOUR_RED },
    { LOGWARN,      COLOUR_YELLOW },
    { LOGINFO,      COLOUR_WHITE },
    { LOGDEBUG,     COLOUR_GREEN },
    { LOGTRACE,     COLOUR_BLUE },
};

Logging::Logging(const std::string &logfile, LOGLEVEL max_level, bool to_stderr, bool to_syslog) :
    m_max_level(max_level),
    m_to_stderr(to_stderr),
    m_to_syslog(to_syslog)
{
    if (!logfile.empty())
    {
        m_logfile.open(logfile);
    }

    if (m_to_syslog)
    {
        openlog(PACKAGE, 0, LOG_USER);
    }
}

Logging::Logger::~Logger()
{
    std::lock_guard<std::recursive_mutex> lck (m_mutex);

    // Construct string containing time
    time_t now = time(nullptr);
    struct tm buffer;
    std::string time_string(30, '\0');
    std::string loglevel;
    std::string filename;
    std::string msg;

    time_string.resize(strftime(&time_string[0], time_string.size(), "%F %T", localtime_r(&now, &buffer)));   // Mind the blank at the end

    loglevel = m_level_name_map.at(m_loglevel) + ":";

    msg = str();
    trim(msg);

    if (!m_filename.empty())
    {
        filename = m_filename;

        if (replace_start(&filename, params.m_basepath))
        {
            filename = "INPUT  [" + filename + "] ";
        }

        else if (replace_start(&filename, params.m_mountpath))
        {
            filename = "OUTPUT [" + filename + "] ";
        }
        else
        {
            std::string cachepath;

            transcoder_cache_path(cachepath);

            if (replace_start(&filename, cachepath + params.m_mountpath + params.m_basepath))
            {
                filename = "CACHE  [" + filename + "] ";
            }
            else
            {
                filename = "OTHER  [" + filename + "] ";
            }
        }
    }

    if (m_logging->m_to_syslog)
    {
        syslog(m_syslog_level_map.at(m_loglevel), "%s %s%s", loglevel.c_str(), filename.c_str(), msg.c_str());
    }

    if (m_logging->m_logfile.is_open())
    {
        m_logging->m_logfile << time_string << " " << loglevel << " " << filename << msg << std::endl;
    }

    if (m_logging->m_to_stderr)
    {
        if (!filename.empty())
        {
            filename = COLOUR_LIGHT_PURPLE + filename + COLOUR_RESET;
        }

        if (m_loglevel <= LOGERROR)
        {
            msg = COLOUR_LIGHT_RED + msg + COLOUR_RESET;
        }

        std::clog << COLOUR_DARK_GRAY << time_string << " " << loglevel << COLOUR_RESET << " " << filename << msg << std::endl;
    }
}

bool Logging::GetFail() const
{
    return m_logfile.fail();
}

Logging::Logger Log(Logging::LOGLEVEL loglevel, const std::string & filename)
{
    return {loglevel, filename};
}

bool Logging::init_logging(const std::string & logfile, LOGLEVEL max_level, bool to_stderr, bool to_syslog)
{
    if (m_logging != nullptr)
    {
        // Do not alloc twice
        return false;
    }

    m_logging = new(std::nothrow) Logging(logfile, max_level, to_stderr, to_syslog);
    if (m_logging == nullptr)
    {
        return false;   // Out of memory...
    }
    return !m_logging->GetFail();
}

void Logging::log_with_level(LOGLEVEL loglevel, const char * filename, const std::string & message)
{
    log_with_level(loglevel, std::string(filename != nullptr ? filename : ""), message);
}

void Logging::log_with_level(LOGLEVEL loglevel, const std::string & filename, const std::string & message)
{
    Log(loglevel, filename) << message;
}

std::string Logging::format_helper(const std::string &string_to_update,
                                   const size_t __attribute__((unused)) size)
{
    return string_to_update;
}
