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
 * @brief FFmpeg AVAudioFifo RAII wrapper.
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
    /**
     * @brief Construct an empty FIFO wrapper.
     */
    FFmpeg_AudioFifo();

    /**
     * @brief Construct and allocate an audio FIFO.
     * @param[in] sample_fmt Sample format stored in the FIFO.
     * @param[in] channels Number of audio channels.
     * @param[in] nb_samples Initial allocation size in samples.
     */
    FFmpeg_AudioFifo(AVSampleFormat sample_fmt, int channels, int nb_samples = 1);

    /**
     * @brief Release the owned FIFO, if any.
     */
    ~FFmpeg_AudioFifo();

    FFmpeg_AudioFifo(const FFmpeg_AudioFifo&) = delete;
    FFmpeg_AudioFifo& operator=(const FFmpeg_AudioFifo&) = delete;

    /**
     * @brief Move-construct a FIFO wrapper.
     * @param[in,out] fifo Source wrapper whose FIFO ownership is transferred.
     */
    FFmpeg_AudioFifo(FFmpeg_AudioFifo&& fifo) noexcept;

    /**
     * @brief Move-assign a FIFO wrapper.
     * @param[in,out] fifo Source wrapper whose FIFO ownership is transferred.
     * @return Reference to this wrapper.
     */
    FFmpeg_AudioFifo& operator=(FFmpeg_AudioFifo&& fifo) noexcept;

    /**
     * @brief Allocate a new audio FIFO, replacing any existing one.
     * @param[in] sample_fmt Sample format stored in the FIFO.
     * @param[in] channels Number of audio channels.
     * @param[in] nb_samples Initial allocation size in samples.
     * @return 0 on success, or a negative FFmpeg error code.
     */
    int             alloc(AVSampleFormat sample_fmt, int channels, int nb_samples = 1);

    /**
     * @brief Free the owned FIFO and reset the wrapper to empty.
     */
    void            reset();

    /**
     * @brief Check whether the wrapper currently owns a FIFO.
     * @return true if no FIFO is owned, false otherwise.
     */
    bool            empty() const;

    /**
     * @brief Return the number of samples currently stored in the FIFO.
     * @return Number of queued samples, or 0 if the wrapper is empty.
     */
    int             size() const;

    /**
     * @brief Resize the FIFO allocation.
     * @param[in] nb_samples New allocation size in samples.
     * @return 0 on success, or a negative FFmpeg error code.
     */
    int             realloc(int nb_samples);

    /**
     * @brief Write audio samples into the FIFO.
     * @param[in] data Per-channel sample buffers as expected by FFmpeg.
     * @param[in] nb_samples Number of samples to write.
     * @return Number of samples written, or a negative FFmpeg error code.
     */
    int             write(void **data, int nb_samples);

    /**
     * @brief Read audio samples from the FIFO.
     * @param[out] data Per-channel destination buffers as expected by FFmpeg.
     * @param[in] nb_samples Number of samples to read.
     * @return Number of samples read, or a negative FFmpeg error code.
     */
    int             read(void **data, int nb_samples);

    /**
     * @brief Get the owned FFmpeg FIFO pointer.
     * @return Mutable AVAudioFifo pointer, or nullptr if empty.
     */
    AVAudioFifo*    get();

    /**
     * @brief Get the owned FFmpeg FIFO pointer.
     * @return Const AVAudioFifo pointer, or nullptr if empty.
     */
    const AVAudioFifo* get() const;

    /**
     * @brief Release ownership without freeing the FIFO.
     * @return Previously owned AVAudioFifo pointer, or nullptr if empty.
     */
    AVAudioFifo*    release();

    /**
     * @brief Convert to the underlying mutable FIFO pointer.
     */
    operator        AVAudioFifo*();

    /**
     * @brief Convert to the underlying const FIFO pointer.
     */
    operator const  AVAudioFifo*() const;

private:
    AVAudioFifo *   m_fifo;     /**< @brief Pointer to underlying AVAudioFifo. */
};

#endif // FFMPEG_AUDIOFIFO_H
