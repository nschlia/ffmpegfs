/*
 * Copyright (C) 2017-2026 Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * @file ffmpeg_packet.cc
 * @brief FFmpeg_Packet class implementation
 *
 * @ingroup ffmpegfs
 */

#ifdef __cplusplus
extern "C" {
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <libavcodec/packet.h>
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

#include "ffmpeg_packet.h"

FFmpeg_Packet::FFmpeg_Packet(int stream_index) :
    m_packet(av_packet_alloc()),
    m_res(0)
{
    m_res = (m_packet != nullptr) ? 0 : AVERROR(ENOMEM);

    if (m_packet != nullptr)
    {
        m_packet->stream_index = stream_index;
    }
}

FFmpeg_Packet::FFmpeg_Packet(const AVPacket *packet) :
    m_packet(nullptr),
    m_res(0)
{
    if (packet != nullptr)
    {
        m_packet = av_packet_clone(packet);
        m_res = (m_packet != nullptr) ? 0 : AVERROR(ENOMEM);
    }
    else
    {
        m_res = AVERROR(EINVAL);
    }
}

FFmpeg_Packet::FFmpeg_Packet(const FFmpeg_Packet &packet) :
    m_packet(nullptr),
    m_res(0)
{
    if (packet.m_packet != nullptr)
    {
        m_packet = av_packet_clone(packet.m_packet);
        m_res = (m_packet != nullptr) ? 0 : AVERROR(ENOMEM);
    }
    else
    {
        m_res = AVERROR(EINVAL);
    }
}

FFmpeg_Packet::FFmpeg_Packet(FFmpeg_Packet &&packet) noexcept :
    m_packet(packet.m_packet),
    m_res(packet.m_res)
{
    packet.m_packet = nullptr;
    packet.m_res = AVERROR(EINVAL);
}

FFmpeg_Packet::~FFmpeg_Packet()
{
    free();
}

FFmpeg_Packet& FFmpeg_Packet::operator=(const FFmpeg_Packet &packet) noexcept
{
    if (this != &packet && m_packet != packet.m_packet)
    {
        AVPacket *new_packet = nullptr;
        int new_res = AVERROR(EINVAL);

        if (packet.m_packet != nullptr)
        {
            new_packet = av_packet_clone(packet.m_packet);
            new_res = (new_packet != nullptr) ? 0 : AVERROR(ENOMEM);
        }

        free();

        m_packet = new_packet;
        m_res = new_res;
    }

    return *this;
}

FFmpeg_Packet& FFmpeg_Packet::operator=(FFmpeg_Packet &&packet) noexcept
{
    if (this != &packet)
    {
        free();

        m_packet = packet.m_packet;
        m_res = packet.m_res;

        packet.m_packet = nullptr;
        packet.m_res = AVERROR(EINVAL);
    }

    return *this;
}

FFmpeg_Packet& FFmpeg_Packet::operator=(const AVPacket *packet) noexcept
{
    if (m_packet != packet)
    {
        AVPacket *new_packet = nullptr;
        int new_res = AVERROR(EINVAL);

        if (packet != nullptr)
        {
            new_packet = av_packet_clone(packet);
            new_res = (new_packet != nullptr) ? 0 : AVERROR(ENOMEM);
        }

        free();

        m_packet = new_packet;
        m_res = new_res;
    }

    return *this;
}

int FFmpeg_Packet::res() const
{
    return m_res;
}

AVPacket* FFmpeg_Packet::clone() const
{
    return av_packet_clone(m_packet);
}

void FFmpeg_Packet::unref()
{
    if (m_packet != nullptr)
    {
        av_packet_unref(m_packet);
    }
}

void FFmpeg_Packet::free()
{
    if (m_packet != nullptr)
    {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }
}

AVPacket* FFmpeg_Packet::get()
{
    return m_packet;
}

const AVPacket* FFmpeg_Packet::get() const
{
    return m_packet;
}

FFmpeg_Packet::operator AVPacket*()
{
    return m_packet;
}

FFmpeg_Packet::operator const AVPacket*() const
{
    return m_packet;
}

AVPacket* FFmpeg_Packet::operator->()
{
    return m_packet;
}

const AVPacket* FFmpeg_Packet::operator->() const
{
    return m_packet;
}
