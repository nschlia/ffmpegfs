Installation Instructions for FFmpegfs
======================================

Installation from the repository
--------------------------------

### Debian Bullseye

FFmpegfs has been added to Debian 11 Bullseye so it is available as a binary distribution.

**On Debian 11 Bullseye, you get V2.2 by simply doing**

    apt-get install ffmpegfs

Newer FFmpegfs versions can be installed on Debian 11 Bullseye from Bullseye Backports and sid (which is not recommended and therefore not described here).

To enable Bullseye Backports:

     echo "deb http://deb.debian.org/debian bullseye-backports main" | sudo tee /etc/apt/sources.list.d/backports.list

     sudo apt-get update

To install FFmpegfs from backports, run `apt-get install ffmpegfs/bullseye-backports` or `apt-get install -t bullseye-backports ffmpegfs`.

**For Ubuntu 20.04 or newer and Linux distributions based on it this is**

    apt-get install ffmpegfs

### Debian Buster

FFmpegfs can be installed on Debian 10 Buster from Buster Backports and sid (which is not recommended and therefore not described here).

To enable Buster Backports:

     echo "deb http://deb.debian.org/debian buster-backports main" | sudo tee /etc/apt/sources.list.d/backports.list

     sudo apt-get update

Then install FFmpegfs:

     sudo apt-get -t buster-backports install ffmpegfs

### Other Distributions

For Arch Linux and Manjaro, it can be found in the Arch User Repository (AUR). It is available as either the latest stable version or the latest code from GIT.

Building FFmpegfs yourself
--------------------------

### Prerequisites

For encoding and decoding, FFmpegfs utilises the FFmpeg multi media framework. The following libraries are necessary:

* gcc and g++ compilers
* fuse (>= 2.6.0)
* sqlite3 (>= 3.7.13)

FFmpeg 4.1.8 "al-Khwarizmi" or newer:

* libavutil (>= 56.22.100)
* libavcodec (>= 58.35.100)
* libavformat (>= 58.20.100)
* libavfilter (>= 7.40.101)
* libswscale (>= 5.3.100)
* libswresample (>= 3.3.100)

For optional DVD support, you need the following libraries:

* libdvdread (>= 5.0.0)
* libdvdnav (>= 5.0.0)

For optional Blu-ray support, you need the following libraries:

* libbluray (>= 0.6.2)

The commands to only install the first set of prerequisites come next.

For more information, see the "Supported Linux Distributions" chapter in README.md.

**On Debian:**

    apt-get install gcc g++ make pkg-config asciidoc-base w3m xxd

    apt-get install fuse libfuse-dev libsqlite3-dev libavcodec-dev libavformat-dev libswresample-dev libavutil-dev libswscale-dev libavfilter-dev libcue-dev libchardet-dev

To get DVD support:

    apt-get install libdvdread-dev libdvdnav-dev

To get Blu-ray support:

    apt-get install libbluray-dev

To use "make doxy" (build Doxygen documentation):

    apt-get install doxygen graphviz curl

To use "make check" (run test suite):

    apt-get install libchromaprint-dev bc

**On Suse** (please read notes before continuing):

    zypper install gcc gcc-c++

    zypper install fuse fuse-devel libsqlite3-devel libavcodec-devel libavformat-devel libswresample-devel libavutil-devel libswscale-devel libcue-devel libchardet-devel

To get DVD support:

    zypper install libdvdread-devel libdvdnav-devel

To get Blu-ray support:

    zypper install libbluray-devel

Suse includes non-proprietary codecs with FFmpeg only, namely mp3, AAC and H264 are *not* available which renders this library next to usesless. But FFmpeg can be built from source, see https://trac.ffmpeg.org/wiki/CompilationGuide and check "FFmpeg compile notes" below.

**On Red Hat:**

    yum install gcc g++

    yum install fuse-devel sqlite-devel libcue-devel libchardet-devel

To get DVD support:

    yum install libdvdread-devel libdvdnav-devel

To get Blu-ray support:

    yum install libbluray-devel

FFmpeg is not provided by Red Hat from its repositories. It must be built from source code. See this guide: https://trac.ffmpeg.org/wiki/CompilationGuide/Centos

