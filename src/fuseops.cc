/*
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2012 K. Henriksson
 * Copyright (C) 2017-2023 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file fuseops.cc
 * @brief Fuse operations implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2023 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "transcode.h"
#include "cache_maintenance.h"
#include "logging.h"
#ifdef USE_LIBVCD
#include "vcdparser.h"
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
#include "dvdparser.h"
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
#include "blurayparser.h"
#endif // USE_LIBBLURAY
#include "cuesheetparser.h"
#include "thread_pool.h"
#include "buffer.h"
#include "cache_entry.h"

#include <dirent.h>
#include <list>
#include <csignal>
#include <cstring>

typedef std::map<const std::string, VIRTUALFILE> FILENAME_MAP;      /**< @brief Map virtual file names to virtual file objects. */
typedef std::map<const std::string, VIRTUALFILE> RFILENAME_MAP;     /**< @brief Map source file names to virtual file objects. */

static void                         init_stat(struct stat *stbuf, size_t fsize, time_t ftime, bool directory);
static LPVIRTUALFILE                make_file(void *buf, fuse_fill_dir_t filler, VIRTUALTYPE type, const std::string & origpath, const std::string & filename, size_t fsize, time_t ftime = time(nullptr), int flags = VIRTUALFLAG_NONE);
static void                         prepare_script();
static bool                         is_passthrough(const std::string & ext);
static bool                         virtual_name(std::string *virtualpath, const std::string &origpath = "", const FFmpegfs_Format **current_format = nullptr);
static FILENAME_MAP::const_iterator find_prefix(const FILENAME_MAP & map, const std::string & search_for);
static void                         stat_to_dir(struct stat *stbuf);
static void                         flags_to_dir(int *flags);
static void                         insert(const VIRTUALFILE & virtualfile);
static int                          get_source_properties(const std::string & origpath, LPVIRTUALFILE virtualfile);
static int                          make_hls_fileset(void * buf, fuse_fill_dir_t filler, const std::string & origpath, LPVIRTUALFILE virtualfile);
static int                          kick_next(LPVIRTUALFILE virtualfile);
static void                         sighandler(int signum);
static std::string                  get_number(const char *path, uint32_t *value);
static size_t                       guess_format_idx(const std::string & filepath);
static int                          parse_file(LPVIRTUALFILE newvirtualfile);
static const FFmpegfs_Format * 		get_format(LPVIRTUALFILE newvirtualfile);
static int                          selector(const struct dirent * de);
static int                          scandir(const char *dirp, std::vector<struct dirent> * _namelist, int (*selector) (const struct dirent *), int (*cmp) (const struct dirent **, const struct dirent **));

static int                          ffmpegfs_readlink(const char *path, char *buf, size_t size);
static int                          ffmpegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int                          ffmpegfs_getattr(const char *path, struct stat *stbuf);
static int                          ffmpegfs_fgetattr(const char *path, struct stat * stbuf, struct fuse_file_info *fi);
static int                          ffmpegfs_open(const char *path, struct fuse_file_info *fi);
static int                          ffmpegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int                          ffmpegfs_statfs(const char *path, struct statvfs *stbuf);
static int                          ffmpegfs_release(const char *path, struct fuse_file_info *fi);
static void *                       ffmpegfs_init(struct fuse_conn_info *conn);
static void                         ffmpegfs_destroy(__attribute__((unused)) void * p);

static FILENAME_MAP         filenames;          /**< @brief Map files to virtual files */
static RFILENAME_MAP        rfilenames;         /**< @brief Reverse map virtual files to real files */
static std::vector<char>    script_file;        /**< @brief Buffer for the virtual script if enabled */

static struct sigaction     oldHandler;         /**< @brief Saves old SIGINT handler to restore on shutdown */

bool                        docker_client;      /**< @brief True if running inside a Docker container */

fuse_operations             ffmpegfs_ops;       /**< @brief FUSE file system operations */

thread_pool*                tp;                 /**< @brief Thread pool object */

/**
 * @brief Check if a file should be treated passthrough, i.e. bitmaps etc.
 * @param[in] ext - Extension of file to check
 * @return Returns true if file is passthrough, false if not
 */
static bool is_passthrough(const std::string & ext)
{
    /**
  *
  * List of passthrough file extensions
  */
    static const std::set<std::string, comp> passthrough_set =
    {
        "AA",
        "ACR",		// Dicom/ACR/IMA file format for medical images
        "AI",		// PostScript Formats (Ghostscript required)
        "ANI",		// Animated Cursor
        "ARW",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "AWD",		// Artweaver format
        "B3D",		// BodyPaint 3D format
        "BMP",		// Windows Bitmap
        "CAM",		// Casio digital camera format (JPG version only)
        "CEL",
        "CGM",		// CAD Formats (Shareware PlugIns)
        "CIN",		// Digital Picture Exchange/Cineon Format
        "CLP",		// Windows Clipboard
        "CPT",		// CorelDraw Photopaint format (CPT version 6 only)
        "CR2",		// Canon RAW format
        "CRW",		// Canon RAW format
        "CUR",		// Animated Cursor
        "DCM",		// Dicom/ACR/IMA file format for medical images
        "DCR",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "DCX",		// Multipage PCX format
        "DDS",		// Direct Draw Surface format
        "DIB",		// Windows Bitmap
        "DJVU",		// DjVu File Format
        "DNG",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "DPX",		// Digital Picture Exchange/Cineon Format
        "DWG",		// CAD Formats (Shareware PlugIns)
        "DXF",		// Drawing Interchange Format, CAD format
        "ECW",		// Enhanced Compressed Wavelet
        "EEF",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "EMF",		// Enhanced Metafile Format
        "EPS",		// PostScript Formats (Ghostscript required)
        "EXR",		// EXR format
        "FITS",     // Flexible Image Transport System
        "FLI",
        "FLIF",     // Free Lossless Image format
        "FPX",		// FlashPix format
        "G3",		// Group 3 Facsimile Apparatus format
        "GIF",		// Graphics Interchange Format
        "HDP",		// JPEG-XR/Microsoft HD Photo format
        "HDR",		// High Dynamic Range format
        "HEIC",     // High Efficiency Image format
        "HPGL",		// CAD Formats (Shareware PlugIns)
        "HRZ",
        "ICL",		// Icon Library formats
        "ICO",		// Windows Icon
        "ICS",		// Image Cytometry Standard format
        "IFF",		// Interchange File Format
        "IMA",		// Dicom/ACR/IMA file format for medical images
        "IMG",		// GEM Raster format
        "IW44",     // DjVu File Format
        "J2K",		// JPEG 2000 format
        "JLS",		// JPEG-LS, JPEG Lossless
        "JNG",		// Multiple Network Graphics
        "JP2",		// JPEG 2000 format
        "JPC",		// JPEG 2000 format
        "JPEG",     // Joint Photographic Experts Group
        "JPG",		// Joint Photographic Experts Group
        "JPM",		// JPEG2000/Part6, LuraDocument.jpm
        "JXR",		// JPEG-XR/Microsoft HD Photo format
        "KDC",		// Kodak digital camera format
        "LBM",		// Interchange File Format
        "M3U8",     // Apple HTTP Live Streaming
        "MIFF",
        "MNG",		// Multiple Network Graphics
        "MRC",		// MRC format
        "MrSID",	// LizardTech's SID Wavelet format
        "MRW",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "NEF",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "NRW",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "ORF",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "PBM",		// Portable Bitmap format
        "PCD",		// Kodak Photo CD
        "PCX",		// PC Paintbrush format from ZSoft Corporation
        "PDF",		// PostScript Formats (Ghostscript required)
        "PDF",		// Portable Document format
        "PDN",		// Paint.NET file format
        "PEF",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "PGM",		// Portable Greymap format
        "PICT",		// Macintosh PICT format
        "PIX",
        "PNG",		// Portable Network Graphics
        "PNM",
        "PPM",		// Portable Pixelmap format
        "PS",		// PostScript Formats (Ghostscript required)
        "PSD",		// Adobe PhotoShop format
        "PSP",		// Paint Shop Pro format
        "PVR",		// DreamCast Texture format
        "QTIF",     // Macintosh PICT format
        "RAF",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "RAS",		// Sun Raster format
        "RAW",		// Raw (binary) data
        "RGB",		// Silicon Graphics format
        "RLE",		// Utah RLE format
        "RW2",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "SFF",		// Structured Fax File
        "SFW",		// Seattle Film Works format
        "SGI",		// Silicon Graphics format
        "SID",		// LizardTech's SID Wavelet format
        "SIF",		// SIF format
        "SRF",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "SUN",		// Sun Raster format
        "Sunras",
        "SVG",		// CAD Formats (Shareware PlugIns)
        "TGA",		// Truevision Advanced Raster Graphics Adapter (TARGA)
        "TIF",		// Tagged Image File Format
        "TIFF",
        "TTF",		// True Type Font
        "TXT",		// Text (ASCII) File (as image)
        "VTF",		// Valve Texture format
        "WAD",		// WAD3 Game format
        "WAL",		// Quake 2 textures
        "WBC",		// Webshots formats
        "WBZ",		// Webshots formats
        "WBMP",     // WAP Bitmap format
        "WDP",		// JPEG-XR/Microsoft HD Photo format
        "WebP",     // Weppy file format
        "WMF",		// Windows Metafile Format
        "WSQ",		// Wavelet Scaler Quantization format
        "X",
        "X3F",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
        "XBM",		// X11 Bitmap
        "XCF",		// GIMP file format
        "XPM",
        "XWD",
        "YUV"		// Raw (binary) data
    };

    return (passthrough_set.find(ext) != passthrough_set.cend());
}

