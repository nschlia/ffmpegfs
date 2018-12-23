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
    , m_cur_pack(0)
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
    if ( !m_dvd )
    {
        Logging::error(m_path, "Couldn't open DVD.");
        return EINVAL;
    }

    // Load the video manager to find out the information about the titles on this disc.
    m_vmg_file = ifoOpen( m_dvd, 0 );
    if ( !m_vmg_file )
    {
        Logging::error(m_path, "Can't open VMG info." );
        DVDClose( m_dvd );
        return EINVAL;
    }
    tt_srpt = m_vmg_file->tt_srpt;

    // Make sure our title number is valid.
    Logging::trace(nullptr, "There are %1 titles on this DVD.", static_cast<uint16_t>(tt_srpt->nr_of_srpts) );

    if ( m_title_no < 0 || m_title_no >= tt_srpt->nr_of_srpts )
    {
        Logging::error(m_path, "Invalid title %1.", m_title_no + 1 );
        ifoClose( m_vmg_file );
        DVDClose( m_dvd );
        return EINVAL;
    }

    // Make sure the chapter number is valid for this title.
    Logging::trace(nullptr, "There are %1 chapters in this title.", static_cast<uint16_t>(tt_srpt->title[ m_title_no ].nr_of_ptts));

    if ( m_chapter_no < 0 || m_chapter_no >= tt_srpt->title[ m_title_no ].nr_of_ptts )
    {
        Logging::error(m_path, "Invalid chapter %1", m_chapter_no + 1 );
        ifoClose( m_vmg_file );
        DVDClose( m_dvd );
        return EINVAL;
    }

    // Make sure the angle number is valid for this title.
    Logging::trace(m_path, "There are %1 angles in this title.", tt_srpt->title[ m_title_no ].nr_of_angles );

    if ( m_angle_no < 0 || m_angle_no >= tt_srpt->title[ m_title_no ].nr_of_angles )
    {
        Logging::error(nullptr, "Invalid angle %1", m_angle_no + 1 );
        ifoClose( m_vmg_file );
        DVDClose( m_dvd );
        return EINVAL;
    }

    // Load the VTS information for the title set our title is in.
    m_vts_file = ifoOpen( m_dvd, tt_srpt->title[ m_title_no ].title_set_nr );
    if ( !m_vts_file )
    {
        Logging::error(m_path, "Can't open the title %1 info file.", tt_srpt->title[ m_title_no ].title_set_nr );
        ifoClose( m_vmg_file );
        DVDClose( m_dvd );
        return EINVAL;
    }

    // Determine which program chain we want to watch.  This is based on the chapter number.
    ttn = tt_srpt->title[ m_title_no ].vts_ttn;
    vts_ptt_srpt = m_vts_file->vts_ptt_srpt;
    pgc_id = vts_ptt_srpt->title[ ttn - 1 ].ptt[ m_chapter_no ].pgcn;
    pgn = vts_ptt_srpt->title[ ttn - 1 ].ptt[ m_chapter_no ].pgn;
    m_cur_pgc = m_vts_file->vts_pgcit->pgci_srp[ pgc_id - 1 ].pgc;
    m_start_cell = m_cur_pgc->program_map[ pgn - 1 ] - 1;

    // We've got enough info, time to open the title set data.
    m_dvd_title = DVDOpenFile( m_dvd, tt_srpt->title[ m_title_no ].title_set_nr, DVD_READ_TITLE_VOBS );
    if ( !m_dvd_title )
    {
        Logging::error(m_path, "Can't open title VOBS (VTS_%<%02d>1_1.VOB).", tt_srpt->title[ m_title_no ].title_set_nr );
        ifoClose( m_vts_file );
        ifoClose( m_vmg_file );
        DVDClose( m_dvd );
        return EINVAL;
    }

    m_next_cell = m_start_cell;
    m_last_cell = m_cur_pgc->nr_of_cells;
    m_cur_cell = m_start_cell;

    {
        int first_cell = m_next_cell;

        // Check if we're entering an angle block.
        if ( m_cur_pgc->cell_playback[ first_cell ].block_type == BLOCK_TYPE_ANGLE_BLOCK )
        {
            first_cell += m_angle_no;
        }

        int framerate = ((m_cur_pgc->cell_playback[ first_cell ].playback_time.frame_u & 0xc0) >> 6);
        int64_t frac = static_cast<int64_t>((m_cur_pgc->cell_playback[ first_cell ].playback_time.frame_u & 0x3f) * AV_TIME_BASE / ((framerate == 3) ? 25 : 29.97));
        m_duration = static_cast<int64_t>((m_cur_pgc->cell_playback[ first_cell ].playback_time.hour * 60 + m_cur_pgc->cell_playback[ first_cell ].playback_time.minute) * 60 + m_cur_pgc->cell_playback[ first_cell ].playback_time.second) * AV_TIME_BASE + frac;
    }

    m_goto_next_cell = true;
    m_is_eof = false;
    m_errno = 0;
    m_rest_size = 0;
    m_rest_pos = 0;
    m_cur_pos = 0;

    return 0;
}