If you want to build the documentation, you will find "asciidoc" missing from the Red Hat repositories. To get it, use a beta repository:

    yum --enablerepo=rhel-7-server-optional-beta-rpms install asciidoc

**On Funtoo Linux:**

To get fuse support and chromaprint (for make check):

    emerge sys-fs/fuse
    emerge media-libs/chromaprint

To get FFmpeg with H264 etc. support, specify some "USE flags" when doing emerge:

Create a file /etc/portage/package.use, for example "vi vi /etc/portage/package.use" and add this line:

    media-video/ffmpeg mp3 x264 opus vorbis vpx

This will enable H264, mp3, Opus and WebM support. Next...

    emerge media-libs/openh264
    emerge media-sound/twolame
    emerge media-video/ffmpeg

to build FFmpeg.

### Building FFmpegfs from source code

#### Building from tar ball

Get the most recent release archive here:

    wget https://github.com/nschlia/ffmpegfs/releases/download/v2.12/ffmpegfs-2.12.tar.gz

As I frequently fail to update this URL, you can check https://github.com/nschlia/ffmpegfs/releases to see if there are newer releases available.

Alternatively, utilise these instructions, being sure to download the most recent version:

```
curl -s https://api.github.com/repos/nschlia/ffmpegfs/releases/latest | \
grep "browser_download_url.*gz" | \
cut -d : -f 2,3 | \
tr -d \" | \
wget -i -
```

Unpack the file, and then cd to the source directory. To build and install, run:

    ./configure
    make
    make install

Build and execute the test suite by doing:

    make check

This will test audio conversion, tagging and size prediction.

#### Building from GIT

Clone the most recent version from Github to build from GIT and stay up to date:

```
git clone https://github.com/nschlia/ffmpegfs.git
```

Or from the mirror server:

```
git clone https://salsa.debian.org/nschlia/ffmpegfs.git
```

**If building from git, you'll need these additional prerequisites:**

* autoconf
* automake
* asciidoc (or at least asciidoc-base to save disk space)
* w3m
* docbook-xml

For those who are lazy like me, simply copy and issue this command to get all the prerequisites:

    apt-get install gcc g++ make pkg-config autoconf automake asciidoc-base docbook-xml xsltproc w3m libchromaprint-dev bc doxygen graphviz

FFmpegfs uses the GNU build system, so you'll need to run first:

    ./autogen.sh

If you are downloading a release, this has already been done for you. To build and install, run:

    ./configure
    make
    make install

To build and run the test suite, do:

    make check

### Switching between repository version and source builds

It is easy to switch between both worlds. You can do that as many times as you want to, but alas, the cache directory will be cleared every time. But it will be rebuilt in the background, so this will almost go unnoticed.

To switch from repository to a source build do

```
apt-get remove ffmpegfs
```

Then follow the steps under "Building FFmpegfs yourself". If you do not remove the repository version, your self-build could inadvertently be up- or downgraded when a new version becomes available from the repository.

To switch from a source build to a repository installation, change to the build directory and do

```
make uninstall
```

Then follow the steps under "Installation from repository".

### Building Documentation

The help can be created in doc or html format by running

    make help-pdf
    make help-html

respectively.

Building FFmpeg
-------------------------------

### Building FFmpeg with optimisations

The precompiled package of FFmpeg available for Debian, Ubuntu, etc. is built with common options so that it can run on many processors. To leverage its full potential, it may be useful to build it with optimisation options for the target CPU. The resulting binaries may not run on other computers.

FFmpeg must be built with at least libx264, libfdk_aac, and libmp3lame support. Other libraries, for example, ogg, Windows Media, or FLAC, must be added when these formats should be used as sources.

**Attention!** Remember to do "apt remove ffmpeg" before proceeding with the next steps to avoid maleficent blends of custom and official distribution binaries.

For a minimum build that contains all the libraries required by FFmpegfs and SSL support for convenience use:

	configure \
		--extra-cflags='-march=native' \
		--extra-cflags=-Ofast \
		--enable-runtime-cpudetect \
		--disable-static \
		--enable-gpl \
		--enable-shared \
		--enable-version3 \
		--enable-nonfree \
		--enable-pthreads \
		--enable-postproc \
		--enable-openssl \
		--enable-libvpx \
		--enable-libvorbis \
		--enable-swresample \
		--enable-libmp3lame \
		--enable-libtheora \
		--enable-libxvid \
		--enable-libx264

