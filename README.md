FFmpegfs
========

FFmpegfs is a read-only FUSE filesystem which transcodes various audio and video formats to MP4, WebM, and many more on the fly when opened and read using the FFmpeg library, thus supporting a multitude of input formats and a variety of common output formats.

This allows access to a multi-media file collection with software and/or hardware which only understands one of the supported output formats, or transcodes files through simple drag-and-drop in a file browser.

Web site: https://nschlia.github.io/ffmpegfs/

News
----

### Version 2.12 is under development

- The code has been run through clang-tidy to detect areas that could be updated to C++17 and to find areas that are prone to bugs or are inefficient. Many problems could be fixed. Sometimes a few lines of code can take the place of many. Some components function far more effectively than they did in the past. C++17 is cool! I must purchase a t-shirt. 
- **Bugfix:** In get prores bitrate(), a crash that might have happened under unusual circumstances has been corrected. If the best match resolution could not be found, array access out-of-bounds could happen.
- **Bugfix:** Several unlikely, but potential problems that could have happened when subtitle decoding failed or delayed video/audio packets couldn't be decoded have been fixed.
- **Bugfix:** An internal problem could cause the application to crash. Should never happen, though. Fixed anyway.

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

You can use a collection of several media files with software and/or hardware that only supports one of the permitted output formats or convert files using simple drag-and-drop operations in a file browser with FFmpegfs.

Choose WebM or MP4 for live streaming for the best results. While *MP3* will work if video transcoding is not needed, *WebM* and *MP4* produce superior results. The *OGG* encoder is too slow for real-time file recoding.

All the frames from a video source file will be displayed in a virtual directory called after the source file when a destination of *JPG*, *PNG*, or *BMP* is selected. Audio will not be available.

By choosing *HLS*, TS segments and an M3U playlist (master.m3u8 and index 0 av.m3u8) are created in a directory. A generated hls.html file that can be accessed in a browser can also be used to play the segments.

