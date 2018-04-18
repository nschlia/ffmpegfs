TESTS  = test_filenames_mp4 test_filenames_mp3 test_filenames_wav  test_filenames_ogg
TESTS += test_tags_mp4 test_tags_mp3 test_tags_wav test_tags_ogg
TESTS += test_audio_mp4 test_audio_mp3 test_audio_wav test_audio_ogg
TESTS += test_filesize_mp4 test_filesize_mp3 test_filesize_wav test_filesize_ogg

# NOT IN RELEASE 1.0! Add later: test_picture_mp4 test_picture_mp3 test_picture_wav test_picture_ogg

EXTRA_DIST = $(TESTS) funcs.sh srcdir test_filenames test_tags test_audio test_filesize
# NOT IN RELEASE 1.0! Add later: test_picture 

CLEANFILES = $(patsubst %,%.builtin.log,$(TESTS))

AM_CPPFLAGS=-Ofast
check_PROGRAMS = fpcompare
fpcompare_SOURCES = fpcompare.c
fpcompare_LDADD = -lchromaprint -lavcodec -lavformat -lavutil

if USE_LIBSWRESAMPLE
AM_CPPFLAGS += -DUSE_LIBSWRESAMPLE
AM_CPPFLAGS += $(libswresample_CFLAGS)
fpcompare_LDADD += $(libswresample_LIBS)
endif

if USE_LIBAVRESAMPLE
AM_CPPFLAGS += -DUSE_LIBAVRESAMPLE
AM_CPPFLAGS += $(libavresample_CFLAGS)
fpcompare_LDADD += $(libavresample_LIBS)
endif

