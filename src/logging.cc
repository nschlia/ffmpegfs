/*
 * Copyright (C) 2017-2019 K. Henriksson
 * Extensions (c) 2017 by Norbert Schlia (nschlia@oblivion-software.de)
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

/**
 * @file
 * @brief Log facilities implementation
 *
 * @ingroup ffmpegfs
 *
 * @author K. Henriksson, Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017 Original author K. Henriksson @n
 * Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "logging.h"
#include "ffmpeg_utils.h"

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

namespace
{
Logging* logging;
}

Logging::Logging(const std::string &logfile, level max_level, bool to_stderr, bool to_syslog) :
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
    if (!m_logging || m_loglevel > m_logging->m_max_level)
    {
        return;
    }

    // Construct string containing time
    time_t now = time(nullptr);
    std::string time_string(30, '\0');
    std::string msg;

    time_string.resize(strftime(&time_string[0], time_string.size(), "%F %T ", localtime(&now)));   // Mind the blank at the end

    msg = time_string + m_level_name_map.at(m_loglevel) + ": ";

    if (!m_filename.empty())
    {
        msg += "[";
        msg += m_filename;
        msg += "] ";
    }

    msg += str();
    rtrim(msg);

    if (m_logging->m_to_syslog)
    {
        syslog(m_syslog_level_map.at(m_loglevel), "%s", msg.c_str());
    }

    if (m_logging->m_logfile.is_open())
    {
        m_logging->m_logfile << msg << std::endl;
    }

    if (m_logging->m_to_stderr)
    {
        msg = COLOUR_DARK_GRAY + time_string + m_level_colour_map.at(m_loglevel) + m_level_name_map.at(m_loglevel) + COLOUR_RESET + ": ";

        if (!m_filename.empty())
        {
            msg += COLOUR_LIGHT_PURPLE;
            msg += "[";
            msg += m_filename;
            msg += "] ";
            msg += COLOUR_RESET;
        }

        msg += str();
        rtrim(msg);

        std::clog << msg << std::endl;
    }
}

bool Logging::GetFail() const
{
    return m_logfile.fail();
}

const std::map<Logging::level, int> Logging::Logger::m_syslog_level_map =
{
    { ERROR,     LOG_ERR },
    { WARNING,   LOG_WARNING },
    { INFO,      LOG_INFO },
    { DEBUG,     LOG_DEBUG },
    { TRACE,     LOG_DEBUG },
};

const std::map<Logging::level, std::string> Logging::Logger::m_level_name_map =
{
    { ERROR,     "ERROR  " },
    { WARNING,   "WARNING" },
    { INFO,      "INFO   " },
    { DEBUG,     "DEBUG  " },
    { TRACE,     "TRACE  " },
};

const std::map<Logging::level, std::string> Logging::Logger::m_level_colour_map =
{
    { ERROR,     COLOUR_RED },
    { WARNING,   COLOUR_YELLOW },
    { INFO,      COLOUR_WHITE },
    { DEBUG,     COLOUR_GREEN },
    { TRACE,     COLOUR_BLUE },
};

Logging::Logger Log(Logging::level loglevel, const std::string & filename)
{
    return {loglevel, filename, logging};
}

bool Logging::init_logging(const std::string & logfile, Logging::level max_level, bool to_stderr, bool to_syslog)
{
    logging = new(std::nothrow) Logging(logfile, max_level, to_stderr, to_syslog);
    if (logging == nullptr)
    {
        return false;   // Out of memory...
    }
    return !logging->GetFail();
}

void Logging::log_with_level(Logging::level loglevel, const std::string & filename, const std::string & message)
{
    Log(loglevel, filename) << message;
}

std::string Logging::format_helper(
        const std::string &string_to_update,
        const size_t)
{
    return string_to_update;
}
