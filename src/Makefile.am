EXTRA_DIST = $(wildcard scripts/*) ../win/ffmpeg/bin64/readme.txt ../win/ffmpeg/bin/readme.txt ../win/ffmpeg/include/readme.txt ../win/ffmpeg/lib64/readme.txt ../win/ffmpeg/lib/readme.txt

# Add fuse
AM_CPPFLAGS = $(fuse_CFLAGS)

bin_PROGRAMS = ffmpegfs
ffmpegfs_SOURCES = ffmpegfs.cc ffmpegfs.h fuseops.cc transcode.cc transcode.h cache.cc cache.h buffer.cc buffer.h logging.cc logging.h cache_entry.cc cache_entry.h cache_maintenance.cc cache_maintenance.h id3v1tag.h aiff.h wave.h cuesheetparser.cc cuesheetparser.h diskio.cc diskio.h fileio.cc fileio.h ffmpeg_compat.h ffmpeg_profiles.h thread_pool.cc thread_pool.h
ffmpegfs_LDADD = $(libcue_LIBS) $(fuse_LIBS) -lstdc++fs
ffmpegfs_LDFLAGS =
if WINDOWS
ffmpegfs_LDADD += -lws2_32 -liconv -L../win/ffmpeg/lib64 -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale
ffmpegfs_LDFLAGS += -mconsole -Wl,-subsystem,console
#ffmpegfs_LDFLAGS += -mwindows -Wl,-subsystem,windows
#ffmpegfs_LDFLAGS += -mconsole
#-mconsole -mcygwin -mno-cygwin -mdll
#-mnop-fun-dllimport  -mthread -municode  -mwin32  -mwindows
#-fno-set-stack-executable
ffmpegfs_SOURCES += ../win/src/mman.c ../win/include/sys/mman.h
CFLAGS += -Wno-unused-parameter
else
ffmpegfs_LDADD += -lrt
endif

ffmpegfs_SOURCES += ffmpeg_base.cc ffmpeg_base.h ffmpeg_transcoder.cc ffmpeg_transcoder.h ffmpeg_utils.cc ffmpeg_utils.h ffmpeg_profiles.cc ffmpeg_frame.h ffmpeg_frame.cc ffmpeg_subtitle.h ffmpeg_subtitle.cc
ffmpegfs_LDADD += $(libavcodec_LIBS) $(libavutil_LIBS) $(libavformat_LIBS) $(libswscale_LIBS) $(libavfilter_LIBS) $(libswresample_LIBS)
AM_CPPFLAGS += $(libavcodec_CFLAGS) $(libavutil_CFLAGS) $(libavformat_CFLAGS) $(libswscale_CFLAGS) $(libavfilter_CFLAGS) $(libswresample_CFLAGS)

# Add sqlite3
ffmpegfs_LDADD += $(sqlite3_LIBS)
# Add libchardet
AM_CPPFLAGS += $(chardet_CFLAGS)
ffmpegfs_LDADD += $(chardet_LIBS)

# DVD support: requires both libdbdnav and libdvdread
if USE_LIBDVD
AM_CPPFLAGS += -DUSE_LIBDVD
AM_CPPFLAGS += $(libdvdnav_CFLAGS) $(libdvdread_CFLAGS)
ffmpegfs_SOURCES += dvdio.cc dvdio.h dvdparser.cc dvdparser.h
ffmpegfs_LDADD += $(libdvdnav_LIBS) $(libdvdread_LIBS)
endif

# Blu-ray support: requires libbluray
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

# Add conversion of manpages source. Will be used in binary.
BUILT_SOURCES = ../ffmpegfs.1.text ffmpegfshelp.h

../ffmpegfs.1.text: ../ffmpegfs.1.txt
	$(AM_V_GEN)a2x -a revnumber="$(VERSION)" \
		-a revdate="$(shell date +'%B %Y')" -D ".." -f text $<

# sed in makefiles is a menace so we use a helper script
ffmpegfshelp.h: ../ffmpegfs.1.text
	$(srcdir)/makehelp.sh $@