Fix the complaints by configure, i.e., installing the required development packages, then

    make install

This works for me. Decoding runs much faster with these settings.

**NOTE:** Depending on the source formats you have, it may be required to add additional libraries.

Trouble Shooting
----------------

### Mounting via /etc/fstab fails

An fstab line with fuse.ffmpegfs file system fails with a strange message when attempted to mount with "mount -av". No log entries, no other hints...

```
   mount: wrong fs type, bad option, bad superblock on /mnt/sdf1/mp3base1,
          missing codepage or helper program, or other error

​      In some cases useful info is found in syslog - try
​      dmesg | tail or so.
```

Fuse is missing! Do...

    apt-get install fuse

### fuse: failed to exec fusermount: No such file or directory

Trying to mount as any other than root, the message

    fuse: failed to exec fusermount: No such file or directory

is printed.

Fuse is missing! Do...

    apt-get install fuse

### "ERROR: libmp3lame >= 3.98.3 not found"

If you run into this "ERROR: libmp3lame >= 3.98.3 not found" although you have built and installed libmp3lame you may find a solution here:
https://stackoverflow.com/questions/35937403/error-libmp3lame-3-98-3-not-found

### autogen.sh displays "possibly undefined macro"

    Running autoreconf --install
    configure.ac:46: error: possibly undefined macro: AC_DEFINE
      If this token and others are legitimate, please use m4_pattern_allow.
      See the Autoconf documentation.
    autoreconf: /usr/bin/autoconf failed with exit status: 1

You are probably missing out on pkg-config; either it is not installed or not in path. "apt-get install pkg-config" (on Debian or equivalent on other Linux distributions) should help.

### If the videotag.php script does not work under PHP7

The script runs fine under PHP5, but when upgrading to PHP7 (or using PHP7) it suddenly stops showing the list of files.

Check the Apache2 error.log, you might see this:

"PHP Fatal error:  Uncaught Error: Call to undefined function utf8_encode() in index.php"

This is because, for some reason, utf8_encode() has been moved to the XML library. Just do (or similar):

    apt-get install php7.0-xml
    systemctl restart apache2

And your troubles should be gone.

### "make check": all audio checks fail

Log files contain "bc: command not found", so the command line communicator is missing.

Fix it by installing (or similar):

     apt-get install bc

### Make reports "/bin/sh: a2x: command not found"

You are missing out on asciidoc. To install it, do (or similar):

     apt-get install asciidoc-base

That should fix it.

### "make help-pdf" reports "non-zero exit status 127"

Running "make help-pdf" fails like this:

    $ make -s help-pdf
      GEN      ffmpegfs.1.pdf
    a2x: ERROR: "fop"   -fo "ffmpegfs.1.fo" -pdf "ffmpegfs.1.pdf" returned non-zero exit status 127

    make: *** [Makefile:918: ffmpegfs.1.pdf] Error 1

This happens when "fop" is missing, a command line wrapper for the Java version of fop.

     apt-get install fop

That should do it.

### Make reports xmllint" --nonet --noout --valid "/home/jenkins/dev/ffmpegfs/ffmpegfs.1.xml" returned non-zero exit

To find out more, run "make V=1".  If you see something like (sic) "validity error : Validation failed: no DTD found !", this means that xmlint cannot access a DTD file because it is run with the --nonet option.

This can be solved by

     apt-get install docbook-xml

which will make the required files available offline.

### libbluray fails to load libbluray.jar

When you see this message while accessing blurays:

    bdj.c:340: libbluray-j2se-0.9.3.jar not found.
    bdj.c:466: BD-J check: Failed to load libbluray.jar

To get rid of this message, simply install "libbluray-bdj". This will make it go away. Though not necessary, as to read the Blu-ray tracks, Java support is not required, so this is simply cosmetical.

Copyright
---------

This fork with FFmpeg support copyright \(C) 2017-2023 Norbert Schlia (nschlia@oblivion-software.de).

Based on work Copyright \(C) 2006-2008 David Collett, 2008-2013 K. Henriksson.

This is free software: you are free to change and redistribute it under the terms of the GNU General Public License (GPL) version 3 or later.