void init_fuse_ops()
{
    std::memset(&ffmpegfs_ops, 0, sizeof(fuse_operations));
    ffmpegfs_ops.getattr  = ffmpegfs_getattr;
    ffmpegfs_ops.fgetattr = ffmpegfs_fgetattr;
    ffmpegfs_ops.readlink = ffmpegfs_readlink;
    ffmpegfs_ops.readdir  = ffmpegfs_readdir;
    ffmpegfs_ops.open     = ffmpegfs_open;
    ffmpegfs_ops.read     = ffmpegfs_read;
    ffmpegfs_ops.statfs   = ffmpegfs_statfs;
    ffmpegfs_ops.release  = ffmpegfs_release;
    ffmpegfs_ops.init     = ffmpegfs_init;
    ffmpegfs_ops.destroy  = ffmpegfs_destroy;
}

/**
 * @brief Read the target of a symbolic link.
 * @param[in] path
 * @param[in] buf - FUSE buffer to fill.
 * @param[in] size
 * @return On success, returns 0. On error, returns -errno.
 */
static int ffmpegfs_readlink(const char *path, char *buf, size_t size)
{
    std::string origpath;
    std::string transcoded;
    ssize_t len;

    Logging::trace(path, "readlink");

    append_basepath(&origpath, path);
    find_original(&origpath);

    len = readlink(origpath.c_str(), buf, size - 2);
    if (len != -1)
    {
        buf[len] = '\0';

        transcoded = buf;
        virtual_name(&transcoded, origpath);

        buf[0] = '\0';
        strncat(buf, transcoded.c_str(), size);

        errno = 0;  // Just to make sure - reset any error
    }

    return -errno;
}

/**
 * @brief Read directory
 * @param[in] path - Physical path to load.
 * @param[in] buf - FUSE buffer to fill.
 * @param[in] filler - Filler function.
 * @return On success, returns 0. On error, returns -errno.
 */
static int ffmpegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t /*offset*/, struct fuse_file_info * /*fi*/)
{
    std::string origpath;

    Logging::trace(path, "readdir");

    append_basepath(&origpath, path);
    append_sep(&origpath);

    // Add a virtual script if enabled
    if (params.m_enablescript)
    {
        LPVIRTUALFILE virtualfile = make_file(buf, filler, VIRTUALTYPE_SCRIPT, origpath, params.m_scriptfile, script_file.size());
        virtualfile->m_file_contents = script_file;
    }

    LPVIRTUALFILE virtualfile = find_original(origpath);

    if (virtualfile == nullptr)
    {
#if defined(USE_LIBBLURAY) || defined(USE_LIBDVD) || defined(USE_LIBVCD)
        int res;
#endif

#ifdef USE_LIBVCD
        res = check_vcd(origpath, buf, filler);
        if (res != 0)
        {
            // Found VCD or error reading VCD
            return (res >= 0 ?  0 : res);
        }
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
        res = check_dvd(origpath, buf, filler);
        if (res != 0)
        {
            // Found DVD or error reading DVD
            return (res >= 0 ?  0 : res);
        }
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
        res = check_bluray(origpath, buf, filler);
        if (res != 0)
        {
            // Found Blu-ray or error reading Blu-ray
            return (res >= 0 ?  0 : res);
        }
#endif // USE_LIBBLURAY
    }

    if (virtualfile == nullptr || !(virtualfile->m_flags & VIRTUALFLAG_FILESET))
    {
        DIR *dp = opendir(origpath.c_str());
        if (dp != nullptr)
        {
            try
            {
                std::map<const std::string, struct stat> files;

                // Read directory contents
                for (struct dirent *de = readdir(dp); de != nullptr; de = readdir(dp))
                {
                    struct stat stbuf;

                    if (lstat((origpath + de->d_name).c_str(), &stbuf) == -1)
                    {
                        // Should actually not happen, file listed by readdir, so it should exist
                        throw ENOENT;
                    }

                    files.insert({ de->d_name, stbuf });
                }

                // Process files
                for (auto& [key, value] : files)
                {
                    std::string origname(key);

                    if (is_blocked(origname))
                    {
                        continue;
                    }

                    std::string origfile;
                    std::string filename(key);
                    struct stat & stbuf = value;
                    int flags = 0;

                    origfile = origpath + origname;

                    std::string origext;
                    find_ext(&origext, filename);

                    if (S_ISREG(stbuf.st_mode) || S_ISLNK(stbuf.st_mode))
                    {
                        const FFmpegfs_Format *current_format = nullptr;

                        // Check if file can be transcoded
                        if (virtual_name(&filename, origpath, &current_format))
                        {
                            if (current_format->video_codec() == AV_CODEC_ID_NONE)
                            {
                                LPVIRTUALFILE newvirtualfile = find_file_from_orig(origfile);

                                if (newvirtualfile == nullptr)
                                {
                                    // Should never happen, we've just created this entry in virtual_name...
                                    Logging::error(origfile, "INTERNAL ERROR: ffmpegfs_readdir()! newvirtualfile is NULL.");
                                    throw EINVAL;
                                }

                                // If target supports no video, we need to do some extra work and checkF
                                // the input file to actually have an audio stream. If not, hide the file,
                                // makes no sense to transcode anyway.
                                if (!newvirtualfile->m_has_audio)
                                {
                                    Logging::debug(origfile, "Unable to transcode. The source has no audio stream, but the target just supports audio.");
                                    flags |= VIRTUALFLAG_HIDDEN;
                                }
                            }

                            if (!(flags & VIRTUALFLAG_HIDDEN))
                            {
                                if (check_cuesheet(origfile, buf, filler) < 0)
                                {
                                    throw EINVAL;
                                }
                            }

                            std::string newext;
                            find_ext(&newext, filename);

                            if (!current_format->is_multiformat())
                            {
                                if (origext != newext || params.m_recodesame == RECODESAME_YES)
                                {
                                    insert_file(VIRTUALTYPE_DISK, origpath + filename, origfile, &stbuf, flags);
                                }
                                else
                                {
                                    insert_file(VIRTUALTYPE_DISK, origpath + filename, origfile, &stbuf, flags | VIRTUALFLAG_PASSTHROUGH);
                                }
                            }
                            else
                            {
                                // Change file to directory for the frame set
                                stat_to_dir(&stbuf);
                                flags_to_dir(&flags);

                                filename = origname;	// Restore original name

                                insert_file(VIRTUALTYPE_DISK, origfile, &stbuf, flags);
                            }
                        }
                    }

                    if (!(flags & VIRTUALFLAG_HIDDEN))
                    {
                        if (add_fuse_entry(buf, filler, filename, &stbuf, 0))
                        {
                            break;
                        }
                    }
                }

                closedir(dp);

                errno = 0;  // Just to make sure - reset any error
            }
            catch (int _errno)
            {
                closedir(dp);

                errno = _errno;
            }
        }
    }
    else
    {
        try
        {
            if (virtualfile->m_flags & (VIRTUALFLAG_FILESET | VIRTUALFLAG_FRAME | VIRTUALFLAG_HLS | VIRTUALFLAG_DIRECTORY))
            {
                add_dotdot(buf, filler, &virtualfile->m_st, 0);
            }

            const FFmpegfs_Format *ffmpegfs_format = params.current_format(virtualfile);

            if (ffmpegfs_format->is_frameset())
            {
                // Generate set of all frames
                if (!virtualfile->m_video_frame_count)
                {
                    int res = get_source_properties(origpath, virtualfile);
                    if (res < 0)
                    {
                        throw EINVAL;
                    }
                }

                //Logging::debug(origpath, "readdir: Creating frame set of %1 frames. %2", virtualfile->m_video_frame_count, virtualfile->m_origfile);

                for (uint32_t frame_no = 1; frame_no <= virtualfile->m_video_frame_count; frame_no++)
                {
                    make_file(buf, filler, virtualfile->m_type, origpath, make_filename(frame_no, params.current_format(virtualfile)->fileext()), virtualfile->m_predicted_size, virtualfile->m_st.st_ctime, VIRTUALFLAG_FRAME); /**< @todo Calculate correct file size for frame image in set */
                }
            }
            else if (ffmpegfs_format->is_hls())
            {
                int res = make_hls_fileset(buf, filler, origpath, virtualfile);
                if (res < 0)
                {
                    throw EINVAL;
                }
            }
            else if (/*virtualfile != nullptr && */virtualfile->m_flags & VIRTUALFLAG_CUESHEET)
            {
                // Fill in list for cue sheet
                load_path(origpath, nullptr, buf, filler);
            }

            errno = 0;  // Just to make sure - reset any error
        }
        catch (int _errno)
        {
            errno = _errno;
        }
    }

    return -errno;
}

/**
 * @brief Get file attributes.
 * @param[in] path - Path of virtual file.
 * @param[in] stbuf - Buffer to store information.
 * @return On success, returns 0. On error, returns -errno.
 */
