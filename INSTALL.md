Installation Instructions for FFmpegfs
======================================

Copyright (C) 2017-2020 Norbert Schlia (nschlia@oblivion-software.de)
This file was originally copyright (C) 2013-2014 K. Henriksson.

This documentation may be distributed under the GNU Free Documentation License
(GFDL) 1.3 or later with no invariant sections, or alternatively under the GNU
General Public License (GPL) version 3 or later.

Prerequisites
-------------

FFmpegfs uses FFmpeg lib for decoding/encoding. It requires the following
libraries:

* gcc and g++ compilers

* fuse (>= 2.6.0)
* sqlite3 (>= 3.7.13)

* libavutil      (>= 54.3.0)
* libavcodec     (>= 56.1.0)
* libavformat    (>= 56.1.0)
* libavfilter    (>= 5.40.0)
* libswscale     (>= 3.0.0)

One of these is required, preferably libswresample as libavresample is deprecated.
It will be removed some day from FFmpeg API.

* libswresample  (>= 1.0.0)
* libavresample  (>= 2.1.0)

For optional DVD support you need the following libraries

* libdvdread      (>= 5.0.0)
* libdvdnav       (>= 5.0.0)

For optional Bluray support you need the following library

* libbluray       (>= 0.6.2)

The commands to install just the first prerequisites follow.

Please read the "Supported Linux Distributions" chapter in README.md
for details.

**On Debian:**

    apt-get install gcc g++ make pkg-config asciidoc-base w3m

    apt-get install fuse libfuse-dev libsqlite3-dev libavcodec-dev libavformat-dev libswresample-dev libavutil-dev libswscale-dev libavfilter-dev

To get DVD support:

    apt-get install libdvdread-dev libdvdnav-dev

To get Bluray support:

    apt-get install libbluray-dev

To "make doxy (build Doxygen documentation):

    apt-get install doxygen graphviz

To "make check" (run test suite):

    apt-get install libchromaprint-dev bc

**On Suse** (please read notes before continuing):

    zypper install gcc gcc-c++

    zypper install fuse fuse-devel libsqlite3-devel libavcodec-devel libavformat-devel libswresample-devel libavutil-devel libswscale-devel

To get DVD support:

    zypper install libdvdread-devel libdvdnav-devel

To get Bluray support:

    zypper install libbluray-devel

Suse includes non-proprietary codecs with FFmpeg only, namely mp3, AAC and H264
are *not* available which renders this library next to usesless. But FFmpeg can
be built from source, see https://trac.ffmpeg.org/wiki/CompilationGuide and check
"FFmpeg compile notes" below.

**On Red Hat:**

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

**On Funtoo Linux:**

To get fuse suppprt and chromaprint (for make check):

    emerge sys-fs/fuse
    emerge media-libs/chromaprint

To get FFmpeg with H264 etc. support, specify some "USE flags" when doing emerge:

Create file /etc/portage/package.use, e.g. "vi vi /etc/portage/package.use" and add this line:

    media-video/ffmpeg mp3 x264 opus vorbis vpx

This will enable H264, mp3, Opus and WebM support. Next...

    emerge media-libs/openh264
    emerge media-sound/twolame

    emerge media-video/ffmpeg

to build ffmpeg.

FFmpeg compile notes:

FFmpeg must be built with at least libx264, libfdk_aac and libmp3lame support.
Other libraries, e.g. ogg, Windows Media or FLAC must be added when these
formats should be used as source.

**If building from git, you'll also need:**

* autoconf
* automake
* asciidoc (or at least asciidoc-base to save disk space)
* w3m

For those who are lazy like me, simply copy and issue this command to get all prerequisites:

    apt-get install gcc g++ make pkg-config autoconf asciidoc-base w3m libchromaprint-dev bc doxygen graphviz

Then do

    ./autogen.sh
    ./configure

Installation from repository
----------------------------

FFmpegfs 1.10 has recently been added to Debian Bullseye and sid, so it is
available as binary distribution.

On Debian 11 Bullseye you can simply do

    apt-get install ffmpegfs

For Ubuntu 20.04 and Linux distributions based on it this is

    apt install ffmpegfs

For Arch Linux and Manjaro, it can be found in the Arch User Repository (AUR).
It is available as either the latest stable version or the latest code
from Git.

Installation from source
------------------------

If you want to build FFmpegfs yourself, e.g. check out the latest version
from Git, you may build it yourself.

FFmpegfs uses the GNU build system. If you are installing from git, you'll
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

Documentation
-------------

The help can be created in doc or html format by running

    make help-pdf
    make help-html

respectively.

Trouble Shooting
----------------

**Mounting via /etc/fstab fails:**

An fstab line with fuse.ffmpegfs file system fails with a strange message when
attempted to mount with "mount -av". No log entries, no other hints...

   mount: wrong fs type, bad option, bad superblock on /mnt/sdf1/mp3base1,
          missing codepage or helper program, or other error

          In some cases useful info is found in syslog - try
          dmesg | tail or so.

Fuse is missing! Do...

    apt-get install fuse

**fuse: failed to exec fusermount: No such file or directory:**

Trying to mount as other than root, the message

    fuse: failed to exec fusermount: No such file or directory

is printed.

Fuse is missing! Do...

    apt-get install fuse

**"ERROR: libmp3lame >= 3.98.3 not found":**

If you run into this "ERROR: libmp3lame >= 3.98.3 not found" although you have built
and installed libmp3lame you may find a solution here:
https://stackoverflow.com/questions/35937403/error-libmp3lame-3-98-3-not-found

**autogen.sh displays "possibly undefined macro":**

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

    apt-get install php7.0-xml
    systemctl restart apache2

And your troubles should be gone.

**"make check": all audio checks fail**

Logfiles contain "bc: command not found", so the command line communicator is
missing.

Fix by installing it (or similar):

     apt-get install bc

**Songs get cut short**

If songs do not play to the very end and you are using SAMBA or NFS you're in trouble.

Happens when the files are transcoded on the fly, but never when file comes from
cache. This is because the result is never exactly what was predicted.

SAMBA fills files with zeroes if the result is smaller, or cuts off the rest if the
file ist larger than predicted.

NFS arbitrarily sends the correct file, or one that is cut or padded like SAMBA.
This can be repeated as many times as one wants to - once the file is OK, once
not.

As of yet there seems to be no way around that. Maybe NFS or SAMBA can be configured
to cope with that, but how to is unknown to me.

**Make reports "/bin/sh: a2x: command not found"**

You are missing out on asciidoc, to install do (or similar):

     apt-get install asciidoc

That should fix it. You may just install asciidoc-base to safe disk space.

**libbluray fails to load libbluray.jar**

When you see this message accessing blurays:

    bdj.c:340: libbluray-j2se-0.9.3.jar not found.
    bdj.c:466: BD-J check: Failed to load libbluray.jar

To get rid of this message simply install "libbluray-bdj", this will make it go away.
Though not necessary, as to read the bluray tracks java support is not required, so
this is simply cosmetical.
