/*
 * dvdparser header for ffmpegfs
 *
 * Copyright (C) 2017-2018 Norbert Schlia (nschlia@oblivion-software.de)
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

#ifndef DVDPARSER_H
#define DVDPARSER_H

#pragma once

#ifdef USE_LIBDVD

#include <string>

int parse_dvd(const std::string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);
int check_dvd(const std::string & path, void *buf = nullptr, fuse_fill_dir_t filler = nullptr);

#endif // USE_LIBDVD
#endif // DVDPARSER_H
