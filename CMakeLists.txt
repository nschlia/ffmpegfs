# --------------------------------------------------------------------------
# Copyright (C) 2017-2022 by Norbert Schlia (nschlia@oblivion-software.de)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
#
# On Debian systems, the complete text of the GNU General Public License
# Version 3 can be found in `/usr/share/common-licenses/GPL-3'.
# --------------------------------------------------------------------------
# To build run
#   cmake --configure .
# once, then
#   cmake --build . --target all
#
# --------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
#set(CMAKE_VERBOSE_MAKEFILE ON) # Enable if you are curious. Make builds very slow...

project(ffmpegfs VERSION 2.9)

# Project metadata
set(PROJECT_VENDOR      "Oblivion Software/Norbert Schlia")
set(PROJECT_CONTACT     "nschlia@oblivion-software.de")
set(PROJECT_URL         "https://nschlia.github.io/ffmpegfs/")
set(PROJECT_COPYRIGHT   "(c) Oblivion Software/Norbert Schlia nschlia@oblivion-software.de")
set(PROJECT_DESCRIPTION "FUSE-based transcoding filesystem with video support from many formats")
set(PROJECT             ${PROJECT_NAME})

set(PACKAGE             "ffmpegfs")
set(PACKAGE_BUGREPORT   "https://github.com/nschlia/ffmpegfs/issues")
set(PACKAGE_NAME        "FFMPEGFS")
set(PACKAGE_URL         ${PROJECT_URL})
set(PACKAGE_STRING      "FFMPEGFS ${PROJECT_VERSION}")
set(PACKAGE_TARNAME     "ffmpegfs")
set(PACKAGE_VERSION     ${PROJECT_VERSION})
set(VERSION             ${PROJECT_VERSION})

# Includes for doing all of our 'configure' checks. Things like checking
# for headers, functions, libraries, types and size of types.
INCLUDE(CheckIncludeFile)
INCLUDE(CheckTypeSize)
#INCLUDE(CheckFunctionExists)
#INCLUDE(CheckCXXSourceCompiles)
#INCLUDE(TestBigEndian)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckLibraryExists)
INCLUDE(FindPkgConfig)

set(CONFIGURE_ARGS " ") # TODO: Mimic this

PKG_CHECK_MODULES(SQLITE3 REQUIRED sqlite3>=3.7.13)
CHECK_LIBRARY_EXISTS(sqlite3 sqlite3_errstr         "/lib;/usr/lib;/usr/local/lib"  HAVE_SQLITE_ERRSTR)
CHECK_LIBRARY_EXISTS(sqlite3 sqlite3_db_cacheflush  "/lib;/usr/lib;/usr/local/lib"  HAVE_SQLITE_CACHEFLUSH)
CHECK_LIBRARY_EXISTS(sqlite3 sqlite3_expanded_sql   "/lib;/usr/lib;/usr/local/lib"  HAVE_SQLITE_EXPANDED_SQL)

PKG_CHECK_MODULES(FUSE REQUIRED fuse>=2.6.0)
PKG_CHECK_MODULES(LIBCUE REQUIRED libcue>=2.2.1)
PKG_CHECK_MODULES(CHARDET REQUIRED chardet>=1.0.4)

PKG_CHECK_MODULES(LIBAVUTIL REQUIRED libavutil>=54.3.0)
PKG_CHECK_MODULES(LIBAVCODEC REQUIRED libavcodec>=56.1.0)
PKG_CHECK_MODULES(LIBAVFORMAT REQUIRED libavformat>=56.1.0)
PKG_CHECK_MODULES(LIBSWSCALE REQUIRED libswscale>=3.0.0)
PKG_CHECK_MODULES(LIBAVFILTER REQUIRED libavfilter>=5.40.0)
IF (LIBAVUTIL_FOUND AND LIBAVCODEC_FOUND AND LIBAVFORMAT_FOUND AND LIBSWSCALE_FOUND AND LIBAVFILTER_FOUND)
    message( "--   Sufficiently recent FFmpeg library version found." )
    set(HAVE_FFMPEG 1)
ENDIF (LIBAVUTIL_FOUND AND LIBAVCODEC_FOUND AND LIBAVFORMAT_FOUND AND LIBSWSCALE_FOUND AND LIBAVFILTER_FOUND)

PKG_CHECK_MODULES(LIBAVRESAMPLE libavresample>=2.1.0)
PKG_CHECK_MODULES(LIBSWRESAMPLE libswresample>=2.0.0)

IF (LIBSWRESAMPLE_FOUND)
    set(USE_LIBSWRESAMPLE 1)
ENDIF (LIBSWRESAMPLE_FOUND)
IF (LIBAVRESAMPLE_FOUND)
    set(USE_LIBAVRESAMPLE 1)
ENDIF (LIBAVRESAMPLE_FOUND)
IF (NOT USE_LIBSWRESAMPLE AND NOT USE_LIBAVRESAMPLE)
    message( FATAL_ERROR "Either libswresample or libavresample must be present and enabled. Please make sure its development modules are installed." )
