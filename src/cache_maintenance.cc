/*
 * Copyright (C) 2017-2021 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file
 * @brief #Cache maintenance implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2021 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "cache_maintenance.h"
#include "ffmpegfs.h"
#include "ffmpeg_utils.h"
#include "logging.h"

#include <signal.h>
#include <unistd.h>
#include <sys/shm.h>        /* shmat(), IPC_RMID        */
#include <semaphore.h>      /* sem_open(), sem_destroy(), sem_wait().. */

#define CLOCKID         CLOCK_REALTIME          /**< @brief Use real time clock here. */
#define SIGMAINT        SIGRTMIN                /**< @brief Map maintenance signal. */

#define SEM_OPEN_FILE   "/" PACKAGE_NAME "_04806785-b5fb-4615-ba56-b30a2946e80b"    /**< @brief Shared semaphore name, should be unique system wide. */

static sigset_t mask;           /**< @brief Process mask for timer */
static timer_t  timerid;        /**< @brief Timer id */

static sem_t *  sem;            /**< @brief Semaphore used to synchronise between master and slave processes */
static int      shmid;          /**< @brief Shared memory segment ID */
static pid_t *  pid_master;     /**< @brief PID of master process */
static bool     master;         /**< @brief If true, we are master */

static void maintenance_handler(int sig, __attribute__((unused)) siginfo_t *si, __attribute__((unused)) void *uc);
static bool start_timer(time_t interval);
static bool stop_timer();
static bool link_up();
static void master_check();
static bool link_down();

/**
  * @brief Run maintenance handler
  * @param[in] sig - Signal, must be SIGMAINT.
  * @param[in] si
  * @param[in] uc
  */
static void maintenance_handler(int sig, __attribute__((unused)) siginfo_t *si, __attribute__((unused)) void *uc)
{
    if (sig != SIGMAINT)
    {
        // Wrong signal. Should never happen.
        return;
    }

    master_check();

    if (master)
    {
        Logging::info(nullptr, "Running periodic cache maintenance.");
        transcoder_cache_maintenance();
    }
}

/**
 * @brief Start the maintenance timer at predefined interval.
 * @param[in] interval - Timer interval in seconds.
 * @return On success, returns true. On error, returns false. Check errno for details.
 */
static bool start_timer(time_t interval)
{
    struct sigevent sev;
    struct itimerspec its;
    long long freq_nanosecs;
    struct sigaction sa;

    freq_nanosecs = interval * 1000000000LL;

    Logging::trace(nullptr, "Starting maintenance timer with %1period.", format_time(interval).c_str());

    // Establish maintenance_handler for timer signal
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = maintenance_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGMAINT, &sa, nullptr) == -1)
    {
        Logging::error(nullptr, "start_timer(): sigaction failed: (%1) %2", errno, strerror(errno));
        return false;
    }

    // Block timer signal temporarily
    sigemptyset(&mask);
    sigaddset(&mask, SIGMAINT);
    if (sigprocmask(SIG_SETMASK, &mask, nullptr) == -1)
    {
        Logging::error(nullptr, "start_timer(): sigprocmask(SIG_SETMASK) failed: (%1) %2", errno, strerror(errno));
        return false;
    }

    // Create the timer
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGMAINT;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCKID, &sev, &timerid) == -1)
    {
        Logging::error(nullptr, "start_timer(): timer_create failed: (%1) %2", errno, strerror(errno));
        return false;
    }

    // Start the timer
    its.it_value.tv_sec = static_cast<time_t>(freq_nanosecs / 1000000000);
    its.it_value.tv_nsec = static_cast<long>(freq_nanosecs % 1000000000);
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timer_settime(timerid, 0, &its, nullptr) == -1)
    {
        Logging::error(nullptr, "start_timer(): timer_settime failed: (%1) %2", errno, strerror(errno));
        return false;
    }

    if (sigprocmask(SIG_UNBLOCK, &mask, nullptr) == -1)
    {
        Logging::error(nullptr, "start_timer(): sigprocmask(SIG_UNBLOCK) failed: (%1) %2", errno, strerror(errno));
    }

    Logging::trace(nullptr, "Maintenance timer started successfully.");

    return true;
}

/**
 * @brief Stop the maintenance timer.
 * @return On success, returns true. On error, returns false. Check errno for details.
 */
static bool stop_timer()
{
    Logging::info(nullptr, "Stopping maintenance timer.");

    if (timer_delete(timerid) == -1)
    {
        Logging::error(nullptr, "stop_timer(): timer_delete failed: (%1) %2", errno, strerror(errno));
        return false;
    }

    return true;
}

