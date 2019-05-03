/*
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2012 K. Henriksson
 * Copyright (C) 2017-2019 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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

/**
 * @file
 * @brief Fuse operations implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2006-2008 David Collett @n
 * Copyright (C) 2008-2013 K. Henriksson @n
 * Copyright (C) 2017-2019 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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

#include <dirent.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <regex>
#include <assert.h>
#include <signal.h>

/**
 * @brief Map source file names to virtual file objects.
 */
typedef std::map<std::string, VIRTUALFILE> filenamemap;

static void init_stat(struct stat *st, size_t fsize, time_t ftime, bool directory);
static LPVIRTUALFILE make_file(void *buf, fuse_fill_dir_t filler, VIRTUALTYPE type, const std::string & origpath, const std::string & filename, size_t fsize, time_t ftime = time(nullptr));
static void prepare_script();
static void translate_path(std::string *origpath, const char* path);
static bool transcoded_name(std::string *filepath, FFmpegfs_Format **current_format = nullptr);
static filenamemap::const_iterator find_prefix(const filenamemap & map, const std::string & search_for);

static int ffmpegfs_readlink(const char *path, char *buf, size_t size);
static int ffmpegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int ffmpegfs_getattr(const char *path, struct stat *stbuf);
static int ffmpegfs_fgetattr(const char *path, struct stat * stbuf, struct fuse_file_info *fi);
static int ffmpegfs_open(const char *path, struct fuse_file_info *fi);
static int ffmpegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int ffmpegfs_statfs(const char *path, struct statvfs *stbuf);
static int ffmpegfs_release(const char *path, struct fuse_file_info *fi);
static void sighandler(int signum);
static void *ffmpegfs_init(struct fuse_conn_info *conn);
static void ffmpegfs_destroy(__attribute__((unused)) void * p);

static filenamemap filenames;                   /**< @brief Map files to virtual files */
static std::vector<char> index_buffer;          /**< @brief Buffer for the virtual script if enabled */

static struct sigaction oldHandler;             /**< @brief Saves old SIGINT handler to restore on shutdown */

fuse_operations ffmpegfs_ops;                   /**< @brief FUSE file system operations */

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
 * @param[in] st - struct stat to fill in.
 * @param[in] fsize - size of the corresponding file.
 * @param[in] ftime - File time (creation/modified/access) of the corresponding file.
 * @param[in] directory - If true, the structure is set up for a directory.
 */
static void init_stat(struct stat * st, size_t fsize, time_t ftime, bool directory)
{
    memset(st, 0, sizeof(struct stat));

    st->st_mode = DEFFILEMODE; //S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
    if (directory)
    {
        st->st_mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
        st->st_nlink = 2;
    }
    else
    {
        st->st_mode |= S_IFREG;
        st->st_nlink = 1;
    }

#if defined __x86_64__ || !defined __USE_FILE_OFFSET64
    st->st_size = static_cast<__off_t>(fsize);
#else
    st->st_size = static_cast<__off64_t>(size);
#endif
    st->st_blocks = (st->st_size + 512 - 1) / 512;

    // Set current user as owner
    st->st_uid = getuid();
    st->st_gid = getgid();

    // Use current date/time
    st->st_atime = st->st_mtime = st->st_ctime = ftime;
}

/**
 * @brief Make a virtual file.
 * @param[in] buf - FUSE buffer to fill.
 * @param[in] filler - Filler function.
 * @param[in] type - Type of virtual file.
 * @param[in] origpath - Original path.
 * @param[in] filename - Name of virtual file.
 * @param[in] fsize - Size of virtual file.
 * @param[in] ftime - Time of virtual file.
 * @return Returns constant pointer to VIRTUALFILE object of file.
 */