ENDIF (NOT USE_LIBSWRESAMPLE AND NOT USE_LIBAVRESAMPLE)

PKG_CHECK_MODULES(LIBDVDREAD dvdread>=5.0.0)
PKG_CHECK_MODULES(LIBBLURAY libbluray>=0.6.2)

# Check for doxygen. If not installed, go on, but make doxy won't work.
FIND_PACKAGE(Doxygen
    COMPONENTS dot)
IF (NOT DOXYGEN_FOUND)
    message( WARNING "Doxygen not found - continuing without Doxygen support. If you intend to use \"make doxy\" you should install it.")
ENDIF (NOT DOXYGEN_FOUND)

# Check for curl binary. If not installed, go on, but make doxy won't work.
find_program (CURL curl)
IF (NOT CURL-NOTFOUND)
    message("-- Found curl: ${CURL}")
ELSE (NOT CURL-NOTFOUND)
    message( WARNING "curl not found - continuing without curl support. If you intend to use \"make doxy\" you should install it.")
ENDIF (NOT CURL-NOTFOUND)

# Header checks
CHECK_INCLUDE_FILES ("stdlib.h;stdarg.h;string.h;float.h" STDC_HEADERS)
IF (STDC_HEADERS)
    MESSAGE (STATUS "ANSI C header files - found")
    SET (STDC_HEADERS 1 CACHE INTERNAL "System has ANSI C header files")
    SET (HAVE_STDLIB_H 1 CACHE INTERNAL "Have include stdlib.h")
    SET (HAVE_STDARG_H 1 CACHE INTERNAL "Have include stdarg.h")
    SET (HAVE_STRING_H 1 CACHE INTERNAL "Have include string.h")
    SET (HAVE_FLOAT_H 1 CACHE INTERNAL "Have include float.h")
ELSE (STDC_HEADERS)
    MESSAGE (STATUS "ANSI C header files - not found")
    SET (STDC_HEADERS 0 CACHE INTERNAL "System has ANSI C header files")