static int ffmpegfs_getattr(const char *path, struct stat *stbuf)
{
    Logging::trace(path, "getattr");

    if (is_blocked(path))
    {
        errno = ENOENT;
        return -errno;
    }

    std::string origpath;

    append_basepath(&origpath, path);

    LPVIRTUALFILE   virtualfile = find_original(&origpath);
    VIRTUALTYPE     type        = (virtualfile != nullptr) ? virtualfile->m_type : VIRTUALTYPE_DISK;
    int             flags       = (virtualfile != nullptr) ? virtualfile->m_flags : VIRTUALFLAG_NONE;

    if (virtualfile != nullptr && (virtualfile->m_flags & VIRTUALFLAG_HIDDEN))
    {
        errno = ENOENT;
        return -errno;
    }

    if (virtualfile == nullptr && lstat(origpath.c_str(), stbuf) == 0)
    {
        // File was not yet created as virtual file, but physically exists
        const FFmpegfs_Format *current_format = nullptr;
        std::string filename(origpath);

        if (S_ISREG(stbuf->st_mode) && virtual_name(&filename, "", &current_format))
        {
            if (!current_format->is_multiformat())
            {
                // Regular files
                Logging::trace(origpath, "getattr: Existing file.");
                errno = 0;
                return 0;
            }
            else
            {
                Logging::trace(origpath, "getattr: Creating frame set directory of file.");

                flags |= VIRTUALFLAG_FILESET;

                if (current_format->is_frameset())
                {
                    flags |= VIRTUALFLAG_FRAME;
                }
                else if (current_format->is_hls())
                {
                    flags |= VIRTUALFLAG_HLS;
                }
            }
        }
        else
        {
            // Pass-through for regular files
            Logging::trace(origpath, "getattr: Treating existing file/directory as passthrough.");
            errno = 0;
            return 0;
        }
    }
    else if ((flags & VIRTUALFLAG_PASSTHROUGH) && lstat(origpath.c_str(), stbuf) == 0)
    {
        // File physically exists and is marked as passthrough
        Logging::debug(origpath, "getattr: File not recoded because --recodesame=NO.");
        errno = 0;
        return 0;
    }
    else
    {
        // Not really an error.
        errno = 0;
    }

    // This is a virtual file
    bool no_check = false;

    switch (type)
    {
    case VIRTUALTYPE_SCRIPT:
    {
        // Use stored status
        mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
        errno = 0;
        break;
    }
#ifdef USE_LIBVCD
    case VIRTUALTYPE_VCD:
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
    case VIRTUALTYPE_DVD:
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
    case VIRTUALTYPE_BLURAY:
#endif // USE_LIBBLURAY
    {
        // Use stored status
        mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
        no_check = true;    // FILETYPE already known, no need to check again.
        [[clang::fallthrough]];
    }
    case VIRTUALTYPE_DISK:
    {
        if (virtualfile != nullptr && (flags & (VIRTUALFLAG_FRAME | VIRTUALFLAG_HLS | VIRTUALFLAG_DIRECTORY | VIRTUALFLAG_CUESHEET)))
        {
            mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));

            errno = 0;  // Just to make sure - reset any error
            break;
        }

        if (virtualfile == nullptr || !(virtualfile->m_flags & VIRTUALFLAG_FILESET))
        {
            if (!no_check && lstat(origpath.c_str(), stbuf) == -1)
            {
                // If file does not exist here we can assume it's some sort of virtual file: Regular, DVD, S/VCD, cue sheet track
                int error = -errno;

                virtualfile = find_original(&origpath);

                if (virtualfile == nullptr)
                {
                    std::string pathonly(origpath);
                    int res = 0;

                    remove_filename(&pathonly);

#ifdef USE_LIBVCD
                    //if (res <= 0) Not necessary here, will always be true
                    {
                        // Returns -errno or number or titles on VCD
                        res = check_vcd(pathonly);
                    }
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
                    if (res <= 0)
                    {
                        // Returns -errno or number or titles on DVD
                        res = check_dvd(pathonly);
                    }
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
                    if (res <= 0)
                    {
                        // Returns -errno or number or titles on Blu-ray
                        res = check_bluray(pathonly);
                    }
#endif // USE_LIBBLURAY
                    if (res <= 0)
                    {
                        if (check_ext(TRACKDIR, origpath))
                        {
                            std::string origfile(origpath);

                            remove_ext(&origfile);      // remove TRACKDIR extension to get original media file name

                            Logging::trace(origfile, "getattr: Checking for cue sheet.");

                            if (virtual_name(&origfile))
                            {
                                // Returns -errno or number or titles in cue sheet
                                res = check_cuesheet(origfile);
                            }
                        }
                    }

                    if (ffmpeg_format[0].is_frameset())
                    {
                        LPVIRTUALFILE parent_file = find_parent(origpath);

                        if (parent_file != nullptr && (parent_file->m_flags & VIRTUALFLAG_DIRECTORY) && (parent_file->m_flags & VIRTUALFLAG_FILESET))
                        {
                            // Generate set of all frames
                            if (!parent_file->m_video_frame_count)
                            {
                                int res2 = get_source_properties(origpath, parent_file);
                                if (res2 < 0)
                                {
                                    return res2;
                                }
                            }

                            for (uint32_t frame_no = 1; frame_no <= parent_file->m_video_frame_count; frame_no++)
                            {
                                make_file(nullptr, nullptr, parent_file->m_type, parent_file->m_destfile + "/", make_filename(frame_no, params.current_format(parent_file)->fileext()), parent_file->m_predicted_size, parent_file->m_st.st_ctime, VIRTUALFLAG_FRAME); /**< @todo Calculate correct file size for frame image in set */
                            }

                            LPVIRTUALFILE virtualfile2 = find_original(origpath);
                            if (virtualfile2 == nullptr)
                            {
                                // File does not exist
                                return -ENOENT;
                            }

                            mempcpy(stbuf, &virtualfile2->m_st, sizeof(struct stat));

                            // Clear errors
                            errno = 0;

                            return 0;
                        }
                    }
                    else if (ffmpeg_format[0].is_hls())
                    {
                        LPVIRTUALFILE parent_file = find_parent(origpath);

                        if (parent_file != nullptr && (parent_file->m_flags & VIRTUALFLAG_DIRECTORY) && (parent_file->m_flags & VIRTUALFLAG_FILESET))
                        {
                            if (!parent_file->m_video_frame_count)  //***< @todo HLS format: Do audio files source properties get checked over and over?
                            {
                                int res2 = get_source_properties(origpath, parent_file);
                                if (res2 < 0)
                                {
                                    return res2;
                                }
                            }

                            make_hls_fileset(nullptr, nullptr, parent_file->m_destfile + "/", parent_file);

                            LPVIRTUALFILE virtualfile2 = find_original(origpath);
                            if (virtualfile2 == nullptr)
                            {
                                // File does not exist
                                return -ENOENT;
                            }

                            mempcpy(stbuf, &virtualfile2->m_st, sizeof(struct stat));

                            // Clear errors
                            errno = 0;

                            return 0;
                        }
                    }

                    if (res <= 0)
                    {
                        // No Blu-ray/DVD/VCD found or error reading disk
                        return (!res ?  error : res);
                    }
                }

                virtualfile = find_original(&origpath);

                if (virtualfile == nullptr)
                {
                    // Not a DVD/VCD/Blu-ray file or cue sheet track
                    return -ENOENT;
                }

                mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
            }

            if (flags & VIRTUALFLAG_FILESET)
            {
                int flags2 = 0;

                // Change file to virtual directory for the frame set. Keep permissions.
                stat_to_dir(stbuf);
                flags_to_dir(&flags2);

                append_sep(&origpath);

                insert_file(type, origpath, stbuf, flags2);
            }
            else if (S_ISREG(stbuf->st_mode))
            {
                // Get size for resulting output file from regular file, otherwise it's a symbolic link or a virtual frame set.
                if (virtualfile == nullptr)
                {
                    // We should not never end here - report bad file number.
                    return -EBADF;
                }

                if (!transcoder_cached_filesize(virtualfile, stbuf))
                {
                    Cache_Entry* cache_entry = transcoder_new(virtualfile, false);
                    if (cache_entry == nullptr)
                    {
                        return -errno;
                    }

                    stat_set_size(stbuf, transcoder_get_size(cache_entry));

                    transcoder_delete(cache_entry);
                }
            }

            errno = 0;  // Just to make sure - reset any error
        }
        else // if (virtualfile != nullptr && (virtualfile->m_flags & VIRTUALFLAG_DIRECTORY))
        {
            // Frame set, simply report stat.
            mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
            errno = 0;
        }
        break;
    }
        // We should never come here but this shuts up a warning
    case VIRTUALTYPE_PASSTHROUGH:
    case VIRTUALTYPE_BUFFER:
    {
        break;
    }
    }

    return 0;
}

/**
 * @brief Get attributes from an open file
 * @param[in] path
 * @param[in] stbuf
 * @param[in] fi
 * @return On success, returns 0. On error, returns -errno.
 */
