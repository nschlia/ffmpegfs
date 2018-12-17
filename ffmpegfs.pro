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
    src/config.h.in \
    test/tags/obama.aiff.ffmpeg3.2-.tag \
    test/tags/obama.aiff.ffmpeg3.2+.tag \
    test/tags/obama.aiff.libav.tag \
    test/tags/obama.mov.ffmpeg3.2-.tag \
    test/tags/obama.mov.ffmpeg3.2+.tag \
    test/tags/obama.mov.libav.tag \
    test/tags/obama.mp3.ffmpeg3.2-.tag \
    test/tags/obama.mp3.ffmpeg3.2+.tag \
    test/tags/obama.mp3.libav.tag \
    test/tags/obama.mp4.ffmpeg3.2-.tag \
    test/tags/obama.mp4.ffmpeg3.2+.tag \
    test/tags/obama.mp4.libav.tag \
    test/tags/obama.ogg.ffmpeg3.2-.tag \
    test/tags/obama.ogg.ffmpeg3.2+.tag \
    test/tags/obama.ogg.libav.tag \
    test/tags/obama.opus.ffmpeg3.2-.tag \
    test/tags/obama.opus.ffmpeg3.2+.tag \
    test/tags/obama.opus.libav.tag \
    test/tags/obama.prores.ffmpeg3.2-.tag \
    test/tags/obama.prores.ffmpeg3.2+.tag \
    test/tags/obama.prores.libav.tag \
    test/tags/obama.wav.ffmpeg3.2-.tag \
    test/tags/obama.wav.ffmpeg3.2+.tag \
    test/tags/obama.wav.libav.tag \
    test/tags/obama.webm.ffmpeg3.2-.tag \
    test/tags/obama.webm.ffmpeg3.2+.tag \
    test/tags/obama.webm.libav.tag \
    test/tags/raven.aiff.ffmpeg3.2-.tag \
    test/tags/raven.aiff.ffmpeg3.2+.tag \
    test/tags/raven.aiff.libav.tag \
    test/tags/raven.mov.ffmpeg3.2-.tag \
    test/tags/raven.mov.ffmpeg3.2+.tag \
    test/tags/raven.mov.libav.tag \
    test/tags/raven.mp3.ffmpeg3.2-.tag \
    test/tags/raven.mp3.ffmpeg3.2+.tag \
    test/tags/raven.mp3.libav.tag \
    test/tags/raven.mp4.ffmpeg3.2-.tag \
    test/tags/raven.mp4.ffmpeg3.2+.tag \
    test/tags/raven.mp4.libav.tag \
    test/tags/raven.ogg.ffmpeg3.2-.tag \
    test/tags/raven.ogg.ffmpeg3.2+.tag \
    test/tags/raven.ogg.libav.tag \
    test/tags/raven.opus.ffmpeg3.2-.tag \
    test/tags/raven.opus.ffmpeg3.2+.tag \
    test/tags/raven.opus.libav.tag \
    test/tags/raven.prores.ffmpeg3.2-.tag \
    test/tags/raven.prores.ffmpeg3.2+.tag \
    test/tags/raven.prores.libav.tag \
    test/tags/raven.wav.ffmpeg3.2-.tag \
    test/tags/raven.wav.ffmpeg3.2+.tag \
    test/tags/raven.wav.libav.tag \
    test/tags/raven.webm.ffmpeg3.2-.tag \
    test/tags/raven.webm.ffmpeg3.2+.tag \
    test/tags/raven.webm.libav.tag \
    test/addtest \
    test/funcs.sh \
    test/test_audio \
    test/test_audio_aiff \
    test/test_audio_mov \
    test/test_audio_mp3 \
    test/test_audio_mp4 \
    test/test_audio_ogg \
    test/test_audio_opus \
    test/test_audio_prores \
    test/test_audio_wav \
    test/test_audio_webm \
    test/test_filenames \
    test/test_filenames_aiff \
    test/test_filenames_mov \
    test/test_filenames_mp3 \
    test/test_filenames_mp4 \
    test/test_filenames_ogg \
    test/test_filenames_opus \
    test/test_filenames_prores \
    test/test_filenames_wav \
    test/test_filenames_webm \
    test/test_filesize \
    test/test_filesize_aiff \
    test/test_filesize_mov \
    test/test_filesize_mp3 \
    test/test_filesize_mp4 \
    test/test_filesize_ogg \
    test/test_filesize_opus \
    test/test_filesize_prores \
    test/test_filesize_wav \
    test/test_filesize_webm \
    test/test_picture \
    test/test_picture_aiff \
    test/test_picture_mov \
    test/test_picture_mp3 \
    test/test_picture_mp4 \
    test/test_picture_ogg \
    test/test_picture_opus \
    test/test_picture_prores \
    test/test_picture_wav \
    test/test_picture_webm \
    test/test_tags \
    test/test_tags_aiff \
    test/test_tags_mov \
    test/test_tags_mp3 \
    test/test_tags_mp4 \
    test/test_tags_ogg \
    test/test_tags_opus \
    test/test_tags_prores \
    test/test_tags_wav \
    test/test_tags_webm \
    test/Makefile.am
