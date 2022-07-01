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
 *
 * On Debian systems, the complete text of the GNU General Public License
 * Version 3 can be found in `/usr/share/common-licenses/GPL-3'.
 */

/**
 * @file cuesheetparser.cc
 * @brief Cue sheet parser implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "ffmpegfs.h"
#include "cuesheetparser.h"
#include "transcode.h"
#include "logging.h"

#include <libcue.h>

#define FPS (75)                                            ///<* On sector contains 75 frames per CD specs
#define VAL_OR_EMPTY(val)   ((val) != nullptr ? (val) : "") ///<* Return string or empty string if val is nullptr

/**
  * @brief Cuesheet structure
  * Structure see https://en.wikipedia.org/wiki/Cue_sheet_(computing) @n
  * @n
  * Real life example: @n
  * @n
  * PERFORMER "Subway to Sally" @n
  * TITLE "Nord Nord Ost" @n
  * CATALOG 0727361134129 @n
  * REM DATE 2005 @n
  * REM DISCNUMBER 1 @n
  * REM TOTALDISCS 1 @n
  * FILE "Subway to Sally - Nord Nord Ost.flac" WAVE @n
  *   TRACK 01 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Sarabande de noir" @n
  *     INDEX 01 00:00:00 @n
  *   TRACK 02 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Schneekönigin" @n
  *     INDEX 01 00:55:30 @n
  *   TRACK 03 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Feuerland" @n
  *     INDEX 01 06:41:38 @n
  *   TRACK 04 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Sieben" @n
  *     INDEX 01 10:48:42 @n
  *   TRACK 05 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Lacrimae '74" @n
  *     INDEX 01 14:11:14 @n
  *   TRACK 06 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Feuerkind" @n
  *     INDEX 01 15:57:07 @n
  *   TRACK 07 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Das Rätsel II" @n
  *     INDEX 01 22:03:28 @n
  *   TRACK 08 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "S.O.S." @n
  *     INDEX 01 26:25:23 @n
  *   TRACK 09 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Eisblumen" @n
  *     INDEX 01 32:21:42 @n
  *   TRACK 10 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Seemannslied" @n
  *     INDEX 01 36:53:71 @n
  */

static bool create_cuesheet_virtualfile(LPCVIRTUALFILE virtualfile, Track *track, int titleno, const std::string & path, const struct stat * statbuf, int trackcount, int trackno, const std::string &aperformer, const std::string & album, const std::string & genre, const std::string & date, int64_t *remainingduration, LPVIRTUALFILE *lastfile);
static int parse_cuesheet_file(LPCVIRTUALFILE virtualfile, const std::string & cuesheet, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);
static int parse_cuesheet_text(LPCVIRTUALFILE virtualfile, void *buf, fuse_fill_dir_t filler);
static int parse_cuesheet(LPCVIRTUALFILE virtualfile, const std::string & cuesheet, Cd *cd, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);

/**
 * @brief Create a virtual file entry of a cue sheet title.
 * @param[in] virtualfile - VIRTUALFILE struct of a file.
 * @param[in] track - libcue2 track handle
 * @param[in] titleno - Title number.
 * @param[in] path - Path to check.
 * @param[in] statbuf - File status structure of original file.
 * @param[in] trackcount - Number of tracks in cue sheet.
 * @param[in] trackno - Track number.
 * @param[in] aperformer - Album performer.
 * @param[in] album - Name of album.
 * @param[in] genre - Album genre.
 * @param[in] date - Publishing date.
 * @param[inout] remainingduration - Remaining duration of file in AV_TIME_BASE fractions.
 * @param[in] lastfile - Pointer to last virtual file. May be nullptr if none exists.
 * @return On error, returns false. On success, returns true.
 */
static bool create_cuesheet_virtualfile(LPCVIRTUALFILE virtualfile, Track *track, int titleno, const std::string & path, const struct stat * statbuf, int trackcount, int trackno, const std::string & aperformer, const std::string & album, const std::string & genre, const std::string & date, int64_t *remainingduration, LPVIRTUALFILE *lastfile)
{
    Cdtext *cuesheetcdtext = track_get_cdtext(track);
    if (cuesheetcdtext == nullptr)
    {
        Logging::error(virtualfile->m_origfile, "The track CD text could not be extracted from the cue sheet.");
        errno = EIO;
        return false;
    }

    std::string performer   = VAL_OR_EMPTY(cdtext_get(PTI_PERFORMER, cuesheetcdtext));
    std::string title       = VAL_OR_EMPTY(cdtext_get(PTI_TITLE, cuesheetcdtext));

    if (performer.empty())
    {
		// If track performer is empty, try album performer.
        performer = aperformer;
    }

    int64_t start           = AV_TIME_BASE * static_cast<int64_t>(track_get_start(track)) / FPS;
    int64_t length          = track_get_length(track);
    int64_t duration;

    if (length > -1)
    {
        duration            = AV_TIME_BASE * length / FPS;
        *remainingduration  -= duration;
    }
    else
    {
        // Length of
        duration            = *remainingduration;
    }

    std::string virtfilename;

    strsprintf(&virtfilename, "%02d. %s - %s [%s].%s",
             titleno,
             performer.c_str(),
             title.c_str(),
             replace_all(format_duration(duration), ":", "-").c_str(),
             ffmpeg_format[virtualfile->m_format_idx].fileext().c_str());

    // Filenames can't contain '/' in POSIX etc.
    std::replace(virtfilename.begin(), virtfilename.end(), '/', '-');

    LPVIRTUALFILE newvirtualfile = nullptr;
    if (!ffmpeg_format[0].is_multiformat())
    {
        newvirtualfile = insert_file(VIRTUALTYPE_DISK, path + virtfilename, virtualfile->m_origfile, statbuf, VIRTUALFLAG_CUESHEET);
    }
    else
    {
        newvirtualfile = insert_dir(VIRTUALTYPE_DISK, path + virtfilename, statbuf, VIRTUALFLAG_CUESHEET);
    }

    if (newvirtualfile == nullptr)
    {
        Logging::error(path, "Failed to create virtual path: %1", (path + virtfilename).c_str());
        errno = EIO;
        return false;
    }

    // We do not add the file to fuse here because it's in a sub directory.
    // Will be done later on request by load_path()

    newvirtualfile->m_format_idx = virtualfile->m_format_idx;    // Store the correct index (audio) in m_format_idx

    if (!transcoder_cached_filesize(newvirtualfile, &newvirtualfile->m_st))
    {
        BITRATE video_bit_rate  = 0;	///< @todo probe original file for info
        BITRATE audio_bit_rate  = 0;

        int width               = 0;
        int height              = 0;
        AVRational framerate    = { 0, 0 };
        bool interleaved        = false;

        newvirtualfile->m_duration                      = duration;
        newvirtualfile->m_cuesheet_track.m_duration     = duration;
        newvirtualfile->m_cuesheet_track.m_start        = start;
        newvirtualfile->m_cuesheet_track.m_tracktotal   = trackcount;
        newvirtualfile->m_cuesheet_track.m_trackno      = trackno;
        newvirtualfile->m_cuesheet_track.m_artist       = performer;
        newvirtualfile->m_cuesheet_track.m_title        = title;
        newvirtualfile->m_cuesheet_track.m_album        = album;
        newvirtualfile->m_cuesheet_track.m_genre        = genre;
        newvirtualfile->m_cuesheet_track.m_date         = date;
        if (*lastfile != nullptr)
        {
            (*lastfile)->m_cuesheet_track.m_nextfile    = newvirtualfile;
        }
        *lastfile                                       = newvirtualfile;

        transcoder_set_filesize(newvirtualfile, duration, audio_bit_rate, virtualfile->m_channels, virtualfile->m_sample_rate, AV_SAMPLE_FMT_NONE, video_bit_rate, width, height, interleaved, framerate);

        stat_set_size(&newvirtualfile->m_st, newvirtualfile->m_predicted_size);
    }

    return true;
}

/**
 * @brief Parse a cue sheet file and build virtual set of files
 * @param[in] virtualfile - VIRTUALFILE struct of a file.
 * @param[in] cuesheet - Name of cue sheet file
 * @param[in] statbuf - File status structure of original file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @return On success, returns number of titles in cue sheet. On error, returns -errno.
 */
static int parse_cuesheet_file(LPCVIRTUALFILE virtualfile, const std::string & cuesheet, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    // Check for cue sheet
    std::string text;
    int res = read_file(cuesheet, text);
    if (res >= 0)
    {
        return -res;
    }

    // Found cue sheet
    Logging::trace(cuesheet, "Found an external cue sheet file.");

    return parse_cuesheet(virtualfile, cuesheet, cue_parse_string(text.c_str()), statbuf, buf, filler);
}

/**
 * @brief Parse a cue sheet and build virtual set of files
 * @param[in] virtualfile - VIRTUALFILE struct of a file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @return On success, returns number of titles in cue sheet. On error, returns -errno.
 */
static int parse_cuesheet_text(LPCVIRTUALFILE virtualfile, void *buf, fuse_fill_dir_t filler)
{
    // Found cue sheet
    Logging::trace(virtualfile->m_origfile, "Found an embedded cue sheet file.");

    return parse_cuesheet(virtualfile, virtualfile->m_origfile, cue_parse_string(virtualfile->m_cuesheet.c_str()), &virtualfile->m_st, buf, filler);
}

/**
 * @brief Parse a cue sheet and build virtual set of files
 * @param[in] virtualfile - VIRTUALFILE struct of a file.
 * @param[in] cuesheet - Name of cue sheet file
 * @param[in] cd - libcue2 cue sheet handle
 * @param[in] statbuf - File status structure of original file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @return On success, returns number of titles in cue sheet or 0 if not found. On error, returns -errno.
 */
static int parse_cuesheet(LPCVIRTUALFILE virtualfile, const std::string & cuesheet, Cd *cd, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    int res = 0;

    try
    {
        if (cd == nullptr)
        {
            Logging::error(cuesheet, "The cue sheet could not be parsed.");
            throw AVERROR(EIO);
        }

        Rem *rem = cd_get_rem(cd);
        if (rem == nullptr)
        {
            Logging::error(cuesheet, "Unable to parse remarks from the cue sheet.");
            throw AVERROR(EIO);
        }

        Cdtext *cdtext = cd_get_cdtext(cd);
        if (cdtext == nullptr)
        {
            Logging::error(cuesheet, "The CD text could not be extracted from the cue sheet.");
            throw AVERROR(EIO);
        }

        std::string performer  = VAL_OR_EMPTY(cdtext_get(PTI_PERFORMER, cdtext));
        std::string album      = VAL_OR_EMPTY(cdtext_get(PTI_TITLE, cdtext));
        std::string genre      = VAL_OR_EMPTY(cdtext_get(PTI_GENRE, cdtext));
        std::string date       = VAL_OR_EMPTY(rem_get(REM_DATE, rem));

        int trackcount = static_cast<int>(cd_get_ntrack(cd));
        if (trackcount)
        {
            LPVIRTUALFILE insertedvirtualfile = nullptr;
            std::string subbdir(virtualfile->m_origfile);

            append_ext(&subbdir, TRACKDIR);

            std::string dirname(subbdir);

            append_sep(&subbdir);
            remove_path(&dirname);

            insertedvirtualfile = insert_dir(VIRTUALTYPE_DISK, subbdir, statbuf, VIRTUALFLAG_CUESHEET);

            if (insertedvirtualfile == nullptr)
            {
                Logging::error(cuesheet, "Failed to create virtual path: %1", subbdir.c_str());
                errno = EIO;
                return -errno;
            }

            if (buf != nullptr && filler(buf, dirname.c_str(), &insertedvirtualfile->m_st, 0))
            {
                // break;
            }

            std::string path(virtualfile->m_origfile);

            remove_filename(&path);

            LPVIRTUALFILE lastfile = nullptr;
            int64_t remainingduration   = virtualfile->m_duration;

            for (int trackno = 1; trackno <= trackcount; trackno++)
            {
                Track *track = cd_get_track(cd, trackno);
                if (track == nullptr)
                {
                    Logging::error(cuesheet, "Track no. %1 could not be obtained from the cue sheet.", trackno);
                    errno = EIO;
                    throw -errno;
                }

                if (!create_cuesheet_virtualfile(virtualfile, track, trackno, path + dirname + "/", statbuf, trackcount, trackno, performer, album, genre, date, &remainingduration, &lastfile))
                {
                    throw -errno;
                }
            }
        }

        res = trackcount;
    }
    catch (int _res)
    {
        res = _res;
    }

    if (cd != nullptr)
    {
        cd_delete(cd);
    }

    return res;
}

int check_cuesheet(const std::string & filename, void *buf, fuse_fill_dir_t filler)
{
    std::string trackdir(filename);     // Tracks directory (with extra TRACKDIR extension)
    std::string cuesheet(filename);     // Cue sheet name (original name, extension replaced by .cue)
    struct stat stbuf;
    int res = 0;

    append_ext(&trackdir, TRACKDIR);    // Need to add TRACKDIR to file name
    append_sep(&trackdir);
    replace_ext(&cuesheet, "cue");      // Get the cue sheet file name by replacing the extension with .cue

    try
    {
        LPCVIRTUALFILE virtualfile = find_file_from_orig(filename);

        if (virtualfile == nullptr)
        {
            // Should never happen...
            Logging::error(filename, "INTERNAL ERROR: check_cuesheet()! virtualfile is NULL.");
            errno = EINVAL;
            throw -errno;
        }

        if (lstat(filename.c_str(), &stbuf) == -1)
        {
            // Media file does not exist, can be ignored silently
            throw 0;
        }

        if (lstat(cuesheet.c_str(), &stbuf) != -1)
        {
            // Cue sheet file exists, preferrably use its contents
            if (!check_path(trackdir))
            {
                // Not a virtual directory
                res = parse_cuesheet_file(virtualfile, cuesheet, &stbuf, buf, filler);

                Logging::trace(cuesheet, "%1 titles were discovered.", res);
            }
            else
            {
                // Obviously a virtual directory, simply add it
                std::string dirname(trackdir);

                remove_path(&dirname);

                LPCVIRTUALFILE virtualdir = find_file(trackdir);

                if (virtualdir == nullptr)
                {
                    Logging::error(filename, "INTERNAL ERROR: check_cuesheet()! virtualdir is NULL.");
                    errno = EIO;
                    throw -errno;
                }

                if (buf != nullptr && filler(buf, dirname.c_str(), &virtualdir->m_st, 0))
                {
                    // break;
                }

                res = 0;
            }
        }
        else if (!virtualfile->m_cuesheet.empty())
        {
            // No cue sheet file, but there is one embedded in media file
            res = parse_cuesheet_text(virtualfile, buf, filler);
        }
    }
    catch (int _res)
    {
        res = _res;
    }
    return res;
}
