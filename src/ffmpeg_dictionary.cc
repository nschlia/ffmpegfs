/*
 * Copyright (C) 2017-2026 by Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * @file ffmpeg_dictionary.cc
 * @brief FFmpeg_Dictionary class implementation
 *
 * @ingroup ffmpegfs
 */

#ifdef __cplusplus
extern "C" {
#endif
// Disable annoying warnings outside our code
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <libavutil/dict.h>
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

#include "ffmpeg_dictionary.h"

FFmpeg_Dictionary::FFmpeg_Dictionary()
    : m_dict(nullptr)
{
}

FFmpeg_Dictionary::~FFmpeg_Dictionary()
{
    reset();
}

FFmpeg_Dictionary::FFmpeg_Dictionary(FFmpeg_Dictionary&& dict) noexcept
    : m_dict(dict.m_dict)
{
    dict.m_dict = nullptr;
}

FFmpeg_Dictionary& FFmpeg_Dictionary::operator=(FFmpeg_Dictionary&& dict) noexcept
{
    if (this != &dict)
    {
        reset();
        m_dict = dict.m_dict;
        dict.m_dict = nullptr;
    }

    return *this;
}

void FFmpeg_Dictionary::reset()
{
    if (m_dict != nullptr)
    {
        av_dict_free(&m_dict);
    }
}

bool FFmpeg_Dictionary::empty() const
{
    return m_dict == nullptr;
}

AVDictionary* FFmpeg_Dictionary::get()
{
    return m_dict;
}

const AVDictionary* FFmpeg_Dictionary::get() const
{
    return m_dict;
}

AVDictionary** FFmpeg_Dictionary::address()
{
    return &m_dict;
}

AVDictionary* FFmpeg_Dictionary::release()
{
    AVDictionary* dict = m_dict;
    m_dict = nullptr;
    return dict;
}

FFmpeg_Dictionary::operator AVDictionary*()
{
    return m_dict;
}

FFmpeg_Dictionary::operator const AVDictionary*() const
{
    return m_dict;
}
