/*
 * Copyright (C) 2017-2026 by Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * @file ffmpeg_dictionary.h
 * @brief FFmpeg AVDictionary RAII wrapper
 *
 * @ingroup ffmpegfs
 */

#ifndef FFMPEG_DICTIONARY_H
#define FFMPEG_DICTIONARY_H

#pragma once

struct AVDictionary;

/**
 * @brief RAII wrapper for AVDictionary.
 *
 * Owns an AVDictionary created by av_dict_set() / FFmpeg APIs and releases it
 * with av_dict_free(). The wrapper is movable but not copyable.
 */
class FFmpeg_Dictionary
{
public:
    FFmpeg_Dictionary();
    ~FFmpeg_Dictionary();

    FFmpeg_Dictionary(const FFmpeg_Dictionary&) = delete;
    FFmpeg_Dictionary& operator=(const FFmpeg_Dictionary&) = delete;

    FFmpeg_Dictionary(FFmpeg_Dictionary&& dict) noexcept;
    FFmpeg_Dictionary& operator=(FFmpeg_Dictionary&& dict) noexcept;

    void            reset();
    bool            empty() const;

    AVDictionary*   get();
    const AVDictionary* get() const;
    AVDictionary**  address();
    AVDictionary*   release();

    operator        AVDictionary*();
    operator const  AVDictionary*() const;

private:
    AVDictionary *  m_dict;     /**< @brief Pointer to underlying AVDictionary */
};

#endif // FFMPEG_DICTIONARY_H
