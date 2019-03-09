/*
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
 * @brief %Cache maintenance
 *
 * Creates a POSIX timer that starts the cache maintenance in preset
 * intervals. To ensure that only one instance of FFmpegfs cleans up
 * the cache a shared memory area and a named semaphore is also created.
 *
 * The first FFmpegfs process acts as master, all subsequently started
 * instances will be clients. If the master process goes away one of
 * the clients will automatically take over as master.
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef CACHE_MAINTENANCE_H
#define CACHE_MAINTENANCE_H

#pragma once

#include <time.h>

/**
 * @brief Start cache maintenance timer.
 * @param[in] interval - Interval in seconds to run timer at.
 * @return On success, returns true. On error, returns false. Check errno for details.
 */
bool start_cache_maintenance(time_t interval);
/**
 * @brief Stop cache maintenance timer.
 * @return On success, returns true. On error, returns false. Check errno for details.
 */
bool stop_cache_maintenance();

#endif // CACHE_MAINTENANCE_H
