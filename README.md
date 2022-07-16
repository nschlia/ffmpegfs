FFmpegfs
========

FFmpegfs is a read-only FUSE filesystem which transcodes various audio and video formats to MP4, WebM, and many more on the fly when opened and read using the FFmpeg library, thus supporting a multitude of input formats and a variety of common output formats.

This allows access to a multi-media file collection with software and/or hardware which only understands one of the supported output formats, or transcodes files through simple drag-and-drop in a file browser.

Web site: https://nschlia.github.io/ffmpegfs/

News
----

### Version 2.12 is under development

- The code has been run through clang-tidy to identify bug-prone or inefficient code, and to find parts that could be modernised to C++17. Several issues could be resolved. Sometimes, many lines of code could be replaced by a few. Some parts run far more inefficiently than before. C++17 is cool. I need to get a t-shirt.
- **Bugfix:** A potential crash under rare circumstances in get_prores_bitrate() has been fixed. Array access out-of-bounds could occur if best match resolution could not be determined.
- **Bugfix:** Fixed several unlikely, but potential crashes when subtitle decoding failed or delayed video/audio packets could not be encoded.
- **Bugfix:** The application could crash after an internal error. Should never happen, though. Fixed anyway.

### Version 2.11 has been released

* **Feature:** [Issue #86](https://github.com/nschlia/ffmpegfs/issues/86): Smart transcode now detects if a source file is audio only and uses the correct target format. For example, with --destination=webm+mp3, if one MP4 input file contains a video stream and another an audio stream only, the resulting files will be WebM (for the video input) and mp3 for the audio only file.
* **Feature:** [Issue #137](https://github.com/nschlia/ffmpegfs/issues/137): Add --no_subtitles option to turn subtitles off.
* **Bugfix:** Smart encode selected the video format for cue sheet tracks, regardless of the input format. This has been fixed now.
* **Bugfix:** Fix a crash that occurs when a DVD/Blu-ray is transcoded to audio only.
* **Bugfix:** If the track performer field in the cuesheet is blank, try album performer instead.
* **Bugfix:** Failing to mount Fuse during "make check" went unnoticed as the result code (which was supposed to be 99) was actually 0. Return the correct result code, failing the operation as expected.
* **Bugfix:** The Docker build command contained a "make check" which actually failed altogether. Step has been removed. "make check" mounts Fuse, but this requires privileges that do not exist during "docker build".
* **Bugfix:** On error, mremap () returns MAP_FAILED rather than NULL. Fixed a check for incorrect error conditions, which could cause the application to crash or return illogical error messages.
* **Bugfix:** [Issue #119](https://github.com/nschlia/ffmpegfs/issues/119): Fix a problem that caused frame set generation to fail sometimes. It seems to be related to the nremap() issue.
* Generally revisited documentation, logging, and display texts. Improved grammar, formatting, and fixed quite a few typos that escaped all proofreading sessions.
* The FFmpeg API INFO and DEBUG level log output has been changed to the FFmpegfs DEBUG level. What FFmpeg considers "INFO" is far too chatty.
* Frequent memory reallocations when creating HLS segments have been reduced to speed up processing.
* Optimised logging to save CPU time by not formatting log entries that are not written anyway at their log level.
* Logging has been revised to shorten file paths and remove mount, input, and cache paths. Log the additional portion only to reduce log file size and improve readability.
* **Bugfix:** To fix the build with GCC 12, add the missing include headers (closes: [#1012925](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1012925)).

## Full History

See [history](HISTORY.md) for details.

## Code Status

| Branch                                                    | Compiler     | FFmpeg | Host OS      | CPU    | Build State                                                  |
| --------------------------------------------------------- | ------------ | ------ | ------------ | ------ | ------------------------------------------------------------ |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | GCC 12.1.0   | 5.0.1  | Debian 12    | AMD/64 | ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs+%28github-latest%29) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | GCC 10.2.1   | 4.3.2  | Debian 11    | AMD/64 | ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs%20(github-ffmpeg)) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | GCC 9.3.0    | 4.2.4  | Ubuntu 20.04 | AMD/64 | [![Build Status](https://github.com/nschlia/ffmpegfs/actions/workflows/make-gcc.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/make-gcc.yml) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | GCC 10.2.1   | 4.3.4  | Debian 11    | ARM/32 | ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs+%28github-arm32%29) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | Clang 11.0.1 | 4.3.2  | Debian 11    | AMD/64 | ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs%20(github-ffmpeg-clang)) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | Clang 11.0.1 | 4.2.4  | Ubuntu 20.04 | AMD/64 | [![Build Status](https://github.com/nschlia/ffmpegfs/actions/workflows/make-clang.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/make-clang.yml) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) |              |        |              |        | [![CodeQL](https://github.com/nschlia/ffmpegfs/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/codeql-analysis.yml) |

[![Packaging status](https://repology.org/badge/vertical-allrepos/ffmpegfs.svg?columns=4)](https://repology.org/project/ffmpegfs/versions)

Supported Formats
-----

### Input

Making a full list of the formats the FFmpeg API supports would be somewhat pointless.  See [Demuxers](https://ffmpeg.org/ffmpeg-formats.html#Demuxers) on FFmpeg's home pages and [Supported Formats](https://en.wikipedia.org/wiki/FFmpeg#Supported_formats) on Wikipedia to get an idea, .

Unfortunately, it also depends on the codecs that have been compiled into the library; use this command to find out: 

```
ffmpeg -formats
```

A list of the available codecs will be created as a result. 

### Output

Only formats that can be read while being written to can be used as output. While MP4 isn't one of them, it can still be supported with the help of some format extensions. 

#### Audio Formats
| Format | Description | Audio |
| ------------- | ------------- | ------------- |
| AIFF | Audio Interchange File Format | PCM 16 bit BE |
| ALAC | Apple Lossless Audio Codec | ALAC |
| FLAC | Free Lossless Audio | FLAC |
| MP3 | MPEG-2 Audio Layer III | MP3 |
| Opus |Opus Audio| Opus |
| WAV | Waveform Audio File Format | PCM 16 bit LE |

#### Video Formats
| Format | Description | Video | Audio |
| ------------- | ------------- | ------------- | ------------- |
| HLS | HTTP Live Streaming | H264 | AAC |
| MOV | QuickTime File Format | H264 | AAC |
| MP4 | MPEG-4 | H264 | AAC |
| OGG|Free Open Container Format| Theora | Vorbis |
| ProRes | Apple ProRes | ProRes | PCM 16 bit LE |
| TS | MPEG Transport Stream | H264 | AAC |
| WebM|Free Open Web Media Project| VP9 | Opus |

#### Stills

| Format | Description | Video |
| ------------- | ------------- | ------------- |
| BMP | Video to frameset |BMP|
| JPG | Video to frameset |JPEG|
| PNG | Video to frameset |PNG|

### What can it do?

This can let you use a multi media file collection with software and/or hardware which only understands one of the supported output formats, or transcode files through simple drag-and-drop in a file browser.

For live streaming, select *WebM* or *MP4* for best results. If video transcoding is not required, *MP3* will also do, but *WebM* and *MP4* create better results. The *OGG* encoder is not fast enough for real-time recoding of files.

When a destination *JPG*, *PNG*, or *BMP* is chosen, all frames of a video source file will be presented in a virtual directory named after the source file. Audio will not be available.

Selecting *HLS* creates a directory with TS segments together with an M3U playlist (index_0_av.m3u8 and master.m3u8). There is also a hls.html that can be opened in a browser to play the segments.

Please note that the files must be on a web server because restrictions prevent most browsers from opening the files from disk. See [problems](PROBLEMS.md) for details.

Installation Instructions
-------------------------

You'll find a rather extensive explanation under [install](INSTALL.md).

Fixing Problems
---------------

This section has grown too large and has been moved to a separate file. See [problems](PROBLEMS.md) for details.

Usage
-----

Mount your file system like this:

    ffmpegfs [--audiobitrate bitrate] [--videobitrate bitrate] musicdir mountpoint [-o fuse_options]

For example, to run FFmpegfs as a daemon and encode to MPEG-4:

    ffmpegfs --audiobitrate=256K --videobitrate=1.5M /mnt/music /mnt/ffmpegfs -o allow_other,ro,desttype=mp4

This will run FFmpegfs in the foreground and print the log output to the screen:

    ffmpegfs -f --log_stderr --audiobitrate=256K --videobitrate=1.5M --audiobitrate=256K --videobitrate=1.5M /mnt/music /mnt/ffmpegfs -o allow_other,ro,desttype=mp4

In recent versions of FUSE the same can be achieved with the following entry in `/etc/fstab`:

    ffmpegfs#/mnt/music /mnt/ffmpegfs fuse allow_other,ro,audiobitrate=256K,videobitrate=2000000,desttype=mp4 0 0

Another (more modern) form of this command:

    /mnt/music /mnt/ffmpegfs fuse.ffmpegfs allow_other,ro,audiobitrate=256K,videobitrate=2000000,desttype=mp4 0 0

At this point, files like `/mnt/music/**.flac` and `/mnt/music/**.ogg` will show up as `/mnt/ffmpegfs/**.mp4`.

Audio bitrates will be reduced to 256 KBit, video to 1.5 MBit. If the source bitrate is lower, it will not be scaled up but rather left at the lower value.

Note that the "allow_other" option by default can only be used by root. You must either run FFmpegfs as root or better yet, add a "user_allow_other" key to /etc/fuse.conf.

"allow_other" is required to allow any user access to the mount. By default, this is only possible for the user who launched FFmpegfs.

Examples:

    ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache

Run FFmpegfs transcoding files from /test/in to /test/out, logging up to a chatty TRACE level to stderr. The cache resides in test/cache. All directories are under the current user's home directory.

     ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache,videowidth=640

Similar to the preceding, but limit the video width to 640 pixels. Larger videos will be scaled down, preserving the aspect ratio. Smaller videos will not be scaled up.

     ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache,deinterlace

Enable deinterlacing for enhanced image quality.

## More About Features

There is a [feature list](FEATURES.md) with detailed explanations.

How It Works
------------

When a file is opened, the decoder and encoder are initialised, and the file metadata is read. At this time, the final file size can be determined approximately. This works well for MP3, AIFF, or WAV output files, but only fair-to-good for MP4 or WebM because the actual size heavily depends on the content encoded.

As the file is read, it is transcoded into an internal per-file buffer. This buffer continues to grow while the file is being read until the whole file is transcoded. Once decoded, the file is kept in a disk buffer and can be accessed very quickly.

Transcoding is done in an extra thread, so if other processes access the same file, they will share the same transcoded data, saving CPU time. If all processes close the file before its end, transcoding will continue for some time. If the file is accessed again before the timeout, transcoding will continue. If not, it will stop, and the chunk created so far will be discarded to save disk space.

Seeking within a file will cause the file to be transcoded up to the seek point (if not already done). This is not usually a problem since most programmes will read a file from start to finish. Futzure enhancements may provide true random seeking (but if this is feasible, it is not yet clear due to restrictions to positioning inside compressed streams). This already works when HLS streaming is selected. FFmpegfs simply skips to the requested segment.

MP3: ID3 version 2.4 and 1.1 tags are created from the comments in the source file. They are located at the start and end of the file, respectively.

MP4: The same applies to meta atoms in MP4 containers.

MP3 target only: A special optimisation is made so that applications which scan for id3v1 tags do not have to wait for the whole file to be transcoded before reading the tag. This *dramatically* speeds up such applications.

WAV: A pro format WAV header will be created with estimates of the WAV file size. This header will be replaced when the file is finished. It does not seem necessary, though, as most modern players obviously ignore this information and play the file anyway.

About Output Formats
--------------------

A few words about the supported output formats: There is not much to say about the MP3 output as these are regular constant bitrate (CBR) MP3 files with no strings attached. They should be played well by any modern player.

MP4 files are special, though, as regular MP4s are not quite suited for live streaming. The reason is that the start block of an MP4 contains a field with the size of the compressed data section. Suffice it to say that this field cannot be filled in until the size is known, which means compression must be completed first, a file seek done to the beginning, and the size atom updated. 

For a continuous live stream, that size will never be known. For our transcoded files, one would have to wait for the whole file to be recoded to get that value. If that was not enough, some important pieces of information are located at the end of the file, including meta tags with artist, album, etc. Also, there is only one big data block, a fact that hampers random seeking inside the contents without having the complete data section.

Subsequently, many applications will go to the end of an MP4 to read important information before going back to the head of the file and starting playing. This will break the whole transcode-on-demand idea of FFmpegfs.

To get around the restriction, several extensions have been developed, one of which is called "faststart", which relocates the aforementioned meta data from the end to the beginning of the MP4. Additionally, the size field can be left empty (0). isml (smooth live streaming) is another extension.

For direct-to-stream transcoding, several new features in MP4 need to be active (ISMV, faststart, separate_moof/empty_moov to name a few) which are not implemented in older versions of FFmpeg (or if available, not working properly).

By default, faststart files will be created with an empty size field so that the file can be started to be written out at once instead of encoding it as a whole before this is possible. Encoding it completely would mean it would take some time before playback could start.

The data part is divided into chunks of about 1 second each, all with their own header, so it is possible to fill in the size fields early enough.

As a drawback, not all players support the format, or play it with strange side effects. VLC plays the file, but updates the time display every few seconds only. When streamed over HTML5 video tags, sometimes there will be no total time shown, but that is OK, as long as the file plays. Playback can not be positioned past the current playback position, only backwards.

But that's the price of starting playback fast.

Development
-----------

FFmpegfs uses Git for revision control. You can obtain the full repository here:

  git clone https://github.com/nschlia/ffmpegfs.git

FFmpegfs is written in a little bit of C and mostly C++17. It uses the following libraries:

\* [FUSE](http://fuse.sourceforge.net/)

FFmpeg library:

\* [FFmpeg](https://www.FFmpeg.org/)

Please note that FFmpegfs is in active development, so the main branch may be unstable (but offers nice gimmicks). If you need a stable version, please get one (preferably the latest) release.

Feel free to clone this project and add your own features. If they are interesting to others, they might be pushed back into this project. The same applies to bug fixes; if you discover a bug, you're welcome to fix it!

Supported Linux Distributions
-----------------------------

Tested with:

| Distribution             | FFmpeg Version                       | Remarks                                                      | Result |
| ------------------------ | ------------------------------------ | ------------------------------------------------------------ | ------ |
| **Daily build**          | N-107143-g56419428a8                 |                                                              | OK     |
| **Debian 9 Stretch**     | 3.2.8-1~deb9u1                       |                                                              | OK     |
| **Debian 10 Buster**     | 4.1.6-1~deb10u1                      |                                                              | OK     |
| **Debian 11 Bullseye**   | 4.3.2-0+deb11u2                      |                                                              | OK     |
| **Debian 12 Bookworm**   | 5.0.1                                |                                                              | OK     |
| **Raspbian 10 Buster**   | 4.1.6-1~deb10u1+rpt1                 |                                                              | OK     |
| **Raspbian 11 Bullseye** | 4.3.2-0+rpt3+deb11u2                 | Added subtitle support                                       | OK     |
| **Ubuntu 16.04.3 LTS**   | 2.8.11-0ubuntu0.16.04.1              |                                                              | OK     |
| **Ubuntu 17.10**         | 3.3.4-2                              |                                                              | OK     |
| **Ubuntu 20.04 LTS**     | 4.2.2-1ubuntu1                       |                                                              | OK     |
| **Suse 42**              | 3.3.4                                | See notes below                                              | not OK |
| **Red Hat 7**            | FFmpeg must be compiled from sources |                                                              | OK     |
| **Funtoo 7.3.1**         | 3.4.1                                | FFmpeg needs to be installed with correct "USE flags", see [install](INSTALL.md) | OK     |

**Suse** does not provide proprietary formats like AAC and H264, so the distribution of FFmpeg is crippled. FFmpegfs will not be able to encode to H264 and AAC. End of story.

See https://en.opensuse.org/Restricted_formats.

**Tips on other operating systems and distributions, such as Mac or other nixes, are welcome.**

Future Objectives
-----------------

*Any ideas or wishes?* Free to create [an issue](https://github.com/nschlia/ffmpegfs/issues) and let me know. Some great features started this way!

For more information, see [TODO](TODO).

Authors
-------

This fork with FFmpeg support has been maintained by Norbert Schlia (nschlia@oblivion-software.de) since 2017 to date.

Based on work by K. Henriksson (from 2008 to 2017) and the original author, David Collett (from 2006 to 2008).

Much thanks to them for the original work and giving me a good head start!

License
-------

This program can be distributed under the terms of the GNU GPL version 3 or later. It can be found [online](http://www.gnu.org/licenses/gpl-3.0.html) or in the COPYING file.

This and other documentation may be distributed under the GNU Free Documentation License (GFDL) 1.3 or later with no invariant sections, or alternatively under the GNU General Public License (GPL) version 3 or later. The GFDL can be found [online](http://www.gnu.org/licenses/fdl-1.3.html) or in the COPYING.DOC file.

FFmpeg License
--------------

FFmpeg is licensed under the GNU Lesser General Public License (LGPL) version 2.1 or later. However, FFmpeg incorporates several optional parts and optimizations that are covered by the GNU General Public License (GPL) version 2 or later. If those parts get used the GPL applies to all of FFmpeg.

See https://www.ffmpeg.org/legal.html for details.

Copyright
---------

This fork with FFmpeg support copyright \(C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de).

Based on work Copyright \(C) 2006-2008 David Collett, 2008-2013 K. Henriksson.

This is free software: you are free to change and redistribute it under the terms of the GNU General Public License (GPL) version 3 or later.
