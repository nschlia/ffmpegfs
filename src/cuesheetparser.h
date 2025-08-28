/*
 * Copyright (C) 2018-2025 by Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file cuesheetparser.h
 * @brief Clue sheet parser
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2018-2025 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef CUESHEETPARSER_H
#define CUESHEETPARSER_H

#pragma once

#include <string>

#define TRACKDIR            "tracks"                        ///<* Extension of virtual tracks directory

struct AVFormatContext;

/**
 * @brief Get number of titles in cue sheet
 * @param[in] filename - Path to check
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @note buf and filler can be nullptr. In that case the call will run faster, so these parameters should only be passed if to be filled in.
 * @return -errno or number or titles in cue sheet
 */
int check_cuesheet(const std::string & filename, void *buf = nullptr, fuse_fill_dir_t filler = nullptr);

#endif // CUESHEETPARSER_H
