/*
 * Logging class source for ffmpegfs
 *
 * Copyright (C) 2017-2018 K. Henriksson
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

#include "logging.h"
#include "ffmpeg_utils.h"

#include <cstdarg>
#include <iostream>
#include <syslog.h>
#include <ostream>

namespace
{
Logging* logging;
}

Logging::Logging(string logfile, level max_level, bool to_stderr, bool to_syslog) :
    max_level_(max_level),
    to_stderr_(to_stderr),
    to_syslog_(to_syslog)
{
    if (!logfile.empty())
    {
        logfile_.open(logfile);
    }
    if (to_syslog_)
    {
        openlog(PACKAGE, 0, LOG_USER);
    }
}

Logging::Logger::~Logger()
{
    if (!logging_ || loglevel_ > logging_->max_level_) return;

    // Construct string containing time
    time_t now = time(nullptr);
    string time_string(30, '\0');
    time_string.resize(strftime(&time_string[0], time_string.size(),
                       "%F %T", localtime(&now)));

    string msg = "[" + time_string + "] " +
            level_name_map_.at(loglevel_) + ": " + str();

    rtrim(msg);

    if (logging_->to_syslog_)
    {
        syslog(syslog_level_map_.at(loglevel_), "%s", msg.c_str());
    }
    if (logging_->logfile_.is_open()) logging_->logfile_ << msg << endl;
    if (logging_->to_stderr_) clog << msg << endl;
}

const map<Logging::level,int> Logging::Logger::syslog_level_map_ =
{
    {ERROR,     LOG_ERR},
    {WARNING,   LOG_WARNING},
    {INFO,      LOG_INFO},
    {DEBUG,     LOG_DEBUG},
    {TRACE,     LOG_DEBUG},
};

const map<Logging::level,string> Logging::Logger::level_name_map_ =
{
    {ERROR,     "ERROR  "},
    {WARNING,   "WARNING"},
    {INFO,      "INFO   "},
    {DEBUG,     "DEBUG  "},
    {TRACE,     "TRACE  "},
};

Logging::Logger Log(Logging::level lev)
{
    return {lev, logging};
}

bool InitLogging(string logfile, Logging::level max_level, bool to_stderr, bool to_syslog)
{
    logging = new Logging(logfile, max_level, to_stderr, to_syslog);
    return !logging->GetFail();
}

void log_with_level(Logging::level level, const char *filename, const char* format, va_list ap)
{
    log_with_level(level, "", filename, format, ap);
}

void log_with_level(Logging::level level, const char* prefix, const char* filename, const char* format, va_list ap)
{
    // This copy is because we call vsnprintf twice, and ap is undefined after
    // the first call.
    va_list ap2;
    va_copy(ap2, ap);

    int size = vsnprintf(nullptr, 0, format, ap);
    string buffer(size, '\0');
    vsnprintf(&buffer[0], buffer.size() + 1, format, ap2);

    va_end(ap2);

    if (filename == NULL)
    {
        Log(level) << prefix << buffer;
    }
    else
    {
        Log(level) << prefix << buffer << " [" << filename << "]";
    }
}

void log_with_level(Logging::level level, const char* prefix, const char *filename, const char* message)
{
    if (filename == NULL)
    {
        Log(level) << prefix << message;
    }
    else
    {
        Log(level) << prefix << message << " [" << filename << "]";
    }
}
