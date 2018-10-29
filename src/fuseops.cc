/*
 * FFMPEGFS: A read-only FUSE filesystem which transcodes audio formats
 * to MP3/MP4 on the fly when opened and read. See README
 * for more details.
 *
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2012 K. Henriksson
 * Copyright (C) 2017-2018 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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
#include "coders.h"
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

LPVIRTUALFILE insert_file(VIRTUALTYPE type, const string &filename, const string & origfile, const struct stat *st);

static void init_stat(struct stat *st, size_t size, bool directory);
static void prepare_script();
static void translate_path(string *origpath, const char* path);
static bool transcoded_name(string *path);
static LPCVIRTUALFILE find_original(string *path);

static vector<char> index_buffer;
static map<string, VIRTUALFILE> filenames;

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

    st->st_size = size;
    st->st_blocks = (st->st_size + 512 - 1) / 512;

    // Set current user as owner
    st->st_uid = getuid();
    st->st_gid = getgid();

    // Use current date/time
    st->st_atime = st->st_mtime = st->st_ctime = time(NULL);
}

static void prepare_script()
{
    string scriptsource;

    exepath(&scriptsource);
    scriptsource += params.m_scriptsource;

    ffmpegfs_debug(scriptsource.c_str(), "Reading virtual script source.");

    FILE *fpi = fopen(scriptsource.c_str(), "rt");
    if (fpi == NULL)
    {
        ffmpegfs_warning(scriptsource.c_str(), "File open failed. Disabling script: %s", strerror(errno));
        params.m_enablescript = false;
    }
    else
    {
        struct stat st;
        if (fstat(fileno(fpi), &st) == -1)
        {
            ffmpegfs_warning(scriptsource.c_str(), "File could not be accessed. Disabling script: %s", strerror(errno));
            params.m_enablescript = false;
        }
        else
        {
            index_buffer.resize(st.st_size);

            if (fread(&index_buffer[0], 1, st.st_size, fpi) != (size_t)st.st_size)
            {
                ffmpegfs_warning(scriptsource.c_str(), "File could not be read. Disabling script: %s", strerror(errno));
                params.m_enablescript = false;
            }
            else
            {
                ffmpegfs_trace(scriptsource.c_str(), "Read %zu bytes of script file.", index_buffer.size());
            }
        }

        fclose(fpi);
    }
}

// Translate file names from FUSE to the original absolute path.
static void translate_path(string *origpath, const char* path)
{
    *origpath = params.m_basepath;
    *origpath += path;
}

// Convert file name from source to destination name.
// Returns true if filename has been changed
static bool transcoded_name(string * path)
{
    AVOutputFormat* format = av_guess_format(NULL, path->c_str(),NULL);
    
    if (format != NULL)
    {
        if ((params.m_audio_codecid != AV_CODEC_ID_NONE && format->audio_codec != AV_CODEC_ID_NONE) ||
            (params.m_video_codecid != AV_CODEC_ID_NONE && format->video_codec != AV_CODEC_ID_NONE))
        {
            replace_ext(path, params.m_desttype);
            return true;
        }
    }
    return false;
}

// Add new virtual file to internal list
// Returns constant pointer to VIRTUALFILE object of file, NULL if not found
LPVIRTUALFILE insert_file(VIRTUALTYPE type, const string & filename, const string & origfile, const struct stat *st)
{
    VIRTUALFILE virtualfile;

    virtualfile.m_type = type;
    virtualfile.m_origfile = origfile;
    memcpy(&virtualfile.m_st, st, sizeof(struct stat));

    filenames.insert(make_pair(filename, virtualfile));

    map<string, VIRTUALFILE>::iterator it = filenames.find(filename);
    return &it->second;
}

static int selector(const struct dirent * de)
{
    if (de->d_type & (DT_REG | DT_LNK))
    {
        AVOutputFormat* format = av_guess_format(NULL, de->d_name, NULL);

        return (format != NULL);
    }
    else
    {
        return 0;
    }
}

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

// Given the destination (post-transcode) file name, determine the name of
// the original file to be transcoded.
// Returns contstant pointer to VIRTUALFILE object of file, NULL if not found
static LPCVIRTUALFILE find_original(string * path)
{
    // 1st do fast map lookup
    map<string, VIRTUALFILE>::iterator it = filenames.find(*path);

    errno = 0;

    if (it != filenames.end())
    {
        *path = it->second.m_origfile;
        return &it->second;
    }
    else
    {
        // Fallback to old method (required if file accessed directly)
        string ext;
        if (find_ext(&ext, *path) && strcasecmp(ext.c_str(), params.m_desttype) == 0)
        {
            string dir(*path);
            string filename(*path);
            string tmppath;
            struct dirent **namelist;
            struct stat st;
            int count;
            int found = 0;

            remove_filename(&dir);
            tmppath = dir;

            count = scandir(dir.c_str(), &namelist, selector, NULL);
            if (count == -1)
            {
                perror("scandir");
                return NULL;
            }

            remove_path(&filename);
            std::regex specialChars { R"([-[\]{}()*+?.,\^$|#\s])" };
            filename = std::regex_replace(filename, specialChars, R"(\$&)" );
            replace_ext(&filename, "*");

            for (int n = 0; n < count; n++)
            {
                if (!found && !compare(namelist[n]->d_name, filename.c_str()))
                {
                    append_filename(&tmppath, namelist[n]->d_name);
                    found = 1;
                }
                free(namelist[n]);
            }
            free(namelist);

            if (found && lstat(tmppath.c_str(), &st) == 0)
            {
                // File exists with this extension
                LPCVIRTUALFILE virtualfile = insert_file(VIRTUALTYPE_REGULAR, *path, tmppath, &st);
                *path = tmppath;
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
    return NULL;
}

int ffmpegfs_readlink(const char *path, char *buf, size_t size)
{
    string origpath;
    string transcoded;
    ssize_t len;

    ffmpegfs_trace(path, "readlink");

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

int ffmpegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t /*offset*/, struct fuse_file_info * /*fi*/)
{
    string origpath;
    DIR *dp;
    struct dirent *de;
#if defined(USE_LIBBLURAY) || defined(USE_LIBDVD) || defined(USE_LIBVCD)
    int res;
#endif

    ffmpegfs_trace(path, "readdir");

    translate_path(&origpath, path);
    append_sep(&origpath);

    // Add a virtual script if enabled
    if (params.m_enablescript)
    {
        string filename(params.m_scriptfile);
        string origfile;
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
    if (dp)
    {
        try
        {
            while ((de = readdir(dp)))
            {
                string filename(de->d_name);
                string origfile;
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

int ffmpegfs_getattr(const char *path, struct stat *stbuf)
{
    string origpath;
#if defined(USE_LIBBLURAY) || defined(USE_LIBDVD) || defined(USE_LIBVCD)
    int res = 0;
#endif

    ffmpegfs_trace(path, "getattr");

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
    VIRTUALTYPE type = (virtualfile != NULL) ? virtualfile->m_type : VIRTUALTYPE_REGULAR;

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
    }
    case VIRTUALTYPE_REGULAR:
    {
        if (!no_lstat)
        {
            if (lstat(origpath.c_str(), stbuf) == -1)
            {
                int error = -errno;
#if defined(USE_LIBBLURAY) || defined(USE_LIBDVD) || defined(USE_LIBVCD)
                // Returns -errno or number or titles on DVD
                string path(origpath);

                remove_filename(&path);
#ifdef USE_LIBVCD
                if (res <= 0)
                {
                    res = check_vcd(path);
                }
#endif // USE_LIBVCD
#ifdef USE_LIBDVD
                if (res <= 0)
                {
                    res = check_dvd(path);
                }
#endif // USE_LIBDVD
#ifdef USE_LIBBLURAY
                if (res <= 0)
                {
                    res = check_bluray(path);
                }
#endif // USE_LIBBLURAY
                if (res <= 0)
                {
                    // No Bluray/DVD/VCD found or error reading disk
                    return (!res ?  error : res);
                }

                virtualfile = find_original(&origpath);

                if (virtualfile == NULL)
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
            if (!transcoder_cached_filesize(origpath, stbuf))
            {
                struct Cache_Entry* cache_entry;

                cache_entry = transcoder_new(virtualfile, false);
                if (!cache_entry)
                {
                    return -errno;
                }

#if defined __x86_64__ || !defined __USE_FILE_OFFSET64
                stbuf->st_size = (__off_t)transcoder_get_size(cache_entry);
#else
                stbuf->st_size = (__off64_t)transcoder_get_size(cache_entry);
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

int ffmpegfs_fgetattr(const char *path, struct stat * stbuf, struct fuse_file_info *fi)
{
    string origpath;

    ffmpegfs_trace(path, "fgetattr");

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

    assert(virtualfile != NULL);

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
            struct Cache_Entry* cache_entry;

            cache_entry = (struct Cache_Entry*)(uintptr_t)fi->fh;

            if (!cache_entry)
            {
                ffmpegfs_error(path, "Tried to stat unopen file.");
                errno = EBADF;
                return -errno;
            }

#if defined __x86_64__ || !defined __USE_FILE_OFFSET64
            stbuf->st_size = (__off_t)transcoder_buffer_watermark(cache_entry);
#else
            stbuf->st_size = (__off64_t)transcoder_buffer_watermark(cache_entry);
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

int ffmpegfs_open(const char *path, struct fuse_file_info *fi)
{
    string origpath;
    struct Cache_Entry* cache_entry;
    int fd;

    ffmpegfs_trace(path, "open");

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
    LPCVIRTUALFILE virtualfile = find_original(&origpath);

    assert(virtualfile != NULL);

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
        if (!cache_entry)
        {
            return -errno;
        }

        // Store transcoder in the fuse_file_info structure.
        fi->fh = (uintptr_t)cache_entry;
        // Need this because we do not know the exact size in advance.
        fi->direct_io = 1;

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

int ffmpegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    string origpath;
    int fd;
    ssize_t read = 0;
    struct Cache_Entry* cache_entry;

    ffmpegfs_trace(path, "read %zu bytes from %jd.", size, (intmax_t)offset);

    translate_path(&origpath, path);

    fd = open(origpath.c_str(), O_RDONLY);
    if (fd != -1)
    {
        // If this is a real file, pass the call through.
        read = pread(fd, buf, size, offset);
        close(fd);
        if (read >= 0)
        {
            return (int)read;
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

    assert(virtualfile != NULL);

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

        read = (ssize_t)bytes;
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
        cache_entry = (struct Cache_Entry*)(uintptr_t)fi->fh;

        if (!cache_entry)
        {
            ffmpegfs_error(origpath.c_str(), "Tried to read from unopen file.");
            return -errno;
        }

        read = transcoder_read(cache_entry, buf, offset, size);

        break;
    }
    case VIRTUALTYPE_PASSTHROUGH: // We should never come here but this shuts up a warning
    case VIRTUALTYPE_BUFFER:
    {
        assert(false);
        break;
    }
    }

    if (read >= 0)
    {
        return (int)read;
    }
    else
    {
        return -errno;
    }
}

int ffmpegfs_statfs(const char *path, struct statvfs *stbuf)
{
    string origpath;

    ffmpegfs_trace(path, "statfs");

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

int ffmpegfs_release(const char *path, struct fuse_file_info *fi)
{
    struct Cache_Entry* cache_entry;

    ffmpegfs_trace(path, "release");

    cache_entry = (struct Cache_Entry*)(uintptr_t)fi->fh;
    if (cache_entry)
    {
        transcoder_delete(cache_entry);
    }

    return 0;
}

void *ffmpegfs_init(struct fuse_conn_info *conn)
{
    ffmpegfs_info(NULL, "%s V%s initialising.", PACKAGE_NAME, PACKAGE_VERSION);
    ffmpegfs_info(NULL, "Target type: %s Profile: %s", params.m_desttype, get_profile_text(params.m_profile));
    ffmpegfs_info(NULL, "Mapping '%s' to '%s'.", params.m_basepath, params.m_mountpath);

    // We need synchronous reads.
    conn->async_read = 0;

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

    return NULL;
}

void ffmpegfs_destroy(__attribute__((unused)) void * p)
{
    ffmpegfs_info(NULL, "%s V%s terminating", PACKAGE_NAME, PACKAGE_VERSION);
    printf("%s V%s terminating\n", PACKAGE_NAME, PACKAGE_VERSION);

    stop_cache_maintenance();

    transcoder_exit();
    transcoder_free();

    index_buffer.clear();

    ffmpegfs_debug(NULL, "%s V%s terminated", PACKAGE_NAME, PACKAGE_VERSION);
}