int dvdio::read(void * dataX, int size)
{
    unsigned int cur_output_size;
    ssize_t maxlen;
    int result_len = 0;

    if (m_rest_size)
    {
        result_len = static_cast<int>(m_rest_size);

        assert(m_rest_size < static_cast<size_t>(size));

        memcpy(dataX, &m_data[m_rest_pos], m_rest_size);

        m_rest_size = m_rest_pos = 0;

        return result_len;
    }

    // Playback by cell in this pgc, starting at the cell for our chapter.
    //while (next_cell < last_cell)
    {
        if (m_goto_next_cell)
        {
            m_goto_next_cell = false;

            m_cur_cell = m_next_cell;

            /* Check if we're entering an angle block. */
            if ( m_cur_pgc->cell_playback[ m_cur_cell ].block_type == BLOCK_TYPE_ANGLE_BLOCK )
            {
                int i;

                m_cur_cell += m_angle_no;
                for( i = 0;; ++i )
                {
                    if ( m_cur_pgc->cell_playback[ m_cur_cell + i ].block_mode == BLOCK_MODE_LAST_CELL )
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

            m_cur_pack = m_cur_pgc->cell_playback[ m_cur_cell ].first_sector;
        }

        if (m_cur_pack >= m_cur_pgc->cell_playback[ m_cur_cell ].last_sector)
        {
            m_is_eof = false;
            return 0;
        }

        // We loop until we're out of this cell.
        //        for( cur_pack = cur_pgc->cell_playback[ cur_cell ].first_sector;
        //             cur_pack < cur_pgc->cell_playback[ cur_cell ].last_sector; )
        {

            dsi_t dsi_pack;
            unsigned int next_vobu; //, cur_output_size;

            /**
             * Read NAV packet.
             */
            maxlen = DVDReadBlocks( m_dvd_title, m_cur_pack, 1, m_data );
            if ( maxlen != 1 )
            {
                Logging::error(m_path, "Read failed for block %1", m_cur_pack );
                //                ifoClose( vts_file );
                //                ifoClose( vmg_file );
                //                DVDCloseFile( title );
                //                DVDClose( dvd );
                m_errno = EIO;
                return -1;
            }
            //assert( is_nav_pack( m_data ) );

            // Parse the contained dsi packet.
            navRead_DSI( &dsi_pack, &(m_data[ DSI_START_BYTE ]) );
            assert( m_cur_pack == dsi_pack.dsi_gi.nv_pck_lbn );


            // Determine where we go next.  These values are the ones we mostly care about.
#if 0
            next_ilvu_start = cur_pack + dsi_pack.sml_agli.data[ angle ].address;
#endif
            cur_output_size = dsi_pack.dsi_gi.vobu_ea;


            /**
             * If we're not at the end of this cell, we can determine the next
             * VOBU to display using the VOBU_SRI information section of the
             * DSI.  Using this value correctly follows the current angle,
             * avoiding the doubled scenes in The Matrix, and makes our life
             * really happy.
             *
             * Otherwise, we set our next address past the end of this cell to
             * force the code above to go to the next cell in the program.
             */
            if ( dsi_pack.vobu_sri.next_vobu != SRI_END_OF_CELL )
            {
                next_vobu = m_cur_pack + ( dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
            }
            else
            {
                next_vobu = m_cur_pack + cur_output_size + 1;
            }

            assert( cur_output_size < 1024 );
            m_cur_pack++;

            /**
             * Read in and output cursize packs.
             */
            maxlen = DVDReadBlocks( m_dvd_title, static_cast<int>(m_cur_pack), cur_output_size, m_data);
            if (maxlen != static_cast<int>(cur_output_size))
            {
                Logging::error(m_path, "Read failed for %d blocks at %1", cur_output_size, m_cur_pack );
                m_errno = EIO;
                return -1;
            }

            //fwrite( data, cur_output_size, DVD_VIDEO_LB_LEN, stdout );
            if (cur_output_size * DVD_VIDEO_LB_LEN > static_cast<unsigned int>(size))
            {
                result_len = size;
                memcpy(dataX, m_data, result_len);

                m_rest_size = cur_output_size * DVD_VIDEO_LB_LEN - static_cast<unsigned int>(size);
                m_rest_pos = size;
            }
            else
            {
                result_len = cur_output_size * DVD_VIDEO_LB_LEN;
                memcpy(dataX, m_data, result_len);
            }
            m_cur_pack = next_vobu;
        }

        //break;
    }

    if (m_cur_pack >= m_cur_pgc->cell_playback[ m_cur_cell ].last_sector)
    {
        m_is_eof = true;
    }

    m_cur_pos += result_len;

    return result_len;
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
        return (m_cur_pgc->cell_playback[ m_start_cell ].last_sector - m_cur_pgc->cell_playback[ m_start_cell ].first_sector) * DVD_VIDEO_LB_LEN;
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

// Returns true if the pack is a NAV pack.  This check is clearly insufficient,
// and sometimes we incorrectly think that valid other packs are NAV packs.  I
// need to make this stronger.
bool dvdio::is_nav_pack(const unsigned char *buffer) const
{
    return (buffer[ 41 ] == 0xbf && buffer[ 1027 ] == 0xbf);
}

#endif // USE_LIBDVD

