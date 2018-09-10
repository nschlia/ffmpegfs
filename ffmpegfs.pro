TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    src/buffer.cc \
    src/coders.cc \
    src/ffmpeg_transcoder.cc \
    src/ffmpeg_utils.cc \
    src/fuseops.cc \
    src/logging.cc \
    src/ffmpegfs.c \
    src/transcode.cc \
    src/cache.cc \
    src/cache_entry.cc \
    src/cache_maintenance.cc \
    src/ffmpeg_base.cc \
    src/blurayio.cc \
    src/blurayparser.cc \
    src/diskio.cc \
    src/dvdio.cc \
    src/dvdparser.cc \
    src/fileio.cc \
    src/vcdio.cc \
    src/vcdparser.cc \
    src/vcd/vcdchapter.cc \
    src/vcd/vcdentries.cc \
    src/vcd/vcdinfo.cc \
    src/vcd/vcdutils.cc \
    src/ffmpeg_profiles.cc

HEADERS += \
    src/buffer.h \
    src/coders.h \
    src/ffmpeg_transcoder.h \
    src/ffmpeg_utils.h \
    src/ffmpeg_compat.h \
    src/logging.h \
    src/ffmpegfs.h \
    src/transcode.h \
    src/cache.h \
    src/cache_entry.h \
    src/id3v1tag.h \
    src/cache_maintenance.h \
    src/ffmpeg_base.h \
    src/wave.h \
    configure.ac \
    src/fileio.h \
    src/diskio.h \
    src/dvdio.h \
    src/dvdparser.h \
    src/vcdio.h \
    src/vcdparser.h \
    src/blurayio.h \
    src/blurayparser.h \
    src/vcd/vcdchapter.h \
    src/vcd/vcdentries.h \
    src/vcd/vcdinfo.h \
    src/vcd/vcdutils.h

DEFINES+=_DEBUGencoder_list
DEFINES+=PACKAGE_NAME='"\\\"FFMPEGFS\\\""' HOST_OS='"\\\"My OS\\\""' CONFIGURE_ARGS='"\\\"\\\""' PACKAGE_TARNAME='"\\\"ffmpegfs"'\\\" PACKAGE_VERSION='"\\\"1.3"'\\\" PACKAGE_STRING='"\\\"FFMPEGFS\ 1.3"'\\\" PACKAGE_BUGREPORT='"\\\""'\\\" PACKAGE_URL='"\\\""'\\\" PACKAGE='"\\\"ffmpegfs"'\\\" VERSION='"\\\"0.91"'\\\"
DEFINES+=STDC_HEADERS=1 HAVE_SYS_TYPES_H=1 HAVE_SYS_STAT_H=1 HAVE_STDLIB_H=1 HAVE_STRING_H=1 HAVE_MEMORY_H=1 HAVE_STRINGS_H=1 HAVE_INTTYPES_H=1 HAVE_STDINT_H=1 HAVE_UNISTD_H=1 HAVE_SQLITE_CACHEFLUSH=1 HAVE_SQLITE_ERRSTR=1 SIZEOF_INT=4 _POSIX_C_SOURCE=200809L _FILE_OFFSET_BITS=64 FFMPEGFS_FORMAT_TIME_T=\\\"ld\\\" FFMPEGFS_FORMAT_PTHREAD_T=\\\"lx\\\" USE_LIBSWRESAMPLE
#DEFINES+=HAVE_SQLITE_EXPANDED_SQL=1
DEFINES+=HAVE_FFMPEG=1 _GNU_SOURCE
DEFINES+=USE_LIBDVD
DEFINES+=USE_LIBVCD
DEFINES+=USE_LIBBLURAY
INCLUDEPATH+=$$PWD/src /usr/include/fuse $$PWD/lib
LIBS+=-lfuse -pthread
LIBS+=-lavformat -lavcodec -lavutil
LIBS+=-lswscale
LIBS+=-lswresample
LIBS+=-lavresample
LIBS+=-lavfilter
LIBS+=-lpostproc
#-lavdevice
LIBS+=-lsqlite3
LIBS+=-lrt
LIBS+=-ldvdnav -ldvdread
LIBS+=-lbluray

QMAKE_CFLAGS += -std=c99 -Wall -Wextra -Wconversion
QMAKE_CXXFLAGS += -std=c++11 -Wall -Wextra -Wconversion
#QMAKE_CFLAGS += -O2 -MT -MD -MP -MF
#QMAKE_CXXFLAGS += -O2 -MT -MD -MP -MF

INCLUDEPATH += /home/norbert/dev/ffmpeg/include
LIBS += \
        -L/home/norbert/dev/ffmpeg/lib \
        -L/home/norbert/dev/ffmpeg/bin

#INCLUDEPATH += "/home/dev special/dev/ffmpeg/include"
#LIBS += \
#        -L"/home/schlia/dev special/ffmpeg/lib" \
#        -L"/home/schlia/dev special/ffmpeg/bin"

DISTFILES += \
    COPYING.DOC \
    README.md \
    NEWS \
    COPYING \
    INSTALL.md \
    README.md \
    autogen.sh \
    buildloc \
    configure.ac \
    ffmpegfs.1.txt \
    test/test_audio \
    test/test_filenames \
    test/test_filesize \
    test/test_picture \
    test/test_tags \
    test/funcs.sh \
    TODO \
    src/scripts/videotag.php \
    src/scripts/videotag.txt \
    Makefile.am \
    src/Makefile.am \
    test/Makefile.am
