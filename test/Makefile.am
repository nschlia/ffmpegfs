TESTS=\
test_audio_aiff \
test_audio_alac \
test_audio_flac \
test_audio_mov \
test_audio_mp3 \
test_audio_mp4 \
test_audio_ogg \
test_audio_opus \
test_audio_prores \
test_audio_ts \
test_audio_wav \
test_audio_webm \
test_cuesheet_file \
test_cuesheet_embedded \
test_filecount_hls \
test_filenames_aiff \
test_filenames_alac \
test_filenames_flac \
test_filenames_hls \
test_filenames_mov \
test_filenames_mp3 \
test_filenames_mp4 \
test_filenames_ogg \
test_filenames_opus \
test_filenames_prores \
test_filenames_ts \
test_filenames_wav \
test_filenames_webm \
test_filesize_aiff \
test_filesize_alac \
test_filesize_flac \
test_filesize_hls \
test_filesize_mov \
test_filesize_mp3 \
test_filesize_mp4 \
test_filesize_ogg \
test_filesize_opus \
test_filesize_prores \
test_filesize_ts \
test_filesize_video_mov \
test_filesize_video_mp4 \
test_filesize_video_prores \
test_filesize_video_ts \
test_filesize_video_webm \
test_filesize_wav \
test_filesize_webm \
test_frameset_bmp \
test_frameset_jpg \
test_frameset_png \
test_tags_aiff \
test_tags_alac \
test_tags_flac \
test_tags_mov \
test_tags_mp3 \
test_tags_mp4 \
test_tags_ogg \
test_tags_opus \
test_tags_prores \
test_tags_ts \
test_tags_wav \
test_tags_webm \
test_video_hw_dec_mmal \
test_video_hw_dec_vaapi \
test_video_hw_enc_omx \
test_video_hw_enc_vaapi

EXTRA_DIST = $(TESTS) funcs.sh srcdir test_filenames test_tags test_audio test_filesize test_frameset test_filesize_video test_video_hw test_cuesheet
EXTRA_DIST += $(wildcard tags/*)
# NOT IN RELEASE 1.0! Add later: test_picture

CLEANFILES = $(patsubst %,%_builtin.log,$(TESTS))

AM_CPPFLAGS=-Ofast $(libswresample_CFLAGS)
check_PROGRAMS = fpcompare metadata
fpcompare_SOURCES = fpcompare.c
fpcompare_LDADD = -lchromaprint -lavcodec -lavformat -lavutil $(libswresample_LIBS)
metadata_SOURCES = metadata.c
metadata_LDADD =  -lavcodec -lavformat -lavutil $(libswresample_LIBS)
