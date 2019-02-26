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
 * @brief Video CD and Super Video CD parser
 *
 * This is only available if built with -DUSE_LIBVCD parameter.
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2018-2019 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef VCDPARSER_H
#define VCDPARSER_H

#pragma once

#ifdef USE_LIBVCD

/** @brief Get number of chapters on S/VCD
 *  @param[in] path - path to check
 *  @param[in, out] buf - the buffer passed to the readdir() operation.
 *  @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 *	@note buf and filler can be nullptr. In that case the call will run faster, so these parameters should only be passed if to be filled in.
 *  @return -errno or number of chapters on S/VCD.
 */
int check_vcd(const std::string & path, void *buf = nullptr, fuse_fill_dir_t filler = nullptr);

#endif // USE_LIBVCD
#endif // VCDPARSER_H
