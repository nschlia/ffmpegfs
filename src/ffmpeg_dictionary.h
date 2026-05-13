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
 * @brief FFmpeg AVDictionary RAII wrapper.
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
    /**
     * @brief Construct an empty dictionary wrapper.
     */
    FFmpeg_Dictionary();

    /**
     * @brief Release the owned dictionary, if any.
     */
    ~FFmpeg_Dictionary();

    FFmpeg_Dictionary(const FFmpeg_Dictionary&) = delete;
    FFmpeg_Dictionary& operator=(const FFmpeg_Dictionary&) = delete;

    /**
     * @brief Move-construct a dictionary wrapper.
     * @param[in,out] dict Source wrapper whose dictionary ownership is transferred.
     */
    FFmpeg_Dictionary(FFmpeg_Dictionary&& dict) noexcept;

    /**
     * @brief Move-assign a dictionary wrapper.
     * @param[in,out] dict Source wrapper whose dictionary ownership is transferred.
     * @return Reference to this wrapper.
     */
    FFmpeg_Dictionary& operator=(FFmpeg_Dictionary&& dict) noexcept;

    /**
     * @brief Free the owned dictionary and reset the wrapper to empty.
     */
    void            reset();

    /**
     * @brief Check whether the wrapper currently owns a dictionary.
     * @return true if no dictionary is owned, false otherwise.
     */
    bool            empty() const;

    /**
     * @brief Get the owned FFmpeg dictionary pointer.
     * @return Mutable AVDictionary pointer, or nullptr if empty.
     */
    AVDictionary*   get();

    /**
     * @brief Get the owned FFmpeg dictionary pointer.
     * @return Const AVDictionary pointer, or nullptr if empty.
     */
    const AVDictionary* get() const;

    /**
     * @brief Get a writable pointer-to-pointer for FFmpeg APIs.
     *
     * Any currently owned dictionary is freed first so that FFmpeg can write a
     * fresh dictionary pointer without leaking the previous one.
     *
     * @return Address of the managed dictionary pointer.
     */
    AVDictionary**  address();

    /**
     * @brief Release ownership without freeing the dictionary.
     * @return Previously owned AVDictionary pointer, or nullptr if empty.
     */
    AVDictionary*   release();

    /**
     * @brief Convert to the underlying mutable dictionary pointer.
     */
    operator        AVDictionary*();

    /**
     * @brief Convert to the underlying const dictionary pointer.
     */
    operator const  AVDictionary*() const;

private:
    AVDictionary *  m_dict;     /**< @brief Pointer to underlying AVDictionary. */
};

#endif // FFMPEG_DICTIONARY_H
