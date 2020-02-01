/*
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2012 K. Henriksson
 * Copyright (C) 2017-2020 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file
 * @brief Fuse operations implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2020 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "transcode.h"
#include "ffmpeg_utils.h"
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
#include "thread_pool.h"
#include "buffer.h"
#include "cache_entry.h"

#include <dirent.h>
#include <unistd.h>
#include <map>
#include <regex>
#include <list>
#include <assert.h>
#include <signal.h>

/**
 * @brief Map source file names to virtual file objects.
 */
typedef std::map<std::string, VIRTUALFILE> filenamemap;

static void             init_stat(struct stat *stbuf, size_t fsize, time_t ftime, bool directory);
static LPVIRTUALFILE    make_file(void *buf, fuse_fill_dir_t filler, VIRTUALTYPE type, const std::string & origpath, const std::string & filename, size_t fsize, time_t ftime = time(nullptr), int flags = VIRTUALFLAG_NONE);
static void             prepare_script();
static void             translate_path(std::string *origpath, const char* path);
static bool             transcoded_name(std::string *filepath, FFmpegfs_Format **current_format = nullptr);
static filenamemap::const_iterator find_prefix(const filenamemap & map, const std::string & search_for);
static int              get_source_properties(const std::string & origpath, LPVIRTUALFILE virtualfile);

static int              ffmpegfs_readlink(const char *path, char *buf, size_t size);
static int              ffmpegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int              ffmpegfs_getattr(const char *path, struct stat *stbuf);
static int              ffmpegfs_fgetattr(const char *path, struct stat * stbuf, struct fuse_file_info *fi);
static int              ffmpegfs_open(const char *path, struct fuse_file_info *fi);
static int              ffmpegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int              ffmpegfs_statfs(const char *path, struct statvfs *stbuf);
static int              ffmpegfs_release(const char *path, struct fuse_file_info *fi);
static void             sighandler(int signum);
static void *           ffmpegfs_init(struct fuse_conn_info *conn);
static void             ffmpegfs_destroy(__attribute__((unused)) void * p);
static std::string      get_number(const char *path, uint32_t *value);

static filenamemap          filenames;          /**< @brief Map files to virtual files */
static std::vector<char>    script_file;        /**< @brief Buffer for the virtual script if enabled */

static struct sigaction     oldHandler;         /**< @brief Saves old SIGINT handler to restore on shutdown */

fuse_operations             ffmpegfs_ops;       /**< @brief FUSE file system operations */

thread_pool*                tp;                 /**< @brief Thread pool object */

/**
  *
  * List of passthrough file extension
  *
  * Note: Must be in ascending aplhabetical order for the
  * binary_search function used on it to work! New elements
  * must be inserted in the correct position, elements
  * removed by simply deleting them from the list.
  */
static const std::list<std::string> passthrough_map =
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
    "MIFF",
    "MNG",		// Multiple Network Graphics
    "MRC",		// MRC format
    "MrSID ",	// LizardTech's SID Wavelet format
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
    "QTIF ",    // Macintosh PICT format
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
    "WBMP ",    // WAP Bitmap format
    "WDP",		// JPEG-XR/Microsoft HD Photo format
    "WebP ",    // Weppy file format
    "WMF",		// Windows Metafile Format
    "WSQ",		// Wavelet Scaler Quantization format
    "X",
    "X3F",		// Digital camera RAW formats (Adobe, Epson, Nikon, Minolta, Olympus, Fuji, Kodak, Sony, Pentax, Sigma)
    "XBM",		// X11 Bitmap
    "XCF",		// GIMP file format
    "XPM",
    "XWD",
    "YUV "		// Raw (binary) data
};

