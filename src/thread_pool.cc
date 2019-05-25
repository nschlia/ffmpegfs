/*
 * Copyright (C) 2006-2008 David Collett
 * Copyright (C) 2008-2013 K. Henriksson
 * Copyright (C) 2017-2019 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file
 * @brief Thread pool class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2019 FFmpeg support by Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "thread_pool.h"
#include "logging.h"
#include "config.h"

thread_pool::thread_pool(unsigned int num_threads)
    : m_queue_shutdown(false)
    , m_num_threads(num_threads)
    , m_cur_threads(0)

{
}

thread_pool::~thread_pool()
{
    tear_down(true);
}

void thread_pool::loop_function_starter(thread_pool & tp)
{
    tp.loop_function();
}

void thread_pool::loop_function()
{
    unsigned int thread_no = ++m_cur_threads;

    Logging::trace(nullptr, "Starting pool thread no. %1 with id 0x%<%" FFMPEGFS_FORMAT_PTHREAD_T ">2.", thread_no, pthread_self());

    while (true)
    {
        THREADINFO info;
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_queue_condition.wait(lock, [this]{ return (!m_thread_queue.empty() || m_queue_shutdown); });

            if (m_queue_shutdown)
            {
                lock.unlock();
                break;
            }

            Logging::trace(nullptr, "Starting job using pool thread no. %1 with id 0x%<%" FFMPEGFS_FORMAT_PTHREAD_T ">2.", thread_no, pthread_self());
            info = m_thread_queue.front();
            m_thread_queue.pop();
        }

        info.m_thread_func(info.m_opaque);
    }

    Logging::trace(nullptr, "Exiting pool thread no. %1 with id 0x%<%" FFMPEGFS_FORMAT_PTHREAD_T ">2.", thread_no, pthread_self());
}

bool thread_pool::new_thread(void (*thread_func)(void *), void *opaque)
{
    if (!m_queue_shutdown)
    {
        Logging::trace(nullptr, "Queueing new thread. %1 threads already in queue.", m_thread_pool.size());

        {
            //std::unique_lock<std::mutex> lock(m_queue_mutex);
            std::lock_guard<std::mutex> lock(m_queue_mutex);

            THREADINFO info;

            info.m_thread_func  = thread_func;
            info.m_opaque       = opaque;
            m_thread_queue.push(info);
        }

        //m_queue_mutex.lock();
        m_queue_condition.notify_one();
        //m_queue_mutex.unlock();

        return true;
    }
    else
    {
        return false;
    }
}

void thread_pool::init(unsigned int num_threads /*= 0*/)
{
    if (num_threads)
    {
        m_num_threads = num_threads;
    }

    Logging::info(nullptr, "Initialising thread pool with max. %1 threads.", m_num_threads);

    for(unsigned int i = 0; i < m_num_threads; i++)
    {
        m_thread_pool.push_back(std::thread(&thread_pool::loop_function_starter, std::ref(*this)));
    }
}

void thread_pool::tear_down(bool silent)
{
    if (!silent)
    {
        Logging::debug(nullptr, "Tearing down thread pool. %1 threads still in queue.", m_thread_queue.size());
    }

    m_queue_mutex.lock();
    m_queue_shutdown = true;
    m_queue_mutex.unlock();
    m_queue_condition.notify_all();

    while (!m_thread_pool.empty())
    {
        m_thread_pool.back().join();
        m_thread_pool.pop_back();
    }
}

