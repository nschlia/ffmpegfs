/*
 * Copyright (C) 2019-2025 Norbert Schlia (nschlia@oblivion-software.de)
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

/**
 * @file thread_pool.cc
 * @brief Thread pool class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2019-2025 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "thread_pool.h"
#include "logging.h"
#include "config.h"

thread_pool::thread_pool(unsigned int num_threads)
    : m_queue_shutdown(false)
    , m_num_threads(num_threads)
    , m_cur_threads(0)
    , m_threads_running(0)
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

    while (true)
    {
        FunctionPointer info;
        {
            std::unique_lock<std::mutex> lock_queue_mutex(m_queue_mutex);
            m_queue_cond.wait(lock_queue_mutex, [this]{ return (!m_thread_queue.empty() || m_queue_shutdown); });

            if (m_queue_shutdown)
            {
                lock_queue_mutex.unlock();
                break;
            }

            Logging::trace(nullptr, "Starting job taking pool thread no. %1 with id 0x%<" FFMPEGFS_FORMAT_PTHREAD_T ">2.", thread_no, pthread_self());
            info = m_thread_queue.front();
            m_thread_queue.pop();
        }

        int ret = info();

        Logging::trace(nullptr, "The job using pool thread no. %1 with id 0x%<" FFMPEGFS_FORMAT_PTHREAD_T ">2 has exited with return code %3.", thread_no, pthread_self(), ret);
    }
}

bool thread_pool::schedule_thread(FunctionPointer &&func)
{
    if (!m_queue_shutdown)
    {
        Logging::trace(nullptr, "Queuing up a new thread. There are %1 threads in the pool.", m_thread_pool.size());

        {
            std::lock_guard<std::mutex> lock_queue_mutex(m_queue_mutex);

            m_thread_queue.push(func);
        }

        m_queue_cond.notify_one();

        return true;
    }
    else
    {
        return false;
    }
}

unsigned int thread_pool::current_running() const
{
    return m_threads_running;
}

unsigned int thread_pool::current_queued()
{
    std::lock_guard<std::mutex> lock_queue_mutex(m_queue_mutex);

    return static_cast<unsigned int>(m_thread_queue.size());
}

unsigned int thread_pool::pool_size() const
{
    return static_cast<unsigned int>(m_thread_pool.size());
}

int thread_pool::init(unsigned int num_threads /*= 0*/)
{
    if (!m_thread_pool.empty())
    {
        Logging::warning(nullptr, "The thread pool already initialised");
        return 0;
    }

    if (num_threads)
    {
        m_num_threads = num_threads;
    }

    Logging::info(nullptr, "The thread pool is being initialised with a maximum of %1 threads.", m_num_threads);

    for(unsigned int i = 0; i < m_num_threads; i++)
    {
        m_thread_pool.emplace_back(std::thread(&thread_pool::loop_function_starter, std::ref(*this)));
    }

    return static_cast<int>(m_thread_pool.size());
}

void thread_pool::tear_down(bool silent)
{
    if (!silent)
    {
        Logging::debug(nullptr, "Tearing down the thread pool. There are %1 threads still in the pool.", m_thread_queue.size());
    }

    m_queue_mutex.lock();
    m_queue_shutdown = true;
    m_queue_mutex.unlock();
    m_queue_cond.notify_all();

    while (!m_thread_pool.empty())
    {
        m_thread_pool.back().join();
        m_thread_pool.pop_back();
    }
}

