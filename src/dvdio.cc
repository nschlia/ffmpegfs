/*
 * DVD file I/O for ffmpegfs
 *
 * Copyright (C) 2017-2018 Norbert Schlia (nschlia@oblivion-software.de)
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

#ifdef USE_LIBDVD

#include "dvdio.h"
#include "ffmpegfs.h"
#include "ffmpeg_utils.h"
#include "logging.h"

#include <string.h>
#include <assert.h>

//#include <dvdnav/dvdnav.h>
#include <dvdread/dvd_reader.h>
//#include <dvdread/ifo_types.h>
//#include <dvdread/ifo_read.h>
//#include <dvdread/dvd_udf.h>
#include <dvdread/nav_read.h>
//#include <dvdread/nav_print.h>

dvdio::dvdio()
    : m_dvd(nullptr)
    , m_dvd_title(nullptr)
    , m_vmg_file(nullptr)
    , m_vts_file(nullptr)
    , m_cur_pgc(nullptr)
    , m_start_cell(0)
    , m_cur_cell(0)
    , m_next_cell(0)
    , m_last_cell(0)
    , m_goto_next_cell(false)
    , m_cur_block(0)
    , m_is_eof(false)
    , m_errno(0)
    , m_rest_size(0)
    , m_rest_pos(0)
    , m_cur_pos(0)
    , m_title_no(0)
    , m_chapter_no(0)
    , m_angle_no(0)
    , m_duration(-1)
{
    memset(&m_data, 0, sizeof(m_data));
    memset(&m_buffer, 0, sizeof(m_buffer));
}

dvdio::~dvdio()
{
}

VIRTUALTYPE dvdio::type() const
{
    return VIRTUALTYPE_DVD;
}

int dvdio::bufsize() const
{
    return sizeof(m_data);
}

/*
  static void print_time(dvd_time_t *dtime) {
  const char *rate;

  assert((dtime->hour>>4) < 0xa && (dtime->hour&0xf) < 0xa);
  assert((dtime->minute>>4) < 0x7 && (dtime->minute&0xf) < 0xa);
  assert((dtime->second>>4) < 0x7 && (dtime->second&0xf) < 0xa);
  assert((dtime->frame_u&0xf) < 0xa);

  fprintf(MSG_OUT,"%02x:%02x:%02x.%02x",
         dtime->hour,
         dtime->minute,
         dtime->second,
         dtime->frame_u & 0x3f);
  switch((dtime->frame_u & 0xc0) >> 6) {
  case 1:
    rate = "25.00";
    break;
  case 3:
    rate = "29.97";
    break;
  default:
    rate = "(please send a bug report)";
    break;
  }
  fprintf(MSG_OUT," @ %s fps", rate);
}
*/