Please be aware that since most browsers cannot open the files from disc due to restrictions, they must be on a web server. For further information, see [problems](PROBLEMS.md#open-hlshtml-from-disk-to-play-hls-output).

Installation Instructions
-------------------------

A rather detailed description can be found under [install] (INSTALL.md).

Fixing Problems
---------------

This part has been transferred to a different file because it has gotten too big. Details can be found in [problems](PROBLEMS.md).

Usage
-----

Mount your file system as follows:

    ffmpegfs [--audiobitrate bitrate] [--videobitrate bitrate] musicdir mountpoint [-o fuse_options]

To use FFmpegfs as a daemon and encode to MPEG-4, for instance:

    ffmpegfs --audiobitrate=256K --videobitrate=1.5M /mnt/music /mnt/ffmpegfs -o allow_other,ro,desttype=mp4

This will run FFmpegfs in the foreground and print the log output to the screen:

    ffmpegfs -f --log_stderr --audiobitrate=256K --videobitrate=1.5M --audiobitrate=256K --videobitrate=1.5M /mnt/music /mnt/ffmpegfs -o allow_other,ro,desttype=mp4

With the following entry in "/etc/fstab," the same result can be obtained with more recent versions of FUSE:

    ffmpegfs#/mnt/music /mnt/ffmpegfs fuse allow_other,ro,audiobitrate=256K,videobitrate=2000000,desttype=mp4 0 0

Another (more current) way to express this command:

    /mnt/music /mnt/ffmpegfs fuse.ffmpegfs allow_other,ro,audiobitrate=256K,videobitrate=2000000,desttype=mp4 0 0

At this point, files like `/mnt/music/**.flac` and `/mnt/music/**.ogg` will show up as `/mnt/ffmpegfs/**.mp4`.

Audio bitrates will be reduced to 256 KBit, video to 1.5 MBit. The source bitrate will not be scaled up if it is lower; it will remain at the lower value.

Keep in mind that only root can, by default, utilise the "allow other" option. Either use the "user allow other" key in /etc/fuse.conf or run FFmpegfs as root.

Any user must have "allow other" enabled in order to access the mount. By default, only the user who initiated FFmpegfs has access to this.

Examples:

    ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache

Transcode files using FFmpegfs from test/in to test/out while logging to stderr at a noisy TRACE level. The cache resides in test/cache. All directories are under the current user's home directory.

     ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache,videowidth=640

Similar to the previous, but with a 640-pixel maximum video width. The aspect ratio will be maintained when scaling down larger videos. Videos that are smaller won't be scaled up.

     ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache,deinterlace

Deinterlacing can be enabled for better image quality.

## More About Features

There is a [feature list](FEATURES.md) with detailed explanations.

How It Works
------------

The decoder and encoder are initialised when a file is opened, and the file's metadata is also read. At this point, a rough estimate of the total file size can be made. Because the actual size greatly depends on the material encoded, this technique works fair-to-good for MP4 or WebM output files but works well for MP3, AIFF, or WAV output files.

The file is transcoded as it is being read and stored in a private per-file buffer. This buffer keeps expanding as the file is read until the entire file has been transcoded. After being decoded, the file is stored in a disc buffer and is readily accessible.

Other processes will share the same transcoded data if they access the same file because transcoding is done in a single additional thread, which saves CPU time. Transcoding will continue for a while if all processes close the file before it is finished. Transcoding will resume if the file is viewed once more before the timer expires. If not, it will halt and delete the current chunk to free up storage space.

A file will be transcoded up to the seek point when you seek within it (if not already done). Since the majority of programmes will read a file from beginning to end, this is typically not a problem. Future upgrades might offer actual random seeking (but if this is feasible, it is not yet clear due to restrictions to positioning inside compressed streams). When HLS streaming is chosen, this already functions. The requested segment is immediately skipped to by FFmpegfs.

**MP3:** The source file's comments are used to generate ID3 version 2.4 and 1.1 tags. They are correspondingly at the beginning and the end of the file.

**MP4:** The same is true for meta atoms contained in MP4 containers.

**WAV:** The estimated size of the WAV file will be included in a pro forma WAV header. When the file is complete, this header will be changed. Though most current gamers apparently disregard this information and continue to play the file, it does not seem required.

Only for MP3 targets: A particular optimization has been done so that programmes that look for id3v1 tags don't have to wait for the entire file to be transcoded before reading the tag. This accelerates these apps *dramatically*.

About Output Formats
--------------------

A few remarks regarding the output formats that are supported:

Since these are plain vanilla constant bitrate (CBR) MP3 files, there isn't much to say about the MP3 output. Any modern player should be able to play them well.

However, MP4 files are unique because standard MP4s aren't really ideal for live broadcasting. The start block of an MP4 has a field with the size of the compressed data section, which is the cause. It suffices to say that until the size is known, compression must be finished, a file seek must be performed to the beginning, and the size atom updated.

That size is unknown for a live stream that is ongoing. To obtain that value for our transcoded files, one would need to wait for the entire file to be recoded. As if that weren't enough, the file's final section contains some crucial details, such as meta tags for the artist, album, etc. Additionally, the fact that there is just one enormous data block makes it difficult to do random searches among the contents without access to the entire data section.

Many programmes will then read the crucial information from the end of an MP4 before returning to the file's head and beginning playback. This will destroy FFmpegfs' entire transcode-on-demand concept.

Several extensions have been created to work around the restriction, including "faststart," which moves the aforementioned meta data from the end to the beginning of the MP4 file. Additionally, it is possible to omit the size field (0). An further plugin is isml (smooth live streaming).

Older versions of FFmpeg do not support several new MP4 features that are required for direct-to-stream transcoding, like ISMV, faststart, separate moof/empty moov, to mention a few (or if available, not working properly).

Faststart files are produced by default with an empty size field so that the file can be started to be written out at once rather than having to be encoded as a complete first. It would take some time before playback could begin if it were fully encoded. The data part is divided into chunks of about 1 second each, all with their own header, so it is possible to fill in the size fields early enough.

One disadvantage is that not all players agree with the format, or they play it with odd side effects. VLC only refreshes the time display every several seconds while playing the file. There may not always be a complete duration displayed while streaming using HTML5 video tags, but that is fine as long as the content plays. Playback can only move backwards from the current playback position.


However, that is the cost of commencing playback quickly.

Development
-----------

Git is the revision control system used by FFmpegfs. The complete repository is available here:

`git clone https://github.com/nschlia/ffmpegfs.git`

or the mirror:

`git clone https://salsa.debian.org/nschlia/ffmpegfs.git`

FFmpegfs is composed primarily of C++17 with a small amount of C. The following libraries are utilised:

\* [FUSE](http://fuse.sourceforge.net/)

FFmpeg library:

\* [FFmpeg](https://www.FFmpeg.org/)

Please be aware that the main branch of FFmpegfs may not be stable as it is still under active development (but offers nice gimmicks). Get a release if you require a stable version, ideally the most recent.

You are welcome to clone this project and add new features. They might be brought back into this project if additional people find them intriguing. The same holds true for bug fixes; if you find a bug, feel free to patch it!

Supported Linux Distributions
-----------------------------

FFmpegfs has been tested with:

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
