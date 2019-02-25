/*
 * Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
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

#include <string>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "vcdentries.h"
#include "vcdutils.h"
#include "ffmpegfs.h"

#include <string.h>
#include <sys/stat.h>

#define VCD_SECTOR_SIZE 2352                /**< Video CD sector size */
#define VCD_SECTOR_OFFS 24                  /**< Video CD sector offset */
#define VCD_SECTOR_DATA 2324                /**< Video CD data sector size */

const char SYNC[12]              = { '\x00', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\x00' };
//static const char PICTURE_START_CODE[4] = { '\x00', '\x00', '\x01', '\x00' };
//static const char VIDEO_STREAM_1[4]     = { '\x00', '\x00', '\x01', '\xE0' };

VcdEntries::VcdEntries()
{
    clear();
}

VcdEntries::~VcdEntries()
{
}

void VcdEntries::clear()
{
    m_file_date     = -1;
    m_id.clear();
    m_type          = VCDTYPE_UNKNOWN;
    m_profile_tag   = VCDPROFILETAG_UNKNOWN;
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
        struct stat st;
        int         num_entries = 0;

        fpi = fopen(fullname.c_str(), "rb");
        if (fpi == nullptr)
        {
            throw static_cast<int>(errno);
        }

        if (fstat(fileno(fpi), &st) != 0)
        {
            throw static_cast<int>(ferror(fpi));
        }

        m_file_date      = st.st_mtime;

        memset(&vcdentry, 0, sizeof(vcdentry));

        if (fread(reinterpret_cast<char *>(&vcdentry), 1, sizeof(vcdentry), fpi) != sizeof(vcdentry))
        {
            throw static_cast<int>(ferror(fpi));
        }

        m_id            = VCDUTILS::convert_txt2string(reinterpret_cast<const char *>(vcdentry.m_ID), sizeof(vcdentry.m_ID));
        m_type          = static_cast<VCDTYPE>(vcdentry.m_type);
        m_profile_tag   = static_cast<VCDPROFILETAG>(vcdentry.m_profile_tag);
        num_entries     = htons(vcdentry.m_num_entries);
        m_duration      = 0;

        int sec = BCD2DEC(vcdentry.m_chapter[0].m_msf.m_min) * 60 + BCD2DEC(vcdentry.m_chapter[0].m_msf.m_sec);
        for (int chapter_no = 0, total = num_entries; chapter_no < total; chapter_no++)
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
    catch (int _errno)
    {
        if (fpi != nullptr)
        {
            fclose(fpi);
        }
        return _errno;
    }

    fclose(fpi);

    return scan_chapters();
}

int VcdEntries::scan_chapters()
{
    FILE *      fpi = nullptr;
    std::string fullname;
    int         last_track_no = -1;
    int64_t     first_sync = -1;
    struct stat st;

    if (!m_chapters.size())
    {
        return EIO;   // Fail safe only: Should not happen, at least 1 chapter is required.
    }

    try
    {
        // Build list of chapters
        for (size_t chapter_no = 0; chapter_no < m_chapters.size(); chapter_no++)
        {
            if (last_track_no != m_chapters[chapter_no].get_track_no())
            {
                last_track_no = m_chapters[chapter_no].get_track_no();

                int _errno = VCDUTILS::locate_video(m_disk_path, last_track_no, fullname);
                if (_errno != 0)
                {
                    throw static_cast<int>(_errno);
                }

                if (chapter_no)
                {
                    m_chapters[chapter_no - 1].m_end_pos = static_cast<uint64_t>(st.st_size);
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

                if (fstat(fileno(fpi), &st) != 0)
                {
                    throw static_cast<int>(ferror(fpi));
                }

                // Locate the first sync bytes
                SEEKRES res = seek_sync(fpi, SYNC, sizeof(SYNC));

                if (res != SEEKRES_FOUND)
                {
                    throw static_cast<int>(EIO);
                }

                first_sync = ftell(fpi) - static_cast<int64_t>(sizeof(SYNC));
            }

            int64_t total_chunks    = (st.st_size - first_sync) / VCD_SECTOR_SIZE;
            int64_t first           = 0;
            int64_t last            = total_chunks - 1;
            int64_t middle          = (first + last) / 2;

            // Locate sector with correct start time
            while (first <= last)
            {
                VcdChapter buffer(m_chapters[chapter_no].get_is_vcd());
                __off_t file_pos = first_sync + middle * VCD_SECTOR_SIZE;

                if (fseek(fpi, file_pos, SEEK_SET))
                {
                    throw static_cast<int>(ferror(fpi));
                }

                int _errno = buffer.read(fpi, last_track_no);
                if (_errno)
                {
                    throw static_cast<int>(_errno);
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
            VcdChapter buffer(m_chapters[m_chapters.size() - 1].get_is_vcd());
            int64_t total_chunks    = (st.st_size - first_sync) / VCD_SECTOR_SIZE;

            // Read time stamp of last sector
            if (fseek(fpi, first_sync + (total_chunks - 1) * VCD_SECTOR_SIZE, SEEK_SET))
            {
                throw static_cast<int>(ferror(fpi));
            }

            int _errno = buffer.read(fpi, last_track_no);
            if (_errno)
            {
                throw static_cast<int>(_errno);
            }

            VcdChapter & chapter1 = m_chapters[m_chapters.size() - 1];
            int64_t chapter_duration = buffer.get_start_time() - chapter1.get_start_time();

            // Chapter duration
            chapter1.m_duration = chapter_duration;
            // Total duration
            m_duration += chapter_duration;
        }
    }
    catch (int _errno)
    {
        if (fpi != nullptr)
        {
            fclose(fpi);
        }
        return _errno;
    }

    // End of last chapter
    m_chapters[m_chapters.size() - 1].m_end_pos = static_cast<uint64_t>(st.st_size);

    if (fpi != nullptr)
    {
        fclose(fpi);
    }

    return 0;
}

VcdEntries::SEEKRES VcdEntries::seek_sync(FILE *fpi, const char * sync, int len) const
{
    char ch;

    // Read first char
    if (fread(&ch, 1, 1, fpi) != 1)
    {
        return SEEKRES_NOTFOUND;
    }

    for (int n = 0; n < len; n++)
    {
        if (ch != *(sync + n))
        {
            if (n > 0)
            {
                // Restart check
                n = -1;
                continue;
            }

            n = -1;
        }

        if (n == len - 1)
        {
            // Found!
            break;
        }

        if (fread(&ch, 1, 1, fpi) != 1)
        {
            return SEEKRES_NOTFOUND;
        }
    }

    return SEEKRES_FOUND;
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

