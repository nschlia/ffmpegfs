/*
 * Copyright (C) 2017 K. Henriksson
 * Copyright (C) 2017-2021 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>

#include <libswresample/swresample.h>

// For LIBAVFILTER_VERSION_INT
#include <libavfilter/avfilter.h>

#include <chromaprint.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

int decode_audio_file(ChromaprintContext *chromaprint_ctx, const char *file_name, int max_length, int *duration)
{
    int ok = 0, remaining, length, codec_ctx_opened = 0, stream_index;
    AVFormatContext *format_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
#if (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 0, 0))
    const AVCodec *codec = NULL;
#else
    AVCodec *codec = NULL;
#endif
    AVStream *stream = NULL;
    AVFrame *frame = NULL;
    SwrContext *swr_ctx = NULL;
    int max_dst_nb_samples = 0;
    uint8_t *dst_data = NULL ;
    AVPacket packet;
    int16_t* samples;

    if (!strcmp(file_name, "-")) {
        file_name = "pipe:0";
    }

    if (avformat_open_input(&format_ctx, file_name, NULL, NULL) != 0) {
        fprintf(stderr, "ERROR: couldn't open the file\n");
        goto done;
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        fprintf(stderr, "ERROR: couldn't find stream information in the file\n");
        goto done;
    }

    stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (stream_index < 0) {
        fprintf(stderr, "ERROR: couldn't find any audio stream in the file\n");
        goto done;
    }

    stream = format_ctx->streams[stream_index];

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "ERROR: couldn't allocate codec context\n");
        goto done;
    }

    if (avcodec_parameters_to_context(codec_ctx, stream->codecpar) < 0) {
        fprintf(stderr, "ERROR: couldn't populate codex context\n");
        goto done;
    }

    codec_ctx->request_sample_fmt = AV_SAMPLE_FMT_S16;

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "ERROR: couldn't open the codec\n");
        goto done;
    }
    codec_ctx_opened = 1;

#if (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 0))
    int channels = codec_ctx->ch_layout.nb_channels;
#else
    int channels = codec_ctx->channels;
#endif

    if (channels <= 0) {
        fprintf(stderr, "ERROR: no channels found in the audio stream\n");
        goto done;
    }

    if (codec_ctx->sample_fmt != AV_SAMPLE_FMT_S16) {
#if (LIBSWRESAMPLE_VERSION_INT >= AV_VERSION_INT(4, 5, 0))
        int ret = swr_alloc_set_opts2(&swr_ctx,
                                      &codec_ctx->ch_layout, AV_SAMPLE_FMT_S16, codec_ctx->sample_rate,
                                      &codec_ctx->ch_layout, codec_ctx->sample_fmt, codec_ctx->sample_rate,
                                      0, NULL);
        if (ret < 0)
        {
            fprintf(stderr, "ERROR: couldn't allocate audio resampler\n");
            return ret;
        }
#else
        swr_ctx = swr_alloc_set_opts(NULL,
                                     (int64_t)codec_ctx->channel_layout, AV_SAMPLE_FMT_S16, codec_ctx->sample_rate,
                                     (int64_t)codec_ctx->channel_layout, codec_ctx->sample_fmt, codec_ctx->sample_rate,
                                     0, NULL);
        if (!swr_ctx) {
            fprintf(stderr, "ERROR: couldn't allocate audio resampler\n");
            goto done;
        }
#endif
        if (swr_init(swr_ctx) < 0) {
            fprintf(stderr, "ERROR: couldn't initialize the audio resampler\n");
            goto done;
        }
    }

    *duration = (int)(stream->time_base.num * stream->duration / stream->time_base.den);

    remaining = max_length * channels * codec_ctx->sample_rate;
    chromaprint_start(chromaprint_ctx, codec_ctx->sample_rate, channels);
    frame = av_frame_alloc();

    while (1) {
        if (av_read_frame(format_ctx, &packet) < 0) {
            break;
        }

        if (packet.stream_index == stream_index) {
            if (avcodec_send_packet(codec_ctx, &packet) < 0) {
                fprintf(stderr, "WARNING: error sending audio data\n");
                continue;
            }

            while(1) {
                int recv_result = avcodec_receive_frame(codec_ctx, frame);
                if (recv_result == AVERROR(EAGAIN)) break;

                if (recv_result < 0) {
                    fprintf(stderr, "WARNING: error decoding audio\n");
                    break;
                }
                if (swr_ctx) {
                    if (frame->nb_samples > max_dst_nb_samples) {
                        max_dst_nb_samples = frame->nb_samples;
                        av_freep(&dst_data);
                        if (av_samples_alloc(&dst_data, NULL,
                                             channels, max_dst_nb_samples,
                                             AV_SAMPLE_FMT_S16, 0) < 0) {
                            fprintf(stderr, "ERROR: couldn't allocate audio converter buffer\n");
                            goto done;
                        }
                    }

                    if (swr_convert(swr_ctx, &dst_data, frame->nb_samples,
                                    (const uint8_t**)frame->data, frame->nb_samples) < 0) {
                        fprintf(stderr, "ERROR: couldn't convert the audio\n");
                        goto done;
                    }
                    samples = (int16_t*)dst_data;
                } else {
                    samples = (int16_t*)frame->data[0];
                }

                length = MIN(remaining, frame->nb_samples * channels);
                if (!chromaprint_feed(chromaprint_ctx, samples, length)) {
                    goto done;
                }

                if (max_length) {
                    remaining -= length;
                    if (remaining <= 0) {
                        goto finish;
                    }
                }
            }
        }

        av_packet_unref(&packet);
    }

finish:
    if (!chromaprint_finish(chromaprint_ctx)) {
        fprintf(stderr, "ERROR: fingerprint calculation failed\n");
        goto done;
    }

    ok = 1;

done:
    if (frame) {
        av_frame_unref(frame);
    }
    if (dst_data) {
        av_freep(&dst_data);
    }

    if (swr_ctx) {
        swr_free(&swr_ctx);
    }

    if (codec_ctx_opened) {
        avcodec_close(codec_ctx);
    }
    if (format_ctx) {
        avformat_close_input(&format_ctx);
    }
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }
    return ok;
}

int main(int argc, char **argv)
{
    int i, j, max_length = 120, num_file_names = 0/*, raw = 0*/, duration;
    int raw_fingerprint_size[2];
    uint32_t *raw_fingerprints[2] = {0};
    char *file_name, **file_names;
    ChromaprintContext *chromaprint_ctx;
    int algo = CHROMAPRINT_ALGORITHM_DEFAULT, num_failed = 0;
    uint32_t thisdiff;
    int setbits = 0;

    file_names = malloc((size_t)argc * sizeof(char *));
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (!strcmp(arg, "-length") && i + 1 < argc) {
            max_length = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "-version") || !strcmp(arg, "-v")) {
            printf("fpcalc version %s\n", chromaprint_get_version());
            free(file_names);
            return 0;
        }
        //else if (!strcmp(arg, "-raw")) {
        //    raw = 1;
        //}
        else if (!strcmp(arg, "-algo") && i + 1 < argc) {
            const char *v = argv[++i];
            if (!strcmp(v, "test1")) { algo = CHROMAPRINT_ALGORITHM_TEST1; }
            else if (!strcmp(v, "test2")) { algo = CHROMAPRINT_ALGORITHM_TEST2; }
            else if (!strcmp(v, "test3")) { algo = CHROMAPRINT_ALGORITHM_TEST3; }
            else if (!strcmp(v, "test4")) { algo = CHROMAPRINT_ALGORITHM_TEST4; }
            else {
                fprintf(stderr, "WARNING: unknown algorithm, using the default\n");
            }
        }
        else if (!strcmp(arg, "-set") && i + 1 < argc) {
            i += 1;
        }
        else {
            file_names[num_file_names++] = argv[i];
        }
    }

    if (num_file_names != 2) {
        printf("usage: %s [OPTIONS] FILE1 FILE2\n\n", argv[0]);
        free(file_names);
        return 2;
    }

