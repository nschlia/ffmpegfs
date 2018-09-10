AM_CPPFLAGS = $(fuse_CFLAGS)

bin_PROGRAMS = ffmpegfs
ffmpegfs_SOURCES = ffmpegfs.c ffmpegfs.h fuseops.cc transcode.cc transcode.h cache.cc cache.h buffer.cc buffer.h coders.cc coders.h logging.cc logging.h cache_entry.cc cache_entry.h cache_maintenance.cc cache_maintenance.h id3v1tag.h wave.h diskio.cc diskio.h fileio.cc fileio.h ffmpeg_compat.h
ffmpegfs_LDADD = $(fuse_LIBS) -lrt

ffmpegfs_SOURCES += ffmpeg_base.cc ffmpeg_base.h ffmpeg_transcoder.cc ffmpeg_transcoder.h ffmpeg_utils.cc ffmpeg_utils.h ffmpeg_profiles.cc
ffmpegfs_LDADD += $(libavcodec_LIBS) $(libavutil_LIBS) $(libavformat_LIBS) $(libswscale_LIBS) $(libavfilter_LIBS)
AM_CPPFLAGS += $(libavcodec_CFLAGS) $(libavutil_CFLAGS) $(libavformat_CFLAGS) $(libswscale_CFLAGS) $(libavfilter_CFLAGS)

if USE_LIBSWRESAMPLE
AM_CPPFLAGS += -DUSE_LIBSWRESAMPLE
AM_CPPFLAGS += $(libswresample_CFLAGS)
ffmpegfs_LDADD += $(libswresample_LIBS)
endif

if USE_LIBAVRESAMPLE
AM_CPPFLAGS += -DUSE_LIBAVRESAMPLE
AM_CPPFLAGS += $(libavresample_CFLAGS)
ffmpegfs_LDADD += $(libavresample_LIBS)
endif

# DVD support: requires both libdbdnav and libdvdread
if USE_LIBDVD
AM_CPPFLAGS += -DUSE_LIBDVD
AM_CPPFLAGS += $(libdvdnav_CFLAGS) $(libdvdread_CFLAGS)
ffmpegfs_SOURCES += dvdio.cc dvdio.h dvdparser.cc dvdparser.h
ffmpegfs_LDADD += $(libdvdnav_LIBS) $(libdvdread_LIBS)
endif

# Bluray support: requires libbluray
if USE_LIBBLURAY
AM_CPPFLAGS += -DUSE_LIBBLURAY
AM_CPPFLAGS += $(libbluray_CFLAGS)
ffmpegfs_SOURCES += blurayio.cc blurayio.h blurayparser.cc blurayparser.h
ffmpegfs_LDADD += $(libbluray_LIBS)
endif

# VCD support: uses internal code
if USE_LIBVCD
AM_CPPFLAGS += -DUSE_LIBVCD
AM_CPPFLAGS += $(libvcd_CFLAGS)
ffmpegfs_SOURCES += vcdio.cc vcdio.h vcdparser.cc vcdparser.h vcd/vcdchapter.cc vcd/vcdchapter.h vcd/vcdentries.cc vcd/vcdentries.h vcd/vcdinfo.cc vcd/vcdinfo.h vcd/vcdutils.cc vcd/vcdutils.h
endif

ffmpegfs_LDADD += $(sqlite3_LIBS)
