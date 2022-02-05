/*
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
 */

/**
 * @file vcdutils.cc
 * @brief S/VCD utility functions implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2013-2022 Norbert Schlia (nschlia@oblivion-software.de) @n
 * From BullysPLayer Copyright (C) 1984-2022 by Oblivion Software/Norbert Schlia
 */

#include "vcdutils.h"
#include "ffmpeg_utils.h"

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

namespace VCDUTILS
{
std::string convert_txt2string(const char * txt, int size, bool trimmed)
{
    char * buffer = new(std::nothrow) char[size + 1];
    std::string ret_value;

    if (buffer != nullptr)
    {
        memcpy(buffer, txt, static_cast<size_t>(size));
        *(buffer + size) = '\0';

        ret_value = buffer;

        delete [] buffer;
    }

    if (trimmed)
    {
        return trim(ret_value);
    }
    else
    {
        return ret_value;
    }
}

bool locate_file(const std::string & path, const std::string & filename, std::string & fullname, bool & is_vcd)
{
    is_vcd = false;

    // Try VCD
    fullname = path + "VCD/" + filename + ".VCD";

    if (!access(fullname.c_str(), F_OK))
    {
        return true;
    }

    // Try SVCD
    fullname = path + "SVCD/" + filename + ".SVD";

    if (!access(fullname.c_str(), F_OK))
    {
        is_vcd = true;
        return true;
    }

    return false;
}

int locate_video(const std::string & path, int track_no, std::string & fullname)
{
    char buffer[PATH_MAX + 1];

    // Try VCD
    snprintf(buffer, sizeof(buffer) - 1, "MPEGAV/AVSEQ%02i.DAT", track_no - 1);
    fullname = path + buffer;
    if (!access(fullname.c_str(), F_OK))
    {
        return 0;
    }

    // Try SVCD
    snprintf(buffer, sizeof(buffer) - 1, "MPEG2/AVSEQ%02i.MPG", track_no - 1);
    fullname = path + buffer;
    if (!access(fullname.c_str(), F_OK))
    {
        return 0;
    }

    return ENOENT;
}

std::string get_type_str(int type)
{
    switch (type)
    {
    case 1:
    {
        return "VCD 1.0, VCD 1.1, SVCD 1.0, HQVCD";
    }

    case 2:
    {
        return "VCD 2.0";
    }

    default:
    {
        return "";
    }
    }
}

std::string get_profile_tag_str(int tag)
{
    switch (tag)
    {
    case 0:
    {
        return "VCD 1.0, VCD 2.0, SVCD, HQVCD";
    }
    case 1:
    {
        return "VCD 1.1";
    }
    default:
    {
        return "";
    }
    }
}

void get_directory(const std::string & fullname, std::string *directory)
{
    struct stat stbuf;

    stat(fullname.c_str(), &stbuf);

    if (S_ISDIR(stbuf.st_mode))
    {
        // Already a directory
        *directory = fullname;
        append_sep(directory);
    }

    else
    {
        // Make it a directory
        *directory = fullname;
        remove_filename(directory);
    }
}
}
