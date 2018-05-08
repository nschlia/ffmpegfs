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

#include <dirent.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <assert.h>

typedef enum _tagVIRTUALTYPE
{
    VIRTUALTYPE_PASSTHROUGH,        // passthrough file, not used
    VIRTUALTYPE_REGULAR,            // Regular file to transcode
    VIRTUALTYPE_SCRIPT,             // Virtual script
} VIRTUALTYPE;
typedef VIRTUALTYPE const *LPCVIRTUALTYPE;
typedef VIRTUALTYPE LPVIRTUALTYPE;

typedef struct _tagVIRTUALFILE
{
    VIRTUALTYPE m_type;
    string      m_origfile;
    struct stat m_st;

} VIRTUALFILE;
typedef VIRTUALFILE const *LPCVIRTUALFILE;
typedef VIRTUALFILE *LPVIRTUALFILE;

static LPCVIRTUALFILE insert_file(VIRTUALTYPE type, const string &filename, const string & origfile, const struct stat *st);
static void init_stat(struct stat *st, bool directory);
static void prepare_script();
static void translate_path(string *origpath, const char* path);
static bool transcoded_name(string *path);
static LPCVIRTUALFILE find_original(string *path);

static vector<char> index_buffer;
static map<string, VIRTUALFILE> filenames;