void init_fuse_ops(void)
{
    memset(&ffmpegfs_ops, 0, sizeof(fuse_operations));
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
 * @brief Initialise a stat structure.
 * @param[in] stbuf - struct stat to fill in.
 * @param[in] fsize - size of the corresponding file.
 * @param[in] ftime - File time (creation/modified/access) of the corresponding file.
 * @param[in] directory - If true, the structure is set up for a directory.
 */
static void init_stat(struct stat * stbuf, size_t fsize, time_t ftime, bool directory)
{
    memset(stbuf, 0, sizeof(struct stat));

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

#if defined __x86_64__ || !defined __USE_FILE_OFFSET64
    stbuf->st_size = static_cast<__off_t>(fsize);
#else
    stbuf->st_size = static_cast<__off64_t>(fsize);
#endif
    stbuf->st_blocks = (stbuf->st_size + 512 - 1) / 512;

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

    if (buf != nullptr && filler != nullptr)
    {
        filler(buf, filename.c_str(), &stbuf, 0);
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
 * @brief Translate file names from FUSE to the original absolute path.
 * @param[out] origpath - Upon return, contains the name and path of the original file.
 * @param[in] path - Filename and relative path of the original file.
 */
static void translate_path(std::string *origpath, const char* path)
{
    *origpath = params.m_basepath;
    if (*path == '/')
    {
        ++path;
    }
    *origpath += path;
    sanitise_filepath(origpath);
}

/**
 * @brief Convert file name from source to destination name.
 * @param[in] filepath - Name of source file, will be changed to destination name.
 * @param[out] current_format - If format has been found points to format info, nullptr if not.
 * @return Returns true if format has been found and filename changed, false if not.
 */
static bool transcoded_name(std::string * filepath, FFmpegfs_Format **current_format /*= nullptr*/)
{
    AVOutputFormat* format = av_guess_format(nullptr, filepath->c_str(), nullptr);

    if (format != nullptr)
    {
        std::string ext;

        if (!find_ext(&ext, *filepath) || !std::binary_search(passthrough_map.begin(), passthrough_map.end(), ext, nocasecompare))
        {
            FFmpegfs_Format *ffmpegfs_format = params.current_format(*filepath);

            if ((ffmpegfs_format->audio_codec_id() != AV_CODEC_ID_NONE && format->audio_codec != AV_CODEC_ID_NONE) ||
                    (ffmpegfs_format->video_codec_id() != AV_CODEC_ID_NONE && format->video_codec != AV_CODEC_ID_NONE))
            {
                *current_format = params.current_format(*filepath);
                replace_ext(filepath, (*current_format)->fileext());
                return true;
            }
        }
    }

    if (current_format != nullptr)
    {
        *current_format = nullptr;
    }
    return false;
}

/**
 * @brief Find mapped file by prefix. Normally used to find a path.
 * @param[in] map - File map with virtual files.
 * @param[in] search_for - Prefix (path) to search for.
 * @return If found, returns const_iterator to map entry. Returns map.end() if not found.
 */
static filenamemap::const_iterator find_prefix(const filenamemap & map, const std::string & search_for)
{
    filenamemap::const_iterator it = map.lower_bound(search_for);
    if (it != map.end())
    {
        const std::string & key = it->first;
        if (key.compare(0, search_for.size(), search_for) == 0) // Really a prefix?
        {
            return it;
        }
    }
    return map.end();
}

LPVIRTUALFILE insert_file(VIRTUALTYPE type, const std::string & virtfilepath, const struct stat * stbuf, int flags)
{
    return insert_file(type, virtfilepath, virtfilepath, stbuf, flags);
}

LPVIRTUALFILE insert_file(VIRTUALTYPE type, const std::string & virtfilepath, const std::string & origfile, const struct stat * stbuf, int flags)
{
    std::string sanitised_filepath = sanitise_filepath(virtfilepath);

    filenamemap::iterator it    = filenames.find(sanitised_filepath);

    if (it != filenames.end())
    {
        VIRTUALFILE & virtualfile = it->second;

        memcpy(&virtualfile.m_st, stbuf, sizeof(struct stat));

        virtualfile.m_type          = type;
        virtualfile.m_flags         = flags;
        virtualfile.m_format_idx    = params.guess_format_idx(origfile);
        virtualfile.m_origfile      = sanitise_filepath(origfile);
    }
    else
    {
        VIRTUALFILE virtualfile;

        memcpy(&virtualfile.m_st, stbuf, sizeof(struct stat));

        virtualfile.m_type          = type;
        virtualfile.m_flags         = flags;
        virtualfile.m_format_idx    = params.guess_format_idx(origfile);
        virtualfile.m_origfile      = sanitise_filepath(origfile);

        filenames.insert(make_pair(sanitised_filepath, virtualfile));
        it    = filenames.find(sanitised_filepath);
    }

    return &it->second;
}

LPVIRTUALFILE find_file(const std::string & virtfilepath)
{
    filenamemap::iterator it = filenames.find(sanitise_filepath(virtfilepath));

    errno = 0;

    if (it != filenames.end())
    {
        return &it->second;
    }
    return nullptr;
}

bool check_path(const std::string & path)
{
    filenamemap::const_iterator it = find_prefix(filenames, path);

    return (it != filenames.end());
}

int load_path(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    int title_count = 0;

    filenamemap::const_iterator it = filenames.lower_bound(path);
    while (it != filenames.end())
    {
        const std::string & key = it->first;
        if (key.compare(0, path.size(), path) == 0) // Really a prefix?
        {
            LPCVIRTUALFILE virtualfile = &it->second;
            struct stat stbuf;
            std::string destfile;

            get_destname(&destfile, virtualfile->m_origfile);
            remove_path(&destfile);

            title_count++;

            memcpy(&stbuf, statbuf, sizeof(struct stat));

            stbuf.st_size   = virtualfile->m_st.st_size;
            stbuf.st_blocks = (stbuf.st_size + 512 - 1) / 512;

            if (buf != nullptr && filler(buf, destfile.c_str(), &stbuf, 0))
            {
                // break;
            }
        }
        it++;
    }

    return title_count;
}

/**
 * @brief Filter function used for scandir.
 *
 * Selects files that can be processed with FFmpeg API.
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
        if (!params.m_format[0].is_hls() && find_ext(&ext, *filepath) && (strcasecmp(ext, params.m_format[0].fileext()) == 0 || (params.smart_transcode() && strcasecmp(ext, params.m_format[1].fileext()) == 0)))
        {
            std::string dir(*filepath);
            std::string filename(*filepath);
            std::string tmppath;
            struct dirent **namelist;
            struct stat stbuf;
            int count;
            int found = 0;

            remove_filename(&dir);
            tmppath = dir;

            count = scandir(dir.c_str(), &namelist, selector, nullptr);
            if (count == -1)
            {
                if (errno != ENOTDIR)   // If not a directory, simply ignore error
                {
                    Logging::error(dir, "Error scanning directory: (%1) %2", errno, strerror(errno));
                }
                return nullptr;
            }

            remove_path(&filename);
            std::regex specialChars { R"([-[\]{}()*+?.,\^$|#\s])" };
            filename = std::regex_replace(filename, specialChars, R"(\$&)");
            replace_ext(&filename, "*");

            for (int n = 0; n < count; n++)
            {
                if (!found && !compare(namelist[n]->d_name, filename))
                {
                    append_filename(&tmppath, namelist[n]->d_name);
                    found = 1;
                }
                free(namelist[n]);
            }
            free(namelist);

            sanitise_filepath(&tmppath);

            if (found && lstat(tmppath.c_str(), &stbuf) == 0)
            {
                // File exists with this extension
                LPVIRTUALFILE virtualfile;

                if (*filepath != tmppath)
                {
                    virtualfile = insert_file(VIRTUALTYPE_DISK, *filepath, tmppath, &stbuf);
                    *filepath = tmppath;
                }
                else
                {
                    virtualfile = insert_file(VIRTUALTYPE_DISK, tmppath, &stbuf, VIRTUALFLAG_PASSTHROUGH); /**< @todo Feature #2447 / Issue #25: add command line option */
                }
                return virtualfile;
            }
            else
            {
                // File does not exist; not an error
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

    translate_path(&origpath, path);
    find_original(&origpath);

    len = readlink(origpath.c_str(), buf, size - 2);
    if (len != -1)
    {
        buf[len] = '\0';

        transcoded = buf;
        transcoded_name(&transcoded);

        buf[0] = '\0';
        strncat(buf, transcoded.c_str(), size);

        errno = 0;  // Just to make sure - reset any error
    }

    return -errno;
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

    Logging::debug(origpath, "Duration: %1 Frames: %2 Segments: %3", virtualfile->m_duration, virtualfile->m_video_frame_count, virtualfile->m_segment_count);

    return 0;
}

int make_hls(void * buf, fuse_fill_dir_t filler, const std::string & origpath, LPVIRTUALFILE virtualfile)
{
    // Generate set of TS segment files and necessary M3U lists
    LPVIRTUALFILE child_file;
    std::string master_contents;
    std::string index_0_av_contents;

    if (!virtualfile->m_segment_count)
    {
        int res = get_source_properties(origpath, virtualfile);
        if (res < 0)
        {
            return res;
        }
    }

    if (virtualfile->m_segment_count)
    {
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

        master_contents =
                "#EXTM3U\n"
                "#EXT-X-STREAM-INF:PROGRAM-ID=1\n"
                "index_0_av.m3u8\n";

        index_0_av_contents =
                "#EXTM3U\n"
                "#EXT-X-TARGETDURATION:10\n"
                "#EXT-X-ALLOW-CACHE:YES\n"
                "#EXT-X-PLAYLIST-TYPE:VOD\n"
                "#EXT-X-VERSION:3\n"
                "#EXT-X-MEDIA-SEQUENCE:1\n";

        int64_t remaining_duration  = virtualfile->m_duration % params.m_segment_duration;
        size_t  segment_size        = virtualfile->m_predicted_size / virtualfile->m_segment_count; // @todo: Feature #2506 - calculate correct file size
        size_t  remaining_size      = virtualfile->m_predicted_size % virtualfile->m_segment_count; // @todo: Feature #2506 - calculate correct file size

        for (uint32_t file_no = 1; file_no <= virtualfile->m_segment_count; file_no++)
        {
            std::string buffer;
            std::string segment_name = make_filename(file_no, params.current_format(virtualfile)->fileext());

            //**< @todo; Rework this test code
            struct stat stbuf;
            std::string cachefile;
            std::string _origpath(origpath);
            remove_sep(&_origpath);
            Buffer::make_cachefile_name(cachefile, _origpath + "." + segment_name, params.current_format(virtualfile)->fileext(), false);

            if (!lstat(cachefile.c_str(), &stbuf))
            {
                make_file(buf, filler, virtualfile->m_type, origpath, segment_name, static_cast<size_t>(stbuf.st_size), virtualfile->m_st.st_ctime, VIRTUALFLAG_HLS);
                if (file_no < virtualfile->m_segment_count)
                {
                    buffer = string_format("#EXTINF:%.3f,\n", static_cast<double>(params.m_segment_duration) / AV_TIME_BASE);
                }
                else
                {
                    buffer = string_format("#EXTINF:%.3f,\n", static_cast<double>(remaining_duration) / AV_TIME_BASE);
                }
            }
            else
            {
                if (file_no < virtualfile->m_segment_count)
                {
                    make_file(buf, filler, virtualfile->m_type, origpath, segment_name, segment_size, virtualfile->m_st.st_ctime, VIRTUALFLAG_HLS);
                    buffer = string_format("#EXTINF:%.3f,\n", static_cast<double>(params.m_segment_duration) / AV_TIME_BASE);
                }
                else
                {
                    make_file(buf, filler, virtualfile->m_type, origpath, segment_name, remaining_size, virtualfile->m_st.st_ctime, VIRTUALFLAG_HLS);
                    buffer = string_format("#EXTINF:%.3f,\n", static_cast<double>(remaining_duration) / AV_TIME_BASE);
                }
            }

            index_0_av_contents += buffer;
#if 0			
			std::string buffer2(origpath);
			
			//remove_filename(&buffer2);
			buffer2 = replace_all(buffer2, "/mnt/sdc1/", "https://mp3mania.de/files/hls/1M/");
            index_0_av_contents += buffer2 + "/" + segment_name;
#else		
            index_0_av_contents += segment_name;
#endif
            index_0_av_contents += "\n";
        }

        index_0_av_contents += "#EXT-X-ENDLIST\n";

        child_file = make_file(buf, filler, VIRTUALTYPE_SCRIPT, origpath, "master.m3u8", master_contents.size(), virtualfile->m_st.st_ctime);
        std::copy(master_contents.begin(), master_contents.end(), std::back_inserter(child_file->m_file_contents));

        child_file = make_file(buf, filler, VIRTUALTYPE_SCRIPT, origpath, "index_0_av.m3u8", index_0_av_contents.size(), virtualfile->m_st.st_ctime, VIRTUALFLAG_NONE);
        std::copy(index_0_av_contents.begin(), index_0_av_contents.end(), std::back_inserter(child_file->m_file_contents));

        {
            //**< @todo; Rework this test code
            std::string hls_html;

            hls_html =
                    "<html>\n"
                    "\n"
                    "<head>\n"
                    "    <title>HLS Demo</title>\n"
                    "    <script src=\"https://cdn.jsdelivr.net/npm/hls.js@0.12.2/dist/hls.min.js\"></script>\n"
                    "    <meta charset=\"utf-8\">\n"
                    "</head>\n"
                    "\n"
                    "<body>\n"
                    "    <center>\n"
                    "        <h1>Hls.js demo - basic usage</h1>\n"
                    "        <video height=\"600\" id=\"video\" controls></video>\n"
                    "    </center>\n"
                    "    <script>\n"
                    "        if (Hls.isSupported()) {\n"
                    "            var video = document.getElementById(\"video\");\n"
                    "            var hls = new Hls();\n"
                    "            hls.loadSource(\"index_0_av.m3u8\");\n"
                    "            hls.attachMedia(video);\n"
                    "            hls.on(Hls.Events.MANIFEST_PARSED, function() {\n"
                    "                video.play();\n"
                    "            });\n"
                    "        }\n"
                    "        // hls.js is not supported on platforms that do not have Media Source Extensions (MSE) enabled.\n"
                    "        // When the browser has built-in HLS support (check using `canPlayType`), we can provide an HLS manifest (i.e. .m3u8 URL) directly to the video element throught the `src` property.\n"
                    "        // This is using the built-in support of the plain video element, without using hls.js.\n"
                    "        else if (video.canPlayType(\"application/vnd.apple.mpegurl\")) {\n"
                    "            video.src = \"index_0_av.m3u8\";\n"
                    "            video.addEventListener(\"canplay\", function() {\n"
                    "                video.play();\n"
                    "            });\n"
                    "        }\n"
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
 * @brief Read directory
 * @param[in] path - Physical path to load.
 * @param[in] buf - FUSE buffer to fill.
 * @param[in] filler - Filler function.
 * @return On success, returns 0. On error, returns -errno.
 */
static int ffmpegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t /*offset*/, struct fuse_file_info * /*fi*/)
{
    std::string origpath;
    struct dirent *de;
#if defined(USE_LIBBLURAY) || defined(USE_LIBDVD) || defined(USE_LIBVCD)
    int res;
#endif

    Logging::trace(path, "readdir");

    translate_path(&origpath, path);
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
            // Found Bluray or error reading Bluray
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
                while ((de = readdir(dp)) != nullptr)
                {
                    std::string origname(de->d_name);
                    std::string origfile;
                    std::string filename(de->d_name);
                    struct stat stbuf;

                    origfile = origpath + origname;

                    if (lstat(origfile.c_str(), &stbuf) == -1)
                    {
                        throw false;
                    }

                    if (S_ISREG(stbuf.st_mode) || S_ISLNK(stbuf.st_mode))
                    {
                        FFmpegfs_Format *current_format = nullptr;
                        std::string origext;
                        find_ext(&origext, filename);

                        if (transcoded_name(&filename, &current_format))
                        {
                            std::string newext;
                            find_ext(&newext, filename);

                            if (!current_format->is_multiformat())
                            {
                                if (origext != newext || params.m_recodesame == RECODESAME_YES)
                                {
                                    insert_file(VIRTUALTYPE_DISK, origpath + filename, origfile, &stbuf);
                                }
                                else
                                {
                                    insert_file(VIRTUALTYPE_DISK, origpath + filename, origfile, &stbuf, VIRTUALFLAG_PASSTHROUGH);
                                }
                            }
                            else
                            {
                                int flags = VIRTUALFLAG_FILESET | VIRTUALFLAG_DIRECTORY;

                                // Change file to directory for the frame set
                                stbuf.st_mode &=  ~static_cast<mode_t>(S_IFREG | S_IFLNK);
                                stbuf.st_mode |=  S_IFDIR;
                                stbuf.st_size = stbuf.st_blksize;

                                filename = origname;	// Restore original name

                                if (current_format->is_frameset())
                                {
                                    flags |= VIRTUALFLAG_FRAME;
                                }
                                else if (current_format->is_hls())
                                {
                                    flags |= VIRTUALFLAG_HLS;
                                }

                                insert_file(VIRTUALTYPE_DISK, origfile, &stbuf, flags);
                            }
                        }
                    }

                    if (filler(buf, filename.c_str(), &stbuf, 0))
                    {
                        break;
                    }
                }
            }
            catch (bool)
            {

            }

            closedir(dp);

            errno = 0;  // Just to make sure - reset any error
        }
    }
    else
    {
        FFmpegfs_Format *ffmpegfs_format = params.current_format(virtualfile);

        if (ffmpegfs_format->is_frameset())
        {
            // Generate set of all frames
            if (!virtualfile->m_video_frame_count)
            {
                int res = get_source_properties(origpath, virtualfile);
                if (res < 0)
                {
                    return res;
                }
            }

            //Logging::debug(origpath, "readdir: Creating frame set of %1 frames. %2", virtualfile->m_video_frame_count, virtualfile->m_origfile);

            for (uint32_t frame_no = 1; frame_no <= virtualfile->m_video_frame_count; frame_no++)
            {
                make_file(buf, filler, virtualfile->m_type, origpath, make_filename(frame_no, params.current_format(virtualfile)->fileext()), virtualfile->m_predicted_size, virtualfile->m_st.st_ctime, VIRTUALFLAG_FRAME); /**< @todo Calculate correct file size for frame image */
            }
        }
        else if (ffmpegfs_format->is_hls())
        {
            int res = make_hls(buf, filler, origpath, virtualfile);
            if (res < 0)
            {
                return res;
            }
        }

        errno = 0;  // Just to make sure - reset any error
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
    std::string origpath;
    int flags = 0;

    Logging::trace(path, "getattr");

    translate_path(&origpath, path);
    LPVIRTUALFILE virtualfile = find_original(&origpath);
    VIRTUALTYPE type = (virtualfile != nullptr) ? virtualfile->m_type : VIRTUALTYPE_DISK;

    flags = (virtualfile != nullptr) ? virtualfile->m_flags : VIRTUALFLAG_NONE;

    if (virtualfile == nullptr && lstat(origpath.c_str(), stbuf) == 0)
    {
        // File was not yet created as virtual file, but physically exists
        FFmpegfs_Format *current_format = nullptr;
        std::string filename(origpath);

        if (transcoded_name(&filename, &current_format))
        {
            if (!current_format->is_multiformat())
            {
                // Pass-through for regular files
                /**< @todo Feature #2447 / Issue #25:  Reencode to same file format as source file */
                Logging::trace(origpath, "getattr: Not transcoding existing file.");
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
            Logging::trace(origpath, "getattr: Treating existing file as passthrough.");
            errno = 0;
            return 0;
        }
    }
    else if (flags & VIRTUALFLAG_PASSTHROUGH && lstat(origpath.c_str(), stbuf) == 0)
    {
        // File pysically exists and is marked as pass-through
        Logging::trace(origpath, "getattr: File is marked as passthrough.");
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
        if (virtualfile != nullptr && (flags & (VIRTUALFLAG_FRAME | VIRTUALFLAG_DIRECTORY | VIRTUALFLAG_HLS)))
        {
            mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));

            errno = 0;  // Just to make sure - reset any error
            break;
        }

        if (virtualfile == nullptr || !(virtualfile->m_flags & VIRTUALFLAG_FILESET))
        {
            if (!no_check && lstat(origpath.c_str(), stbuf) == -1)
            {
                // If file does not exist here we can assume it's some sort of virtual file: Regular, DVD, S/VCD
                int error = -errno;
#if defined(USE_LIBBLURAY) || defined(USE_LIBDVD) || defined(USE_LIBVCD)
                int res = 0;

                std::string _origpath(origpath);

                remove_filename(&_origpath);

                virtualfile = find_original(&origpath);

                if (virtualfile == nullptr)
                {
#ifdef USE_LIBVCD
                    if (res <= 0)
                    {
                        // Returns -errno or number or titles on VCD
                        res = check_vcd(_origpath);
                    }
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
                    if (res <= 0)
                    {
                        // Returns -errno or number or titles on DVD
                        res = check_dvd(_origpath);
                    }
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
                    if (res <= 0)
                    {
                        // Returns -errno or number or titles on Bluray
                        res = check_bluray(_origpath);
                    }
#endif // USE_LIBBLURAY

                    if (params.m_format[0].is_hls())
                    {
                        LPVIRTUALFILE parent_file = find_parent(origpath);

                        if (parent_file != nullptr && (parent_file->m_flags & VIRTUALFLAG_DIRECTORY) && (parent_file->m_flags & VIRTUALFLAG_FILESET))
                        {
                            if (!parent_file->m_video_frame_count)  //***< @todo: was ist mit audio only? Werden die dann nicht immer wieder gecheckt?!?
                            {
                                int res = get_source_properties(origpath, parent_file);
                                if (res < 0)
                                {
                                    return res;
                                }
                            }


                            make_hls(nullptr, nullptr, parent_file->m_origfile + "/", parent_file); // TEST

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
                        // No Bluray/DVD/VCD found or error reading disk
                        return (!res ?  error : res);
                    }
                }

                virtualfile = find_original(&origpath);

                if (virtualfile == nullptr)
                {
                    // Not a DVD/VCD/Bluray file
                    return -ENOENT;
                }

                mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
#else
                return error;
#endif
            }

            if (flags & VIRTUALFLAG_FILESET)
            {
                // Change file to virtual directory for the frame set. Keep permissions.
                stbuf->st_mode  &= ~static_cast<mode_t>(S_IFREG | S_IFLNK);
                stbuf->st_mode  |= S_IFDIR;
                stbuf->st_nlink = 2;
                stbuf->st_size  = stbuf->st_blksize;

                append_sep(&origpath);

                int flags = VIRTUALFLAG_FILESET | VIRTUALFLAG_DIRECTORY;

                if (params.m_format[0].is_frameset())
                {
                    flags |= VIRTUALFLAG_FRAME;
                }
                else if (params.m_format[0].is_hls())
                {
                    flags |= VIRTUALFLAG_HLS;
                }

                insert_file(type, origpath, stbuf, flags);
            }
            else if (S_ISREG(stbuf->st_mode))
            {
                // Get size for resulting output file from regular file, otherwise it's a symbolic link or a virtual frame set.
                if (virtualfile == nullptr)
                {
                    // We should not never end here - report bad file number.
                    return -EBADF;
                }

                assert(virtualfile->m_origfile == origpath);

                if (!transcoder_cached_filesize(virtualfile, stbuf))
                {
                    Cache_Entry* cache_entry = transcoder_new(virtualfile, false);
                    if (cache_entry == nullptr)
                    {
                        return -errno;
                    }

#if defined __x86_64__ || !defined __USE_FILE_OFFSET64
                    stbuf->st_size = static_cast<__off_t>(transcoder_get_size(cache_entry));
#else
                    stbuf->st_size = static_cast<__off64_t>(transcoder_get_size(cache_entry));
#endif
                    stbuf->st_blocks = (stbuf->st_size + 512 - 1) / 512;

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
        assert(false);
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

    translate_path(&origpath, path);
    LPCVIRTUALFILE virtualfile = find_original(&origpath);

    if ((virtualfile == nullptr || virtualfile->m_flags & VIRTUALFLAG_PASSTHROUGH) && lstat(origpath.c_str(), stbuf) == 0)
    {
        // pass-through for regular files
        errno = 0;
        return 0;
    }
    else
    {
        // Not really an error.
        errno = 0;
    }

    // This is a virtual file
    assert(virtualfile != nullptr);

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
                assert(false);
#if defined __x86_64__ || !defined __USE_FILE_OFFSET64
                stbuf->st_size = static_cast<__off_t>(transcoder_buffer_watermark(cache_entry, segment_no));
#else
                stbuf->st_size = static_cast<__off64_t>(transcoder_buffer_watermark(cache_entry, segment_no));
#endif
                stbuf->st_blocks = (stbuf->st_size + 512 - 1) / 512;
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
        assert(false);
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
    Cache_Entry* cache_entry;

    Logging::trace(path, "open");

    translate_path(&origpath, path);
    LPVIRTUALFILE virtualfile = find_original(&origpath);

    if (virtualfile == nullptr || (virtualfile->m_flags & VIRTUALFLAG_PASSTHROUGH))
    {
        int fd = open(origpath.c_str(), fi->flags);
        if (fd == -1 && errno != ENOENT)
        {
            // File does exist, but can't be opened.
            return -errno;
        }
        else
        {
            // Not really an error.
            errno = 0;
        }

        if (fd != -1)
        {
            close(fd);
            // File is real and can be opened.
            errno = 0;
            return 0;
        }
    }

    // This is a virtual file

    assert(virtualfile != nullptr);

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
        assert(false);
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
 * @param[in] _offset
 * @param[in] fi
 * @return On success, returns 0. On error, returns -errno.
 */
static int ffmpegfs_read(const char *path, char *buf, size_t size, off_t _offset, struct fuse_file_info *fi)
{
    std::string origpath;
    size_t offset = static_cast<size_t>(_offset);  // Cast OK: offset can never be < 0.
    int bytes_read = 0;
    Cache_Entry* cache_entry;

    Logging::trace(path, "read: Reading %1 bytes from %2.", size, offset);

    translate_path(&origpath, path);
    LPVIRTUALFILE virtualfile = find_original(&origpath);

    if (virtualfile == nullptr || (virtualfile->m_flags & VIRTUALFLAG_PASSTHROUGH))
    {
        int fd = open(origpath.c_str(), O_RDONLY);
        if (fd != -1)
        {
            // If this is a real file, pass the call through.
            bytes_read = static_cast<int>(pread(fd, buf, size, _offset));
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

    assert(virtualfile != nullptr);

    switch (virtualfile->m_type)
    {
    case VIRTUALTYPE_SCRIPT:
    {
        if (offset >= virtualfile->m_file_contents.size())
        {
            bytes_read = 0;
            break;
        }

        size_t bytes = size;
        if (offset + bytes > virtualfile->m_file_contents.size())
        {
            bytes = virtualfile->m_file_contents.size() - offset;
        }

        if (bytes)
        {
            memcpy(buf, &virtualfile->m_file_contents[offset], bytes);
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
                Logging::error(origpath.c_str(), "read: Unable to deduct frame no. from file name (%1): (%2) %3", filename, errno, strerror(errno));
                return -errno;
            }

            success = transcoder_read_frame(cache_entry, buf, offset, size, frame_no, &bytes_read, virtualfile);
        }
        else if (!(virtualfile->m_flags & VIRTUALFLAG_FILESET))
        {
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
                    Logging::error(origpath.c_str(), "read: Unable to deduct segment no. from file name (%1): (%2) %3", filename, errno, strerror(errno));
                    return -errno;
                }
            }

            success = transcoder_read(cache_entry, buf, offset, size, &bytes_read, segment_no);
        }
        break;
    }
    case VIRTUALTYPE_PASSTHROUGH:
    case VIRTUALTYPE_BUFFER:
    {
        assert(false);
        break;
    }
    }

    if (success)
    {
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

    translate_path(&origpath, path);

    // pass-through for regular files
    if (statvfs(origpath.c_str(), stbuf) == 0)
    {
        return -errno;
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
                Logging::error(path, "read: Unable to deduct segment no. from file name (%1): (%2) %3", filename, errno, strerror(errno));
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
    assert(signum == SIGINT);

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
 * @brief Initialise filesystem
 * @param[in] conn - fuse_conn_info structure of FUSE. See FUSE docs for details.
 * @return nullptr
 */
static void *ffmpegfs_init(struct fuse_conn_info *conn)
{
    Logging::info(nullptr, "%1 V%2 initialising.", PACKAGE_NAME, PACKAGE_VERSION);
    Logging::info(nullptr, "Mapping '%1' to '%2'.", params.m_basepath.c_str(), params.m_mountpath.c_str());

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
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
    Logging::info(nullptr, "%1 V%2 terminating", PACKAGE_NAME, PACKAGE_VERSION);
    std::printf("%s V%s terminating\n", PACKAGE_NAME, PACKAGE_VERSION);

    stop_cache_maintenance();

    transcoder_exit();
    transcoder_free();

    if (tp != nullptr)
    {
        tp->tear_down();
        delete tp;
        tp = nullptr;
    }

    script_file.clear();

    Logging::info(nullptr, "%1 V%2 terminated", PACKAGE_NAME, PACKAGE_VERSION);
}

/**
 * @brief Extract the number for a file name
 * @param path - Path and filename of requested file
 * @param value - Returns the number extracted
 * @return Returns the filename that was processed, without path.
 */
static std::string get_number(const char *path, uint32_t *value)
{
    std::string filename(path);

    // Get frame number
    remove_path(&filename);

    *value = static_cast<uint32_t>(atoi(filename.c_str())); // Extract frame or segment number. May be more fancy in the future. Currently just get number from filename part.

    return filename;
}