static LPVIRTUALFILE make_file(void *buf, fuse_fill_dir_t filler, VIRTUALTYPE type, const std::string & origpath, const std::string & filename, size_t fsize, time_t ftime)
{
    struct stat st;

    init_stat(&st, fsize, ftime, false);

    if (filler(buf, filename.c_str(), &st, 0))
    {
        // break;
    }

    return insert_file(type, origpath + filename, &st);
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
        struct stat st;
        if (fstat(fileno(fpi), &st) == -1)
        {
            Logging::warning(scriptsource, "File could not be accessed. Disabling script: (%1) %2", errno, strerror(errno));
            params.m_enablescript = false;
        }
        else
        {
            index_buffer.resize(static_cast<size_t>(st.st_size));

            if (fread(&index_buffer[0], 1, static_cast<size_t>(st.st_size), fpi) != static_cast<size_t>(st.st_size))
            {
                Logging::warning(scriptsource, "File could not be read. Disabling script: (%1) %2", errno, strerror(errno));
                params.m_enablescript = false;
            }
            else
            {
                Logging::trace(scriptsource, "Read %1 bytes of script file.", index_buffer.size());
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
        if ((params.current_format(*filepath)->audio_codec_id() != AV_CODEC_ID_NONE && format->audio_codec != AV_CODEC_ID_NONE) ||
                (params.current_format(*filepath)->video_codec_id() != AV_CODEC_ID_NONE && format->video_codec != AV_CODEC_ID_NONE))
        {
            *current_format = params.current_format(*filepath);
            replace_ext(filepath, (*current_format)->format_name());
            return true;
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

LPVIRTUALFILE insert_file(VIRTUALTYPE type, const std::string & virtfilepath, const struct stat * st, int flags)
{
    return insert_file(type, virtfilepath, virtfilepath, st, flags);
}

LPVIRTUALFILE insert_file(VIRTUALTYPE type, const std::string & virtfilepath, const std::string & origfile, const struct stat  * st, int flags)
{
    VIRTUALFILE virtualfile;
    std::string sanitised_filepath = sanitise_filepath(virtfilepath);

    virtualfile.m_type          = type;
    virtualfile.m_flags         = flags;
    virtualfile.m_format_idx    = params.guess_format_idx(origfile);
    virtualfile.m_origfile      = sanitise_filepath(origfile);

    memcpy(&virtualfile.m_st, st, sizeof(struct stat));

    filenames.insert(make_pair(sanitised_filepath, virtualfile));

    filenamemap::iterator it    = filenames.find(sanitised_filepath);
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
        if (find_ext(&ext, *filepath) && (strcasecmp(ext, params.m_format[0].format_name()) == 0 || (params.smart_transcode() && strcasecmp(ext, params.m_format[1].format_name()) == 0)))
        {
            std::string dir(*filepath);
            std::string filename(*filepath);
            std::string tmppath;
            struct dirent **namelist;
            struct stat st;
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

            if (found && lstat(tmppath.c_str(), &st) == 0)
            {
                // File exists with this extension
                LPVIRTUALFILE virtualfile;

                if (*filepath != tmppath)
                {
                    virtualfile = insert_file(VIRTUALTYPE_REGULAR, *filepath, tmppath, &st);
                    *filepath = tmppath;
                }
                else
                {
                    virtualfile = insert_file(VIRTUALTYPE_PASSTHROUGH, tmppath, &st); /**< @todo Feature #2447 / Issue #25: add command line option */
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
        make_file(buf, filler, VIRTUALTYPE_SCRIPT, origpath, params.m_scriptfile, index_buffer.size());
    }

    std::string buffer(origpath);
    LPVIRTUALFILE virtualfile = find_original(&buffer);

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

    if (virtualfile == nullptr || !(virtualfile->m_flags & VIRTUALFLAG_IMAGE_FRAME))
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
                    struct stat st;

                    origfile = origpath + origname;

                    if (lstat(origfile.c_str(), &st) == -1)
                    {
                        throw false;
                    }

                    if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
                    {
                        FFmpegfs_Format *current_format = nullptr;
                        std::string origext;
                        find_ext(&origext, filename);

                        if (transcoded_name(&filename, &current_format))
                        {
                            std::string newext;
                            find_ext(&newext, filename);

                            if (!current_format->export_frameset())
                            {
                                if (origext != newext)
                                {
                                    insert_file(VIRTUALTYPE_REGULAR, origpath + filename, origfile, &st);
                                }
                                else
                                {
                                    insert_file(VIRTUALTYPE_PASSTHROUGH, origpath + filename, origfile, &st); /**< @todo Feature #2447 / Issue #25: add command line option m_recodesame */
                                }
                            }
                            else
                            {
                                // Change file to directory for the frame set
                                st.st_mode &=  ~static_cast<mode_t>(S_IFREG | S_IFLNK);
                                st.st_mode |=  S_IFDIR;
                                st.st_size = st.st_blksize;

                                filename = origname;	// Restore original name

                                insert_file(VIRTUALTYPE_REGULAR, origfile, &st, VIRTUALFLAG_IMAGE_FRAME);
                            }
                        }
                    }

                    if (filler(buf, filename.c_str(), &st, 0))
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
        if (virtualfile->m_video_frame_count == AV_NOPTS_VALUE)
        {
            Cache_Entry* cache_entry = transcoder_new(virtualfile, false);
            if (cache_entry == nullptr)
            {
                return -errno;
            }

            transcoder_delete(cache_entry);

            Logging::debug(origpath, "Duration: %1 Frames: %2", virtualfile->m_duration, virtualfile->m_video_frame_count);

            //            if (!transcoder_cached_filesize(virtualfile, &virtualfile->m_st))
            //            {
            //                Cache_Entry* cache_entry = transcoder_new(virtualfile, false);
            //                if (cache_entry == nullptr)
            //                {
            //                    return -errno;
            //                }

            //#if defined __x86_64__ || !defined __USE_FILE_OFFSET64
            //                virtualfile->m_st.st_size = static_cast<__off_t>(transcoder_get_size(cache_entry));
            //#else
            //                virtualfile->m_st.st_size = static_cast<__off64_t>(transcoder_get_size(cache_entry));
            //#endif
            //                virtualfile->m_st.st_blocks = (virtualfile->m_st.st_size + 512 - 1) / 512;

            //                transcoder_delete(cache_entry);
            //            }
        }

        //Logging::debug(origpath, "readdir: Creating frame set of %1 frames. %2", virtualfile->m_video_frame_count, virtualfile->m_origfile);

        for (int64_t frame_no = 1; frame_no <= virtualfile->m_video_frame_count; frame_no++)
        {
            char filename[PATH_MAX + 1];
            sprintf(filename, "%011" PRId64 ".%s", frame_no, params.current_format(virtualfile)->desttype().c_str());
            make_file(buf, filler, VIRTUALTYPE_FRAME, origpath, filename, 40 * 1024, virtualfile->m_st.st_ctime); /**< @todo Dateigrösse */
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
    VIRTUALTYPE type = (virtualfile != nullptr) ? virtualfile->m_type : VIRTUALTYPE_REGULAR;

    if (virtualfile == nullptr && lstat(origpath.c_str(), stbuf) == 0)
    {
        // File was not yet created as virtual file, but physically exists
        FFmpegfs_Format *current_format = nullptr;
        std::string filename(origpath);

        if (transcoded_name(&filename, &current_format))
        {
            if (!current_format->export_frameset())
            {
                // Pass-through for regular files
                /**< @todo Feature #2447 / Issue #25:  Reencode to same file format as source file */
                Logging::debug(origpath, "getattr: Treating existing file as passthrough.");
                errno = 0;
                return 0;
            }
            else
            {
                Logging::debug(origpath, "getattr: Creating frame set directory of file.");

                flags |= VIRTUALFLAG_IMAGE_FRAME;
            }
        }
    }
    else if (type == VIRTUALTYPE_PASSTHROUGH && lstat(origpath.c_str(), stbuf) == 0)
    {
        // File pysically exists and is marked as pass-through

        Logging::debug(origpath, "getattr: File is marked as passthrough.");
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
    case VIRTUALTYPE_REGULAR:
    {
        if (virtualfile == nullptr || !(virtualfile->m_flags & VIRTUALFLAG_IMAGE_FRAME))
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

            if (flags & VIRTUALFLAG_IMAGE_FRAME)
            {
                // Change file to virtual directory for the frame set. Keep permissions.
                stbuf->st_mode  &= ~static_cast<mode_t>(S_IFREG | S_IFLNK);
                stbuf->st_mode  |= S_IFDIR;
                stbuf->st_nlink = 2;
                stbuf->st_size  = stbuf->st_blksize;

                append_sep(&origpath);

                insert_file(VIRTUALTYPE_DIRECTORY, origpath, stbuf, VIRTUALFLAG_IMAGE_FRAME);

                //                type = (virtualfile != nullptr) ? virtualfile->m_type : VIRTUALTYPE_REGULAR;
                //                insert_file(type, origpath, stbuf, VIRTUALFLAG_IMAGE_FRAME);
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
        else // if (virtualfile != nullptr && (virtualfile->m_flags & VIRTUALFLAG_IMAGE_FRAME))
        {
            // Frame set, simply report stat.
            mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
            errno = 0;
        }
        break;
    }
    case VIRTUALTYPE_FRAME:
    case VIRTUALTYPE_DIRECTORY:
    {
        mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
        errno = 0;  // Just to make sure - reset any error
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

    if ((virtualfile == nullptr || virtualfile->m_type == VIRTUALTYPE_PASSTHROUGH) && lstat(origpath.c_str(), stbuf) == 0)
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
    case VIRTUALTYPE_REGULAR:
    {
        if (!(virtualfile->m_flags & VIRTUALFLAG_IMAGE_FRAME))
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
                    Logging::error(path, "Tried to stat unopen file.");
                    errno = EBADF;
                    return -errno;
                }

#if defined __x86_64__ || !defined __USE_FILE_OFFSET64
                stbuf->st_size = static_cast<__off_t>(transcoder_buffer_watermark(cache_entry));
#else
                stbuf->st_size = static_cast<__off64_t>(transcoder_buffer_watermark(cache_entry));
#endif
                stbuf->st_blocks = (stbuf->st_size + 512 - 1) / 512;
            }

            errno = 0;  // Just to make sure - reset any error
        }
        else
        {
            mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
        }
        break;
    }
    case VIRTUALTYPE_SCRIPT:
    case VIRTUALTYPE_FRAME:
    case VIRTUALTYPE_DIRECTORY:
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

    if (virtualfile == nullptr || virtualfile->m_type == VIRTUALTYPE_PASSTHROUGH)
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
    case VIRTUALTYPE_REGULAR:
    {
        if (!(virtualfile->m_flags & VIRTUALFLAG_IMAGE_FRAME))
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
    case VIRTUALTYPE_FRAME:
    {
        std::string filepath(origpath);

        remove_filename(&filepath);
        remove_sep(&filepath);

        LPVIRTUALFILE virtualfile2 = find_original(&filepath);

        if (virtualfile2 != nullptr)
        {
            cache_entry = transcoder_new(virtualfile2, true);
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
    case VIRTUALTYPE_DIRECTORY:
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

    Logging::trace(path, "Reading %1 bytes from %2.", size, offset);

    translate_path(&origpath, path);

    LPVIRTUALFILE virtualfile = find_original(&origpath);

    if (virtualfile == nullptr || virtualfile->m_type == VIRTUALTYPE_PASSTHROUGH)
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
        size_t bytes = size;
        if (offset + bytes > index_buffer.size())
        {
            bytes = index_buffer.size() - offset;
        }

        if (bytes)
        {
            memcpy(buf, &index_buffer[offset], bytes);
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
    case VIRTUALTYPE_REGULAR:
    {
        if (!(virtualfile->m_flags & VIRTUALFLAG_IMAGE_FRAME))
        {
            cache_entry = reinterpret_cast<Cache_Entry*>(fi->fh);

            if (cache_entry == nullptr)
            {
                if (errno)
                {
                    Logging::error(origpath.c_str(), "Tried to read from unopen file: (%1) %2", errno, strerror(errno));
                }
                return -errno;
            }

            success = transcoder_read(cache_entry, buf, offset, size, &bytes_read);
        }
        break;
    }
    case VIRTUALTYPE_FRAME:
    {
        cache_entry = reinterpret_cast<Cache_Entry*>(fi->fh);

        if (cache_entry == nullptr)
        {
            if (errno)
            {
                Logging::error(origpath.c_str(), "Tried to read from unopen file: (%1) %2", errno, strerror(errno));
            }
            return -errno;
        }

        std::string filename(path);

        // Get frame number
        remove_path(&filename);

        uint32_t frame_no = static_cast<uint32_t>(atoi(filename.c_str()));

        success = transcoder_read_frame(cache_entry, buf, offset, size, frame_no, &bytes_read, virtualfile);
        break;
    }
    case VIRTUALTYPE_DIRECTORY:
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
    Cache_Entry*     cache_entry = reinterpret_cast<Cache_Entry*>(fi->fh);

    Logging::trace(path, "release");

    if (cache_entry != nullptr)
    {
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
 * @param[in] conn
 * @return
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

    return nullptr;
}

/**
 * @brief Clean up filesystem
 * @param[in] p
 */
static void ffmpegfs_destroy(__attribute__((unused)) void * p)
{
    Logging::info(nullptr, "%1 V%2 terminating", PACKAGE_NAME, PACKAGE_VERSION);
    std::printf("%s V%s terminating\n", PACKAGE_NAME, PACKAGE_VERSION);

    stop_cache_maintenance();

    transcoder_exit();
    transcoder_free();

    index_buffer.clear();

    Logging::debug(nullptr, "%1 V%2 terminated", PACKAGE_NAME, PACKAGE_VERSION);
}
