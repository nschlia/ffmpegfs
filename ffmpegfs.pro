TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    src/blurayio.cc \
    src/blurayparser.cc \
    src/buffer.cc \
    src/cache.cc \
    src/cache_entry.cc \
    src/cache_maintenance.cc \
    src/diskio.cc \
    src/dvdio.cc \
    src/dvdparser.cc \
    src/ffmpeg_base.cc \
    src/ffmpegfs.cc \
    src/ffmpeg_profiles.cc \
    src/ffmpeg_transcoder.cc \
    src/ffmpeg_utils.cc \
    src/fileio.cc \
    src/fuseops.cc \
    src/logging.cc \
    src/transcode.cc \
    src/vcdio.cc \
    src/vcdparser.cc \
    src/vcd/vcdchapter.cc \
    src/vcd/vcdentries.cc \
    src/vcd/vcdinfo.cc \
    src/vcd/vcdutils.cc \
    src/thread_pool.cc

HEADERS += \
    src/blurayio.h \
    src/blurayparser.h \
    src/buffer.h \
    src/cache_entry.h \
    src/cache.h \
    src/cache_maintenance.h \
    src/config.h \
    src/diskio.h \
    src/dvdio.h \
    src/dvdparser.h \
    src/ffmpeg_base.h \
    src/ffmpeg_compat.h \
    src/ffmpegfs.h \
    src/ffmpeg_profiles.h \
    src/ffmpeg_transcoder.h \
    src/ffmpeg_utils.h \
    src/fileio.h \
    src/id3v1tag.h \
    src/logging.h \
    src/transcode.h \
    src/vcdio.h \
    src/vcdparser.h \
    src/vcd/vcdchapter.h \
    src/vcd/vcdentries.h \
    src/vcd/vcdinfo.h \
    src/vcd/vcdutils.h \
    src/wave.h \
    src/thread_pool.h

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
#LIBS+=-lavresample
LIBS+=-lavfilter
LIBS+=-lpostproc
#-lavdevice
LIBS+=-lsqlite3
LIBS+=-lrt
LIBS+=-ldvdnav -ldvdread
LIBS+=-lbluray

# -Uno-old-style-cast
QMAKE_CFLAGS += -std=c99  -Wextra -Wconversion -Wsign-compare -Wsign-conversion -Wpedantic -Wall
QMAKE_CXXFLAGS += -std=c++11 -Wextra -Wconversion -Wsign-compare -Wsign-conversion -Wpedantic -Wall
#QMAKE_CFLAGS += -O2 -MT -MD -MP -MF
#QMAKE_CXXFLAGS += -O2 -MT -MD -MP -MF

LIBS += -L$$(HOME)/dev/builds/ffmpeg/lib
INCLUDEPATH += $$(HOME)/dev/builds/ffmpeg/include

#INCLUDEPATH += "${HONE}/dev/builds/ffmpeg/include"
#LIBS += \
#        -L"${HOME}/builds/ffmpeg/lib" \
#        -L"${HOME}/builds/ffmpeg/bin"

