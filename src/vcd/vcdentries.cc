// -------------------------------------------------------------------------------
//  Project:		Bully's Media Player
//
//  File:		vcdentries.cpp
//
// (c) 1984-2017 by Oblivion Software/Norbert Schlia
// All rights reserved.
// -------------------------------------------------------------------------------
//
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "vcdentries.h"
#include "vcdutils.h"

#include <string.h>
#include <sys/stat.h>

#define VCD_SECTOR_SIZE 2352
//#define VCD_SECTOR_OFFS 24
//#define VCD_SECTOR_DATA 2324

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
    m_file_date = -1;
    m_id.clear();
    m_type = 0;
    m_profile_tag = 0;
    m_chapters.clear();
    m_disk_path.clear();
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

        m_id            = VCDUTILS::convert_psz2string(reinterpret_cast<const char *>(vcdentry.m_ID), sizeof(vcdentry.m_ID));
        m_type          = static_cast<int>(vcdentry.m_type);
        m_profile_tag   = static_cast<int>(vcdentry.m_profile_tag);
        num_entries     = htons(vcdentry.m_num_entries);

        int sec = BCD2DEC(vcdentry.m_entry[0].m_msf.m_min) * 60 + BCD2DEC(vcdentry.m_entry[0].m_msf.m_sec);
        for (int chapter_num = 0, total = num_entries; chapter_num < total; chapter_num++)
        {
            if (chapter_num && BCD2DEC(vcdentry.m_entry[chapter_num].m_msf.m_min) * 60 + BCD2DEC(vcdentry.m_entry[chapter_num].m_msf.m_sec) - sec < 1)
            {
                // Skip chapters shorter than 1 second
                sec = BCD2DEC(vcdentry.m_entry[chapter_num].m_msf.m_min) * 60 + BCD2DEC(vcdentry.m_entry[chapter_num].m_msf.m_sec);
                --num_entries;
                continue;
            }

            VcdChapter chapter(vcdentry.m_entry[chapter_num], is_vcd);

            m_chapters.push_back(chapter);
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
    FILE *  fpi = nullptr;
    std::string  fullname;
    int     last_track_no = -1;
    int64_t first_sync = -1;
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
                    throw static_cast<int>(errno);
                }

                if (chapter_no)
                {
                    m_chapters[chapter_no - 1].m_end_pos = st.st_size;
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

                first_sync = ftell(fpi) - sizeof(SYNC);
            }

            int64_t total_chunks = (st.st_size - first_sync) / VCD_SECTOR_SIZE;

            int64_t first = 0;
            int64_t last = total_chunks - 1;
            int64_t middle = (first + last) / 2;

            // Locate sector at with correct start time
            while (first <= last)
            {
                VcdChapter buffer(m_chapters[chapter_no].get_is_vcd());
                int64_t file_pos = first_sync + middle * VCD_SECTOR_SIZE;

                if (fseek(fpi, file_pos, SEEK_SET))
                {
                    throw static_cast<int>(ferror(fpi));
                }

                int _errno = buffer.read(fpi, last_track_no);
                if (_errno)
                {
                    throw static_cast<int>(errno);
                }

                if (buffer < m_chapters[chapter_no])
                {
                    first = middle + 1;
                }
                else if (buffer == m_chapters[chapter_no])
                {
                    m_chapters[chapter_no].m_start_pos = file_pos;

                    if (chapter_no)
                    {
                        m_chapters[chapter_no - 1].m_end_pos = file_pos;
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
    m_chapters[m_chapters.size() - 1].m_end_pos = st.st_size;

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

int VcdEntries::get_type() const
{
    return m_type;
}

std::string VcdEntries::get_type_str() const
{
    return VCDUTILS::get_type_str(m_type);
}

int VcdEntries::get_profile_tag() const
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

const VcdChapter & VcdEntries::get_chapter(int chapter_no) const
{
    return m_chapters[static_cast<size_t>(chapter_no)];
}
