/*
 * FFMPEGFS: A read-only FUSE filesystem which transcodes audio formats
 * to MP3/MP4 on the fly when opened and read. See README
 * for more details.
 *
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

typedef std::map<std::string, VIRTUALFILE> filenamemap;

static void init_stat(struct stat *st, size_t size, bool directory);
static void prepare_script();
static void translate_path(std::string *origpath, const char* path);
static bool transcoded_name(std::string *filepath);
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

static filenamemap filenames;
static std::vector<char> index_buffer;

static struct sigaction oldHandler;

fuse_operations ffmpegfs_ops;

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

static void init_stat(struct stat * st, size_t size, bool directory)
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
    st->st_size = static_cast<__off_t>(size);
#else
    st->st_size = static_cast<__off64_t>(size);
#endif
    st->st_blocks = (st->st_size + 512 - 1) / 512;

    // Set current user as owner
    st->st_uid = getuid();
    st->st_gid = getgid();

    // Use current date/time
    st->st_atime = st->st_mtime = st->st_ctime = time(nullptr);
}

static void prepare_script()
{
    std::string scriptsource;

    exepath(&scriptsource);
    scriptsource += params.m_scriptsource;

    Logging::debug(scriptsource, "Reading virtual script source.");

    FILE *fpi = fopen(scriptsource.c_str(), "rt");
    if (fpi == nullptr)
    {
        Logging::warning(scriptsource, "File open failed. Disabling script: %1", strerror(errno));
        params.m_enablescript = false;
    }
    else
    {
        struct stat st;
        if (fstat(fileno(fpi), &st) == -1)
        {
            Logging::warning(scriptsource, "File could not be accessed. Disabling script: %1", strerror(errno));
            params.m_enablescript = false;
        }
        else
        {
            index_buffer.resize(static_cast<size_t>(st.st_size));

            if (fread(&index_buffer[0], 1, static_cast<size_t>(st.st_size), fpi) != static_cast<size_t>(st.st_size))
            {
                Logging::warning(scriptsource, "File could not be read. Disabling script: %1", strerror(errno));
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

// Translate file names from FUSE to the original absolute path.
static void translate_path(std::string *origpath, const char* path)
{
    *origpath = params.m_basepath;
    *origpath += path;
}

// Convert file name from source to destination name.
// Returns true if filename has been changed
static bool transcoded_name(std::string * filepath)
{
    AVOutputFormat* format = av_guess_format(nullptr, filepath->c_str(), nullptr);
    
    if (format != nullptr)
    {
        if ((params.current_format(*filepath)->m_audio_codec_id != AV_CODEC_ID_NONE && format->audio_codec != AV_CODEC_ID_NONE) ||
                (params.current_format(*filepath)->m_video_codec_id != AV_CODEC_ID_NONE && format->video_codec != AV_CODEC_ID_NONE))
        {
            replace_ext(filepath, params.current_format(*filepath)->m_format_name);
            return true;
        }
    }
    return false;
}

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

LPVIRTUALFILE insert_file(VIRTUALTYPE type, const std::string & filepath, const std::string & origfile, const struct stat *st)
{
    VIRTUALFILE virtualfile;

    virtualfile.m_type          = type;
    virtualfile.m_format_idx    = params.guess_format_idx(origfile);
    virtualfile.m_origfile      = sanitise_name(origfile);

    memcpy(&virtualfile.m_st, st, sizeof(struct stat));

    filenames.insert(make_pair(filepath, virtualfile));

    filenamemap::iterator it = filenames.find(filepath);
    return &it->second;
}

LPVIRTUALFILE find_file(const std::string & filepath)
{
    filenamemap::iterator it = filenames.find(filepath);

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

static int selector(const struct dirent * de)
{
    if (de->d_type & (DT_REG | DT_LNK))
    {
        AVOutputFormat* oformat = av_guess_format(nullptr, de->d_name, nullptr);

        return (oformat != nullptr);
    }
    else
    {
        return 0;
    }
}

LPVIRTUALFILE find_original(std::string * filepath)
{
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
        if (find_ext(&ext, *filepath) && (strcasecmp(ext, params.m_format[0].real_desttype()) == 0 || (params.smart_transcode() && strcasecmp(ext, params.m_format[1].real_desttype()) == 0)))
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
                perror("scandir");
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

            tmppath = sanitise_name(tmppath);

            if (found && lstat(tmppath.c_str(), &st) == 0)
            {
                // File exists with this extension
                LPVIRTUALFILE virtualfile = insert_file(VIRTUALTYPE_REGULAR, *filepath, tmppath, &st);
                *filepath = tmppath;
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

static int ffmpegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t /*offset*/, struct fuse_file_info * /*fi*/)
{
    std::string origpath;
    DIR *dp;
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
        std::string filename(params.m_scriptfile);
        std::string origfile;
        struct stat st;

        origfile = origpath + filename;

        init_stat(&st, index_buffer.size(), false);

        if (filler(buf, filename.c_str(), &st, 0))
        {
            // break;
        }

        insert_file(VIRTUALTYPE_SCRIPT, origpath + filename, origfile, &st);
    }

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

    dp = opendir(origpath.c_str());
    if (dp != nullptr)
    {
        try
        {
            while ((de = readdir(dp)) != nullptr)
            {
                std::string filename(de->d_name);
                std::string origfile;
                struct stat st;

                origfile = origpath + filename;

                if (lstat(origfile.c_str(), &st) == -1)
                {
                    throw false;
                }

                if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
                {
                    if (transcoded_name(&filename))
                    {
                        insert_file(VIRTUALTYPE_REGULAR, origpath + filename, origfile, &st);
                    }
                }

                if (filler(buf, filename.c_str(), &st, 0))
                {
                    break;
                }
            }

            errno = 0;  // Just to make sure - reset any error
        }
        catch (bool)
        {

        }

        closedir(dp);
    }

    return -errno;
}