static int ffmpegfs_fgetattr(const char *path, struct stat * stbuf, struct fuse_file_info *fi)
{
    std::string origpath;

    Logging::trace(path, "fgetattr");

    errno = 0;

    append_basepath(&origpath, path);

    LPCVIRTUALFILE virtualfile = find_original(&origpath);

    if (virtualfile != nullptr && (virtualfile->m_flags & VIRTUALFLAG_HIDDEN))
    {
        errno = ENOENT;
        return -errno;
    }

    if ((virtualfile == nullptr || (virtualfile->m_flags & VIRTUALFLAG_PASSTHROUGH)) && lstat(origpath.c_str(), stbuf) == 0)
    {
        // passthrough for regular files
        errno = 0;
        return 0;
    }
    else
    {
        // Not really an error.
        errno = 0;
    }

    // This is a virtual file

    bool no_check = false;

    switch (virtualfile->m_type)
    {
#ifdef USE_LIBVCD
    case VIRTUALTYPE_VCD:
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
    case VIRTUALTYPE_DVD:
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
    case VIRTUALTYPE_BLURAY:
#endif // USE_LIBBLURAY
    {
        // Use stored status
        mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
        no_check = true;
        [[clang::fallthrough]];
    }
    case VIRTUALTYPE_DISK:
    {
        if (virtualfile->m_flags & (VIRTUALFLAG_FILESET | VIRTUALFLAG_FRAME | VIRTUALFLAG_HLS | VIRTUALFLAG_DIRECTORY))
        {
            mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
        }
        else
        {
            if (!no_check)
            {
                if (lstat(origpath.c_str(), stbuf) == -1)
                {
                    return -errno;
                }
            }

            // Get size for resulting output file from regular file, otherwise it's a symbolic link.
            if (S_ISREG(stbuf->st_mode))
            {
                Cache_Entry* cache_entry = reinterpret_cast<Cache_Entry*>(fi->fh);

                if (cache_entry == nullptr)
                {
                    Logging::error(path, "fgetattr: Tried to stat unopen file.");
                    errno = EBADF;
                    return -errno;
                }

                uint32_t segment_no = 0;

                stat_set_size(stbuf, transcoder_buffer_watermark(cache_entry, segment_no));
            }
        }

        errno = 0;  // Just to make sure - reset any error

        break;
    }
    case VIRTUALTYPE_SCRIPT:
    {
        mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
        break;
    }
        // We should never come here but this shuts up a warning
    case VIRTUALTYPE_PASSTHROUGH:
    case VIRTUALTYPE_BUFFER:
    {
        break;
    }
    }

    return 0;
}

/**
 * @brief File open operation
 * @param[in] path
 * @param[in] fi
 * @return On success, returns 0. On error, returns -errno.
 */
static int ffmpegfs_open(const char *path, struct fuse_file_info *fi)
{
    std::string origpath;

    Logging::trace(path, "open");

    append_basepath(&origpath, path);

    LPVIRTUALFILE virtualfile = find_original(&origpath);

    if (virtualfile == nullptr || (virtualfile->m_flags & VIRTUALFLAG_PASSTHROUGH))
    {
        int fd = open(origpath.c_str(), fi->flags);
        if (fd != -1)
        {
            close(fd);
            // File is real and can be opened.
            errno = 0;
        }
        else
        {
            if (errno == ENOENT)
            {
                // File does not exist? We should never end up here...
                errno = EINVAL;
            }

            // If file does exist, but can't be opened, return error.
        }

        return -errno;
    }

    // This is a virtual file
    kick_next(virtualfile);

    switch (virtualfile->m_type)
    {
    case VIRTUALTYPE_SCRIPT:
    {
        errno = 0;
        break;
    }
#ifdef USE_LIBVCD
    case VIRTUALTYPE_VCD:
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
    case VIRTUALTYPE_DVD:
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
    case VIRTUALTYPE_BLURAY:
#endif // USE_LIBBLURAY
    case VIRTUALTYPE_DISK:
    {
        if (virtualfile->m_flags & (VIRTUALFLAG_FRAME | VIRTUALFLAG_HLS))
        {
            LPVIRTUALFILE parent_file = find_parent(origpath);

            if (parent_file != nullptr)
            {
                Cache_Entry* cache_entry;

                cache_entry = transcoder_new(parent_file, true);
                if (cache_entry == nullptr)
                {
                    return -errno;
                }

                // Store transcoder in the fuse_file_info structure.
                fi->fh = reinterpret_cast<uintptr_t>(cache_entry);
                // Need this because we do not know the exact size in advance.
                fi->direct_io = 1;
                //fi->keep_cache = 1;

                // Clear errors
                errno = 0;
            }
        }
        else if (!(virtualfile->m_flags & VIRTUALFLAG_FILESET))
        {
            Cache_Entry* cache_entry;

            cache_entry = transcoder_new(virtualfile, true);
            if (cache_entry == nullptr)
            {
                return -errno;
            }

            // Store transcoder in the fuse_file_info structure.
            fi->fh = reinterpret_cast<uintptr_t>(cache_entry);
            // Need this because we do not know the exact size in advance.
            fi->direct_io = 1;
            //fi->keep_cache = 1;

            // Clear errors
            errno = 0;
        }
        break;
    }
        // We should never come here but this shuts up a warning
    case VIRTUALTYPE_PASSTHROUGH:
    case VIRTUALTYPE_BUFFER:
    {
        break;
    }
    }

    return 0;
}

/**
 * @brief Read data from an open file
 * @param[in] path
 * @param[in] buf
 * @param[in] size
 * @param[in] offset
 * @param[in] fi
 * @return On success, returns 0. On error, returns -errno.
 */
static int ffmpegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    std::string origpath;
    size_t locoffset = static_cast<size_t>(offset);  // Cast OK: offset can never be < 0.
    int bytes_read = 0;

    Logging::trace(path, "read: Reading %1 bytes from offset %2 to %3.", size, locoffset, size + locoffset);

    append_basepath(&origpath, path);

    LPVIRTUALFILE virtualfile = find_original(&origpath);

    if (virtualfile == nullptr || (virtualfile->m_flags & VIRTUALFLAG_PASSTHROUGH))
    {
        int fd = open(origpath.c_str(), O_RDONLY);
        if (fd != -1)
        {
            // If this is a real file, pass the call through.
            bytes_read = static_cast<int>(pread(fd, buf, size, offset));
            close(fd);
            if (bytes_read >= 0)
            {
                return bytes_read;
            }
            else
            {
                return -errno;
            }
        }
        else if (errno != ENOENT)
        {
            // File does exist, but can't be opened.
            return -errno;
        }
        else
        {
            // File does not exist, and this is fine.
            errno = 0;
        }
    }

    // This is a virtual file
    bool success = true;

    if (virtualfile == nullptr)
    {
        errno = EINVAL;
        Logging::error(origpath.c_str(), "read: INTERNAL ERROR: ffmpegfs_read()! virtualfile == NULL");
        return -errno;
    }

    switch (virtualfile->m_type)
    {
    case VIRTUALTYPE_SCRIPT:
    {
        if (locoffset >= virtualfile->m_file_contents.size())
        {
            bytes_read = 0;
            break;
        }

        size_t bytes = size;
        if (locoffset + bytes > virtualfile->m_file_contents.size())
        {
            bytes = virtualfile->m_file_contents.size() - locoffset;
        }

        if (bytes)
        {
            std::memcpy(buf, &virtualfile->m_file_contents[locoffset], bytes);
        }

        bytes_read = static_cast<int>(bytes);
        break;
    }
#ifdef USE_LIBVCD
    case VIRTUALTYPE_VCD:
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
    case VIRTUALTYPE_DVD:
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
    case VIRTUALTYPE_BLURAY:
#endif // USE_LIBBLURAY
    case VIRTUALTYPE_DISK:
    {
        if (virtualfile->m_flags & VIRTUALFLAG_FRAME)
        {
            Cache_Entry* cache_entry;

            cache_entry = reinterpret_cast<Cache_Entry*>(fi->fh);

            if (cache_entry == nullptr)
            {
                if (errno)
                {
                    Logging::error(origpath.c_str(), "read: Tried to read from unopen file: (%1) %2", errno, strerror(errno));
                }
                return -errno;
            }

            uint32_t frame_no = 0;
            std::string filename = get_number(path, &frame_no);
            if (!frame_no)
            {
                errno = EINVAL;
                Logging::error(origpath.c_str(), "read: Unable to deduct frame no. from file name (%1): (%2) %3", filename.c_str(), errno, strerror(errno));
                return -errno;
            }

            success = transcoder_read_frame(cache_entry, buf, locoffset, size, frame_no, &bytes_read, virtualfile);
        }
        else if (!(virtualfile->m_flags & VIRTUALFLAG_FILESET))
        {
            Cache_Entry* cache_entry;

            cache_entry = reinterpret_cast<Cache_Entry*>(fi->fh);

            if (cache_entry == nullptr)
            {
                if (errno)
                {
                    Logging::error(origpath.c_str(), "read: Tried to read from unopen file: (%1) %2", errno, strerror(errno));
                }
                return -errno;
            }

            uint32_t segment_no = 0;

            if (virtualfile->m_flags & VIRTUALFLAG_HLS)
            {
                std::string filename = get_number(path, &segment_no);
                if (!segment_no)
                {
                    errno = EINVAL;
                    Logging::error(origpath.c_str(), "read: Unable to deduct segment no. from file name (%1): (%2) %3", filename.c_str(), errno, strerror(errno));
                    return -errno;
                }
            }

            success = transcoder_read(cache_entry, buf, locoffset, size, &bytes_read, segment_no);
        }
        break;
    }
    case VIRTUALTYPE_PASSTHROUGH:
    case VIRTUALTYPE_BUFFER:
    {
        break;
    }
    }

    if (success)
    {
        if (bytes_read)
        {
            Logging::trace(path, "read: Read %1 bytes from offset %2 to %3.", bytes_read, locoffset, static_cast<size_t>(bytes_read) + locoffset);
        }
        else
        {
            Logging::trace(path, "read: Read output file to EOF.");
        }

        return bytes_read;
    }
    else
    {
        return -errno;
    }
}

/**
 * @brief Get file system statistics
 * @param[in] path
 * @param[in] stbuf
 * @return On success, returns 0. On error, returns -errno.
 */