#ifndef LIBAVFILTER_VERSION_INT
#error "LIBAVFILTER_VERSION_INT not defined! Did you forget to include libavfilter/avfilter.h?"
#endif
#if (LIBAVFILTER_VERSION_INT < AV_VERSION_INT(7, 14, 0))
    av_register_all();
#endif
    av_log_set_level(AV_LOG_ERROR);

    chromaprint_ctx = chromaprint_new(algo);

    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (!strcmp(arg, "-set") && i + 1 < argc) {
            char *name = argv[++i];
            char *value = strchr(name, '=');
            if (value) {
                *value++ = '\0';
                chromaprint_set_option(chromaprint_ctx, name, atoi(value));
            }
        }
    }

    for (i = 0; i < num_file_names; i++) {
        file_name = file_names[i];
        if (!decode_audio_file(chromaprint_ctx, file_name, max_length, &duration)) {
            fprintf(stderr, "ERROR: unable to calculate fingerprint for file %s, skipping\n", file_name);
            num_failed++;
            continue;
        }
        if (!chromaprint_get_raw_fingerprint(chromaprint_ctx, &raw_fingerprints[i], &raw_fingerprint_size[i])) {
            fprintf(stderr, "ERROR: unable to calculate fingerprint for file %s, skipping\n", file_name);
            num_failed++;
            continue;
        }
    }

    if (num_failed == 0) {
        int min_raw_fingerprint_size = MIN(raw_fingerprint_size[0],
                raw_fingerprint_size[1]);
        int max_raw_fingerprint_size = MAX(raw_fingerprint_size[0],
                raw_fingerprint_size[1]);

        for (j = 0; j < min_raw_fingerprint_size; j++) {
            thisdiff = raw_fingerprints[0][j]^raw_fingerprints[1][j];
            setbits += __builtin_popcount(thisdiff);
        }
        setbits += (int)((max_raw_fingerprint_size-min_raw_fingerprint_size)*32.0);
        printf("%f\n", setbits/(max_raw_fingerprint_size*32.0));
    } else {
        fprintf(stderr, "ERROR: Couldn't calculate both fingerprints; can't compare.\n");
    }

    if (raw_fingerprints[0]) chromaprint_dealloc(raw_fingerprints[0]);
    if (raw_fingerprints[1]) chromaprint_dealloc(raw_fingerprints[1]);

    chromaprint_free(chromaprint_ctx);
    free(file_names);

    return num_failed ? 1 : 0;
}
