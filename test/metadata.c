/*
 * Copyright (c) 2011 Reinhard Tartler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * Shows how the metadata API can be used in application programs.
 * @example metadata.c
 */

#include <stdio.h>

#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#if LIBAVFORMAT_VERSION_MICRO >= 100
#include <libavutil/ffversion.h>
#else
#define LIBAV_VERSION   		"0.0.0"
#define AV_ERROR_MAX_STRING_SIZE 	255
#endif

int main (int argc, char **argv)
{
    AVFormatContext *fmt_ctx = NULL;
    AVDictionaryEntry *tag = NULL;
    int ret;
    unsigned int streamno;

    if (argc != 2) {
        printf("usage: %s <input_file>\n"
               "example program to demonstrate the use of the libavformat metadata API.\n"
               "\n", argv[0]);
        return 1;
    }

    if (!strcmp(argv[1], "-v")) {
#if LIBAVFORMAT_VERSION_MICRO >= 100
        printf("FFmpeg " FFMPEG_VERSION "\n");
#else
        printf("Libav " LIBAV_VERSION "\n");
#endif
        return 0;
    }

    av_register_all();

    if ((ret = avformat_open_input(&fmt_ctx, argv[1], NULL, NULL))) {
        char error[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error, AV_ERROR_MAX_STRING_SIZE);
        fprintf(stderr, "avformat_open_input %s\n", error);
        return ret;
    }

    for (streamno = 0; streamno < fmt_ctx->nb_streams; streamno++)
    {
        while ((tag = av_dict_get(fmt_ctx->streams[streamno]->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
        {
            printf("%s=%s\n", tag->key, tag->value);
        }
    }

    while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    {
        printf("%s=%s\n", tag->key, tag->value);
    }

    avformat_close_input(&fmt_ctx);
    return 0;
}