static void init_stat(struct stat * st, bool directory)
{
    memset(st, 0, sizeof(struct stat));

    st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
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
    st->st_size = index_buffer.size();
#else
    st->st_size = index_text.size();
#endif
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

    ffmpegfs_debug("%s * Reading virtual script source.", scriptsource.c_str());

    FILE *fpi = fopen(scriptsource.c_str(), "rt");
    if (fpi == NULL)
    {
        ffmpegfs_warning("%s * File open failed. Disabling script: %s", scriptsource.c_str(), strerror(errno));
        params.m_enablescript = false;
    }
    else
    {
        struct stat st;
        if (fstat(fileno(fpi), &st) == -1)
        {
            ffmpegfs_warning("%s * File could not be accessed. Disabling script: %s", scriptsource.c_str(), strerror(errno));
            params.m_enablescript = false;
        }
        else
        {
            index_buffer.resize(st.st_size);

            if (fread(&index_buffer[0], 1, st.st_size, fpi) != (size_t)st.st_size)
            {
                ffmpegfs_warning("%s * File could not be read. Disabling script: %s", scriptsource.c_str(), strerror(errno));
                params.m_enablescript = false;
            }
            else
            {
                ffmpegfs_debug("%s * Read %zu bytes of script file.", scriptsource.c_str(), index_buffer.size());
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
    string ext;

    if (find_ext(&ext, *path) && check_decoder(ext.c_str()))
    {
        replace_ext(path, params.m_desttype);
        return true;
    }
    else
    {
        return false;
    }
}

// Add new virtual file to internal list
// Returns constant pointer to VIRTUALFILE object of file, NULL if not found
static LPCVIRTUALFILE insert_file(VIRTUALTYPE type, const string & filename, const string & origfile, const struct stat *st)
{
    VIRTUALFILE virtualfile;

    virtualfile.m_type = type;
    virtualfile.m_origfile = origfile;
    memcpy(&virtualfile.m_st, st, sizeof(struct stat));

    filenames.insert(make_pair(filename, virtualfile));

    map<string, VIRTUALFILE>::iterator p = filenames.end();

    return &p->second;
}

// Given the destination (post-transcode) file name, determine the name of
// the original file to be transcoded.
// Returns contstant pointer to VIRTUALFILE object of file, NULL if not found
static LPCVIRTUALFILE find_original(string * path)
{
    // 1st do fast map lookup
    map<string, VIRTUALFILE>::iterator p = filenames.find(*path);

    errno = 0;

    if (p != filenames.end())
    {
        *path = p->second.m_origfile;
        return &p->second;
    }
    else
    {
        // Fallback to old method (required if file accessed directly)
        string ext;
        if (find_ext(&ext, *path) && strcasecmp(ext.c_str(), params.m_desttype) == 0)
        {
            string tmppath(*path);

            for (size_t i = 0; decoder_list[i]; ++i)
            {
                struct stat st;

                replace_ext(&tmppath, decoder_list[i]);

                //if (access(tmppath.c_str(), F_OK) == 0)
                if (lstat(tmppath.c_str(), &st) == 0)
                {
                    // File exists with this extension
                    LPCVIRTUALFILE p = insert_file(VIRTUALTYPE_REGULAR, *path, tmppath, &st);
                    *path = tmppath;
                    return p;
                }
                else
                {
                    // File does not exist; not an error
                    errno = 0;
                }
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

    ffmpegfs_trace("readlink %s", path);

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

    ffmpegfs_trace("readdir %s", path);

    translate_path(&origpath, path);
    append_sep(&origpath);

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

                // Remove write permissions, make no sense
                st.st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);

                if (filler(buf, filename.c_str(), &st, 0))
                {
                    break;
                }
            }

            // Add a virtual script if enabled
            if (params.m_enablescript)
            {
                string filename(params.m_scriptfile);
                string origfile;
                struct stat st;

                origfile = origpath + filename;

                init_stat(&st, false);

                if (filler(buf, filename.c_str(), &st, 0))
                {
                    // break;
                }

                insert_file(VIRTUALTYPE_SCRIPT, origpath + filename, origfile, &st);
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

    ffmpegfs_trace("getattr %s", path);

    translate_path(&origpath, path);

    if (lstat(origpath.c_str(), stbuf) == 0)
    {
        // Remove write permissions, make no sense
        stbuf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);

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

    switch (type)
    {
    case VIRTUALTYPE_SCRIPT:
    {
        // Use stored status
        mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
        errno = 0;
        break;
    }
    case VIRTUALTYPE_REGULAR:
    {
        if (lstat(origpath.c_str(), stbuf) == -1)
        {
            return -errno;
        }

        // Remove write permissions, make no sense
        stbuf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);

        // Get size for resulting output file from regular file, otherwise it's a symbolic link.
        if (S_ISREG(stbuf->st_mode))
        {
            if (!transcoder_cached_filesize(origpath.c_str(), stbuf))
            {
                struct Cache_Entry* cache_entry;

                cache_entry = transcoder_new(origpath.c_str(), 0);
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
    default:
    {
        break;
    }
    case VIRTUALTYPE_PASSTHROUGH: // We will never come here but this shuts up a warning
    {
        break;
    }
    }

    return 0;
}

int ffmpegfs_fgetattr(const char *path, struct stat * stbuf, struct fuse_file_info *fi)
{
    string origpath;

    ffmpegfs_trace("fgetattr %s", path);

    errno = 0;

    translate_path(&origpath, path);

    if (lstat(origpath.c_str(), stbuf) == 0)
    {
        // Remove write permissions, make no sense
        stbuf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);

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

    switch (virtualfile->m_type)
    {
    case VIRTUALTYPE_SCRIPT:
    {
        // Use stored status
        mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
        errno = 0;
        break;
    }
    case VIRTUALTYPE_REGULAR:
    {
        if (lstat(origpath.c_str(), stbuf) == -1)
        {
            return -errno;
        }

        // Remove write permissions, make no sense
        stbuf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);

        // Get size for resulting output file from regular file, otherwise it's a symbolic link.
        if (S_ISREG(stbuf->st_mode))
        {
            struct Cache_Entry* cache_entry;

            cache_entry = (struct Cache_Entry*)(uintptr_t)fi->fh;

            if (!cache_entry)
            {
                ffmpegfs_error("Tried to stat unopen file: %s.", path);
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
    default:
    {
        break;
    }
    case VIRTUALTYPE_PASSTHROUGH: // We will never come here but this shuts up a warning
    {
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

    ffmpegfs_trace("open %s", path);

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
    case VIRTUALTYPE_REGULAR:
    {
        cache_entry = transcoder_new(origpath.c_str(), 1);
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
    default:
    {
        break;
    }
    case VIRTUALTYPE_PASSTHROUGH: // We will never come here but this shuts up a warning
    {
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

    ffmpegfs_trace("read %s: %zu bytes from %jd.", path, size, (intmax_t)offset);

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
    case VIRTUALTYPE_REGULAR:
    {
        cache_entry = (struct Cache_Entry*)(uintptr_t)fi->fh;

        if (!cache_entry)
        {
            ffmpegfs_error("Tried to read from unopen file: %s.", origpath.c_str());
            return -errno;
        }

        read = transcoder_read(cache_entry, buf, offset, size);

        break;
    }
    default:
    {
        break;
    }
    case VIRTUALTYPE_PASSTHROUGH: // We will never come here but this shuts up a warning
    {
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

    ffmpegfs_trace("statfs %s", path);

    errno = 0;

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

    // This is a virtual file
    LPCVIRTUALFILE virtualfile = find_original(&origpath);

    assert(virtualfile != NULL);

    switch (virtualfile->m_type)
    {
    case VIRTUALTYPE_SCRIPT:
    {
        // Use stored status
        mempcpy(stbuf, &virtualfile->m_st, sizeof(struct stat));
        errno = 0;
        break;
    }
    case VIRTUALTYPE_REGULAR:
    {
        statvfs(origpath.c_str(), stbuf);

        errno = 0;  // Just to make sure - reset any error
        break;
    }
    case VIRTUALTYPE_PASSTHROUGH: // We will never come here but this shuts up a warning
    {
        break;
    }
    }

    return 0;
}

int ffmpegfs_release(const char *path, struct fuse_file_info *fi)
{
    struct Cache_Entry* cache_entry;

    ffmpegfs_trace("release %s", path);

    cache_entry = (struct Cache_Entry*)(uintptr_t)fi->fh;
    if (cache_entry)
    {
        transcoder_delete(cache_entry);
    }

    return 0;
}

void *ffmpegfs_init(struct fuse_conn_info *conn)
{
    ffmpegfs_info("%s V%s initialising.", PACKAGE_NAME, PACKAGE_VERSION);
    ffmpegfs_info("Target type: %s Profile: %s", params.m_desttype, get_profile_text(params.m_profile));
    ffmpegfs_info("Mapping '%s' to '%s'.", params.m_basepath, params.m_mountpath);

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
    ffmpegfs_info("%s V%s terminating", PACKAGE_NAME, PACKAGE_VERSION);
    printf("%s V%s terminating\n", PACKAGE_NAME, PACKAGE_VERSION);

    stop_cache_maintenance();

    transcoder_exit();
    transcoder_free();

    index_buffer.clear();

    ffmpegfs_debug("%s V%s terminated", PACKAGE_NAME, PACKAGE_VERSION);
}
