Installation Instructions for ffmpegfs
======================================

Copyright (C) 2017-2018 Norbert Schlia (nschlia@oblivion-software.de)
This file was originally copyright (C) 2013-2014 K. Henriksson. 

It can be distributed under the terms of the GFDL 1.3 or later. 
See README.md for more information.

Prerequisites
-------------

ffmpegfs uses FFMEG lib for decoding/encoding. It requires the following
libraries:

* gcc and g++ compilers

* fuse (>= 2.6.0)
* sqlite3 (>= 3.7.13)

* libavutil      (>= 54.3.0)
* libavcodec     (>= 56.1.0)
* libavformat    (>= 56.1.0)
* libavfilter    (>= 5.40.0)
* libavresample  (>= 2.1.0)
* libswscale     (>= 3.0.0)
* libswresample  (>= 1.0.0)

For optional DVD support you need the following libraries

* libdvdread      (>= 5.0.0)
* libdvdnav       (>= 5.0.0)

For optional Bluray support you need the following library

* libbluray       (>= 0.6.2)

If building from git, you'll also need:

* autoconf
* automake
* asciidoc
* xmllint
* xmlto

The commands to install just the first prerequisites follow.

Please read the "Supported Linux Distributions" chapter in README.md 
for details.

On Debian:

    aptitude install gcc g++

    aptitude install libfuse-dev libsqlite3-dev libavcodec-dev libavformat-dev libswresample-dev libavutil-dev libswscale-dev
    
To get DVD support:

    aptitude install libdvdread-dev libdvdnav-dev

To get Bluray support:

    aptitude install libbluray-dev

On Ubuntu use the same command with `apt-get` in place of `aptitude`.

On Suse (please read notes before continuing):

    zypper install gcc gcc-c++

    zypper install fuse-devel libsqlite3-devel libavcodec-devel libavformat-devel libswresample-devel libavutil-devel libswscale-devel

To get DVD support:

    zypper install libdvdread-devel libdvdnav-devel
    
To get Bluray support:

    zypper install libbluray-devel

Suse includes non-proprietary codecs with FFmpeg only, namely mp3, AAC and H264
are *not* available which renders this library next to usesless. But FFmpeg can 
be built from source, see https://trac.ffmpeg.org/wiki/CompilationGuide and check
"FFmpeg compile notes" below.

On Red Hat:

    yum install gcc g++

    yum install fuse-devel sqlite-devel

To get DVD support:

    yum install libdvdread-devel libdvdnav-devel
    
To get Bluray support:

    yum install libbluray-devel

Red Hat does not provide FFmpeg from its repositories. It must be built
from source code, see this guide: https://trac.ffmpeg.org/wiki/CompilationGuide/Centos

If you want to build the documentation you will find "asciidoc" missing from
the Red Hat repositories. To get it use a beta repository:

    yum --enablerepo=rhel-7-server-optional-beta-rpms install asciidoc

FFmpeg compile notes:

FFmpeg must be built with at least libx264, libfdk_aac and libmp3lame support. 
Other libraries, e.g. ogg, Windows Media or FLAC must be added when these
formats should be used as source.

If you run into this "ERROR: libmp3lame >= 3.98.3 not found" although you have built 
and installed libmp3lame you may find a solution here: 
https://stackoverflow.com/questions/35937403/error-libmp3lame-3-98-3-not-found

Installation
------------

ffmpegfs uses the GNU build system. If you are installing from git, you'll
need to first run:

    ./autogen.sh

If you are downloading a release, this has already been done for you. To
build and install, run:

    ./configure
    make
    make install
    
To build and run the check suite, do:    

    make checks
    
This will test audio conversion, tagging and size prediction.

NOTE: Image embedding is not yet implemented. The test has been
disabled at the moment.

Trouble Shooting
----------------

**If you run into this error:**
    
    Running autoreconf --install
    configure.ac:46: error: possibly undefined macro: AC_DEFINE
      If this token and others are legitimate, please use m4_pattern_allow.
      See the Autoconf documentation.
    autoreconf: /usr/bin/autoconf failed with exit status: 1

You are probably missing out on pkg-config, either it is not installed or
not in path. "apt-get install pkg-config" (on Debian or equivalent on other
Linux distributions) should help.

**If the videotag.php script does not work under PHP7**

The script runs fine under PHP5, but when upgrading to PHP7 (or using PHP7)
it suddenly stops showing the list of files.

Check the Apache2 error.log, you might see this:

"PHP Fatal error:  Uncaught Error: Call to undefined function utf8_encode() in index.php"

This is because for some reason utf8_encode() has been moved to the XML
library. Just do (or similar):

    aptitude install php7.0-xml
    systemctl restart apache2

And your troubles should be gone.

On Ubuntu use the same command with `apt-get` in place of `aptitude`.

**"make check": all audio checks fail**

Logfiles contain "bc: command not found", so the command line communicator is
missing.

Fix by installing it (or similar):

     aptitude install bc

On Ubuntu use the same command with `apt-get` in place of `aptitude`.

**Songs get cut short**

If songs do not play to the end and you are using SAMBA or NFS you're in trouble.

Happens when the files are transcoded on the fly, but never when file comes from
cache. This is because the result is never exactly what was predicted.

SAMBA fills files with zeroes if the result is smaller, or cuts off the rest if the 
file ist larger than predicted.

NFS arbitrarily sends the correct file, or one that is cut or padded like SAMBA.
This can be repeated as many times as one wants to - once the file is OK, once
not.

As of yet there seems to be no way around that. Maybe NFS or SAMBA can be configured
to cope with that, but how to is unknown to me.