static int ffmpegfs_getattr(const char *path, struct stat *stbuf)
{
    std::string origpath;

    Logging::trace(path, "getattr");

    translate_path(&origpath, path);

    if (lstat(origpath.c_str(), stbuf) == 0)
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
    LPVIRTUALFILE virtualfile = find_original(&origpath);
    VIRTUALTYPE type = (virtualfile != nullptr) ? virtualfile->m_type : VIRTUALTYPE_REGULAR;

    bool no_lstat = false;

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
        no_lstat = true;
        [[clang::fallthrough]];
    }
    case VIRTUALTYPE_REGULAR:
    {
        if (!no_lstat)
        {
            if (lstat(origpath.c_str(), stbuf) == -1)
            {
                int error = -errno;
#if defined(USE_LIBBLURAY) || defined(USE_LIBDVD) || defined(USE_LIBVCD)
                int res = 0;

                // Returns -errno or number or titles on DVD
                std::string _origpath(origpath);

                remove_filename(&_origpath);

                virtualfile = find_original(&origpath);

                if (virtualfile == nullptr)
                {
#ifdef USE_LIBVCD
                    if (res <= 0)
                    {
                        res = check_vcd(_origpath);
                    }
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
                    if (res <= 0)
                    {
                        res = check_dvd(_origpath);
                    }
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
                    if (res <= 0)
                    {
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
                    // Not a DVD file
                    return -ENOENT;
                }

                mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
#else
                return error;
#endif
            }
        }

        // Get size for resulting output file from regular file, otherwise it's a symbolic link.
        if (S_ISREG(stbuf->st_mode))
        {
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
        break;
    }
    case VIRTUALTYPE_PASSTHROUGH: // We should never come here but this shuts up a warning
    case VIRTUALTYPE_BUFFER:
    {
        assert(false);
        break;
    }
    }

    return 0;
}

static int ffmpegfs_fgetattr(const char *path, struct stat * stbuf, struct fuse_file_info *fi)
{
    std::string origpath;

    Logging::trace(path, "fgetattr");

    errno = 0;

    translate_path(&origpath, path);

    if (lstat(origpath.c_str(), stbuf) == 0)
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
    LPCVIRTUALFILE virtualfile = find_original(&origpath);

    assert(virtualfile != nullptr);

    bool no_lstat = false;

    switch (virtualfile->m_type)
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
        no_lstat = true;
        [[clang::fallthrough]];
    }
    case VIRTUALTYPE_REGULAR:
    {
        if (!no_lstat)
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
        break;
    }
    case VIRTUALTYPE_PASSTHROUGH: // We should never come here but this shuts up a warning
    case VIRTUALTYPE_BUFFER:
    {
        assert(false);
        break;
    }
    }

    return 0;
}

static int ffmpegfs_open(const char *path, struct fuse_file_info *fi)
{
    std::string origpath;
    Cache_Entry* cache_entry;
    int fd;

    Logging::trace(path, "open");

    translate_path(&origpath, path);

    fd = open(origpath.c_str(), fi->flags);

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

    // This is a virtual file
    LPVIRTUALFILE virtualfile = find_original(&origpath);

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
        cache_entry = transcoder_new(virtualfile, true);
        if (cache_entry == nullptr)
        {
            return -errno;
        }

        // Store transcoder in the fuse_file_info structure.
        fi->fh = reinterpret_cast<uintptr_t>(cache_entry);
        // Need this because we do not know the exact size in advance.
        fi->direct_io = 1;
        //        fi->keep_cache = 1;

        // Clear errors
        errno = 0;
        break;
    }
    case VIRTUALTYPE_PASSTHROUGH: // We should never come here but this shuts up a warning
    case VIRTUALTYPE_BUFFER:
    {
        assert(false);
        break;
    }
    }

    return 0;
}

static int ffmpegfs_read(const char *path, char *buf, size_t size, off_t _offset, struct fuse_file_info *fi)
{
    std::string origpath;
    int fd;
    size_t offset = static_cast<size_t>(_offset);  // Cast OK: offset can never be < 0.
    int bytes_read = 0;
    Cache_Entry* cache_entry;

    Logging::trace(path, "Reading %1 bytes from %2.", size, offset);

    translate_path(&origpath, path);

    fd = open(origpath.c_str(), O_RDONLY);
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

    LPCVIRTUALFILE virtualfile = find_original(&origpath);
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
        cache_entry = reinterpret_cast<Cache_Entry*>(fi->fh);

        if (cache_entry == nullptr)
        {
            Logging::error(origpath.c_str(), "Tried to read from unopen file.");
            return -errno;
        }

        success = transcoder_read(cache_entry, buf, offset, size, &bytes_read);

        break;
    }
    case VIRTUALTYPE_PASSTHROUGH: // We should never come here but this shuts up a warning
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
        if (start_cache_maintenance(params.m_cache_maintenance))
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
