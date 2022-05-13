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

set(formats_video
    mov
    mp4
    prores
    ts
    webm
)

set(formats_streaming
    hls
    #dash   Not implemented yet
)

set(formats
    aiff
    alac
    flac
    mp3
    ogg
    opus
    wav
    ${formats_video}
)

set(images
    bmp
    jpg
    png
)

set (decoders
    mmal
    vaapi
)
set (encoders
    omx
    vaapi
)

set(functional_tests
    audio
    filenames
    filesize
    #picture    Test not implemented
    tags
)

set(functional_video
    filesize_video
)

set(functional_streaming
    filenames
    filesize
    filecount
)

FOREACH(test ${functional_tests})
    FOREACH(format ${formats})
        ADD_TEST(NAME test_${test}_${format} COMMAND test_${test} ${format})
    ENDFOREACH()
ENDFOREACH()

FOREACH(test ${functional_video})
    FOREACH(format ${formats_video})
        ADD_TEST(NAME test_${test}_${format} COMMAND test_${test} ${format})
    ENDFOREACH()
ENDFOREACH()

FOREACH(test ${functional_streaming})
    FOREACH(format ${formats_streaming})
        ADD_TEST(NAME test_${test}_${format} COMMAND test_${test}_${format})
    ENDFOREACH()
ENDFOREACH()

FOREACH(format ${images})
    ADD_TEST(NAME test_frameset_${format} COMMAND test_frameset ${format})
ENDFOREACH()

FOREACH(format ${decoders})
    ADD_TEST(NAME test_video_hw_dec_${format} COMMAND test_video_hw DEC ${format})
    SET_TESTS_PROPERTIES(test_video_hw_dec_${format} PROPERTIES SKIP_RETURN_CODE 77)    # Soft fail
ENDFOREACH()

FOREACH(format ${encoders})
    ADD_TEST(NAME test_video_hw_enc_${format} COMMAND test_video_hw ENC ${format})
    SET_TESTS_PROPERTIES(test_video_hw_enc_${format} PROPERTIES SKIP_RETURN_CODE 77)    # Soft fail
ENDFOREACH()

ADD_TEST(NAME test_cuesheet_embedded COMMAND test_cuesheet_embedded)
ADD_TEST(NAME test_cuesheet_file COMMAND test_cuesheet_file)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -R test_)

# Build audio comparator required by tests
add_executable(fpcompare fpcompare.c)
target_link_libraries(fpcompare chromaprint avcodec avformat avutil swresample sqlite3)
target_compile_definitions(fpcompare PUBLIC
    -D_FILE_OFFSET_BITS=64
    -DHAVE_CONFIG_H
    -D_GNU_SOURCE
)
add_dependencies(check fpcompare)

# Build metadata extractor required by tests
add_executable(metadata metadata.c)
target_link_libraries(metadata avcodec avformat avutil swresample sqlite3)
target_compile_definitions(metadata PUBLIC
    -D_FILE_OFFSET_BITS=64
    -DHAVE_CONFIG_H
    -D_GNU_SOURCE
)
add_dependencies(check metadata)

# Main binary required by tests
add_dependencies(check ${PROJECT_NAME})
