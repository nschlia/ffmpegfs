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
 */

/**
 * @file vcdchapter.cc
 * @brief S/VCD VcdChapter class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2013-2024 Norbert Schlia (nschlia@oblivion-software.de) @n
 * From BullysPLayer Copyright (C) 1984-2024 by Oblivion Software/Norbert Schlia
 */

#include "ffmpegfs.h"
#include "vcdchapter.h"
#include "vcdutils.h"
#include "ffmpeg_utils.h"

#include <climits>

VcdChapter::VcdChapter(bool is_svcd) :
    m_is_svcd(is_svcd),
    m_track_no(0),
    m_min(0),
    m_sec(0),
    m_frame(0),
    m_duration(0),
    m_start_pos(0),
    m_end_pos(0)
{

}

VcdChapter::VcdChapter(const VCDCHAPTER & VcdChapter, bool is_svcd) :
    m_is_svcd(is_svcd),
    m_track_no(VcdChapter.m_track_no),
    m_min(BCD2DEC(VcdChapter.m_msf.m_min)),
    m_sec(BCD2DEC(VcdChapter.m_msf.m_sec)),
    m_frame(BCD2DEC(VcdChapter.m_msf.m_frame)),
    m_duration(0),
    m_start_pos(0),
    m_end_pos(0)
{

}

VcdChapter::VcdChapter(int track_no, int min, int sec, int frame, bool is_svcd, int64_t duration) :
    m_is_svcd(is_svcd),
    m_track_no(track_no),
    m_min(min),
    m_sec(sec),
    m_frame(frame),
    m_duration(duration),
    m_start_pos(0),
    m_end_pos(0)
{

}

int VcdChapter::readio(FILE *fpi, int track_no)
{
    VCDMSF msf;
    std::array<char, SYNC.size()> buffer;

    // Read first sync
    if (fread(&buffer, 1, buffer.size(), fpi) != buffer.size())
    {
        return ferror(fpi);
    }

    // Validate sync
    if (buffer != SYNC)
    {
        return EIO;
    }

    // Read position block
    std::memset(&msf, 0, sizeof(msf));

    if (fread(reinterpret_cast<char *>(&msf), 1, sizeof(msf), fpi) != sizeof(msf))
    {
        return ferror(fpi);
    }

    m_track_no  = track_no;
    m_min       = BCD2DEC(msf.m_min);
    m_sec       = BCD2DEC(msf.m_sec);
    m_frame     = BCD2DEC(msf.m_frame);

    return 0;
}

bool VcdChapter::get_is_svcd() const
{
    return m_is_svcd;
}

int VcdChapter::get_track_no() const
{
    return m_track_no;
}

int VcdChapter::get_min() const
{
    return m_min;
}

int VcdChapter::get_sec() const
{
    return m_sec;
}

int VcdChapter::get_frame() const
{
    return m_frame;
}

int64_t VcdChapter::get_duration() const
{
    return m_duration;
}

std::string VcdChapter::get_filename() const
{
    std::string buffer;

    if (m_is_svcd)
    {
        strsprintf(&buffer, "MPEG2/AVSEQ%02i.MPG", m_track_no - 1);
    }
    else
    {
        strsprintf(&buffer, "MPEGAV/AVSEQ%02i.DAT", m_track_no - 1);
    }
    return buffer;
}

uint64_t VcdChapter::get_start_pos() const
{
    return m_start_pos;
}

uint64_t VcdChapter::get_end_pos() const
{
    return m_end_pos;
}

uint64_t VcdChapter::get_size() const
{
    return (m_end_pos - m_start_pos);
}

int64_t VcdChapter::get_start_time() const
{
    // MSF format: minutes, seconds, and fractional seconds called frames. Each timecode frame is one seventy-fifth of a second.
    return static_cast<int64_t>(m_min * 60 + m_sec) * AV_TIME_BASE + (static_cast<int64_t>(m_frame) * AV_TIME_BASE / 75);
}

//Conversion from MSF to LBA
//--------------------------
//As from Red book because there are 75 frames in 1 second, so,
//LBA = Minute * 60 * 75 + Second * 75 + Frame - 150
//The minus 150 is the 2 second pregap that is recorded on every CD.

//Conversion from LBA to MSF
//--------------------------
//Minute = Int((LBA + 150) / (60 * 75))
//Second = Int(LBA + 150 - Minute * 60 * 75) / 75)
//Frame = LBA + 150 - Minute * 60 * 75 - Second * 75
//Where Int() is a function that truncates the fractional part giving only the whole number part.

int VcdChapter::get_lba() const
{
    return m_frame + (m_sec + m_min * 60) * 75;
}

VcdChapter & VcdChapter::operator= (VcdChapter const & other)
{
    if (this != & other)  //oder if (*this != rhs)
    {
        m_is_svcd   = other.m_is_svcd;
        m_track_no  = other.m_track_no;
        m_min       = other.m_min;
        m_sec       = other.m_sec;
        m_frame     = other.m_frame;
        m_start_pos = other.m_start_pos;
        m_end_pos   = other.m_end_pos;
        m_duration  = other.m_duration;
    }

    return *this; //Referenz auf das Objekt selbst zurückgeben
}

int VcdChapter::operator==(const VcdChapter & other) const
{
    return (m_track_no == other.m_track_no &&
            m_min == other.m_min &&
            m_sec == other.m_sec &&
            m_frame == other.m_frame);
}

int VcdChapter::operator<(const VcdChapter & other) const
{
    int res;

    res = (m_track_no - other.m_track_no);

    if (res < 0)
    {
        return 1;
    }

    if (res > 0)
    {
        return 0;
    }

    res = (m_min - other.m_min);

    if (res < 0)
    {
        return 1;
    }

    if (res > 0)
    {
        return 0;
    }

    res = (m_sec - other.m_sec);

    if (res < 0)
    {
        return 1;
    }

    if (res > 0)
    {
        return 0;
    }

    res = (m_frame - other.m_frame);

    if (res < 0)
    {
        return 1;
    }

    return 0;
}

int VcdChapter::operator<=(const VcdChapter & other) const
{
    if (*this == other)
    {
        return 1;
    }

    return (*this < other);
}

int VcdChapter::operator>(const VcdChapter & other) const
{
    int res;
    res = (m_track_no - other.m_track_no);

    if (res > 0)
    {
        return 1;
    }

    if (res < 0)
    {
        return 0;
    }

    res = (m_min - other.m_min);

    if (res > 0)
    {
        return 1;
    }

    if (res < 0)
    {
        return 0;
    }

    res = (m_sec - other.m_sec);

    if (res > 0)
    {
        return 1;
    }

    if (res < 0)
    {
        return 0;
    }

    res = (m_frame - other.m_frame);

    if (res > 0)
    {
        return 1;
    }

    //if (res <= 0)
    return 0;
}

int VcdChapter::operator>=(const VcdChapter & other) const
{
    if (*this == other)
    {
        return 1;
    }

    return (*this > other);
}

int VcdChapter::operator!=(const VcdChapter & other) const
{
    return (m_track_no != other.m_track_no &&
            m_min != other.m_min &&
            m_sec != other.m_sec &&
            m_frame != other.m_frame);
}
