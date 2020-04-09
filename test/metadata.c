/*
 * Copyright (C) 2020 Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * On Debian systems, the complete text of the GNU General Public License
 * Version 3 can be found in `/usr/share/common-licenses/GPL-3'.
 */

#include <stdio.h>

#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/ffversion.h>

int main (int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <input_file>\n"
               "Print all file meta data to screen.\n"
               "\n", argv[0]);
        fprintf(stderr, "ERROR: Exactly one parameter required\n\n");
        return 1;
    }

    if (!strcmp(argv[1], "-v")) {
        printf("FFmpeg " FFMPEG_VERSION "\n");
        return 0;
    }
	
	av_register_all();

    const char *filename = argv[1];
    AVFormatContext *fmt_ctx = NULL;
    int ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL);
    if (ret)
    {
        char error[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error, AV_ERROR_MAX_STRING_SIZE);
        fprintf(stderr, "avformat_open_input() returns error for file '%s'\n%s\n", filename, error);
        return ret;
    }

    for (unsigned int streamno = 0; streamno < fmt_ctx->nb_streams; streamno++)
    {        
        for (AVDictionaryEntry *tag = NULL; (tag = av_dict_get(fmt_ctx->streams[streamno]->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)) != NULL;)
        {
            printf("%s=%s\n", tag->key, tag->value);
        }
    }


    for (AVDictionaryEntry *tag = NULL; (tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)) != NULL;)
    {
        printf("%s=%s\n", tag->key, tag->value);
    }

    avformat_close_input(&fmt_ctx);

    return 0;
}
