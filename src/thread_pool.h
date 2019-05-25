#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

#include <unistd.h>

class thread_pool
{
    typedef struct THREADINFO                       /**< Thread info structure */
    {
        void (*m_thread_func)(void *);              /**< Job function pointer */
        void *m_opaque;                             /**< Parameter for job function */
    } THREADINFO;

public:
    explicit thread_pool(unsigned int num_threads = std::thread::hardware_concurrency() * 4);
    virtual ~thread_pool();

    void        init(unsigned int num_threads = 0);
    void        tear_down(bool silent = false);

    bool        new_thread(void (*thread_func)(void *), void *opaque);

private:
    static void loop_function_starter(thread_pool &tp);
    void        loop_function();

protected:
    std::vector<std::thread>    m_thread_pool;      /**< Thread pool */
    std::mutex                  m_queue_mutex;      /**< Mutex for critical section */
    std::condition_variable     m_queue_condition;  /**< Condition for critical section */
    std::queue<THREADINFO>      m_thread_queue;     /**< Thread queue parameters */
    volatile bool               m_queue_shutdown;   /**< If true all threads have been shut down */
    unsigned int                m_num_threads;      /**< Max. number of threads. Defaults to 4 x number of CPU cores. */
    unsigned int                m_cur_threads;      /**< Current number of threads. */
};

#endif // THREAD_POOL_H