ENDIF (STDC_HEADERS)
CHECK_INCLUDE_FILE("inttypes.h"     HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE("memory.h"       HAVE_MEMORY_H)
CHECK_INCLUDE_FILE("stdint.h"       HAVE_STDINT_H)
CHECK_INCLUDE_FILE("strings.h"      HAVE_STRINGS_H)
CHECK_INCLUDE_FILE("sys/stat.h"     HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE("sys/types.h"    HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE("unistd.h"       HAVE_UNISTD_H)

CHECK_TYPE_SIZE("int"               SIZEOF_INT)
CHECK_TYPE_SIZE("long"              SIZEOF_LONG)
CHECK_TYPE_SIZE("off_t"             SIZEOF_OFF_T)
CHECK_TYPE_SIZE("pthread_t"         SIZEOF_PTHREAD_T)
CHECK_TYPE_SIZE("time_t"            SIZEOF_TIME_T)

IF (SIZEOF_LONG EQUAL SIZEOF_TIME_T)
    set(FFMPEGFS_FORMAT_TIME_T "ld")
ELSE (SIZEOF_LONG EQUAL SIZEOF_TIME_T)
    set(FFMPEGFS_FORMAT_TIME_T "d")
ENDIF (SIZEOF_LONG EQUAL SIZEOF_TIME_T)

IF (SIZEOF_LONG EQUAL SIZEOF_PTHREAD_T)
    set(FFMPEGFS_FORMAT_PTHREAD_T "lx")
ELSE (SIZEOF_LONG EQUAL SIZEOF_PTHREAD_T)
set(FFMPEGFS_FORMAT_PTHREAD_T "x")
ENDIF (SIZEOF_LONG EQUAL SIZEOF_PTHREAD_T)

set(HOST_OS                         ${CMAKE_SYSTEM})
set(_POSIX_C_SOURCE                 200809L)

# Create config.h
configure_file(${CMAKE_SOURCE_DIR}/cmake/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)

# Project
project(ffmpegfs
    DESCRIPTION ${PROJECT_DESCRIPTION}
    HOMEPAGE_URL ${PROJECT_URL}
    LANGUAGES CXX)

# Source files
set(SOURCES
    src/blurayio.cc
    src/blurayparser.cc
    src/buffer.cc
    src/cache.cc
    src/cache_entry.cc
    src/cache_maintenance.cc
    src/cuesheetparser.cc
    src/diskio.cc
    src/dvdio.cc
    src/dvdparser.cc
    src/ffmpeg_base.cc
    src/ffmpegfs.cc
    src/ffmpeg_profiles.cc
    src/ffmpeg_transcoder.cc
    src/ffmpeg_utils.cc
    src/fileio.cc
    src/fuseops.cc
    src/logging.cc
    src/transcode.cc
    src/vcdio.cc
    src/vcdparser.cc
    src/vcd/vcdchapter.cc
    src/vcd/vcdentries.cc
    src/vcd/vcdinfo.cc
    src/vcd/vcdutils.cc
    src/thread_pool.cc
)

# Header files
set(HEADERS
    src/aiff.h
     src/blurayio.h
     src/blurayparser.h
     src/buffer.h
     src/cache_entry.h
     src/cache.h
     src/cache_maintenance.h
     src/cuesheetparser.h
     src/diskio.h
     src/dvdio.h
     src/dvdparser.h
     src/ffmpeg_base.h
     src/ffmpeg_compat.h
     src/ffmpegfs.h
     src/ffmpeg_profiles.h
     src/ffmpeg_transcoder.h
     src/ffmpeg_utils.h
     src/fileio.h
     src/id3v1tag.h
     src/logging.h
     src/transcode.h
     src/vcdio.h
     src/vcdparser.h
     src/vcd/vcdchapter.h
     src/vcd/vcdentries.h
     src/vcd/vcdinfo.h
     src/vcd/vcdutils.h
     src/wave.h
     src/thread_pool.h
)

# Other files
#test/addtest
#test/mkvid
list(APPEND SOURCES
    configure.ac
    COPYING
    COPYING.DOC
    docker/Dockerfile
    ffmpegfs.1.txt
    HISTORY.md
    INSTALL.md
    Makefile.am
    NEWS
    README.md
    sql/ffmpegfs.sql
    src/Makefile.am
    src/makehelp.sh
    src/scripts/hls.html
    src/scripts/videotag.php
    src/scripts/videotag.txt
    test/CMakeLists.txt
    test/funcs.sh
    test/Makefile.am
    test/tags/ffmpeg4.2/raven_e.opus.tag
    test/tags/ffmpeg4.2/raven_d.prores.tag
    test/tags/ffmpeg4.2/raven_e.mp3.tag
    test/tags/ffmpeg4.2/raven_d.ts.tag
    test/tags/ffmpeg4.2/raven_e.webm.tag
    test/tags/ffmpeg4.2/snowboard.webm.tag
    test/tags/ffmpeg4.2/raven_d.wav.tag
    test/tags/ffmpeg4.2/raven_e.ogg.tag
    test/tags/ffmpeg4.2/snowboard.prores.tag
    test/tags/ffmpeg4.2/raven_e.flac.tag
    test/tags/ffmpeg4.2/snowboard.mp4.tag
    test/tags/ffmpeg4.2/raven_e.mp4.tag
    test/tags/ffmpeg4.2/raven_e.ts.tag
    test/tags/ffmpeg4.2/raven.ts.tag
    test/tags/ffmpeg4.2/raven_d.aiff.tag
    test/tags/ffmpeg4.2/raven_d.flac.tag
    test/tags/ffmpeg4.2/raven_e.aiff.tag
    test/tags/ffmpeg4.2/raven_e.mov.tag
    test/tags/ffmpeg4.2/raven_d.opus.tag
    test/tags/ffmpeg4.2/raven_d.mp3.tag
    test/tags/ffmpeg4.2/raven_d.webm.tag
    test/tags/ffmpeg4.2/snowboard.ts.tag
    test/tags/ffmpeg4.2/snowboard.mov.tag
    test/tags/ffmpeg4.2/raven_e.wav.tag
    test/tags/ffmpeg4.2/raven_e.prores.tag
    test/tags/ffmpeg4.2/raven_d.mp4.tag
    test/tags/ffmpeg4.2/raven_d.alac.tag
    test/tags/ffmpeg4.2/raven_e.alac.tag
    test/tags/ffmpeg4.2/raven_d.mov.tag
    test/tags/ffmpeg4.2/raven_d.ogg.tag
    test/tags/ffmpeg4.2/obama.ts.tag
    test/tags/libav/raven_e.opus.tag
    test/tags/libav/raven_d.prores.tag
    test/tags/libav/raven_e.mp3.tag
    test/tags/libav/raven_d.ts.tag
    test/tags/libav/raven_e.webm.tag
    test/tags/libav/snowboard.webm.tag
    test/tags/libav/raven_d.wav.tag
    test/tags/libav/raven_e.ogg.tag
    test/tags/libav/snowboard.prores.tag
    test/tags/libav/snowboard.mp4.tag
    test/tags/libav/raven_e.mp4.tag
    test/tags/libav/raven_e.ts.tag
    test/tags/libav/raven.ts.tag
    test/tags/libav/raven_d.aiff.tag
    test/tags/libav/raven_e.aiff.tag
    test/tags/libav/raven_e.mov.tag
    test/tags/libav/raven_d.opus.tag
    test/tags/libav/raven_d.mp3.tag
    test/tags/libav/raven_d.webm.tag
    test/tags/libav/snowboard.ts.tag
    test/tags/libav/snowboard.mov.tag
    test/tags/libav/raven_e.wav.tag
    test/tags/libav/raven_e.prores.tag
    test/tags/libav/raven_d.mp4.tag
    test/tags/libav/raven_d.alac.tag
    test/tags/libav/raven_e.alac.tag
    test/tags/libav/raven_d.mov.tag
    test/tags/libav/raven_d.ogg.tag
    test/tags/libav/obama.ts.tag
    test/tags/ffmpegN/raven_e.opus.tag
    test/tags/ffmpegN/raven_d.prores.tag
    test/tags/ffmpegN/raven_e.mp3.tag
    test/tags/ffmpegN/raven_d.ts.tag
    test/tags/ffmpegN/raven_e.webm.tag
    test/tags/ffmpegN/snowboard.webm.tag
    test/tags/ffmpegN/raven_d.wav.tag
    test/tags/ffmpegN/raven_e.ogg.tag
    test/tags/ffmpegN/snowboard.prores.tag
    test/tags/ffmpegN/raven_e.flac.tag
    test/tags/ffmpegN/snowboard.mp4.tag
    test/tags/ffmpegN/raven_e.mp4.tag
    test/tags/ffmpegN/raven_e.ts.tag
    test/tags/ffmpegN/raven.ts.tag
    test/tags/ffmpegN/raven_d.aiff.tag
    test/tags/ffmpegN/raven_d.flac.tag
    test/tags/ffmpegN/raven_e.aiff.tag
    test/tags/ffmpegN/raven_e.mov.tag
    test/tags/ffmpegN/raven_d.opus.tag
    test/tags/ffmpegN/raven_d.mp3.tag
    test/tags/ffmpegN/raven_d.webm.tag
    test/tags/ffmpegN/snowboard.ts.tag
    test/tags/ffmpegN/snowboard.mov.tag
    test/tags/ffmpegN/raven_e.wav.tag
    test/tags/ffmpegN/raven_e.prores.tag
    test/tags/ffmpegN/raven_d.mp4.tag
    test/tags/ffmpegN/raven_d.alac.tag
    test/tags/ffmpegN/raven_e.alac.tag
    test/tags/ffmpegN/raven_d.mov.tag
    test/tags/ffmpegN/raven_d.ogg.tag
    test/tags/ffmpegN/obama.ts.tag
    test/tags/ffmpeg3.2+/raven_e.opus.tag
    test/tags/ffmpeg3.2+/raven_d.prores.tag
    test/tags/ffmpeg3.2+/raven_e.mp3.tag
    test/tags/ffmpeg3.2+/raven_d.ts.tag
    test/tags/ffmpeg3.2+/raven_e.webm.tag
    test/tags/ffmpeg3.2+/snowboard.webm.tag
    test/tags/ffmpeg3.2+/raven_d.wav.tag
    test/tags/ffmpeg3.2+/raven_e.ogg.tag
    test/tags/ffmpeg3.2+/snowboard.prores.tag
    test/tags/ffmpeg3.2+/raven_e.flac.tag
    test/tags/ffmpeg3.2+/snowboard.mp4.tag
    test/tags/ffmpeg3.2+/raven_e.mp4.tag
    test/tags/ffmpeg3.2+/raven_e.ts.tag
    test/tags/ffmpeg3.2+/raven.ts.tag
    test/tags/ffmpeg3.2+/raven_d.aiff.tag
    test/tags/ffmpeg3.2+/raven_d.flac.tag
    test/tags/ffmpeg3.2+/raven_e.aiff.tag
    test/tags/ffmpeg3.2+/raven_e.mov.tag
    test/tags/ffmpeg3.2+/raven_d.opus.tag
    test/tags/ffmpeg3.2+/raven_d.mp3.tag
    test/tags/ffmpeg3.2+/raven_d.webm.tag
    test/tags/ffmpeg3.2+/snowboard.ts.tag
    test/tags/ffmpeg3.2+/snowboard.mov.tag
    test/tags/ffmpeg3.2+/raven_e.wav.tag
    test/tags/ffmpeg3.2+/raven_e.prores.tag
    test/tags/ffmpeg3.2+/raven_d.mp4.tag
    test/tags/ffmpeg3.2+/raven_d.alac.tag
    test/tags/ffmpeg3.2+/raven_e.alac.tag
    test/tags/ffmpeg3.2+/raven_d.mov.tag
    test/tags/ffmpeg3.2+/raven_d.ogg.tag
    test/tags/ffmpeg3.2+/obama.ts.tag
    test/tags/ffmpeg3.2-/raven_e.opus.tag
    test/tags/ffmpeg3.2-/raven_d.prores.tag
    test/tags/ffmpeg3.2-/raven_e.mp3.tag
    test/tags/ffmpeg3.2-/raven_d.ts.tag
    test/tags/ffmpeg3.2-/raven_e.webm.tag
    test/tags/ffmpeg3.2-/snowboard.webm.tag
    test/tags/ffmpeg3.2-/raven_d.wav.tag
    test/tags/ffmpeg3.2-/raven_e.ogg.tag
    test/tags/ffmpeg3.2-/snowboard.prores.tag
    test/tags/ffmpeg3.2-/raven_e.flac.tag
    test/tags/ffmpeg3.2-/snowboard.mp4.tag
    test/tags/ffmpeg3.2-/raven_e.mp4.tag
    test/tags/ffmpeg3.2-/raven_e.ts.tag
    test/tags/ffmpeg3.2-/raven.ts.tag
    test/tags/ffmpeg3.2-/raven_d.aiff.tag
    test/tags/ffmpeg3.2-/raven_d.flac.tag
    test/tags/ffmpeg3.2-/raven_e.aiff.tag
    test/tags/ffmpeg3.2-/raven_e.mov.tag
    test/tags/ffmpeg3.2-/raven_d.opus.tag
    test/tags/ffmpeg3.2-/raven_d.mp3.tag
    test/tags/ffmpeg3.2-/raven_d.webm.tag
    test/tags/ffmpeg3.2-/snowboard.ts.tag
    test/tags/ffmpeg3.2-/snowboard.mov.tag
    test/tags/ffmpeg3.2-/raven_e.wav.tag
    test/tags/ffmpeg3.2-/raven_e.prores.tag
    test/tags/ffmpeg3.2-/raven_d.mp4.tag
    test/tags/ffmpeg3.2-/raven_d.alac.tag
    test/tags/ffmpeg3.2-/raven_e.alac.tag
    test/tags/ffmpeg3.2-/raven_d.mov.tag
    test/tags/ffmpeg3.2-/raven_d.ogg.tag
    test/tags/ffmpeg3.2-/obama.ts.tag
    test/tags/ffmpeg4.0/raven_e.opus.tag
    test/tags/ffmpeg4.0/raven_d.prores.tag
    test/tags/ffmpeg4.0/raven_e.mp3.tag
    test/tags/ffmpeg4.0/raven_d.ts.tag
    test/tags/ffmpeg4.0/raven_e.webm.tag
    test/tags/ffmpeg4.0/snowboard.webm.tag
    test/tags/ffmpeg4.0/raven_d.wav.tag
    test/tags/ffmpeg4.0/raven_e.ogg.tag
    test/tags/ffmpeg4.0/snowboard.prores.tag
    test/tags/ffmpeg4.0/raven_e.flac.tag
    test/tags/ffmpeg4.0/snowboard.mp4.tag
    test/tags/ffmpeg4.0/raven_e.mp4.tag
    test/tags/ffmpeg4.0/raven_e.ts.tag
    test/tags/ffmpeg4.0/raven.ts.tag
    test/tags/ffmpeg4.0/raven_d.aiff.tag
    test/tags/ffmpeg4.0/raven_d.flac.tag
    test/tags/ffmpeg4.0/raven_e.aiff.tag
    test/tags/ffmpeg4.0/raven_e.mov.tag
    test/tags/ffmpeg4.0/raven_d.opus.tag
    test/tags/ffmpeg4.0/raven_d.mp3.tag
    test/tags/ffmpeg4.0/raven_d.webm.tag
    test/tags/ffmpeg4.0/snowboard.ts.tag
    test/tags/ffmpeg4.0/snowboard.mov.tag
    test/tags/ffmpeg4.0/raven_e.wav.tag
    test/tags/ffmpeg4.0/raven_e.prores.tag
    test/tags/ffmpeg4.0/raven_d.mp4.tag
    test/tags/ffmpeg4.0/raven_d.alac.tag
    test/tags/ffmpeg4.0/raven_e.alac.tag
    test/tags/ffmpeg4.0/raven_d.mov.tag
    test/tags/ffmpeg4.0/raven_d.ogg.tag
    test/tags/ffmpeg4.0/obama.ts.tag
    test/tags/ffmpeg4.3/raven_e.opus.tag
    test/tags/ffmpeg4.3/raven_d.prores.tag
    test/tags/ffmpeg4.3/raven_e.mp3.tag
    test/tags/ffmpeg4.3/raven_d.ts.tag
    test/tags/ffmpeg4.3/raven_e.webm.tag
    test/tags/ffmpeg4.3/snowboard.webm.tag
    test/tags/ffmpeg4.3/raven_d.wav.tag
    test/tags/ffmpeg4.3/raven_e.ogg.tag
    test/tags/ffmpeg4.3/snowboard.prores.tag
    test/tags/ffmpeg4.3/raven_e.flac.tag
    test/tags/ffmpeg4.3/snowboard.mp4.tag
    test/tags/ffmpeg4.3/raven_e.mp4.tag
    test/tags/ffmpeg4.3/raven_e.ts.tag
    test/tags/ffmpeg4.3/raven.ts.tag
    test/tags/ffmpeg4.3/raven_d.aiff.tag
    test/tags/ffmpeg4.3/raven_d.flac.tag
    test/tags/ffmpeg4.3/raven_e.aiff.tag
    test/tags/ffmpeg4.3/raven_e.mov.tag
    test/tags/ffmpeg4.3/raven_d.opus.tag
    test/tags/ffmpeg4.3/raven_d.mp3.tag
    test/tags/ffmpeg4.3/raven_d.webm.tag
    test/tags/ffmpeg4.3/snowboard.ts.tag
    test/tags/ffmpeg4.3/snowboard.mov.tag
    test/tags/ffmpeg4.3/raven_e.wav.tag
    test/tags/ffmpeg4.3/raven_e.prores.tag
    test/tags/ffmpeg4.3/raven_d.mp4.tag
    test/tags/ffmpeg4.3/raven_d.alac.tag
    test/tags/ffmpeg4.3/raven_e.alac.tag
    test/tags/ffmpeg4.3/raven_d.mov.tag
    test/tags/ffmpeg4.3/raven_d.ogg.tag
    test/tags/ffmpeg4.3/obama.ts.tag
    test/test_audio
    test/test_audio_aiff
    test/test_audio_alac
    test/test_audio_flac
    test/test_audio_mov
    test/test_audio_mp3
    test/test_audio_mp4
    test/test_audio_ogg
    test/test_audio_opus
    test/test_audio_prores
    test/test_audio_ts
    test/test_audio_wav
    test/test_audio_webm
    test/test_filecount_hls
    test/test_filenames
    test/test_filenames_aiff
    test/test_filenames_alac
    test/test_filenames_flac
    test/test_filenames_hls
    test/test_filenames_mov
    test/test_filenames_mp3
    test/test_filenames_mp4
    test/test_filenames_ogg
    test/test_filenames_opus
    test/test_filenames_prores
    test/test_filenames_ts
    test/test_filenames_wav
    test/test_filenames_webm
    test/test_filesize
    test/test_filesize_aiff
    test/test_filesize_alac
    test/test_filesize_flac
    test/test_filesize_hls
    test/test_filesize_mov
    test/test_filesize_mp3
    test/test_filesize_mp4
    test/test_filesize_ogg
    test/test_filesize_opus
    test/test_filesize_prores
    test/test_filesize_ts
    test/test_filesize_video
    test/test_filesize_video_mov
    test/test_filesize_video_mp4
    test/test_filesize_video_prores
    test/test_filesize_video_ts
    test/test_filesize_video_webm
    test/test_filesize_wav
    test/test_filesize_webm
    test/test_frameset
    test/test_frameset_bmp
    test/test_frameset_jpg
    test/test_frameset_png
    test/test_picture
    test/test_picture_aiff
    test/test_picture_alac
    test/test_picture_flac
    test/test_picture_mov
    test/test_picture_mp3
    test/test_picture_mp4
    test/test_picture_ogg
    test/test_picture_opus
    test/test_picture_prores
    test/test_picture_ts
    test/test_picture_wav
    test/test_picture_webm
    test/test_tags
    test/test_tags_aiff
    test/test_tags_alac
    test/test_tags_flac
    test/test_tags_mov
    test/test_tags_mp3
    test/test_tags_mp4
    test/test_tags_ogg
    test/test_tags_opus
    test/test_tags_prores
    test/test_tags_ts
    test/test_tags_wav
    test/test_tags_webm
    test/test_video_hw
    test/test_video_hw_dec_cuda
    test/test_video_hw_dec_mmal
    test/test_video_hw_dec_v4l2m2m
    test/test_video_hw_dec_vaapi
    test/test_video_hw_enc_cuda
    test/test_video_hw_enc_omx
    test/test_video_hw_enc_v4l2m2m
    test/test_video_hw_enc_vaapi
    TODO
    Doxyfile
)

# The binary
add_executable(${PROJECT_NAME} ${OPTIONS} ${SOURCES} ${HEADERS})
set_target_properties(${PROJECT_NAME}
  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src"
)

# manpages
execute_process(COMMAND "date +'%B %Y'"
                RESULT_VARIABLE REVDATE
)

# make manpage-text
add_custom_command(
     COMMENT "Building text version of manpages"
     DEPENDS ${CMAKE_SOURCE_DIR}/ffmpegfs.1.txt
     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
     COMMAND a2x -a revnumber=\"${PACKAGE_VERSION}\" -a revdate=\"${REVDATE}\" -f text ffmpegfs.1.txt
     OUTPUT ${CMAKE_BINARY_DIR}/ffmpegfs.1.text
 )
 add_custom_target(manpage-text DEPENDS ${CMAKE_BINARY_DIR}/ffmpegfs.1.text)
 add_dependencies(${PROJECT_NAME} manpage-text)

# make manpage-header
#sed in makefiles is a menace so we use a helper script
add_custom_command(
     COMMENT "Building header version of manpages"
     DEPENDS ${CMAKE_BINARY_DIR}/ffmpegfs.1.text
     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src/
     COMMAND ${CMAKE_SOURCE_DIR}/src/makehelp.sh \"${CMAKE_BINARY_DIR}/src/ffmpegfshelp.h\"
     OUTPUT ${CMAKE_BINARY_DIR}/src/ffmpegfshelp.h
)
add_custom_target(manpage-header DEPENDS ${CMAKE_BINARY_DIR}/src/ffmpegfshelp.h)
add_dependencies(${PROJECT_NAME} manpage-header)

# make manpage
add_custom_command(
     COMMENT "Building manpages"
     DEPENDS ${CMAKE_SOURCE_DIR}/ffmpegfs.1.txt
     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
     COMMAND a2x -a revnumber=\"${PACKAGE_VERSION}\" -a revdate=\"${REVDATE}\" -f manpage --xsltproc-opts=\"-param ulink.show 1\" ffmpegfs.1.txt
     OUTPUT ${CMAKE_BINARY_DIR}/ffmpegfs.1
)
add_custom_target(manpage DEPENDS ${CMAKE_BINARY_DIR}/ffmpegfs.1)
add_dependencies(${PROJECT_NAME} manpage)

# make help-pdf
add_custom_command(
     COMMENT "Building PDF help"
     DEPENDS ${CMAKE_SOURCE_DIR}/ffmpegfs.1.txt
     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
     COMMAND a2x -a revnumber=\"${PACKAGE_VERSION}\" -a revdate=\"${REVDATE}\" -f pdf --fop --xsltproc-opts=\"-param ulink.show 1\" ffmpegfs.1.txt
     OUTPUT ${CMAKE_BINARY_DIR}/ffmpegfs.1.pdf
)
add_custom_target(help-pdf DEPENDS ${CMAKE_BINARY_DIR}/ffmpegfs.1.pdf)
#add_dependencies(${PROJECT_NAME} help-pdf)

# make help-html
add_custom_command(
     COMMENT "Building HTML help"
     DEPENDS ${CMAKE_SOURCE_DIR}/ffmpegfs.1.txt
     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
     COMMAND a2x -a revnumber=\"${PACKAGE_VERSION}\" -a revdate=\"${REVDATE}\" -f htmlhelp ffmpegfs.1.txt
     COMMAND mv ffmpegfs.1.htmlhelp/index.html ffmpegfs.1.htmlhelp/manpages.html
     COMMAND echo a2x -a revnumber=\"${PACKAGE_VERSION}\" -a revdate=\"${REVDATE}\" -f htmlhelp ffmpegfs.1.txt
     OUTPUT ${CMAKE_BINARY_DIR}/ffmpegfs.1.htmlhelp/manpages.html
)
add_custom_target(help-html DEPENDS ${CMAKE_BINARY_DIR}/ffmpegfs.1.htmlhelp/manpages.html)
#add_dependencies(${PROJECT_NAME} help-html)

# make doxy
add_custom_command(
     COMMENT "Building Doxygen"
     DEPENDS ${CMAKE_BINARY_DIR}/Doxyfile
     DEPENDS ${CMAKE_BINARY_DIR}/doxyfile.inc
     DEPENDS ${CMAKE_BINARY_DIR}/ffmpegfs.1.htmlhelp/manpages.html
     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
     COMMAND doxygen Doxyfile
     OUTPUT ${CMAKE_BINARY_DIR}/doxygen/html/index.html
)
add_custom_target(doxy DEPENDS ${CMAKE_BINARY_DIR}/doxygen/html/index.html)
#add_dependencies(${PROJECT_NAME} doxy)

# make doxyfile-inc
add_custom_command(
     COMMENT "Building doxyfile.inc"
     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
     DEPENDS ${CMAKE_SOURCE_DIR}/Doxyfile
     COMMAND @echo "PROJECT_NAME           = \"${PROJECT_DESCRIPTION}\"" > ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "OUTPUT_DIRECTORY       = doxygen" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "PREDEFINED             = USE_LIBBLURAY" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "PREDEFINED             += USE_LIBDVD" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "PREDEFINED             += USE_LIBVCD" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "INPUT                  = src test" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "FILE_PATTERNS          = *.h *.cc" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "EXCLUDE                = src/config.h" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "EXCLUDE                += src/ffmpegfshelp.h" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "EXCLUDE                += doxyfile.inc" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "HTML_EXTRA_FILES       += ffmpegfs.1.htmlhelp/manpages.html" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "HTML_EXTRA_FILES       += ffmpegfs.1.htmlhelp/docbook-xsl.css" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "INPUT                  += README.md" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "INPUT	           += INSTALL.md" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "INPUT                  += HISTORY.md" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "EXAMPLE_PATH           += NEWS" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "EXAMPLE_PATH           += TODO" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "EXAMPLE_PATH           += COPYING" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "EXAMPLE_PATH           += COPYING.DOC" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "EXAMPLE_PATH           += COPYING.CC0" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "RECURSIVE              = YES" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "SOURCE_BROWSER         = YES" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "DISTRIBUTE_GROUP_DOC   = YES" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "REFERENCED_BY_RELATION = YES" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "REFERENCES_RELATION    = YES" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "EXTRACT_STATIC         = YES" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "EXTRACT_PRIVATE        = YES" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     COMMAND @echo "PROJECT_NUMBER         = ${VERSION}" >> ${CMAKE_BINARY_DIR}/doxyfile.inc
     OUTPUT ${CMAKE_BINARY_DIR}/doxyfile.inc
)
add_custom_target(doxyfile-inc DEPENDS ${CMAKE_BINARY_DIR}/doxyfile.inc)
add_dependencies(doxy doxyfile-inc)
add_dependencies(doxy help-html)

# Additional headers and libraries
target_include_directories(${PROJECT_NAME} BEFORE PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/src/ ${CMAKE_CURRENT_SOURCE_DIR}/src/ /usr/include/fuse/ /usr/include/chardet/)
# Debug library
#target_include_directories(${PROJECT_NAME} BEFORE PUBLIC $ENV{HOME}/dev/builds/ffmpeg/include)
#target_link_directories(${PROJECT_NAME} BEFORE PUBLIC $ENV{HOME}/dev/builds/ffmpeg/lib)

set(FLAGS "-Wextra -Wconversion -Wsign-compare -Wsign-conversion -Wpedantic -Wall")

set(CMAKE_C_FLAGS_DEBUG "-std=c99 ${FLAGS} -D_DEBUG -ggdb")
set(CMAKE_C_FLAGS_MINSIZEREL "-std=c99 ${FLAGS} -DNDEBUG -Osize")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-std=c99 ${FLAGS} -DNDEBUG -O0 -g")
set(CMAKE_C_FLAGS_RELEASE "-std=c99 ${FLAGS} -DNDEBUG -Ofast")

set(CMAKE_CXX_FLAGS_DEBUG "-std=c++11 ${FLAGS} -ggdb")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-std=c++11 ${FLAGS} -DNDEBUG -Osize")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-std=c++11 ${FLAGS} -DNDEBUG -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-std=c++11 ${FLAGS} -DNDEBUG -Ofast")

target_compile_definitions(${PROJECT_NAME} PUBLIC
    -D_FILE_OFFSET_BITS=64
    -DHAVE_CONFIG_H
    -D_GNU_SOURCE
    -DUSE_LIBVCD=1
)

IF (LIBDVDREAD_FOUND)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        -DUSE_LIBDVD=1
    )
ENDIF (LIBDVDREAD_FOUND)
IF (LIBBLURAY_FOUND)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        -DUSE_LIBBLURAY=1
    )
