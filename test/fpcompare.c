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

#ifdef USE_LIBSWRESAMPLE
#include <libswresample/swresample.h>
#elif USE_LIBAVRESAMPLE
#warning "Falling back to libavresample which is deprecated."
#include <libavresample/avresample.h>
#else
#error "Must have either libavresample or libswresample!"
#endif
// For LIBAVFILTER_VERSION_INT
#include <libavfilter/avfilter.h>

#include <chromaprint.h>

#define LAVC_NEW_PACKET_INTERFACE           (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 0))
#define LAVF_DEP_AVSTREAM_CODEC             (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 33, 0))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

int decode_audio_file(ChromaprintContext *chromaprint_ctx, const char *file_name, int max_length, int *duration)
{
    int ok = 0, remaining, length, codec_ctx_opened = 0, stream_index;
    AVFormatContext *format_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodec *codec = NULL;
    AVStream *stream = NULL;
    AVFrame *frame = NULL;
#ifdef USE_LIBSWRESAMPLE
    SwrContext *swr_ctx = NULL;
#else // USE_LIBSAVRESAMPLE
    AVAudioResampleContext *avresample_ctx = NULL;
#endif
#if LAVF_DEP_AVSTREAM_CODEC
    int max_dst_nb_samples = 0;
    uint8_t *dst_data = NULL ;
    AVPacket packet;
    int16_t* samples;
#else
    int got_frame, consumed;
    int max_dst_nb_samples = 0, dst_linsize = 0;
    uint8_t *dst_data[1] = { NULL };
    uint8_t **data;
    AVPacket packet;
#endif

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

#if LAVF_DEP_AVSTREAM_CODEC
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "ERROR: couldn't allocate codec context\n");
        goto done;
    }

    if (avcodec_parameters_to_context(codec_ctx, stream->codecpar) < 0) {
        fprintf(stderr, "ERROR: couldn't populate codex context\n");
        goto done;
    }
#else
    codec_ctx = stream->codec;
#endif
    codec_ctx->request_sample_fmt = AV_SAMPLE_FMT_S16;

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "ERROR: couldn't open the codec\n");
        goto done;
    }
    codec_ctx_opened = 1;

    if (codec_ctx->channels <= 0) {
        fprintf(stderr, "ERROR: no channels found in the audio stream\n");
        goto done;
    }

    if (codec_ctx->sample_fmt != AV_SAMPLE_FMT_S16) {
#ifdef USE_LIBSWRESAMPLE
        swr_ctx = swr_alloc_set_opts(NULL,
                                     (int64_t)codec_ctx->channel_layout, AV_SAMPLE_FMT_S16, codec_ctx->sample_rate,
                                     (int64_t)codec_ctx->channel_layout, codec_ctx->sample_fmt, codec_ctx->sample_rate,
                                     0, NULL);
        if (!swr_ctx) {
            fprintf(stderr, "ERROR: couldn't allocate audio resampler\n");
            goto done;
        }
        if (swr_init(swr_ctx) < 0) {
            fprintf(stderr, "ERROR: couldn't initialize the audio resampler\n");
            goto done;
        }
#else // USE_LIBSAVRESAMPLE
        avresample_ctx = avresample_alloc_context();
        if (!avresample_ctx) {
            fprintf(stderr, "ERROR: couldn't allocate audio resampler\n");
            goto done;
        }

        av_opt_set_int(avresample_ctx, "in_channel_layout", (int)codec_ctx->channel_layout, 0);
        av_opt_set_int(avresample_ctx, "out_channel_layout", (int)codec_ctx->channel_layout, 0);
        av_opt_set_int(avresample_ctx, "in_sample_rate", codec_ctx->sample_rate, 0);
        av_opt_set_int(avresample_ctx, "out_sample_rate", codec_ctx->sample_rate, 0);
        av_opt_set_int(avresample_ctx, "in_sample_fmt", codec_ctx->sample_fmt, 0);
        av_opt_set_int(avresample_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);


        if (avresample_open(avresample_ctx) < 0) {
            fprintf(stderr, "ERROR: couldn't initialize the audio resampler\n");
            goto done;
        }
#endif
    }

    *duration = (int)(stream->time_base.num * stream->duration / stream->time_base.den);

    remaining = max_length * codec_ctx->channels * codec_ctx->sample_rate;
    chromaprint_start(chromaprint_ctx, codec_ctx->sample_rate, codec_ctx->channels);
//#if LAVF_DEP_AVSTREAM_CODEC
    frame = av_frame_alloc();