static int ffmpegfs_statfs(const char *path, struct statvfs *stbuf)
{
    std::string origpath;

    Logging::trace(path, "statfs");

    append_basepath(&origpath, path);

    // passthrough for regular files
    if (!origpath.empty() && statvfs(origpath.c_str(), stbuf) == 0)
    {
        errno = 0;  // Just to make sure - reset any error

        return 0;
    }
    else
    {
        // Not really an error.
        errno = 0;
    }

    find_original(&origpath);

    statvfs(origpath.c_str(), stbuf);

    errno = 0;  // Just to make sure - reset any error

    return 0;
}

/**
 * @brief Release an open file
 * @param[in] path
 * @param[in] fi
 * @return On success, returns 0. On error, returns -errno.
 */
static int ffmpegfs_release(const char *path, struct fuse_file_info *fi)
{
    Cache_Entry* cache_entry = reinterpret_cast<Cache_Entry*>(fi->fh);

    Logging::trace(path, "release");

    if (cache_entry != nullptr)
    {
        uint32_t segment_no = 0;

        if (cache_entry->virtualfile()->m_flags & VIRTUALFLAG_HLS)
        {
            std::string filename = get_number(path, &segment_no);
            if (!segment_no)
            {
                errno = EINVAL;
                Logging::error(path, "release: Unable to deduct segment no. from file name (%1): (%2) %3", filename.c_str(), errno, strerror(errno));
            }
            else
            {
                cache_entry->m_buffer->close_file(segment_no - 1, CACHE_FLAG_RO);
            }
        }
        transcoder_delete(cache_entry);
    }

    return 0;
}

/**
 * @brief Initialise filesystem
 * @param[in] conn - fuse_conn_info structure of FUSE. See FUSE docs for details.
 * @return nullptr
 */
static void *ffmpegfs_init(struct fuse_conn_info *conn)
{
    Logging::info(nullptr, "%1 V%2 initialising.", PACKAGE_NAME, FFMPEFS_VERSION);
    Logging::info(nullptr, "Mapping '%1' to '%2'.", params.m_basepath.c_str(), params.m_mountpath.c_str());
    if (docker_client)
    {
        Logging::info(nullptr, "Running inside Docker.");
    }

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sa.sa_handler = sighandler;
    sigaction(SIGINT, &sa, &oldHandler);

    // We need synchronous reads.
    conn->async_read = 0;
    //    conn->async_read = 1;
    //	conn->want |= FUSE_CAP_ASYNC_READ;
    //	conn->want |= FUSE_CAP_SPLICE_READ;

    if (params.m_cache_maintenance)
    {
        if (!start_cache_maintenance(params.m_cache_maintenance))
        {
            exit(1);
        }
    }

    if (params.m_enablescript)
    {
        prepare_script();
    }

    if (tp == nullptr)
    {
        tp = new(std::nothrow)thread_pool(params.m_max_threads);
    }

    tp->init();

    return nullptr;
}

/**
 * @brief Clean up filesystem
 * @param[in] p - unused
 */
static void ffmpegfs_destroy(__attribute__((unused)) void * p)
{
    Logging::info(nullptr, "%1 V%2 terminating.", PACKAGE_NAME, FFMPEFS_VERSION);
    std::printf("%s V%s terminating\n", PACKAGE_NAME, FFMPEFS_VERSION);

    stop_cache_maintenance();

    transcoder_exit();
    transcoder_free();

    if (tp != nullptr)
    {
        tp->tear_down();
        save_delete(&tp);
    }

    script_file.clear();

    Logging::info(nullptr, "%1 V%2 terminated.", PACKAGE_NAME, FFMPEFS_VERSION);
}

/**
 * @brief Calculate the video frame count.
 * @param[in] origpath - Path of original file.
 * @param[inout] virtualfile - Virtual file object to modify.
 * @return On success, returns 0. On error, returns -errno.
 */
static int get_source_properties(const std::string & origpath, LPVIRTUALFILE virtualfile)
{
    Cache_Entry* cache_entry = transcoder_new(virtualfile, false);
    if (cache_entry == nullptr)
    {
        return -errno;
    }

    transcoder_delete(cache_entry);

    Logging::debug(origpath, "Duration: %1 Frames: %2 Segments: %3", virtualfile->m_duration, virtualfile->m_video_frame_count, virtualfile->get_segment_count());

    return 0;
}

/**
 * @brief Initialise a stat structure.
 * @param[in] stbuf - struct stat to fill in.
 * @param[in] fsize - size of the corresponding file.
 * @param[in] ftime - File time (creation/modified/access) of the corresponding file.
 * @param[in] directory - If true, the structure is set up for a directory.
 */
static void init_stat(struct stat * stbuf, size_t fsize, time_t ftime, bool directory)
{
    std::memset(stbuf, 0, sizeof(struct stat));

    stbuf->st_mode = DEFFILEMODE; //S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
    if (directory)
    {
        stbuf->st_mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
        stbuf->st_nlink = 2;
    }
    else
    {
        stbuf->st_mode |= S_IFREG;
        stbuf->st_nlink = 1;
    }

    stat_set_size(stbuf, fsize);

    // Set current user as owner
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();

    // Use current date/time
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = ftime;
}

/**
 * @brief Make a virtual file.
 * @param[in] buf - FUSE buffer to fill.
 * @param[in] filler - Filler function.
 * @param[in] type - Type of virtual file.
 * @param[in] origpath - Original path.
 * @param[in] filename - Name of virtual file.
 * @param[in] flags - On of the VIRTUALFLAG_ macros.
 * @param[in] fsize - Size of virtual file.
 * @param[in] ftime - Time of virtual file.
 * @return Returns constant pointer to VIRTUALFILE object of file.
 */
static LPVIRTUALFILE make_file(void *buf, fuse_fill_dir_t filler, VIRTUALTYPE type, const std::string & origpath, const std::string & filename, size_t fsize, time_t ftime, int flags)
{
    struct stat stbuf;

    init_stat(&stbuf, fsize, ftime, false);

    if (add_fuse_entry(buf, filler, filename, &stbuf, 0))
    {
        return nullptr;
    }

    return insert_file(type, origpath + filename, &stbuf, flags);
}

/**
 * @brief Read the virtual script file into memory and store in buffer.
 */
static void prepare_script()
{
    std::string scriptsource;

    exepath(&scriptsource);
    scriptsource += params.m_scriptsource;

    Logging::debug(scriptsource, "Reading virtual script source.");

    FILE *fpi = fopen(scriptsource.c_str(), "rt");
    if (fpi == nullptr)
    {
        Logging::warning(scriptsource, "File open failed. Disabling script: (%1) %2", errno, strerror(errno));
        params.m_enablescript = false;
    }
    else
    {
        struct stat stbuf;
        if (fstat(fileno(fpi), &stbuf) == -1)
        {
            Logging::warning(scriptsource, "File could not be accessed. Disabling script: (%1) %2", errno, strerror(errno));
            params.m_enablescript = false;
        }
        else
        {
            script_file.resize(static_cast<size_t>(stbuf.st_size));

            if (fread(&script_file[0], 1, static_cast<size_t>(stbuf.st_size), fpi) != static_cast<size_t>(stbuf.st_size))
            {
                Logging::warning(scriptsource, "File could not be read. Disabling script: (%1) %2", errno, strerror(errno));
                params.m_enablescript = false;
            }
            else
            {
                Logging::trace(scriptsource, "Read %1 bytes of script file.", script_file.size());
            }
        }

        fclose(fpi);
    }
}

/**
 * @brief Convert file name from source to destination name.
 * @param[in] virtualpath - Name of source file, will be changed to destination name.
 * @param[in] origpath - Original path to file. May be empty string if filepath is already a full path.
 * @param[out] current_format - If format has been found points to format info, nullptr if not.
 * @return Returns true if format has been found and filename changed, false if not.
 */
static bool virtual_name(std::string * virtualpath, const std::string & origpath /*= ""*/, const FFmpegfs_Format **current_format /*= nullptr*/)
{
    std::string ext;

    if (current_format != nullptr)
    {
        *current_format = nullptr;
    }

    if (!find_ext(&ext, *virtualpath) || is_passthrough(ext))
    {
        return false;
    }

    if (!is_selected(ext))
    {
        return false;
    }

    VIRTUALFILE newvirtualfile;

    newvirtualfile.m_origfile = origpath + *virtualpath;

    const FFmpegfs_Format *ffmpegfs_format = get_format(&newvirtualfile);

    if (ffmpegfs_format != nullptr)
    {
        if (params.m_oldnamescheme)
        {
            // Old filename scheme, creates duplicates
            replace_ext(virtualpath, ffmpegfs_format->fileext());
        }
        else
        {
            // New name scheme
            append_ext(virtualpath, ffmpegfs_format->fileext());
        }

        newvirtualfile.m_destfile = origpath + *virtualpath;
        newvirtualfile.m_virtfile = params.m_mountpath + *virtualpath;

        if (lstat(newvirtualfile.m_destfile.c_str(), &newvirtualfile.m_st) == 0)
        {
            if (params.m_recodesame == RECODESAME_NO)
            {
                newvirtualfile.m_flags |= VIRTUALFLAG_PASSTHROUGH;
            }

            if (ffmpegfs_format->is_multiformat())
            {
                // Change file to directory for the frame set
                // Change file to virtual directory for the frame set. Keep permissions.
                stat_to_dir(&newvirtualfile.m_st);
                flags_to_dir(&newvirtualfile.m_flags);
            }
        }

        insert(newvirtualfile);

        if (current_format != nullptr)
        {
            *current_format = ffmpegfs_format;
        }

        return true;
    }

    return false;
}

