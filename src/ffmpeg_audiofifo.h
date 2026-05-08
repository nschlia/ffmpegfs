/*
 * Copyright (C) 2017-2026 by Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * @file ffmpeg_audiofifo.h
 * @brief FFmpeg AVAudioFifo RAII wrapper
 *
 * @ingroup ffmpegfs
 */

#ifndef FFMPEG_AUDIOFIFO_H
#define FFMPEG_AUDIOFIFO_H

#pragma once

#include "ffmpeg_utils.h"

struct AVAudioFifo;

/**
 * @brief RAII wrapper for AVAudioFifo.
 *
 * Owns an AVAudioFifo allocated with av_audio_fifo_alloc() and releases it
 * with av_audio_fifo_free(). The wrapper is movable but not copyable.
 */
class FFmpeg_AudioFifo
{
public:
    FFmpeg_AudioFifo();
    FFmpeg_AudioFifo(AVSampleFormat sample_fmt, int channels, int nb_samples = 1);
    ~FFmpeg_AudioFifo();

    FFmpeg_AudioFifo(const FFmpeg_AudioFifo&) = delete;
    FFmpeg_AudioFifo& operator=(const FFmpeg_AudioFifo&) = delete;

    FFmpeg_AudioFifo(FFmpeg_AudioFifo&& fifo) noexcept;
    FFmpeg_AudioFifo& operator=(FFmpeg_AudioFifo&& fifo) noexcept;

    int             alloc(AVSampleFormat sample_fmt, int channels, int nb_samples = 1);
    void            reset();
    bool            empty() const;

    int             size() const;
    int             realloc(int nb_samples);
    int             write(void **data, int nb_samples);
    int             read(void **data, int nb_samples);

    AVAudioFifo*    get();
    const AVAudioFifo* get() const;
    AVAudioFifo*    release();

    operator        AVAudioFifo*();
    operator const  AVAudioFifo*() const;

private:
    AVAudioFifo *   m_fifo;     /**< @brief Pointer to underlying AVAudioFifo */
};

#endif // FFMPEG_AUDIOFIFO_H
