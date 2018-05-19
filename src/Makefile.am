AM_CPPFLAGS = $(fuse_CFLAGS)

bin_PROGRAMS = ffmpegfs
ffmpegfs_SOURCES = ffmpegfs.c ffmpegfs.h fuseops.cc transcode.cc transcode.h cache.cc cache.h buffer.cc buffer.h coders.cc coders.h logging.cc logging.h cache_entry.cc cache_entry.h cache_maintenance.cc cache_maintenance.h id3v1tag.h wave.h diskio.cc diskio.h fileio.cc fileio.h
ffmpegfs_LDADD = $(fuse_LIBS) -lrt

ffmpegfs_SOURCES += ffmpeg_base.cc ffmpeg_base.h ffmpeg_transcoder.cc ffmpeg_transcoder.h ffmpeg_utils.cc ffmpeg_utils.h ffmpeg_optimisations.cc
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

ffmpegfs_LDADD += $(sqlite3_LIBS)