DISTFILES += \
    autogen.sh \
    buildloc \
    configure.ac \
    COPYING \
    COPYING.DOC \
    ffmpegfs.1.txt \
    INSTALL.md \
    Makefile.am \
    NEWS \
    README.md \
    sql/ffmpegfs.sql \
    src/config.h.in \
    src/Makefile.am \
    src/makehelp.sh \
    src/scripts/videotag.php \
    src/scripts/videotag.txt \
    test/addtest \
    test/funcs.sh \
    test/Makefile.am \
    test/tags/raven_e.aiff.ffmpeg3.2-.tag \
    test/tags/raven_e.aiff.ffmpeg3.2+.tag \
    test/tags/raven_e.aiff.libav.tag \
    test/tags/raven_e.mov.ffmpeg3.2-.tag \
    test/tags/raven_e.mov.ffmpeg3.2+.tag \
    test/tags/raven_e.mov.libav.tag \
    test/tags/raven_e.mp3.ffmpeg3.2-.tag \
    test/tags/raven_e.mp3.ffmpeg3.2+.tag \
    test/tags/raven_e.mp3.libav.tag \
    test/tags/raven_e.mp4.ffmpeg3.2-.tag \
    test/tags/raven_e.mp4.ffmpeg3.2+.tag \
    test/tags/raven_e.mp4.libav.tag \
    test/tags/raven_e.ogg.ffmpeg3.2-.tag \
    test/tags/raven_e.ogg.ffmpeg3.2+.tag \
    test/tags/raven_e.ogg.libav.tag \
    test/tags/raven_e.opus.ffmpeg3.2-.tag \
    test/tags/raven_e.opus.ffmpeg3.2+.tag \
    test/tags/raven_e.opus.libav.tag \
    test/tags/raven_e.prores.ffmpeg3.2-.tag \
    test/tags/raven_e.prores.ffmpeg3.2+.tag \
    test/tags/raven_e.prores.libav.tag \
    test/tags/raven_e.wav.ffmpeg3.2-.tag \
    test/tags/raven_e.wav.ffmpeg3.2+.tag \
    test/tags/raven_e.wav.libav.tag \
    test/tags/raven_e.webm.ffmpeg3.2-.tag \
    test/tags/raven_e.webm.ffmpeg3.2+.tag \
    test/tags/raven_e.webm.libav.tag \
    test/tags/raven_d.aiff.ffmpeg3.2-.tag \
    test/tags/raven_d.aiff.ffmpeg3.2+.tag \
    test/tags/raven_d.aiff.libav.tag \
    test/tags/raven_d.mov.ffmpeg3.2-.tag \
    test/tags/raven_d.mov.ffmpeg3.2+.tag \
    test/tags/raven_d.mov.libav.tag \
    test/tags/raven_d.mp3.ffmpeg3.2-.tag \
    test/tags/raven_d.mp3.ffmpeg3.2+.tag \
    test/tags/raven_d.mp3.libav.tag \
    test/tags/raven_d.mp4.ffmpeg3.2-.tag \
    test/tags/raven_d.mp4.ffmpeg3.2+.tag \
    test/tags/raven_d.mp4.libav.tag \
    test/tags/raven_d.ogg.ffmpeg3.2-.tag \
    test/tags/raven_d.ogg.ffmpeg3.2+.tag \
    test/tags/raven_d.ogg.libav.tag \
    test/tags/raven_d.opus.ffmpeg3.2-.tag \
    test/tags/raven_d.opus.ffmpeg3.2+.tag \
    test/tags/raven_d.opus.libav.tag \
    test/tags/raven_d.prores.ffmpeg3.2-.tag \
    test/tags/raven_d.prores.ffmpeg3.2+.tag \
    test/tags/raven_d.prores.libav.tag \
    test/tags/raven_d.wav.ffmpeg3.2-.tag \
    test/tags/raven_d.wav.ffmpeg3.2+.tag \
    test/tags/raven_d.wav.libav.tag \
    test/tags/raven_d.webm.ffmpeg3.2-.tag \
    test/tags/raven_d.webm.ffmpeg3.2+.tag \
    test/tags/raven_d.webm.libav.tag \
    test/test_audio \
    test/test_audio_aiff \
    test/test_audio_alac \
    test/test_audio_mov \
    test/test_audio_mp3 \
    test/test_audio_mp4 \
    test/test_audio_ogg \
    test/test_audio_opus \
    test/test_audio_prores \
    test/test_audio_ts \
    test/test_audio_wav \
    test/test_audio_webm \
    test/test_filecount_hls \
    test/test_filenames \
    test/test_filenames_aiff \
    test/test_filenames_alac \
    test/test_filenames_mov \
    test/test_filenames_mp3 \
    test/test_filenames_mp4 \
    test/test_filenames_ogg \
    test/test_filenames_opus \
    test/test_filenames_prores \
    test/test_filenames_ts \
    test/test_filenames_wav \
    test/test_filenames_webm \
    test/test_filenames_hls \
    test/test_filesize \
    test/test_filesize_aiff \
    test/test_filesize_alac \
    test/test_filesize_mov \
    test/test_filesize_mp3 \
    test/test_filesize_mp4 \
    test/test_filesize_ogg \
    test/test_filesize_opus \
    test/test_filesize_prores \
    test/test_filesize_ts \
    test/test_filesize_video \
    test/test_filesize_video_mov \
    test/test_filesize_video_mp4 \
    test/test_filesize_video_prores \
    test/test_filesize_video_ts \
    test/test_filesize_video_webm \
    test/test_filesize_wav \
    test/test_filesize_webm \
    test/test_filesize_hls \
    test/test_frameset \
    test/test_frameset_bmp \
    test/test_frameset_jpg \
    test/test_frameset_png \
    test/test_picture \
    test/test_picture_aiff \
    test/test_picture_alac \
    test/test_picture_mov \
    test/test_picture_mp3 \
    test/test_picture_mp4 \
    test/test_picture_ogg \
    test/test_picture_opus \
    test/test_picture_prores \
    test/test_picture_ts \
    test/test_picture_wav \
    test/test_picture_webm \
    test/test_tags \
    test/test_tags_aiff \
    test/test_tags_alac \
    test/test_tags_mov \
    test/test_tags_mp3 \
    test/test_tags_mp4 \
    test/test_tags_ogg \
    test/test_tags_opus \
    test/test_tags_prores \
    test/test_tags_ts \
    test/test_tags_wav \
    test/test_tags_webm \
    TODO \
    Doxyfile \
    doxyfile.inc \
    test/mkvid
