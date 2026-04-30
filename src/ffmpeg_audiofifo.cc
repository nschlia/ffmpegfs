/*
 * Copyright (C) 2017-2026 by Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * @file ffmpeg_audiofifo.cc
 * @brief FFmpeg AVAudioFifo RAII wrapper
 *
 * @ingroup ffmpegfs
 */

#ifdef __cplusplus
extern "C" {
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <libavutil/audio_fifo.h>
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

#include "ffmpeg_audiofifo.h"

FFmpeg_AudioFifo::FFmpeg_AudioFifo()
    : m_fifo(nullptr)
{
}

FFmpeg_AudioFifo::FFmpeg_AudioFifo(AVSampleFormat sample_fmt, int channels, int nb_samples)
    : m_fifo(nullptr)
{
    alloc(sample_fmt, channels, nb_samples);
}

FFmpeg_AudioFifo::~FFmpeg_AudioFifo()
{
    reset();
}

FFmpeg_AudioFifo::FFmpeg_AudioFifo(FFmpeg_AudioFifo&& fifo) noexcept
    : m_fifo(fifo.m_fifo)
{
    fifo.m_fifo = nullptr;
}

FFmpeg_AudioFifo& FFmpeg_AudioFifo::operator=(FFmpeg_AudioFifo&& fifo) noexcept
{
    if (this != &fifo)
    {
        reset();
        m_fifo = fifo.m_fifo;
        fifo.m_fifo = nullptr;
    }
    return *this;
}

int FFmpeg_AudioFifo::alloc(AVSampleFormat sample_fmt, int channels, int nb_samples)
{
    reset();

    m_fifo = av_audio_fifo_alloc(sample_fmt, channels, nb_samples);
    if (m_fifo == nullptr)
    {
        return AVERROR(ENOMEM);
    }

    return 0;
}

void FFmpeg_AudioFifo::reset()
{
    if (m_fifo != nullptr)
    {
        av_audio_fifo_free(m_fifo);
        m_fifo = nullptr;
    }
}

bool FFmpeg_AudioFifo::empty() const
{
    return m_fifo == nullptr;
}

int FFmpeg_AudioFifo::size() const
{
    if (m_fifo == nullptr)
    {
        return 0;
    }
    return av_audio_fifo_size(m_fifo);
}

int FFmpeg_AudioFifo::realloc(int nb_samples)
{
    if (m_fifo == nullptr)
    {
        return AVERROR(EINVAL);
    }
    return av_audio_fifo_realloc(m_fifo, nb_samples);
}

int FFmpeg_AudioFifo::write(void **data, int nb_samples)
{
    if (m_fifo == nullptr)
    {
        return AVERROR(EINVAL);
    }
    return av_audio_fifo_write(m_fifo, data, nb_samples);
}

int FFmpeg_AudioFifo::read(void **data, int nb_samples)
{
    if (m_fifo == nullptr)
    {
        return AVERROR(EINVAL);
    }
    return av_audio_fifo_read(m_fifo, data, nb_samples);
}

AVAudioFifo* FFmpeg_AudioFifo::get()
{
    return m_fifo;
}

const AVAudioFifo* FFmpeg_AudioFifo::get() const
{
    return m_fifo;
}

AVAudioFifo* FFmpeg_AudioFifo::release()
{
    AVAudioFifo *fifo = m_fifo;
    m_fifo = nullptr;
    return fifo;
}

FFmpeg_AudioFifo::operator AVAudioFifo*()
{
    return m_fifo;
}

FFmpeg_AudioFifo::operator const AVAudioFifo*() const
{
    return m_fifo;
}
