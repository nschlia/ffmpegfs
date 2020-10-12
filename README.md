FFmpegfs
========

| Compiler | Library | Build State |
| ------------- | ------------- | ------------- |
| gcc 8.2.0 | FFmpeg 4.1.4| ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs%20(github-ffmpeg)) |
| clang 7.0.1 | FFmpeg 4.1.4 | ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs%20(github-ffmpeg-clang)) |

News
----

**Planned features**

* [Issue #63](https://github.com/nschlia/ffmpegfs/issues/63): Interesting feature request - hardware support for encoding and decoding. Experimental hardware acceleration support has been added. If you feel lucky do "git checkout FB" and try it out.
* Currently I am preparing a Windows version, but this is going to take some time. I need to port the Fuse functionality to Windows which is quite a huge project in itself.

**Version 2.0 released**

Finally. we are at 2.0!

**New in 2.0:**

* Feature: No new features, new release 2.0 after feature freeze.
* Bugfix: Issue #64 - Fixed playback stopping prematurely on some files.

**New in 1.99:**

* FFmpegfs has been added to Debian 10 Buster Backports, Debian 11 Bullseye and
  Ubuntu 20.04. See [INSTALL](INSTALL.md) "Installation from repository" for details.
* Libav support has been dropped. There is no activity on https://git.libav.org/?p=libav.git
  since 21.08.2019, and some features that FFmpegfs required were already lagging behind FFmpeg
  API. Supporting Libav already bound a lot of my time that I could better spend on new features
  or fixes for FFmpegfs.
* Feature: Add PNG/BMP/JPG frame sets to 'make check'. Checks number of files and cp
           operations in arbitrary order with wildcards and absolute file names.
* Feature: Add HLS segments to 'make check'. Checks number of segments, the segment sizes
           and correct file names.
* Feature: Moved FFmpeg capabilities (formats) from --version/-V command to new -caps/-c
           parameter to keep version information brief.
* Bugfix: Issue #61 - fix timeout problem when accessing JPG in arbitrary order.
* Bugfix: Issue #58 - Build list when ls/cp path/to/flles/00001.png is done. Access failed
          because the file was not found.
* Bugfix: Issue #59 - *.m3u8 files were tried to be transcoded while they should simply be
          passed through. Added to ignore and pass through list.
* Bugfix: Issue #57 - Correctly expand tilde when ~/ is used, for example in
          "ffmpegfs --logfile=~/ffmpegfs.log".
* Bugfix: Issue #60 - Reopen cache file after it has been closed so that image frames can
          be accessed after the file has been fully decoded.
* See [NEWS](NEWS) for details.

About
-----

Web site:<br />
https://nschlia.github.io/ffmpegfs/<br />

FFmpegfs is a read-only FUSE filesystem which transcodes between audio
and video formats on the fly when opened and read.

Supported output formats:

| Format | Description | Video | Audio |
| ------------- | ------------- | ------------- | ------------- |
| MP4 | MPEG-4 | H264 | AAC |
| WebM|| VP9 | Opus |
| OGG||| Theora | Vorbis |
| MOV | QuickTime File Format | H264 | AAC |
| Prores || Prores | AAC | PCM 16 bit LE |
| Opus ||| Opus |
| MP3 | MPEG-2 Audio Layer III || MP3 |
| WAV | Waveform Audio File Format || PCM 16 bit LE |
| AIFF | Audio Interchange File Format || PCM 16 bit BE |
| ALAC | Apple Lossless Audio Codec || ALAC |
| JPG | Video to frameset || JPEG |
| PNG | Video to frameset || PNG |
| BMP | Video to frameset || BMP |
| TS | MPEG transport stream | H264 | AAC |
| HLS | HTTP Live Streaming | H264 | AAC |

This can let you use a multi media file collection with software
and/or hardware which only understands one of the supported output
formats, or transcode files through simple drag-and-drop in a file
browser.

For live streaming select *WebM* or *MP4* for best results. If video
transcoding is not required *MP3* will also do, but *WebM* and *MP4* will
create better results. The *OGG* encoder is not fast enough for real-time
recoding files.

When a destination *JPG*, *PNG* or *BMP* is chosen, all frames of a
video source file will be presented in a virtual directory named after
the source file. Audio will not be available.

Selecting *HLS* creates a directory with TS segments together with a
M3U playlist (index_0_av.m3u8 and master.m3u8). There is also a
hls.html that can be opened in a browser to play the segments.

Please note that the files must be on a webserver because restrictions
prevent most browsers from opening the files from disk. See FIXING
PROBLEMS for details.

Installation Instructions
-------------------------

* See the [INSTALL](INSTALL.md) file.

Supported Linux Distributions
-----------------------------

Tested with:

| Distribution | FFmpeg Version | Remarks | Result |
|---|---|---|---|
| `Daily build` | **N-97739-g876cfa67f3** |  | OK! |
| `Debian 9 Stretch` | **3.2.8-1~deb9u1** |  | OK! |
| `Debian 10 Buster` | **3.2.14-1~deb9u1** |  | OK! |
| `Debian 11 Bullseye` | **.2.2-1+b1** |  | OK! |
| `Raspbian 10 Buster` | **4.1.6-1~deb10u1+rpt1** |  | OK! |
| `Ubuntu 16.04.3 LTS` | **.8.11-0ubuntu0.16.04.1** |  | OK! |
| `Ubuntu 17.10` | **3.3.4-2** |  | OK! |
| `Ubuntu 20.04` | **4.2.2-1ubuntu1** |  | OK! |
| `Suse 42` | **3.3.4** | See notes below | not OK |
| `Red Hat 7`| **FFmpeg must be compiled from sources** |  | OK! |
| `Funtoo 7.3.1` | **3.4.1** | FFmpeg needs to be installed with correct "USE flags", see [install](INSTALL.md) | OK! |

**Suse** does not provide proprietary formats like AAC and H264, thus
the distribution FFmpeg is crippled. FFmpegfs will not be able to encode
to H264 and AAC. End of story.
See https://en.opensuse.org/Restricted_formats.

**Tips on other OSs and distributions like Mac or other *nixes are welcome.**

Usage
-----

Mount your filesystem like this:

    ffmpegfs [--audiobitrate bitrate] [--videobitrate bitrate] musicdir mountpoint [-o fuse_options]

For example, to run FFmpegfs as daemon, encode to MPEG-4:

    ffmpegfs --audiobitrate=256K --videobitrate=1.5M /mnt/music /mnt/ffmpegfs -o allow_other,ro,desttype=mp4

This will run ffmpegs in the foreground and print the log output to the screen:

    ffmpegfs -f --log_stderr --audiobitrate=256K --videobitrate=1.5M --audiobitrate=256K --videobitrate=1.5M /mnt/music /mnt/ffmpegfs -o allow_other,ro,desttype=mp4

In recent versions of FUSE the same can be achieved with the following entry in `/etc/fstab`:

    ffmpegfs#/mnt/music /mnt/ffmpegfs fuse allow_other,ro,audiobitrate=256K,videobitrate=2000000,desttype=mp4 0 0

Another (more modern) form of this command:

    /mnt/music /mnt/ffmpegfs fuse.ffmpegfs allow_other,ro,audiobitrate=256K,videobitrate=2000000,desttype=mp4 0 0

At this point files like `/mnt/music/**.flac` and `/mnt/music/**.ogg` will
show up as `/mnt/ffmpegfs/**.mp4`.

Audio bitrates will be reduced to 256 KBit, video to 1.5 MBit. If the source
bitrate is less it will not be scaled up but rather left at the lower value.

Note that the "allow_other" option by default can only be used by root.
You must either run FFmpegfs as root or better add a "user_allow_other" key
to /etc/fuse.conf.

"allow_other" is required to allow any user access to the mount, by
default this is only possible for the user who launched FFmpegfs.

Examples:

     ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache

Run FFmpegfs transcoding files from /test/in to /test/out, logging up to
a chatty TRACE level to stderr. The cache resides in test/cache. All directories
are under the current user's home directory.

     ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache,videowidth=640

Same as above, but also limit video with to 640 pixels. Larger videos will be
scaled down, preserving the aspect ratio. Smaller videos will not be scaled up.

     ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache,deinterlace

Enable deinterlacing to enhance image quality.

HTTP Live Streaming
-------------------

ffmpegfs now supports HLS (HTTP Live Streaming). ffmpegfs will create transport stream
(ts) segments and the required m3u8 playlists. For your convenience it will also create
a virtual test.html file that can playback the segments using the hls.js library
(see https://github.com/video-dev/hls.js/).

To use the new HLS feature invoke ffmpegfs with:

     ffmpegfs -f $HOME/test/in $HOME/test/out -o allow_other,ro,desttype=hls

Please note that this will only work over http, so you need to publish the directory
on a web server. Simply navigate to the directory and open test.html.
Security restrictions prevent direct playback from disk.

TODO:

* Currently the segments are not properly created. Each segment may contain video or audio
frames that actually belong to the previous or next segment. This will be fixed in
future versions. hls.js does not seem to care about improperly cut segments, though,
and playback appears fine. Playing a single segment separately may not work properly.

AUTO COPY
---------

"Auto copy" performs intelligent stream copy, e.g., if transcoding a
transport stream that already represents a H264 video and/or AAC audio
stream it would be possible to simply repackage it to a mp4 container
without recoding.

This is very efficient as it does not require much computing as de- and
encoding does, and it also will not degrade quality as the original file
basically stays the same.

The function detects if the target format supports the source codec and
simply remuxes the stream even if recoding from one format (e.g. TS) to
another (e.g. MOV, MP4).

There are three options:

|Option|Description|
| ------------- | ------------- |
|OFF|no auto copy|
|LIMIT|only auto copy if target file will not become significantly larger|
|ALWAY|auto copy whenever possible even if the target file becomes larger|

SMART TRANSCODING
-----------------

Smart transcoding can create different output formats for video and
audio files. For example, video files can be converted to ProRes and
audio files to AIFF. Of course, combinations like MP4/MP3 or WebM/WAV
are possible but do not make sense as MP4 or WebM work perfectly
with audio only content.

To use the new feature, simply specify a video and audio file type,
separated by a "+" sign. For example, --desttype=mov+aiff will convert
video files to Apple Quicktime MOV and audio only files to AIFF. This
can be handy if the results are consumed e.g. by some Apple Editing
software which is very picky about the input format.

TRANSCODE TO FRAME IMAGES
-------------------------

To transcode a video to frame images, set the destination type to
JPG, PNG or BMP. This will convert videos to virtual folders with
images for each frame.

 $ ls /storage/videos<br />
 video1.mp4<br />
 video2.mov<br />
 <br />
 $ ffmpegfs /storage/videos /mnt/ffmpegfs<br />
 $ find /mnt/ffmpegfs<br />
 <br />
 /mnt/ffmpegfs/video1.mp4/00001.png<br />
 /mnt/ffmpegfs/video1.mp4/00002.png<br />
 ...<br />
 /mnt/ffmpegfs/video1.mov/00001.png<br />
 /mnt/ffmpegfs/video1.mov/00002.png

A FEW WORDS ON PRORES
---------------------

Apple's Prores is a so-called intermediate format, intended for post-production
editing. It combines highest possible quality while still saving some disk space
and not requiring high performance disk systems. On the other hand this means
that Prores encoded videos will become quite large - e.g. a 60 minute video may
require up to 25 GB.

It is not for target audience use, and certainly not suitable for internet streaming.

Also please keep in mind that when using lossy source formats the quality will not
get better, but the files can be fed into software like Final Cut Pro which only
accepts a small number of input formats.

MP4 FORMAT PROFILE
------------------

The MP4 container has several derivative formats that are not compatible with
all target audiences. To feed the resulting files into for example MS Edge,
the subformat must be different as for Firefox, unfortunately.

The --profile option allows to select the format:

| Profile | OS | Target | Remarks |
| ------------- | ------------- | ------------- | ------------- |
| NONE | all | VLC, Windows Media Player etc. | Playback (default) |
| FF | Linux, Win10, Android | Firefox| OK: Playback while transcoding |
| | Win7 | Firefox | OK: Playback while transcoding |
| EDGE | Win10 | MS Edge, IE > 11 | OK: Playback while transcoding |
| | Win10 Mobile | | OK: Playback while transcoding |
| IE | Win10 | MS IE <= 11 | OK: Playback while transcoding |
| | Win7 | | Must decode first (1) |
| CHROME | all | Google Chrome| Must decode first (1) |
| SAFARI | Win | Apple Safari | Must decode first (1) |
| OPERA | All | Opera | Must decode first (1) |
| MAXTHON | Win | Maxthon| Must decode first (1) |

(1)
* error message when opened while transcoding
* must start again when file was transcoded
* Plays fine when file comes directly from buffer

This all boils down to the fact than Firefox and Edge are the
only browsers that support the necessary extensions to start
playback while still transcoding.

In most cases files will not play if not properly optimised.

See [TODO](TODO) for details.

HOW IT WORKS
------------

When a file is opened, the decoder and encoder are initialised and
the file metadata is read. At this time the final filesize can be
determined approximately. This works well for MP3 output files,
but only fair to good for MP4.

As the file is read, it is transcoded into an internal per-file
buffer. This buffer continues to grow while the file is being read
until the whole file is transcoded in memory. Once decoded the
file is kept in a disk buffer and can be accessed very fast.

Transcoding is done in an extra thread, so if other processes should
access the same file they will share the same transcoded data, saving
CPU time. If all processes close the file before its end, transconding will
continue for some time. If the file is accessed again before timeout,
transcoding will go on, if not it stops and the chunk created so far
discarded to save disk space.

Seeking within a file will cause the file to be transcoded up to the
seek point (if not already done). This is not usually a problem
since most programs will read a file from start to finish. Future
enhancements may provide true random seeking (but if this is feasible
is yet unclear due to restrictions to positioning inside compressed
streams).

MP3: ID3 version 2.4 and 1.1 tags are created from the comments in the
source file. They are located at the start and end of the file
respectively.

MP4: Same applies to meta atoms in MP4 containers.

MP3 target only: A special optimisation is made so that applications
which scan for id3v1 tags do not have to wait for the whole file to be
transcoded before reading the tag. This *dramatically* speeds up such
applications.

WAV: A pro forma WAV header will be created with estimates of the WAV
file size. This header will be replaced when the file is finished. It
does not seem necessary, though, as most modern players obviously
ignore this information and play the file anyway.

ABOUT OUTPUT FORMATS
--------------------

A few words to the supported output formats. There is not much to say
about the MP3 output as these are regular MP3 files with no strings
attached. They should play well in any modern player.

The MP4 files created are special, though, as MP4 is not quite suited
for live streaming. Reason being that the start block of an MP4
contains a field with the size of the compressed data section. Suffice
to say that this field cannot be filled in until the size is known,
which means compression must be completed first, a file seek done to
the beginning, and the size atom updated.

Alas, for a continous live stream, that size will never be known or
for our transcoded files one would have to wait for the whole file
to be recoded. If that was not enough some important pieces of
information are located at the end of the file, including meta tags
with artist, album, etc.

Subsequently many applications will go to the end of an MP4 to read
important information before going back to the head of the file and
start playing. This will break the whole transcode-on-demand idea
of FFmpegfs.

To get around the restriction several extensions have been developed,
one of which is called "faststart" that relocates the afformentioned
data from the end to the beginning of the MP4. Additonally, the size field
can be left empty (0). isml (smooth live streaming) is another extension.

For direct to stream transcoding several new features in MP4 need to
be active (ISMV, faststart, separate_moof/empty_moov to name them)
which are not implemented in older versions of FFMpeg (or if available,
not working properly).

By default faststart files will be created with an empty size field so
that the file can be started to be written out at once instead of
encoding it as a whole before this is possible. Encoding it completely
would mean it would take some time before playback can start.

The data part is divided into chunks of about 1 second length, each
with its own header, thus it is possible to fill in the size fields
early enough.

As a draw back not all players support the format, or play it with
strange side effects. VLC plays the file, but updates the time display
every few seconds only. When streamed over HTML5 video tags, sometimes there
will be no total time shown, but that is OK, as long as the file plays.
Playback cannot be positioned past the current playback position, only
backwards.

But that's the price of starting playback fast.

FIXING PROBLEMS
---------------

**Transcoding too slow**

See [Build FFmpeg with optimisations](https://github.com/nschlia/ffmpegfs/blob/master/INSTALL.md#build-ffmpeg-with-optimisations)

**Lock ups when accessing through Samba**

When accessed via Samba the pending read can lock the whole share, causing
Windows Explorer and even KDE Dolphin to freeze. Any access from the same
machine to that share is blocked, Even "ls" is not possible and blocks until
the data was returned.

Seems others had the same problem:

http://samba.2283325.n4.nabble.com/Hangs-Accessing-fuse-filesystem-in-Windows-through-Samba-td4681904.html

Adding this to the [global] config in smb.conf fixes that:

 	oplocks = no
 	level2 oplocks = no
 	aio read size = 1

The "aio read size" parameter may be moved to the share config:

 	aio read size = 1

**rsync, Beyond Compare and other tools**

Some copy tools do not go along very well with dynamically generated files
as in [Issue #23: Partial transcode of some files](https://github.com/nschlia/ffmpegfs/issues/22).

Under Linux ist is best to use (optionally with -r parameter)

        cp -uv /path/to/source /path/to/target

This will copy all missing/changed files without missing parts. On the Windows
side, Windows Explorer or copy/xcopy work. Tools like Beyond Compare may only
copy the predicted size first and not respond to size changes.

**Play HLS output by opening hls.html from disk**

Most browser prevent playback of files from disk. You may put them into
a website directory, but sometimes even https must be used or playback
will be blocked.

To enable disk playback in Firefox:

* Open about:config
* Set security.fileuri.strict_origin_policy to false

DEVELOPMENT
-----------

FFmpegfs uses Git for revision control. You can obtain the full repository
with:

    git clone https://github.com/nschlia/ffmpegfs.git

FFmpegfs is written in a mixture of C and C++ and uses the following libraries:

* [FUSE](http://fuse.sourceforge.net/)

FFmpeg library:

* [FFmpeg](https://www.FFmpeg.org/)

Please note that FFmpegfs is in active development, so the main branch may
be unstable (but offer nice gimmicks, though). If you need a stable version
please get one (preferrably the latest) release.

Feel free to clone this project and add your own features. If they are
interesting for others they might be pushed back into this project. Same
applies to bug fixes, if you discover a bug your welcome to fix it!

Future Plans
------------

* Create a windows version
* and more, see [TODO](TODO)

DEMO CODE
---------

HLS player and demo code see: https://github.com/video-dev/hls.js/

AUTHORS
-------

This fork with FFmpeg support is maintained by Norbert Schlia
(nschlia@oblivion-software.de) since 2017 to date.

Based on work by K. Henriksson (from 2008 to 2017) and the original author
David Collett (from 2006 to 2008).

Much thanks to them for the original work and giving me a good head start!

LICENSE
-------

This program can be distributed under the terms of the GNU GPL version 3
or later. It can be found [online](http://www.gnu.org/licenses/gpl-3.0.html)
or in the COPYING file.

This and other documentation may be distributed under the GNU Free
Documentation License (GFDL) 1.3 or later with no invariant sections, or
alternatively under the GNU General Public License (GPL) version 3 or later.
The GFDL can be found [online](http://www.gnu.org/licenses/fdl-1.3.html) or in
the COPYING.DOC file.

FFMPEG LICENSE
--------------

FFmpeg is licensed under the GNU Lesser General Public License (LGPL)
version 2.1 or later. However, FFmpeg incorporates several optional
parts and optimizations that are covered by the GNU General Public
License (GPL) version 2 or later. If those parts get used the GPL
applies to all of FFmpeg.

See https://www.ffmpeg.org/legal.html for details.

COPYRIGHT
---------

This fork with FFmpeg support copyright \(C) 2017-2020 Norbert Schlia
(nschlia@oblivion-software.de).

Based on work Copyright \(C) 2006-2008 David Collett, 2008-2013
K. Henriksson.

This is free software: you are free to change and redistribute it under
the terms of the GNU General Public License (GPL) version 3 or later.