int dvdio::openX(const std::string & filename)
{
    int pgc_id;
    int ttn, pgn;
    tt_srpt_t *tt_srpt;
    vts_ptt_srpt_t *vts_ptt_srpt;

    set_path(filename);

    if (virtualfile() != nullptr)
    {
        m_title_no     = virtualfile()->dvd.m_title_no - 1;
        m_chapter_no   = virtualfile()->dvd.m_chapter_no - 1;
        m_angle_no     = virtualfile()->dvd.m_angle_no - 1;
    }
    else
    {
        m_title_no     = 0;
        m_chapter_no   = 0;
        m_angle_no     = 0;
    }

    Logging::debug(m_path, "Opening input DVD.");

    // Open the disc.
    m_dvd = DVDOpen(m_path.c_str());
    if (!m_dvd)
    {
        Logging::error(m_path, "Couldn't open DVD.");
        return EINVAL;
    }

    // Load the video manager to find out the information about the titles on this disc.
    m_vmg_file = ifoOpen(m_dvd, 0);
    if (!m_vmg_file)
    {
        Logging::error(m_path, "Can't open VMG info.");
        DVDClose(m_dvd);
        return EINVAL;
    }
    tt_srpt = m_vmg_file->tt_srpt;

    // Make sure our title number is valid.
    Logging::trace(m_path, "There are %1 titles on this DVD.", static_cast<uint16_t>(tt_srpt->nr_of_srpts));

    if (m_title_no < 0 || m_title_no >= tt_srpt->nr_of_srpts)
    {
        Logging::error(m_path, "Invalid title %1.", m_title_no + 1);
        ifoClose(m_vmg_file);
        DVDClose(m_dvd);
        return EINVAL;
    }

    // Make sure the chapter number is valid for this title.
    Logging::trace(nullptr, "There are %1 chapters in this title.", static_cast<uint16_t>(tt_srpt->title[m_title_no].nr_of_ptts));

    if (m_chapter_no < 0 || m_chapter_no >= tt_srpt->title[m_title_no].nr_of_ptts)
    {
        Logging::error(m_path, "Invalid chapter %1", m_chapter_no + 1);
        ifoClose(m_vmg_file);
        DVDClose(m_dvd);
        return EINVAL;
    }

    // Make sure the angle number is valid for this title.
    Logging::trace(m_path, "There are %1 angles in this title.", tt_srpt->title[m_title_no].nr_of_angles);

    if (m_angle_no < 0 || m_angle_no >= tt_srpt->title[m_title_no].nr_of_angles)
    {
        Logging::error(nullptr, "Invalid angle %1", m_angle_no + 1);
        ifoClose(m_vmg_file);
        DVDClose(m_dvd);
        return EINVAL;
    }

    // Load the VTS information for the title set our title is in.
    m_vts_file = ifoOpen(m_dvd, tt_srpt->title[m_title_no].title_set_nr);
    if (!m_vts_file)
    {
        Logging::error(m_path, "Can't open the title %1 info file.", tt_srpt->title[m_title_no].title_set_nr);
        ifoClose(m_vmg_file);
        DVDClose(m_dvd);
        return EINVAL;
    }

    // Determine which program chain we want to watch.  This is based on the chapter number.
    ttn                 = tt_srpt->title[m_title_no].vts_ttn;
    vts_ptt_srpt        = m_vts_file->vts_ptt_srpt;
    pgc_id              = vts_ptt_srpt->title[ttn - 1].ptt[m_chapter_no].pgcn;
    pgn                 = vts_ptt_srpt->title[ttn - 1].ptt[m_chapter_no].pgn;
    m_cur_pgc           = m_vts_file->vts_pgcit->pgci_srp[pgc_id - 1].pgc;
    m_start_cell        = m_cur_pgc->program_map[pgn - 1] - 1;

    // We've got enough info, time to open the title set data.
    m_dvd_title = DVDOpenFile(m_dvd, tt_srpt->title[m_title_no].title_set_nr, DVD_READ_TITLE_VOBS);
    if (!m_dvd_title)
    {
        Logging::error(m_path, "Can't open title VOBS (VTS_%<%02d>1_1.VOB).", tt_srpt->title[m_title_no].title_set_nr);
        ifoClose(m_vts_file);
        ifoClose(m_vmg_file);
        DVDClose(m_dvd);
        return EINVAL;
    }

    m_next_cell         = m_start_cell;
    m_last_cell         = m_cur_pgc->nr_of_cells;
    m_cur_cell          = m_start_cell;

    {
        int first_cell = m_next_cell;

        // Check if we're entering an angle block.
        if (m_cur_pgc->cell_playback[first_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK)
        {
            first_cell += m_angle_no;
        }

        int framerate   = ((m_cur_pgc->cell_playback[first_cell].playback_time.frame_u & 0xc0) >> 6);
        int64_t frac    = static_cast<int64_t>((m_cur_pgc->cell_playback[first_cell].playback_time.frame_u & 0x3f) * AV_TIME_BASE / ((framerate == 3) ? 25 : 29.97));
        m_duration      = static_cast<int64_t>((m_cur_pgc->cell_playback[first_cell].playback_time.hour * 60 + m_cur_pgc->cell_playback[first_cell].playback_time.minute) * 60 + m_cur_pgc->cell_playback[first_cell].playback_time.second) * AV_TIME_BASE + frac;
    }

    m_goto_next_cell    = true;
    m_is_eof            = false;
    m_errno             = 0;
    m_rest_size         = 0;
    m_rest_pos = 0;
    m_cur_pos = 0;

    return 0;
}

// Demux and cell navigation nicked from https://www.videolan.org/vlc/download-sources.html
// More details see http://stnsoft.com/DVD/vobov.html

#define PS_STREAM_ID_END_STREAM         0xB9
#define PS_STREAM_ID_PACK_HEADER        0xBA    // MPEG-2 Pack Header
#define PS_STREAM_ID_SYSTEM_HEADER      0xBB    // Program Stream System Header
#define PS_STREAM_ID_MAP                0xBC
#define PS_STREAM_ID_PRIVATE_STREAM1    0xBD    // Private stream 1 (non MPEG audio, subpictures)
#define PS_STREAM_ID_PADDING            0xBE    // Padding stream
#define PS_STREAM_ID_PRIVATE            0xBF    // Private stream 2 (navigation data)
#define PS_STREAM_ID_AUDIO              0xC0    // - 0xDF	MPEG-1 or MPEG-2 audio stream number (note: DVD allows only 8 audio streams)
#define PS_STREAM_ID_VIDEO              0xE0    // - 0xEF	MPEG-1 or MPEG-2 video stream number (note: DVD allows only 1 video stream)
#define PS_STREAM_ID_EXTENDED           0xFD
#define PS_STREAM_ID_DIRECTORY          0xFF

#define PS_STREAM_ID                    3

// return the size of the next packet
bool dvdio::get_packet_size(const uint8_t *p, unsigned int peek, unsigned int *size) const
{
    if (peek < 4)
    {
        return false;   // Invalid size
    }

    switch (p[PS_STREAM_ID])
    {
    case PS_STREAM_ID_END_STREAM:
    {
        *size = 4;
        return true;
    }
    case PS_STREAM_ID_PACK_HEADER:
    {
        // MPEG-2 pack header, see http://stnsoft.com/DVD/packhdr.html
        if (peek > 4)
        {
            if (peek >= 14 && (p[4] >> 6) == 0x01)
            {
                *size = 14 + (p[13] & 0x07); // Byte 13 Bit 0..2: Pack stuffing length
                return true;
            }
            else if (peek >= 12 && (p[4] >> 4) == 0x02)
            {
                *size = 12;                  // unclear what this is for
                return true;
            }
        }
        break;
    }
    case PS_STREAM_ID_SYSTEM_HEADER:        // http://stnsoft.com/DVD/sys_hdr.html, see http://stnsoft.com/DVD/sys_hdr.html
    case PS_STREAM_ID_MAP:                  // ???
    case PS_STREAM_ID_DIRECTORY:            // ???
    default:
    {
        if (peek >= 6)
        {
            *size = (6 + ((static_cast<unsigned int>(p[4]) << 8) | p[5]));  // Byte 4/5: header length
            return true;
        }
        break;
    }
    }
    return false;   // unknown ID
}

// return the id of a PES (should be valid)
int dvdio::get_pes_id(const uint8_t *buffer, unsigned int size) const
{
    if (buffer[PS_STREAM_ID] == PS_STREAM_ID_PRIVATE_STREAM1)
    {
        uint8_t sub_id = 0;
        if (size >= 9 && size >= static_cast<unsigned int>(9 + buffer[8]))
        {
            const unsigned int start = 9 + buffer[8];
            sub_id = buffer[start];

            if ((sub_id & 0xfe) == 0xa0 &&
                    size >= start + 7 &&
                    (buffer[start + 5] >= 0xc0 ||
                     buffer[start + 6] != 0x80))
            {
                // AOB LPCM/MLP extension
                // XXX for MLP I think that the !=0x80 test is not good and
                // will fail for some valid files
                return (0xa000 | (sub_id & 0x01));
            }
        }

        // VOB extension
        return (0xbd00 | sub_id);
    }
    else if (buffer[PS_STREAM_ID] == PS_STREAM_ID_EXTENDED &&
             size >= 9 &&
             (buffer[6] & 0xC0) == 0x80 &&    // mpeg2
             (buffer[7] & 0x01) == 0x01)      // extension_flag
    {
        // ISO 13818 amendment 2 and SMPTE RP 227
        const uint8_t flags = buffer[7];
        unsigned int skip = 9;

        // Find PES extension
        if ((flags & 0x80))
        {
            skip += 5;        // pts
            if ((flags & 0x40))
            {
                skip += 5;    // dts
            }
        }
        if ((flags & 0x20))
        {
            skip += 6;
        }
        if ((flags & 0x10))
        {
            skip += 3;
        }
        if ((flags & 0x08))
        {
            skip += 1;
        }
        if ((flags & 0x04))
        {
            skip += 1;
        }
        if ((flags & 0x02))
        {
            skip += 2;
        }

        if (skip < size && (buffer[skip] & 0x01))
        {
            const uint8_t flags2 = buffer[skip];

            // Find PES extension 2
            skip += 1;
            if (flags2 & 0x80)
            {
                skip += 16;
            }
            if ((flags2 & 0x40) && skip < size)
            {
                skip += 1 + buffer[skip];
            }
            if (flags2 & 0x20)
            {
                skip += 2;
            }
            if (flags2 & 0x10)
            {
                skip += 2;
            }

            if (skip + 1 < size)
            {
                const int i_extension_field_length = buffer[skip] & 0x7f;
                if (i_extension_field_length >=1)
                {
                    int i_stream_id_extension_flag = (buffer[skip+1] >> 7) & 0x1;
                    if (i_stream_id_extension_flag == 0)
                    {
                        return (0xfd00 | (buffer[skip+1] & 0x7f));
                    }
                }
            }
        }
    }
    return buffer[PS_STREAM_ID];
}

// Extract only the interesting portion of the VOB input stream
unsigned int dvdio::demux_pes(uint8_t *out, const uint8_t *in, unsigned int len) const
{
    unsigned int netsize = 0;
    while (len > 0)
    {
        unsigned int size = 0;
        if (!get_packet_size(in, len, &size) || size > len)
        {
            break;
        }

        // Parse block and copy to buffer
        switch (0x100 | in[PS_STREAM_ID])
        {
        case (0x100 | PS_STREAM_ID_END_STREAM):
        case (0x100 | PS_STREAM_ID_SYSTEM_HEADER):  // Program Stream System Header
        case (0x100 | PS_STREAM_ID_MAP):
        {
            break;
        }
        case (0x100 | PS_STREAM_ID_PACK_HEADER):    // MPEG-2 Pack Header
        {
            memcpy(out, in, size);
            out += size;
            netsize += size;
            break;
        }
        default:
        {
            int id = get_pes_id(in, size);
            if (id >= PS_STREAM_ID_AUDIO)           // Audio/Video/Extended or Directory
            {
                // Probably this is sufficient here:
                // 110x xxxx    0xC0 - 0xDF	MPEG-1 or MPEG-2 audio stream number x xxxx
                // 1110 xxxx    0xE0 - 0xEF	MPEG-1 or MPEG-2 video stream number xxxx
                memcpy(out, in, size);
                out += size;
                netsize += size;
            }
            break;
        }
        }

        in += size;
        len -= size;
    }

    return netsize;
}

dvdio::DSITYPE dvdio::handle_DSI(void *_dsi_pack, unsigned int & cur_output_size, unsigned int & next_vobu, uint8_t *data)
{
    dsi_t * dsi_pack = reinterpret_cast<dsi_t*>(_dsi_pack);
    DSITYPE dsitype = DSITYPE_CONTINUE;

    navRead_DSI(dsi_pack, &data[DSI_START_BYTE]);

    // Determine where we go next.  These values are the ones we mostly
    // care about.
    m_cur_block     = dsi_pack->dsi_gi.nv_pck_lbn;
    cur_output_size = dsi_pack->dsi_gi.vobu_ea;

    // If we're not at the end of this cell, we can determine the next
    // VOBU to display using the VOBU_SRI information section of the
    // DSI.  Using this value correctly follows the current angle,
    // avoiding the doubled scenes in The Matrix, and makes our life
    // really happy.

    next_vobu = m_cur_block + (dsi_pack->vobu_sri.next_vobu & 0x7fffffff);

    if (dsi_pack->vobu_sri.next_vobu != SRI_END_OF_CELL && m_angle_no > 1)
    {
        switch ((dsi_pack->sml_pbi.category & 0xf000) >> 12)
        {
        case 0x4:
        {
            // Interleaved unit with no angle
            // dsi_pack->sml_pbi.ilvu_sa
            // relative offset to the next ILVU block (not VOBU) for this angle or scene.
            // 00 00 00 00 for PREU and non-interleaved blocks
            // ff ff ff ff for the last interleaved block, indicating the end of interleaving
            if (dsi_pack->sml_pbi.ilvu_sa != 0 && dsi_pack->sml_pbi.ilvu_sa != 0xffffffff)
            {
                next_vobu = m_cur_block + dsi_pack->sml_pbi.ilvu_sa;
                cur_output_size = dsi_pack->sml_pbi.ilvu_ea;
            }
            else
            {
                next_vobu = m_cur_block + dsi_pack->dsi_gi.vobu_ea + 1;
            }
            break;
        }
        case 0x5:
        {
            // vobu is end of ilvu
            if (dsi_pack->sml_agli.data[m_angle_no].address)
            {
                next_vobu = m_cur_block + dsi_pack->sml_agli.data[m_angle_no].address;
                cur_output_size = dsi_pack->sml_pbi.ilvu_ea;
                break;
            }
        }
            // fall through
        case 0x6:   // vobu is beginning of ilvu
        case 0x9:   // next scr is 0
        case 0xa:   // entering interleaved section
        case 0x8:   // non interleaved cells in interleaved section
        default:
        {
            next_vobu = m_cur_block + (dsi_pack->vobu_sri.next_vobu & 0x7fffffff);
            break;
        }
        }
    }
    else if (dsi_pack->vobu_sri.next_vobu == SRI_END_OF_CELL)
    {
        if (m_next_cell >= m_cur_pgc->nr_of_cells)
        {
            next_vobu = 0;
            dsitype = DSITYPE_EOF_TITLE;
        }
        else
        {
            next_vobu = m_cur_pgc->cell_playback[m_next_cell].first_sector;

            if (next_vobu >= m_cur_pgc->cell_playback[m_cur_cell].last_sector)
            {
                dsitype = DSITYPE_EOF_CHAPTER;
            }

            m_cur_cell = m_next_cell;

            next_cell();

            next_vobu = m_cur_pgc->cell_playback[m_cur_cell].first_sector;
        }
    }

    return dsitype;
}

void dvdio::next_cell()
{
    // Check if we're entering an angle block
    if (m_cur_pgc->cell_playback[m_cur_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK)
    {
        m_cur_cell += m_angle_no;

        for (int i = 0;; ++i)
        {
            if (m_cur_pgc->cell_playback[m_cur_cell + i].block_mode == BLOCK_MODE_LAST_CELL)
            {
                m_next_cell = m_cur_cell + i + 1;
                break;
            }
        }
    }
    else
    {
        m_next_cell = m_cur_cell + 1;
    }
}

int dvdio::read(void * data, int size)
{
    unsigned int cur_output_size;
    ssize_t maxlen;
    unsigned int result_len = 0;
    DSITYPE dsitype;

    if (m_rest_size)
    {
        size_t rest_size = m_rest_size;

        assert(rest_size < static_cast<size_t>(size));

        memcpy(data, &m_data[m_rest_pos], rest_size);

        m_rest_size = m_rest_pos = 0;

        return static_cast<int>(rest_size);
    }

    // Playback by cell in this pgc, starting at the cell for our chapter.
    //while (next_cell < last_cell)
    {
        if (m_goto_next_cell)
        {
            m_goto_next_cell = false;

            m_cur_cell = m_next_cell;

            next_cell();

            m_cur_block = m_cur_pgc->cell_playback[m_cur_cell].first_sector;
        }

        if (m_cur_block >= m_cur_pgc->cell_playback[m_cur_cell].last_sector)
        {
            m_is_eof = false;
            return 0;
        }

        // We loop until we're out of this cell.
        //        for(cur_pack = cur_pgc->cell_playback[cur_cell].first_sector;
        //             cur_pack < cur_pgc->cell_playback[cur_cell].last_sector;)
        {
            dsi_t dsi_pack;
            unsigned int next_vobu;

            // Read NAV packet.
            maxlen = DVDReadBlocks(m_dvd_title, static_cast<int>(m_cur_block), 1, m_buffer);
            if (maxlen != 1)
            {
                Logging::error(m_path, "Read failed for block %1", m_cur_block);
                m_errno = EIO;
                return -1;
            }
            assert(is_nav_pack(m_buffer));

            // Parse the contained dsi packet.
            dsitype = handle_DSI(&dsi_pack, cur_output_size, next_vobu, m_buffer);
            assert(m_cur_block == dsi_pack.dsi_gi.nv_pck_lbn);
            assert(cur_output_size < 1024);
            m_cur_block++;

            // Read in and output cur_output_size packs.
            maxlen = DVDReadBlocks(m_dvd_title, static_cast<int>(m_cur_block), cur_output_size, m_buffer);

            if (maxlen != static_cast<int>(cur_output_size))
            {
                Logging::error(m_path, "Read failed for %1 blocks at %2", cur_output_size, m_cur_block);
                m_errno = EIO;
                return -1;
            }

            unsigned int netsize = cur_output_size * DVD_VIDEO_LB_LEN;

            netsize = demux_pes(m_data, m_buffer, netsize);

            if (netsize > static_cast<unsigned int>(size))
            {
                result_len = static_cast<unsigned int>(size);
                memcpy(data, m_data, result_len);

                m_rest_size = netsize - static_cast<unsigned int>(size);
                m_rest_pos = static_cast<unsigned int>(size);
            }
            else
            {
                result_len = netsize;
                memcpy(data, m_data, result_len);
            }
            m_cur_block = next_vobu;
        }

        //break;
    }

    // DSITYPE_EOF_TITLE - end of title
    // DSITYPE_EOF_CHAPTER - end of chapter
    if (dsitype != DSITYPE_CONTINUE) //if (hargn == DSITYPE_EOF_TITLE)
    {
        m_is_eof = true;
    }

    m_cur_pos += result_len;

    return static_cast<int>(result_len);
}

int dvdio::error() const
{
    return m_errno;
}

int64_t dvdio::duration() const
{
    return m_duration;
}

size_t dvdio::size() const
{
    if (m_cur_pgc == nullptr)
    {
        return 0;
    }
    else
    {
        return (m_cur_pgc->cell_playback[m_start_cell].last_sector - m_cur_pgc->cell_playback[m_start_cell].first_sector) * DVD_VIDEO_LB_LEN;
    }
}

size_t dvdio::tell() const
{
    return m_cur_pos;
}

int dvdio::seek(long offset, int /*whence*/)
{
    if (!offset)
    {
        m_next_cell = m_start_cell;
        m_last_cell = m_cur_pgc->nr_of_cells;
        m_cur_cell = m_start_cell;

        m_goto_next_cell = true;
        m_is_eof = false;
        m_errno = 0;
        m_rest_size = 0;
        m_rest_pos = 0;
        m_cur_pos = 0;
        return 0;
    }
    errno = EPERM;
    return -1;
}

bool dvdio::eof() const
{
    return m_is_eof;
}

void dvdio::close()
{
    if (m_vts_file != nullptr)
    {
        ifoClose(m_vts_file);
        m_vts_file = nullptr;
    }
    if (m_vmg_file != nullptr)
    {
        ifoClose(m_vmg_file);
        m_vmg_file = nullptr;
    }
    if (m_dvd_title != nullptr)
    {
        DVDCloseFile(m_dvd_title);
        m_dvd_title = nullptr;
    }
    if (m_dvd != nullptr)
    {
        DVDClose(m_dvd);
        m_dvd = nullptr;
    }

}

// Returns true if the pack is a NAV pack. 
// Code nicked from Handbrake (https://github.com/HandBrake/HandBrake/blob/master/libhb/dvd.c)
bool dvdio::is_nav_pack(const unsigned char *buffer) const
{
    /*
     * The NAV Pack is comprised of the PCI Packet and DSI Packet, both
     * of these start at known offsets and start with a special identifier.
     *
     * NAV = {
     *  PCI = { 00 00 01 bf  # private stream header
     *          ?? ??        # length
     *          00           # substream
     *          ...
     *        }
     *  DSI = { 00 00 01 bf  # private stream header
     *          ?? ??        # length
     *          01           # substream
     *          ...
     *        }
     *
     * The PCI starts at offset 0x26 into the sector, and the DSI starts at 0x400
     *
     * This information from: http://dvd.sourceforge.net/dvdinfo/
     */
    if ((buffer[0x26] == 0x00 &&      // PCI
         buffer[0x27] == 0x00 &&
         buffer[0x28] == 0x01 &&
         buffer[0x29] == 0xbf &&
         buffer[0x2c] == 0x00) &&
            (buffer[0x400] == 0x00 &&     // DSI
             buffer[0x401] == 0x00 &&
             buffer[0x402] == 0x01 &&
             buffer[0x403] == 0xbf &&
             buffer[0x406] == 0x01))
    {
        return (1);
    } else {
        return (0);
    }
}

#endif // USE_LIBDVD

