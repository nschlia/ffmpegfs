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

#ifndef VCDPARSER_H
#define VCDPARSER_H

#pragma once

#ifdef USE_LIBVCD

#include <string>

using namespace std;

int parse_vcd(const string & path, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);
int check_vcd(const string & path, void *buf = nullptr, fuse_fill_dir_t filler = nullptr);

#endif // USE_LIBVCD
#endif // VCDPARSER_H