ENDIF (LIBBLURAY_FOUND)
IF (LIBSWRESAMPLE_FOUND)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        -DUSE_LIBSWRESAMPLE=1
    )
ENDIF (LIBSWRESAMPLE_FOUND)
IF (LIBAVRESAMPLE_FOUND)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        -DUSE_LIBAVRESAMPLE=1
    )
ENDIF (LIBAVRESAMPLE_FOUND)

target_link_libraries(${PROJECT_NAME}
    pthread
    fuse
    avfilter
    avformat
    avcodec
    avutil
    #avresample
    #avdevice
    swscale
    swresample
    chardet
    postproc
    sqlite3
    rt
    dvdnav
    dvdread
    bluray
    cue
)

# Test suite
ENABLE_TESTING()
add_subdirectory(test)

# cppcheck
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cppcheck.log"
  COMMAND cppcheck --force --enable=all --inconclusive --library=posix --output-file="${CMAKE_CURRENT_BINARY_DIR}/cppcheck.log" --suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/cppcheck-suppressions.txt -I "${CMAKE_CURRENT_SOURCE_DIR}/src/" -I "${CMAKE_CURRENT_SOURCE_DIR}/src/vcd/" -DLIBAVUTIL_VERSION_MICRO -DUSE_LIBBLURAY -DUSE_LIBDVD -DUSE_LIBVCD -DHAVE_CONFIG_H -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DUSE_LIBSWRESAMPLE -UAV_CODEC_CAP_TRUNCATED -DAV_CODEC_FLAG_TRUNCATED=1 -D__GNUC_ -DPACKAGE_NAME="XXX" -DFFMPEFS_VERSION="V0.0" -U__clang_version__ -UPRESCAN_FRAMES -DEXTRA_VERSION="" "${CMAKE_CURRENT_SOURCE_DIR}/src/" "${CMAKE_CURRENT_SOURCE_DIR}/src/vcd/"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Running cppcheck"
  VERBATIM
)
add_custom_target(cppcheck DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/cppcheck.log")
#add_dependencies(${PROJECT_NAME} cppcheck)

# Package
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/configure"
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/autogen.sh"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/autogen.sh"
  COMMENT "Making configure"
  VERBATIM
)
add_custom_target(make_configure DEPENDS configure)
add_dependencies(${PROJECT_NAME} make_configure)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ChangeLog"
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/changelog.sh"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/changelog.sh"
  COMMENT "Creating ChangeLog file from git log"
  VERBATIM
)
add_custom_target(changelog DEPENDS ChangeLog)
add_dependencies(${PROJECT_NAME} changelog)

foreach ( file ${SOURCES} ${HEADERS} test/fpcompare.c test/metadata.c )
    get_filename_component( dir ${file} DIRECTORY )
    install( FILES ${file} DESTINATION ./${dir} )
endforeach()
install(DIRECTORY test/srcdir/ DESTINATION test/srcdir/)
install(FILES configure ChangeLog DESTINATION .)

set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_NAME          ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION       ${PACKAGE_VERSION})
set(CPACK_PACKAGE_VENDOR        ${PROJECT_VENDOR})
set(CPACK_PACKAGE_CONTACT       ${PROJECT_CONTACT})
#set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_RESOURCE_FILE_README  "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
#set(CPACK_OUTPUT_FILE_PREFIX    "${CMAKE_CURRENT_BINARY_DIR}/package")
set(CPACK_PACKAGE_FILE_NAME     ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION})

include(CPack)


