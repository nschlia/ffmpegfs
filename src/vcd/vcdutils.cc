// -------------------------------------------------------------------------------
//  Project:		Bully's Media Player
//
//  File:		vcdutils.cpp
//
// (c) 1984-2017 by Oblivion Software/Norbert Schlia
// All rights reserved.
// -------------------------------------------------------------------------------
//
#include "vcdutils.h"
#include "ffmpeg_utils.h"

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

namespace VCDUTILS
{
string convert_psz2string(const char * psz, int size, bool trimmed)
{
    char * buffer = new char[size + 1];
    string ret_value;

    if (buffer != NULL)
    {
        memcpy(buffer, psz, size);
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

bool locate_file(const string & path, const string & filename, string & fullname, bool & is_vcd)
{
    is_vcd = false;

    // Try VCD
    fullname = path + "VCD/" + filename + ".VCD";

    if (!access(fullname.c_str(), F_OK))
    {
        fullname = fullname;
        return true;
    }

    // Try SVCD
    fullname = path + "SVCD/" + filename + ".SVD";

    if (!access(fullname.c_str(), F_OK))
    {
        fullname = fullname;
        is_vcd = true;
        return true;
    }

    return false;
}

int locate_video(const string & path, int track_no, string & fullname)
{
    char buffer[PATH_MAX + 1];

    // Try VCD
    sprintf(buffer, "MPEGAV/AVSEQ%02u.DAT", track_no - 1);
    fullname = path + buffer;
    if (!access(fullname.c_str(), F_OK))
    {
        return 0;
    }

    // Try SVCD
    sprintf(buffer, "MPEG2/AVSEQ%02u.MPG", track_no - 1);
    fullname = path + buffer;
    if (!access(fullname.c_str(), F_OK))
    {
        return 0;
    }

    return ENOENT;
}

string get_type_str(int type)
{
    switch (type)
    {
    case 1:
    {
        return "VCD 1.0, VCD 1.1, SVCD 1.0, HQVCD";
        break;
    }

    case 2:
    {
        return "VCD 2.0";
        break;
    }

    default:
    {
        return "";
        break;
    }
    }
}

string get_profile_tag_str(int tag)
{
    switch (tag)
    {
    case 0:
    {
        return "VCD 1.0, VCD 2.0, SVCD, HQVCD";
        break;
    }
    case 1:
    {
        return "VCD 1.1";
        break;
    }
    default:
    {
        return "";
        break;
    }
    }
}

void get_directory(const string & fullname, string *directory)
{
    struct stat st;

    stat(fullname.c_str(), &st);

    if (S_ISDIR(st.st_mode))
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
