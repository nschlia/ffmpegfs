/*
 * Copyright (C) 2017-2025 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file vcdentries.cc
 * @brief S/VCD VcdEntries class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2013-2025 Norbert Schlia (nschlia@oblivion-software.de) @n
 * From BullysPLayer Copyright (C) 1984-2025 by Oblivion Software/Norbert Schlia
 */

#include "vcdentries.h"
#include "vcdutils.h"

#include <arpa/inet.h>
#include <cstring>
#include <sys/stat.h>

#define VCD_SECTOR_SIZE 2352                /**< @brief Video CD sector size */
#define VCD_SECTOR_OFFS 24                  /**< @brief Video CD sector offset */
#define VCD_SECTOR_DATA 2324                /**< @brief Video CD data sector size */

/**
  * Sync bytes for a Video CD sector.
  */
const std::array<char, 12> SYNC = { '\x00', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\x00' };
/**
  * Sync bytes for a Video CD picture start.
  */
//static const char PICTURE_START_CODE[4] = { '\x00', '\x00', '\x01', '\x00' };
/**
  * Sync bytes for a Video CD video stream.
  */
//static const char VIDEO_STREAM_1[4]     = { '\x00', '\x00', '\x01', '\xE0' };

VcdEntries::VcdEntries()
{
    clear();
}

void VcdEntries::clear()
{
    m_file_date     = -1;
    m_id.clear();
    m_type          = VCDTYPE::UNKNOWN;
    m_profile_tag   = VCDPROFILETAG::UNKNOWN;
    m_chapters.clear();
    m_disk_path.clear();
    m_duration      = 0;
}

int VcdEntries::load_file(const std::string & path)
{
    FILE * fpi = nullptr;
    std::string fullname;
    bool is_vcd = false;

    clear();

    if (!VCDUTILS::locate_file(path, "ENTRIES", fullname, is_vcd))
    {
        return ENOENT;
    }

    VCDUTILS::get_directory(path, &m_disk_path);

    try
    {
        VCDENTRY    vcdentry;
        struct stat stbuf;
        uint32_t    num_entries = 0;

        fpi = fopen(fullname.c_str(), "rb");
        if (fpi == nullptr)
        {
            throw static_cast<int>(errno);
        }

        if (fstat(fileno(fpi), &stbuf) != 0)
        {
            throw static_cast<int>(ferror(fpi));
        }

        m_file_date      = stbuf.st_mtime;

        std::memset(&vcdentry, 0, sizeof(vcdentry));

        if (fread(reinterpret_cast<char *>(&vcdentry), 1, sizeof(vcdentry), fpi) != sizeof(vcdentry))
        {
            throw static_cast<int>(ferror(fpi));
        }

        m_id            = VCDUTILS::convert_txt2string(reinterpret_cast<const char *>(vcdentry.m_ID.data()), vcdentry.m_ID.size());
        m_type          = static_cast<VCDTYPE>(vcdentry.m_type);
        m_profile_tag   = static_cast<VCDPROFILETAG>(vcdentry.m_profile_tag);
        num_entries     = htons(vcdentry.m_num_entries);
        m_duration      = 0;

        int sec = BCD2DEC(vcdentry.m_chapter[0].m_msf.m_min) * 60 + BCD2DEC(vcdentry.m_chapter[0].m_msf.m_sec);
        for (uint32_t chapter_no = 0, total = num_entries; chapter_no < total; chapter_no++)
        {
            if (chapter_no && BCD2DEC(vcdentry.m_chapter[chapter_no].m_msf.m_min) * 60 + BCD2DEC(vcdentry.m_chapter[chapter_no].m_msf.m_sec) - sec < 1)
            {
                // Skip chapters shorter than 1 second
                sec = BCD2DEC(vcdentry.m_chapter[chapter_no].m_msf.m_min) * 60 + BCD2DEC(vcdentry.m_chapter[chapter_no].m_msf.m_sec);
                --num_entries;
                continue;
            }

            VcdChapter chapter(vcdentry.m_chapter[chapter_no], is_vcd);

            m_chapters.push_back(chapter);
        }

        // Calculate durations of all chapters until last. This will be done later as we do not yet know the duration of the stream
        for (size_t chapter_no = 0; chapter_no < m_chapters.size() - 1; chapter_no++)
        {
            VcdChapter & chapter1 = m_chapters[chapter_no];
            const VcdChapter & chapter2 = m_chapters[chapter_no + 1];
            int64_t chapter_duration = chapter2.get_start_time() - chapter1.get_start_time();

            // Chapter duration
            chapter1.m_duration = chapter_duration;
            // Total duration
            m_duration += chapter_duration;
        }
    }
    catch (int orgerrno)
    {
        if (fpi != nullptr)
        {
            fclose(fpi);
        }
        return orgerrno;
    }

    fclose(fpi);

    return scan_chapters();
}

int VcdEntries::scan_chapters()
{
    FILE *      fpi = nullptr;
    struct stat stbuf;

    std::memset(&stbuf, 0, sizeof(stbuf));

    if (!m_chapters.size())
    {
        return EIO;   // Fail safe only: Should not happen, at least 1 chapter is required.
    }

    try
    {
        int         last_track_no = -1;
        int64_t     first_sync = -1;

        // Build list of chapters
        for (size_t chapter_no = 0; chapter_no < m_chapters.size(); chapter_no++)
        {
            if (last_track_no != m_chapters[chapter_no].get_track_no())
            {
                std::string fullname;

                last_track_no = m_chapters[chapter_no].get_track_no();

                int orgerrno = VCDUTILS::locate_video(m_disk_path, last_track_no, fullname);
                if (orgerrno != 0)
                {
                    throw static_cast<int>(orgerrno);
                }

                if (chapter_no)
                {
                    m_chapters[chapter_no - 1].m_end_pos = static_cast<uint64_t>(stbuf.st_size);
                }

                if (fpi != nullptr)
                {
                    fclose(fpi);
                }

                fpi = fopen(fullname.c_str(), "rb");

                if (fpi == nullptr)
                {
                    throw static_cast<int>(errno);
                }

                if (fstat(fileno(fpi), &stbuf) != 0)
                {
                    throw static_cast<int>(ferror(fpi));
                }

                // Locate the first sync bytes
                SEEKRES res = seek_sync(fpi, SYNC);

                if (res != SEEKRES::FOUND)
                {
                    throw static_cast<int>(EIO);
                }

                first_sync = ftell(fpi) - static_cast<int64_t>(SYNC.size());
            }

            int64_t total_chunks    = (stbuf.st_size - first_sync) / VCD_SECTOR_SIZE;
            int64_t first           = 0;
            int64_t last            = total_chunks - 1;
            int64_t middle          = (first + last) / 2;

            // Locate sector with correct start time
            while (first <= last)
            {
                VcdChapter buffer(m_chapters[chapter_no].get_is_svcd());
                long int file_pos = static_cast<long int>(first_sync + middle * VCD_SECTOR_SIZE);

                if (fseek(fpi, file_pos, SEEK_SET))
                {
                    throw static_cast<int>(ferror(fpi));
                }

                int orgerrno = buffer.readio(fpi, last_track_no);
                if (orgerrno)
                {
                    throw static_cast<int>(orgerrno);
                }

                if (buffer < m_chapters[chapter_no])
                {
                    first = middle + 1;
                }
                else if (buffer == m_chapters[chapter_no])
                {
                    m_chapters[chapter_no].m_start_pos = static_cast<uint64_t>(file_pos);

                    if (chapter_no)
                    {
                        m_chapters[chapter_no - 1].m_end_pos = static_cast<uint64_t>(file_pos);
                    }
                    break;
                }
                else
                {
                    last = middle - 1;
                }

                middle = (first + last) / 2;
            }
        }

        {
            VcdChapter buffer(m_chapters[m_chapters.size() - 1].get_is_svcd());
            int64_t total_chunks    = (stbuf.st_size - first_sync) / VCD_SECTOR_SIZE;

            // Read time stamp of last sector
            if (fseek(fpi, static_cast<long int>(first_sync + (total_chunks - 1) * VCD_SECTOR_SIZE), SEEK_SET))
            {
                throw static_cast<int>(ferror(fpi));
            }

            int orgerrno = buffer.readio(fpi, last_track_no);
            if (orgerrno)
            {
                throw static_cast<int>(orgerrno);
            }

            VcdChapter & chapter1 = m_chapters[m_chapters.size() - 1];
            int64_t chapter_duration = buffer.get_start_time() - chapter1.get_start_time();

            // Chapter duration
            chapter1.m_duration = chapter_duration;
            // Total duration
            m_duration += chapter_duration;
        }
    }
    catch (int orgerrno)
    {
        if (fpi != nullptr)
        {
            fclose(fpi);
        }
        return orgerrno;
    }

    // End of last chapter
    m_chapters[m_chapters.size() - 1].m_end_pos = static_cast<uint64_t>(stbuf.st_size);

    if (fpi != nullptr)
    {
        fclose(fpi);
    }

    return 0;
}

VcdEntries::SEEKRES VcdEntries::seek_sync(FILE *fpi, const std::array<char, 12> & sync) const
{
    char ch;

    // Read first char
    if (fread(&ch, 1, 1, fpi) != 1)
    {
        return SEEKRES::NOTFOUND;
    }

    for (size_t n = 1; n <= sync.size(); n++)
    {
        if (ch != sync[n - 1])
        {
            if (n > 1)
            {
                // Restart check
                n = 0;
                continue;
            }

            n = 0;
        }

        if (n == sync.size())
        {
            // Found!
            break;
        }

        if (fread(&ch, 1, 1, fpi) != 1)
        {
            return SEEKRES::NOTFOUND;
        }
    }

    return SEEKRES::FOUND;
}

time_t VcdEntries::get_file_date() const
{
    return m_file_date;
}

const std::string & VcdEntries::get_id() const
{
    return m_id;
}

VCDTYPE VcdEntries::get_type() const
{
    return m_type;
}

std::string VcdEntries::get_type_str() const
{
    return VCDUTILS::get_type_str(m_type);
}

VCDPROFILETAG VcdEntries::get_profile_tag() const
{
    return m_profile_tag;
}

std::string VcdEntries::get_profile_tag_str() const
{
    return VCDUTILS::get_profile_tag_str(m_profile_tag);
}

int VcdEntries::get_number_of_chapters() const
{
    return static_cast<int>(m_chapters.size());
}

const VcdChapter *VcdEntries::get_chapter(int chapter_idx) const
{
    if (chapter_idx < 0 || chapter_idx >= get_number_of_chapters())
    {
        return nullptr;
    }
    return &m_chapters[static_cast<size_t>(chapter_idx)];
}

int64_t VcdEntries::get_duration() const
{
    return m_duration;
}

uint64_t VcdEntries::get_size() const
{
    size_t chapters = static_cast<size_t>(get_number_of_chapters());

    if (!chapters)
    {
        return 0;
    }

    return (m_chapters[chapters - 1].get_end_pos() - m_chapters[0].get_start_pos());
}

const std::string & VcdEntries::get_disk_path() const
{
    return m_disk_path;
}