/**
 * @brief Find mapped file by prefix. Normally used to find a path.
 * @param[in] map - File map with virtual files.
 * @param[in] search_for - Prefix (path) to search for.
 * @return If found, returns const_iterator to map entry. Returns map.cend() if not found.
 */
static FILENAME_MAP::const_iterator find_prefix(const FILENAME_MAP & map, const std::string & search_for)
{
    FILENAME_MAP::const_iterator it = map.lower_bound(search_for);
    if (it != map.cend())
    {
        const std::string & key = it->first;
        if (key.compare(0, search_for.size(), search_for) == 0) // Really a prefix?
        {
            return it;
        }
    }
    return map.cend();
}

/**
 * @brief Insert virtualfile into list.
 * @param[in] virtualfile - VIRTUALFILE object to insert
 */
static void insert(const VIRTUALFILE & virtualfile)
{
    filenames.insert(make_pair(virtualfile.m_destfile, virtualfile));
    rfilenames.insert(make_pair(virtualfile.m_origfile, virtualfile));
}

LPVIRTUALFILE insert_file(VIRTUALTYPE type, const std::string & virtfile, const struct stat * stbuf, int flags)
{
    return insert_file(type, virtfile, virtfile, stbuf, flags);
}

LPVIRTUALFILE insert_file(VIRTUALTYPE type, const std::string & virtfile, const std::string & origfile, const struct stat * stbuf, int flags)
{
    std::string sanitised_virtfile(sanitise_filepath(virtfile));

    FILENAME_MAP::iterator it    = filenames.find(sanitised_virtfile);

    if (it == filenames.cend())
    {
        // Create new
        std::string sanitised_origfile(sanitise_filepath(origfile));
        VIRTUALFILE virtualfile;

        std::memcpy(&virtualfile.m_st, stbuf, sizeof(struct stat));

        virtualfile.m_type              = type;
        virtualfile.m_flags             = flags;
        virtualfile.m_format_idx        = guess_format_idx(sanitised_origfile); // Make a guess, will be finalised later
        virtualfile.m_destfile          = sanitised_virtfile;
        virtualfile.m_origfile          = sanitised_origfile;
        virtualfile.m_virtfile          = sanitised_origfile;
        //virtualfile.m_predicted_size    = static_cast<size_t>(stbuf->st_size);

        replace_start(&virtualfile.m_virtfile, params.m_basepath, params.m_mountpath);

        insert(virtualfile);

        it = filenames.find(sanitised_virtfile);
    }

    return &it->second;
}

/**
 * @brief Convert stbuf to directory
 * @param[inout] stbuf - Buffer to convert to directory
 */
static void stat_to_dir(struct stat *stbuf)
{
    stbuf->st_mode  &= ~static_cast<mode_t>(S_IFREG | S_IFLNK);
    stbuf->st_mode  |= S_IFDIR;
    if (stbuf->st_mode & S_IRWXU)
    {
        stbuf->st_mode  |= S_IXUSR;   // Add user execute bit if user has read or write access
    }
    if (stbuf->st_mode & S_IRWXG)
    {
        stbuf->st_mode  |= S_IXGRP;   // Add group execute bit if group has read or write access
    }
    if (stbuf->st_mode & S_IRWXO)
    {
        stbuf->st_mode  |= S_IXOTH;   // Add other execute bit if other has read or write access
    }
    stbuf->st_nlink = 2;
    stbuf->st_size  = stbuf->st_blksize;
}

/**
 * @brief Convert flags to directory
 * @param[inout] flags - Flags variable to change
 */
static void flags_to_dir(int *flags)
{
    *flags |= VIRTUALFLAG_FILESET | VIRTUALFLAG_DIRECTORY;

    if (ffmpeg_format[0].is_frameset())
    {
        *flags |= VIRTUALFLAG_FRAME;
    }
    else if (ffmpeg_format[0].is_hls())
    {
        *flags |= VIRTUALFLAG_HLS;
    }
}

LPVIRTUALFILE insert_dir(VIRTUALTYPE type, const std::string & virtdir, const struct stat * stbuf, int flags)
{
    struct stat stbufdir;

    std::memcpy(&stbufdir, stbuf, sizeof(stbufdir));

    // Change file to directory for the frame set
    // Change file to virtual directory for the frame set. Keep permissions.
    stat_to_dir(&stbufdir);
    flags_to_dir(&flags);

    std::string path(virtdir);
    append_sep(&path);

    return insert_file(type, path, &stbufdir, flags);
}

LPVIRTUALFILE find_file(const std::string & virtfile)
{
    FILENAME_MAP::iterator it = filenames.find(sanitise_filepath(virtfile));

    errno = 0;

    return (it != filenames.end() ? &it->second : nullptr);
}

LPVIRTUALFILE find_file_from_orig(const std::string &origfile)
{
    RFILENAME_MAP::iterator it = rfilenames.find(sanitise_filepath(origfile));

    errno = 0;

    return (it != rfilenames.end() ? &it->second : nullptr);
}

bool check_path(const std::string & path)
{
    FILENAME_MAP::const_iterator it = find_prefix(filenames, path);

    return (it != filenames.cend());
}

int load_path(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    if (buf == nullptr)
    {
        // We can't add anything here if buf == nullptr
        return 0;
    }

    int title_count = 0;

    for (FILENAME_MAP::iterator it = filenames.lower_bound(path); it != filenames.end(); ++it)
    {
        std::string virtfilepath    = it->first;
        LPVIRTUALFILE virtualfile   = &it->second;

        if (
        #ifdef USE_LIBVCD
                (virtualfile->m_type != VIRTUALTYPE_VCD) &&
        #endif // USE_LIBVCD
        #ifdef USE_LIBDVD
                (virtualfile->m_type != VIRTUALTYPE_DVD) &&
        #endif // USE_LIBDVD
        #ifdef USE_LIBBLURAY
                (virtualfile->m_type != VIRTUALTYPE_BLURAY) &&
        #endif // USE_LIBBLURAY
                !(virtualfile->m_flags & VIRTUALFLAG_CUESHEET)
                )
        {
            continue;
        }

        remove_filename(&virtfilepath);
        if (virtfilepath == path) // Really a prefix?
        {
            struct stat stbuf;
            std::string destfile(virtualfile->m_destfile);

            if (virtualfile->m_flags & VIRTUALFLAG_DIRECTORY)
            {
                // Is a directory, no need to translate the file name, just drop terminating separator
                remove_sep(&destfile);
            }
            remove_path(&destfile);

            title_count++;

            std::string cachefile;

            Buffer::make_cachefile_name(cachefile, virtualfile->m_destfile, params.current_format(virtualfile)->fileext(), false);

            struct stat stbuf2;
            if (!lstat(cachefile.c_str(), &stbuf2))
            {
                // Cache file exists, use cache file size here

                stat_set_size(&virtualfile->m_st, static_cast<size_t>(stbuf2.st_size));

                std::memcpy(&stbuf, &virtualfile->m_st, sizeof(struct stat));
            }
            else
            {
                if (statbuf == nullptr)
                {
                    std::memcpy(&stbuf, &virtualfile->m_st, sizeof(struct stat));
                }
                else
                {
                    std::memcpy(&stbuf, statbuf, sizeof(struct stat));

                    stat_set_size(&stbuf, static_cast<size_t>(virtualfile->m_st.st_size));
                }
            }

            if (add_fuse_entry(buf, filler, destfile, &stbuf, 0))
            {
                // break;
            }
        }
    }

    return title_count;
}

/**
 * @brief Filter function used for scandir.
 *
 * Selects files only that can be processed with FFmpeg API.
 *
 * @param[in] de - dirent to check
 * @return Returns nonzero if dirent matches, 0 if not.
 */
