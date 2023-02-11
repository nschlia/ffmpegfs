/*
 * Copyright (C) 2019-2023 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file thread_pool.h
 * @brief Thread pool class implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2019-2023 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <atomic>

/**
 * @brief The thread_pool class.
 */
class thread_pool
{
    typedef struct THREADINFO                       /**< Thread info structure */
    {
        int (*m_thread_func)(void *);               /**< Job function pointer */
        void *m_opaque;                             /**< Parameter for job function */
    } THREADINFO;

public:
    /**
     * @brief Construct a thread_pool object.
     * @param[in] num_threads - Optional: number of threads to create in pool. Defaults to Defaults to 4 x number of CPU cores.
     */
    explicit thread_pool(unsigned int num_threads = std::thread::hardware_concurrency() * 4);
    /**
     * @brief Object destructor. Ends all threads and cleans up resources.
     */
    virtual ~thread_pool();

    /**
     * @brief Initialise thread pool.
     * @param[in] num_threads - Optional: number of threads to create in pool. Defaults to Defaults to 4x number of CPU cores.
     */
    void            init(unsigned int num_threads = 0);
    /**
     * @brief Shut down the thread pool.
     * @param[in] silent - If true, no log messages will be issued.
     */
    void            tear_down(bool silent = false);
    /**
     * @brief Schedule a new thread from pool.
     * @param[in] thread_func - Thread function to start.
     * @param[in] opaque - Parameter passed to thread function.
     * @return Returns true if thread was successfully scheduled, fals if not.
     */
    bool            schedule_thread(int (*thread_func)(void *), void *opaque);
    /**
     * @brief Get number of currently running threads.
     * @return Returns number of currently running threads.
     */
    unsigned int    current_running() const;
    /**
     * @brief Get number of currently queued threads.
     * @return Returns number of currently queued threads.
     */
    unsigned int    current_queued();
    /**
     * @brief Get current pool size.
     * @return Return current pool size.
     */
    unsigned int    pool_size() const;

private:
    /**
     * @brief Start loop function.
     * @param[in] tp - Thread pool object of caller.
     */
    static void     loop_function_starter(thread_pool &tp);
    /**
     * @brief Start loop function
     */
    void            loop_function();

protected:
    std::vector<std::thread>    m_thread_pool;      /**< Thread pool */
    std::mutex                  m_queue_mutex;      /**< Mutex for critical section */
    std::condition_variable     m_queue_cond;       /**< Condition for critical section */
    std::queue<THREADINFO>      m_thread_queue;     /**< Thread queue parameters */
    std::atomic_bool            m_queue_shutdown;   /**< If true all threads have been shut down */
    unsigned int                m_num_threads;      /**< Max. number of threads. Defaults to 4x number of CPU cores. */
    unsigned int                m_cur_threads;      /**< Current number of threads. */
    std::atomic_uint32_t        m_threads_running;  /**< Currently running threads. */
};

#endif // THREAD_POOL_H