/**
 * @brief Set system wide inter process link up.
 * @return On success, returns true. On error, returns false. Check errno for details.
 */
static bool link_up()
{
    key_t shmkey;

    Logging::debug(nullptr, "Activating " PACKAGE " inter-process link.");

    // initialise a shared variable in shared memory
    shmkey = ftok ("/dev/null", 5);     // valid directory name and a number

    if (shmkey == -1)
    {
        Logging::error(nullptr, "link_up(): ftok error (%1) %2", errno, strerror(errno));
        return false;
    }

    // First try to open existing memory.
    shmid = shmget (shmkey, sizeof (pid_t), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (shmid != -1)
    {
        // Shared memory already exists, seems we are client.
        master = false;
    }
    else
    {
        // Ignore error at first, try to create memory.
        shmid = shmget (shmkey, sizeof (pid_t), IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (shmid != -1)
        {
            // Shared memory freshly created, seems we are master.
            master = true;
        }
        else
        {
            Logging::error(nullptr, "link_up(): shmget error (%1) %2", errno, strerror(errno));
            return false;
        }
    }

    pid_master = static_cast<pid_t *>(shmat (shmid, nullptr, 0));   // attach pid_master to shared memory

    if (master)
    {
        *pid_master = getpid();
        Logging::info(nullptr, "Process with PID %1 is now master.", *pid_master);
    }
    else
    {
        Logging::info(nullptr, "Process with PID %1 is now client, master is PID %2.", getpid(), *pid_master);
    }

    // Also create inter-process semaphore.
    // First try to open existing semaphore.
    sem = sem_open(SEM_OPEN_FILE, 0, 0, 0);

    if (sem == SEM_FAILED)
    {
        if (errno == ENOENT)
        {
            // If semaphore does not exist, then try to create one.
            sem = sem_open(const_cast<const char *>(SEM_OPEN_FILE), O_CREAT | O_EXCL, 0777, 1);
        }

        if (sem == SEM_FAILED)
        {
            Logging::error(nullptr, "link_up(): sem_open error (%1) %2", errno, strerror(errno));
            link_down();
            return false;
        }
    }

    return true;
}

/**
 * @brief Check if a master is already running. We become master if not.
 */
static void master_check()
{
    pid_t pid_self = getpid();

    if (*pid_master == pid_self)
    {
        Logging::trace(nullptr, "PID %1 is already master.", pid_self);
        return;
    }

    sem_wait(sem);

    // Check if master process still exists
    int master_running = (getpgid(*pid_master) >= 0);

    Logging::trace(nullptr, "Master with PID %1 is %2 running.", *pid_master, master_running ? "still" : "NOT");

    if (!master_running)
    {
        Logging::info(nullptr, "Master with PID %1 is gone. PID %2 taking over as new master.", *pid_master, pid_self);

        // Register us as master
        *pid_master = pid_self;
        master = true;
    }

    sem_post(sem);
}

/**
 * @brief Set system wide inter process link down.
 * @return On success, returns true. On error, returns false. Check errno for details.
 */
static bool link_down()
{
    struct shmid_ds buf;
    bool success = true;

    Logging::info(nullptr, "Shutting " PACKAGE " inter-process link down.");

    if (sem_close(sem))
    {
        Logging::error(nullptr, "link_down(): sem_close error (%1) %2", errno, strerror(errno));
        success = false;
    }

    // shared memory detach
    if (shmdt (pid_master))
    {
        Logging::error(nullptr, "link_down(): shmdt error (%1) %2", errno, strerror(errno));
        success = false;
    }

    if (shmctl(shmid, IPC_STAT, &buf))
    {
        Logging::error(nullptr, "link_down(): shmctl error (%1) %2", errno, strerror(errno));
        success = false;
    }
    else
    {
        if (!buf.shm_nattch)
        {
            if (shmctl (shmid, IPC_RMID, nullptr))
            {
                Logging::error(nullptr, "link_down(): shmctl error (%1) %2", errno, strerror(errno));
                success = false;
            }

            // unlink prevents the semaphore existing forever
            // if a crash occurs during the execution
            if (sem_unlink(SEM_OPEN_FILE))
            {
                Logging::error(nullptr, "link_down(): sem_unlink error (%1) %2", errno, strerror(errno));
                success = false;
            }
        }
    }

    return success;
}

bool start_cache_maintenance(time_t interval)
{
    // Start link
    if (!link_up())
    {
        return false;
    }

    // Now start timer
    return start_timer(interval);
}

bool stop_cache_maintenance()
{
    bool success = true;

    // Stop timer first
    if (!stop_timer())
    {
        success = false;
    }

    // Now shut down link
    if (!link_down())
    {
        success = false;
    }

    return success;
}
