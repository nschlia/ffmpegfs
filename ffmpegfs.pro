TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    src/buffer.cc \
    src/ffmpeg_transcoder.cc \
    src/ffmpeg_utils.cc \
    src/fuseops.cc \
    src/logging.cc \
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
    src/ffmpeg_profiles.cc \
    src/ffmpegfs.cc

HEADERS += \
    src/buffer.h \
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
    src/vcd/vcdutils.h \
    src/config.h

DEFINES+=_DEBUG
DEFINES+=HAVE_CONFIG_H _FILE_OFFSET_BITS=64 _GNU_SOURCE
DEFINES+=USE_LIBSWRESAMPLE
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
    test/Makefile.am \
    src/config.h.in