//#else
//    frame = avcodec_alloc_frame();
//#endif

    while (1) {
        if (av_read_frame(format_ctx, &packet) < 0) {
            break;
        }

        if (packet.stream_index == stream_index) {
#if LAVC_NEW_PACKET_INTERFACE
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
#ifdef USE_LIBSWRESAMPLE
                if (swr_ctx) {
#else // USE_LIBSAVRESAMPLE
                if (avresample_ctx) {
#endif
                    if (frame->nb_samples > max_dst_nb_samples) {
                        max_dst_nb_samples = frame->nb_samples;
                        av_freep(&dst_data);
                        if (av_samples_alloc(&dst_data, NULL,
                                             codec_ctx->channels, max_dst_nb_samples,
                                             AV_SAMPLE_FMT_S16, 0) < 0) {
                            fprintf(stderr, "ERROR: couldn't allocate audio converter buffer\n");
                            goto done;
                        }
                    }
#ifdef USE_LIBSWRESAMPLE
                    if (swr_convert(swr_ctx, &dst_data, frame->nb_samples,
                                    (const uint8_t**)frame->data, frame->nb_samples) < 0) {
                        fprintf(stderr, "ERROR: couldn't convert the audio\n");
                        goto done;
                    }
#else // USE_LIBSAVRESAMPLE
                    if (avresample_convert(avresample_ctx, &dst_data, 0, frame->nb_samples, (uint8_t**)frame->data, 0, frame->nb_samples) < 0) {
                        fprintf(stderr, "ERROR: couldn't convert the audio\n");
                        goto done;
                    }
#endif
                    samples = (int16_t*)dst_data;
                } else {
                    samples = (int16_t*)frame->data[0];
                }

                length = MIN(remaining, frame->nb_samples * codec_ctx->channels);
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
#else
            //avcodec_get_frame_defaults(frame);

            got_frame = 0;
            consumed = avcodec_decode_audio4(codec_ctx, frame, &got_frame, &packet);
            if (consumed < 0) {
                fprintf(stderr, "WARNING: error decoding audio\n");
                continue;
            }

            if (got_frame) {
                data = frame->data;
#ifdef USE_LIBSWRESAMPLE
                if (swr_ctx) {
                    if (frame->nb_samples > max_dst_nb_samples) {
                        av_freep(&dst_data[0]);
                        if (av_samples_alloc(dst_data, &dst_linsize, codec_ctx->channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 1) < 0) {
                            fprintf(stderr, "ERROR: couldn't allocate audio converter buffer\n");
                            goto done;
                        }
                        max_dst_nb_samples = frame->nb_samples;
                    }
                    if (swr_convert(swr_ctx, dst_data, frame->nb_samples, (const uint8_t **)frame->data, frame->nb_samples) < 0) {
                        fprintf(stderr, "ERROR: couldn't convert the audio\n");
                        goto done;
                    }
                    data = dst_data;
                }
#else // USE_LIBSAVRESAMPLE
                if (avresample_ctx) {
                    if (frame->nb_samples > max_dst_nb_samples) {
                        av_freep(&dst_data[0]);
                        if (av_samples_alloc(dst_data, &dst_linsize, codec_ctx->channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 1) < 0) {
                            fprintf(stderr, "ERROR: couldn't allocate audio converter buffer\n");
                            goto done;
                        }
                        max_dst_nb_samples = frame->nb_samples;
                    }

                    if (avresample_convert(avresample_ctx, dst_data, 0, frame->nb_samples, (const uint8_t **)frame->data, 0, frame->nb_samples) < 0) {
                        fprintf(stderr, "ERROR: couldn't convert the audio\n");
                        goto done;
                    }
                    data = dst_data;
                }
#endif

                length = MIN(remaining, frame->nb_samples * codec_ctx->channels);
                if (!chromaprint_feed(chromaprint_ctx, data[0], length)) {
                    goto done;
                }

                if (max_length) {
                    remaining -= length;
                    if (remaining <= 0) {
                        goto finish;
                    }
                }
            }
#endif
        }

#if LAVF_DEP_AVSTREAM_CODEC
        av_packet_unref(&packet);
#else
        av_free_packet(&packet);
#endif
    }

finish:
    if (!chromaprint_finish(chromaprint_ctx)) {
        fprintf(stderr, "ERROR: fingerprint calculation failed\n");
        goto done;
    }

    ok = 1;

done:
    if (frame) {
//#if LAVF_DEP_AVSTREAM_CODEC
        av_frame_unref(frame);
//#else
//        avcodec_free_frame(&frame);
//#endif
    }
    if (dst_data) {
        av_freep(&dst_data);
    }
#ifdef USE_LIBSWRESAMPLE
    if (swr_ctx) {
        swr_free(&swr_ctx);
    }
#else // USE_LIBSAVRESAMPLE
    if (avresample_ctx) {
        avresample_close(avresample_ctx);
        avresample_free(&avresample_ctx);
    }
#endif
    if (codec_ctx_opened) {
        avcodec_close(codec_ctx);
    }
    if (format_ctx) {
        avformat_close_input(&format_ctx);
    }
#if LAVF_DEP_AVSTREAM_CODEC
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }
#endif
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