static int selector(const struct dirent * de)
{
    if (de->d_type & (DT_REG | DT_LNK))
    {
        return (av_guess_format(nullptr, de->d_name, nullptr) != nullptr);
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Scans the directory dirp
 * Works exactly like the scandir(3) function, the only
 * difference is that it returns the result in a std:vector.
 * @param[in] dirp - Directory to be searched.
 * @param[out] _namelist - Returns the list of files found
 * @param[in] selector - Entries for which selector() returns nonzero are stored in _namelist
 * @param[in] cmp - Entries are sorted using qsort(3) with the comparison function cmp(). May be nullptr for no sort.
 * @return Returns the number of directory entries selected.
 * On error, -1 is returned, with errno set to indicate
 * the error.
 */
static int scandir(const char *dirp, std::vector<struct dirent> * _namelist, int (*selector) (const struct dirent *), int (*cmp) (const struct dirent **, const struct dirent **))
{
    struct dirent **namelist;
    int count = scandir(dirp, &namelist, selector, cmp);

    _namelist->clear();

    if (count != -1)
    {
        for (int n = 0; n < count; n++)
        {
            _namelist->push_back(*namelist[n]);
            free(namelist[n]);
        }
        free(namelist);
    }

    return  count;
}

LPVIRTUALFILE find_original(const std::string & origpath)
{
    std::string buffer(origpath);
    return find_original(&buffer);
}

LPVIRTUALFILE find_original(std::string * filepath)
{
    sanitise_filepath(filepath);

    LPVIRTUALFILE virtualfile = find_file(*filepath);

    errno = 0;

    if (virtualfile != nullptr)
    {
        *filepath = virtualfile->m_origfile;
        return virtualfile;
    }
    else
    {
        // Fallback to old method (required if file accessed directly)
        std::string ext;
        if (!ffmpeg_format[0].is_hls() && find_ext(&ext, *filepath) && (strcasecmp(ext, ffmpeg_format[0].fileext()) == 0 || (params.smart_transcode() && strcasecmp(ext, ffmpeg_format[1].fileext()) == 0)))
        {
            std::string dir(*filepath);
            std::string searchexp(*filepath);
            std::string origfile;
            std::vector<struct dirent> namelist;
            struct stat stbuf;
            int count;
            int found = 0;

            remove_filename(&dir);
            origfile = dir;

            count = scandir(dir.c_str(), &namelist, selector, nullptr);
            if (count == -1)
            {
                if (errno != ENOTDIR)   // If not a directory, simply ignore error
                {
                    Logging::error(dir, "Error scanning directory: (%1) %2", errno, strerror(errno));
                }
                return nullptr;
            }

            remove_path(&searchexp);
            remove_ext(&searchexp);

            for (size_t n = 0; n < static_cast<size_t>(count); n++)
            {
                if (!strcmp(namelist[n].d_name, searchexp.c_str()))
                {
                    append_filename(&origfile, namelist[n].d_name);
                    sanitise_filepath(&origfile);
                    found = 1;
                    break;
                }
            }

            if (found && lstat(origfile.c_str(), &stbuf) == 0)
            {
                // The original file exists
                LPVIRTUALFILE virtualfile2;

                if (*filepath != origfile)
                {
                    virtualfile2 = insert_file(VIRTUALTYPE_DISK, *filepath, origfile, &stbuf); ///<* @todo This probably won't work, need to redo "Fallback to old method"
                    *filepath = origfile;
                }
                else
                {
                    virtualfile2 = insert_file(VIRTUALTYPE_DISK, origfile, &stbuf, VIRTUALFLAG_PASSTHROUGH);
                }
                return virtualfile2;
            }
            else
            {
                // File does not exist; this is a virtual file, not an error
                errno = 0;
            }
        }
    }
    // Source file exists with no supported extension, keep path
    return nullptr;
}

LPVIRTUALFILE find_parent(const std::string & origpath)
{
    std::string filepath(origpath);

    remove_filename(&filepath);
    remove_sep(&filepath);

    return find_original(&filepath);
}

/**
 * @brief Build a virtual HLS file set
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @param[in] origpath - The original file
 * @param[in] virtualfile - LPCVIRTUALFILE of file to create file set for
 * @return On success, returns 0. On error, returns -errno.
 */
static int make_hls_fileset(void * buf, fuse_fill_dir_t filler, const std::string & origpath, LPVIRTUALFILE virtualfile)
{
    // Generate set of TS segment files and necessary M3U lists

    if (!virtualfile->get_segment_count())
    {
        int res = get_source_properties(origpath, virtualfile);
        if (res < 0)
        {
            return res;
        }
    }

    if (virtualfile->get_segment_count())
    {
        std::string master_contents;
        std::string index_0_av_contents;

        // Examples...
        //"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1250,RESOLUTION=720x406,CODECS= \"avc1.77.30, mp4a.40.2 \",CLOSED-CAPTIONS=NONE\n"
        //"index_0_av.m3u8\n";
        //"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=2647000,RESOLUTION=1280x720,CODECS= \"avc1.77.30, mp4a.40.2 \",CLOSED-CAPTIONS=NONE\n"
        //"index_1_av.m3u8\n"
        //"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=922000,RESOLUTION=640x360,CODECS= \"avc1.77.30, mp4a.40.2 \",CLOSED-CAPTIONS=NONE\n"
        //"index_2_av.m3u8\n"
        //"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=448000,RESOLUTION=384x216,CODECS= \"avc1.66.30, mp4a.40.2 \",CLOSED-CAPTIONS=NONE\n"
        //"index_3_av.m3u8\n"
        //"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=61000,CODECS= \"mp4a.40.2 \",CLOSED-CAPTIONS=NONE\n"
        //"index_3_a.m3u8\n";

        master_contents = "#EXTM3U\n"
                          "#EXT-X-STREAM-INF:PROGRAM-ID=1\n"
                          "index_0_av.m3u8\n";

        strsprintf(&index_0_av_contents, "#EXTM3U\n"
                                         "#EXT-X-TARGETDURATION:%i\n"
                                         "#EXT-X-ALLOW-CACHE:YES\n"
                                         "#EXT-X-PLAYLIST-TYPE:VOD\n"
                                         "#EXT-X-VERSION:3\n"
                                         "#EXT-X-MEDIA-SEQUENCE:1\n", static_cast<int32_t>(params.m_segment_duration / AV_TIME_BASE));

        int64_t remaining_duration  = virtualfile->m_duration % params.m_segment_duration;
        size_t  segment_size        = virtualfile->m_predicted_size / virtualfile->get_segment_count();

        for (uint32_t file_no = 1; file_no <= virtualfile->get_segment_count(); file_no++)
        {
            std::string buffer;
            std::string segment_name = make_filename(file_no, params.current_format(virtualfile)->fileext());

            struct stat stbuf;
            std::string cachefile;
            std::string _origpath(origpath);
            remove_sep(&_origpath);

            std::string filename(_origpath);

            filename.append(".");
            filename.append(segment_name);

            Buffer::make_cachefile_name(cachefile, filename, params.current_format(virtualfile)->fileext(), false);

            if (!lstat(cachefile.c_str(), &stbuf))
            {
                make_file(buf, filler, virtualfile->m_type, origpath, segment_name, static_cast<size_t>(stbuf.st_size), virtualfile->m_st.st_ctime, VIRTUALFLAG_HLS);
            }
            else
            {
                make_file(buf, filler, virtualfile->m_type, origpath, segment_name, segment_size, virtualfile->m_st.st_ctime, VIRTUALFLAG_HLS);
            }

            if (file_no < virtualfile->get_segment_count())
            {
                strsprintf(&buffer, "#EXTINF:%.3f,\n", static_cast<double>(params.m_segment_duration) / AV_TIME_BASE);
            }
            else
            {
                strsprintf(&buffer, "#EXTINF:%.3f,\n", static_cast<double>(remaining_duration) / AV_TIME_BASE);
            }

            index_0_av_contents += buffer;
            index_0_av_contents += segment_name;
            index_0_av_contents += "\n";
        }

        index_0_av_contents += "#EXT-X-ENDLIST\n";

        LPVIRTUALFILE child_file;
        child_file = make_file(buf, filler, VIRTUALTYPE_SCRIPT, origpath, "master.m3u8", master_contents.size(), virtualfile->m_st.st_ctime);
        std::copy(master_contents.begin(), master_contents.end(), std::back_inserter(child_file->m_file_contents));

        child_file = make_file(buf, filler, VIRTUALTYPE_SCRIPT, origpath, "index_0_av.m3u8", index_0_av_contents.size(), virtualfile->m_st.st_ctime, VIRTUALFLAG_NONE);
        std::copy(index_0_av_contents.begin(), index_0_av_contents.end(), std::back_inserter(child_file->m_file_contents));

        {
            // Demo code adapted from: https://github.com/video-dev/hls.js/
            std::string hls_html;

            hls_html =
                    "<html>\n"
                    "\n"
                    "<head>\n"
                    "    <title>HLS Demo</title>\n"
                    "    <script src=\"https://cdn.jsdelivr.net/npm/hls.js@latest\"></script>\n"
                    "    <meta charset=\"utf-8\">\n"
                    "</head>\n"
                    "\n"
                    "<body>\n"
                    "    <center>\n"
                    "        <h1>Hls.js demo - basic usage</h1>\n"
                    "        <video height=\"600\" id=\"video\" controls></video>\n"
                    "    </center>\n"
                    "    <script>\n"
                    "      var video = document.getElementById(\"video\");\n"
                    "      var videoSrc = \"index_0_av.m3u8\";\n"
                    "      if (Hls.isSupported()) {\n"
                    "        var hls = new Hls();\n"
                    "        hls.loadSource(videoSrc);\n"
                    "        hls.attachMedia(video);\n"
                    "        hls.on(Hls.Events.MANIFEST_PARSED, function() {\n"
                    "          video.play();\n"
                    "        });\n"
                    "      }\n"
                    "      // hls.js is not supported on platforms that do not have Media Source Extensions (MSE) enabled.\n"
                    "      // When the browser has built-in HLS support (check using `canPlayType`), we can provide an HLS manifest (i.e. .m3u8 URL) directly to the video element through the `src` property.\n"
                    "      // This is using the built-in support of the plain video element, without using hls.js.\n"
                    "      // Note: it would be more normal to wait on the 'canplay' event below, but on Safari (where you are most likely to find built-in HLS support), the video.src URL must be on the user-driven\n"
                    "      // white-list before a 'canplay' event will be emitted; the last video event that can be reliably listened to when the URL is not on the white-list is 'loadedmetadata'.\n"
                    "      else if (video.canPlayType(\"application/vnd.apple.mpegurl\")) {\n"
                    "        video.src = videoSrc;\n"
                    "        video.addEventListener(\"loadedmetadata\", function() {\n"
                    "          video.play();\n"
                    "        });\n"
                    "    }\n"
                    "    </script>\n"
                    "</body>\n"
                    "\n"
                    "</html>\n";

            child_file = make_file(buf, filler, VIRTUALTYPE_SCRIPT, origpath, "hls.html", hls_html.size(), virtualfile->m_st.st_ctime, VIRTUALFLAG_NONE);
            std::copy(hls_html.begin(), hls_html.end(), std::back_inserter(child_file->m_file_contents));
        }
    }
    return 0;
}

/**
 * @brief Give next song in cuesheet list a kick start
 * Starts transcoding of the next song on the cuesheet list
 * to ensure a somewhat gapless start when the current song
 * finishes. Next song can be played from cache and start
 * faster then.
 * @todo Will work only if transcoding finishes within timeout.
 * Probably remove or raise timeout here.
 * @param[in] virtualfile - VIRTUALFILE object of current song
 * @return On success, returns 0. On error, returns -errno.
 */
static int kick_next(LPVIRTUALFILE virtualfile)
{
    if (virtualfile == nullptr)
    {
        // Not OK, should not happen
        return -EINVAL;
    }

    if (virtualfile->m_cuesheet_track.m_nextfile == nullptr)
    {
        // No next file
        return 0;
    }

    LPVIRTUALFILE nextvirtualfile = virtualfile->m_cuesheet_track.m_nextfile;

    Logging::debug(virtualfile->m_destfile, "Preparing next file: %1", nextvirtualfile->m_destfile.c_str());

    Cache_Entry* cache_entry = transcoder_new(nextvirtualfile, true); /** @todo Disable timeout */
    if (cache_entry == nullptr)
    {
        return -errno;
    }

    transcoder_delete(cache_entry);

    return 0;
}

/**
 * @brief Replacement SIGINT handler.
 *
 * FUSE handles SIGINT internally, but because there are extra threads running while transcoding this
 * mechanism does not properly work. We implement our own SIGINT handler to ensure proper shutdown of
 * all threads. Next we restore the original handler and dispatch the signal to it.
 *
 * @param[in] signum - Signal to handle. Must be SIGINT.
 */
static void sighandler(int signum)
{
    if (signum == SIGINT)
    {
        Logging::warning(nullptr, "Caught SIGINT, shutting down now...");
        // Make our threads terminate now
        transcoder_exit();
        // Restore fuse's handler
        sigaction(SIGINT, &oldHandler, nullptr);
        // Dispatch to fuse's handler
        raise(SIGINT);
    }
}

/**
 * @brief Extract the number for a file name
 * @param[in] path - Path and filename of requested file
 * @param[out] value - Returns the number extracted
 * @return Returns the filename that was processed, without path.
 */
static std::string get_number(const char *path, uint32_t *value)
{
    std::string filename(path);

    // Get frame number
    remove_path(&filename);

    *value = static_cast<uint32_t>(std::stoi(filename)); // Extract frame or segment number. May be more fancy in the future. Currently just get number from filename part.

    return filename;
}

/**
 * @brief Try to guess the format index (audio or video) for a file.
 * @param[in] filepath - Name of the file, path my be included, but not required.
 * @return Index 0 or 1
 */
static size_t guess_format_idx(const std::string & filepath)
{
    const AVOutputFormat* oformat = ::av_guess_format(nullptr, filepath.c_str(), nullptr);

    if (oformat != nullptr)
    {
        if (!params.smart_transcode())
        {
            // Not smart encoding: use first format (video file)
            return 0;
        }
        else
        {
            // Smart transcoding
            if (ffmpeg_format[0].video_codec() != AV_CODEC_ID_NONE && oformat->video_codec != AV_CODEC_ID_NONE && !is_album_art(oformat->video_codec))
            {
                // Is a video: use first format (video file)
                return 0;
            }
            else if (ffmpeg_format[1].audio_codec() != AV_CODEC_ID_NONE && oformat->audio_codec != AV_CODEC_ID_NONE)
            {
                // For audio only, use second format (audio only file)
                return 1;
            }
        }
    }

    return 0;
}

/**
 * @brief Open file with FFmpeg API and parse for streams and cue sheet.
 * @param[inout] newvirtualfile - VIRTUALFILE object of file to parse
 * @return On success, returns 0. On error, returns a negative AVERROR value.
 */
static int parse_file(LPVIRTUALFILE newvirtualfile)
{
    AVFormatContext *format_ctx = nullptr;
    int res;

    try
    {
        Logging::debug(newvirtualfile->m_origfile, "Creating a new format context and parsing the file.");

#ifndef __CYGWIN__
        res = avformat_open_input(&format_ctx, newvirtualfile->m_origfile.c_str(), nullptr, nullptr);
#else
        res = avformat_open_input(&format_ctx, posix2win(newvirtualfile->m_origfile.c_str()).c_str(), nullptr, nullptr);
#endif
        if (res)
        {
            Logging::trace(newvirtualfile->m_origfile, "No parseable file: %1", ffmpeg_geterror(res).c_str());
            throw res;
        }

        res = avformat_find_stream_info(format_ctx, nullptr);
        if (res < 0)
        {
            Logging::error(newvirtualfile->m_origfile, "Cannot find stream information: %1", ffmpeg_geterror(res).c_str());
            throw res;
        }

        // Check for an embedded cue sheet
        AVDictionaryEntry *tag = av_dict_get(format_ctx->metadata, "CUESHEET", nullptr, AV_DICT_IGNORE_SUFFIX);
        if (tag != nullptr)
        {
            // Found cue sheet
            Logging::trace(newvirtualfile->m_origfile, "Found an embedded cue sheet.");

            newvirtualfile->m_cuesheet = tag->value;
            newvirtualfile->m_cuesheet += "\r\n";                   // cue_parse_string() reports syntax error if string does not end with newline
            replace_all(&newvirtualfile->m_cuesheet, "\r\n", "\n"); // Convert all to unix
        }

        // Check for audio/video/subtitles
        int ret = get_audio_props(format_ctx, &newvirtualfile->m_channels, &newvirtualfile->m_sample_rate);
        if (ret < 0)
        {
            if (ret != AVERROR_STREAM_NOT_FOUND)    // Not an error, no audio is OK
            {
                Logging::error(newvirtualfile->m_origfile, "Could not find audio stream in input file (error '%1').", ffmpeg_geterror(ret).c_str());
            }
        }
        else
        {
            newvirtualfile->m_has_audio = true;
        }

        newvirtualfile->m_duration = format_ctx->duration;

        struct stat stbuf;
        if (lstat(newvirtualfile->m_origfile.c_str(), &stbuf) == 0)
        {
            std::memcpy(&newvirtualfile->m_st, &stbuf, sizeof(stbuf));
        }

        for (unsigned int stream_idx = 0; stream_idx < format_ctx->nb_streams; stream_idx++)
        {
            switch (format_ctx->streams[stream_idx]->codecpar->codec_type)
            {
            case AVMEDIA_TYPE_VIDEO:
            {
                if (!is_album_art(format_ctx->streams[stream_idx]->codecpar->codec_id))
                {
                    newvirtualfile->m_has_video = true;
                }
                break;
            }
            case AVMEDIA_TYPE_SUBTITLE:
            {
                newvirtualfile->m_has_subtitle = true;
                break;
            }
            default:
            {
                break;
            }
            }
        }
    }
    catch (int _res)
    {
        res = _res;
    }

    if (format_ctx != nullptr)
    {
        avformat_close_input(&format_ctx);
    }

    return res;
}

/**
 * @brief Get FFmpegfs_Format for the file.
 * @param[inout] newvirtualfile - VIRTUALFILE object of file to parse
 * @return On success, returns true. On error, returns false.
 */
static const FFmpegfs_Format * get_format(LPVIRTUALFILE newvirtualfile)
{
    LPVIRTUALFILE virtualfile = find_file_from_orig(newvirtualfile->m_origfile);

    if (virtualfile != nullptr)
    {
        // We already know the file!
        return params.current_format(virtualfile);
    }

    if (parse_file(newvirtualfile) < 0)
    {
        return nullptr;
    }

    Logging::trace(newvirtualfile->m_origfile, "Audio: %1 Video: %2 Subtitles: %3", newvirtualfile->m_has_audio, newvirtualfile->m_has_video, newvirtualfile->m_has_subtitle);

    if (!params.smart_transcode())
    {
        // Not smart encoding: use first format (video file)
        newvirtualfile->m_format_idx = 0;
        return &ffmpeg_format[0];
    }
    else
    {
        if (newvirtualfile->m_has_video)
        {
            newvirtualfile->m_format_idx = 0;
            return &ffmpeg_format[0];
        }

        if (newvirtualfile->m_has_audio)
        {
            newvirtualfile->m_format_idx = 1;
            return &ffmpeg_format[1];
        }
    }

    return nullptr;
}

int add_fuse_entry(void *buf, fuse_fill_dir_t filler, const std::string & name, const struct stat *stbuf, off_t off)
{
    if (buf == nullptr || filler == nullptr)
    {
        return 0;
    }

    return filler(buf, name.c_str(), stbuf, off);
}

int add_dotdot(void *buf, fuse_fill_dir_t filler, const struct stat *stbuf, off_t off)
{
    struct stat *stbuf2 = nullptr;
    struct stat stbuf3;

    if (stbuf != nullptr)
    {
        stbuf2 = &stbuf3;
        init_stat(stbuf2, 0, stbuf->st_ctime, true);
    }

    add_fuse_entry(buf, filler, ".", stbuf2, off);
    add_fuse_entry(buf, filler, "..", stbuf2, off);

    return 0;
}
