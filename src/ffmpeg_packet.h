/*
 * Copyright (C) 2017-2026 by Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * @file ffmpeg_packet.h
 * @brief FFmpeg AVPacket RAII wrapper
 *
 * @ingroup ffmpegfs
 */

#ifndef FFMPEG_PACKET_H
#define FFMPEG_PACKET_H

#pragma once

#include "ffmpeg_utils.h"

/**
 * @brief RAII wrapper for AVPacket.
 *
 * AVPacket must not be stack allocated anymore. Newer FFmpeg versions deprecated
 * av_init_packet() and explicitly warn that sizeof(AVPacket) is no longer meant
 * to be part of the public ABI. This wrapper always uses av_packet_alloc() and
 * releases the packet with av_packet_free().
 */
class FFmpeg_Packet
{
public:
    /**
     * @brief Allocate an empty packet.
     */
    explicit FFmpeg_Packet(int stream_index = INVALID_STREAM);
    /**
     * @brief Clone an existing packet.
     */
    FFmpeg_Packet(const AVPacket *packet);  // cppcheck-suppress noExplicitConstructor
    /**
     * @brief Deep-copy construct from another wrapper.
     */
    FFmpeg_Packet(const FFmpeg_Packet &packet);
    /**
     * @brief Move construct from another wrapper.
     */
    FFmpeg_Packet(FFmpeg_Packet &&packet) noexcept;
    /**
     * @brief Free the underlying packet.
     */
    virtual ~FFmpeg_Packet();

    /**
     * @brief Deep-copy assign from another wrapper.
     */
    FFmpeg_Packet& operator=(const FFmpeg_Packet &packet) noexcept;
    /**
     * @brief Move assign from another wrapper.
     */
    FFmpeg_Packet& operator=(FFmpeg_Packet &&packet) noexcept;
    /**
     * @brief Deep-copy assign from an AVPacket.
     */
    FFmpeg_Packet& operator=(const AVPacket *packet) noexcept;

    /**
     * @brief Get result of last operation.
     * @return 0 if successful, or a negative AVERROR value.
     */
    int             res() const;
    /**
     * @brief Clone packet to a new AVPacket*. Caller owns the returned packet.
     */
    AVPacket*       clone() const;
    /**
     * @brief Unreference packet payload, keep the AVPacket object allocated.
     */
    void            unref();
    /**
     * @brief Free the underlying packet.
     */
    void            free();
    /**
     * @brief Access the underlying packet.
     */
    AVPacket*       get();
    /**
     * @brief Access the underlying packet.
     */
    const AVPacket* get() const;

    /**
     * @brief operator AVPacket*: behave like a packet pointer for FFmpeg APIs.
     */
    operator        AVPacket*();
    /**
     * @brief operator const AVPacket*: behave like a const packet pointer for FFmpeg APIs.
     */
    operator const  AVPacket*() const;
    /**
     * @brief operator ->: behave like a packet pointer.
     */
    AVPacket*       operator->();
    /**
     * @brief operator ->: behave like a const packet pointer.
     */
    const AVPacket* operator->() const;

protected:
    AVPacket *  m_packet;   /**< @brief Pointer to underlying AVPacket struct */
    int         m_res;      /**< @brief 0 if last operation was successful, or negative AVERROR value */
};

#endif // FFMPEG_PACKET_H
