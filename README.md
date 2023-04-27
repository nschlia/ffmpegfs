FFmpegfs
========

FFmpegfs is a read-only FUSE filesystem which transcodes various audio and video formats to MP4, WebM, and many more on the fly when opened and read using the FFmpeg library, thus supporting a multitude of input formats and a variety of common output formats.

This allows access to a multi-media file collection with software and/or hardware which only understands one of the supported output formats, or transcodes files through simple drag-and-drop in a file browser.

Web site: https://nschlia.github.io/ffmpegfs/

## Code Status

| Branch                                                    | Compiler     | FFmpeg | Host OS      | CPU    | Build State                                                  |
| --------------------------------------------------------- | ------------ | ------ | ------------ | ------ | ------------------------------------------------------------ |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | GCC 12.2.0   | 5.0.1  | Debian 12    | AMD/64 | ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs+github-latest) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | GCC 10.2.1   | 4.3.2  | Debian 11    | AMD/64 | ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs%20github-ffmpeg) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | GCC 9.3.0    | 4.2.4  | Ubuntu 20.04 | AMD/64 | [![Build Status](https://github.com/nschlia/ffmpegfs/actions/workflows/make-gcc.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/make-gcc.yml) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | GCC 10.2.1   | 4.3.4  | Debian 11    | ARM/32 | ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs+github-arm32) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | Clang 11.0.1 | 4.3.2  | Debian 11    | AMD/64 | ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs%20github-ffmpeg-clang) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | Clang 11.0.1 | 4.2.4  | Ubuntu 20.04 | AMD/64 | [![Build Status](https://github.com/nschlia/ffmpegfs/actions/workflows/make-clang.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/make-clang.yml) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) |              |        |              |        | [![CodeQL](https://github.com/nschlia/ffmpegfs/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/codeql-analysis.yml) |

## Packaging Status

[![Packaging status](https://repology.org/badge/vertical-allrepos/ffmpegfs.svg?columns=4)](https://repology.org/project/ffmpegfs/versions)

News
----

### Windows Version

A Windows version of FFmpegfs has frequently been requested; see issue [#76](https://github.com/nschlia/ffmpegfs/issues/76) for more information. In essence, this failed because Windows doesn't support Fuse. I discovered [WinFSP](https://winfsp.dev/), which offers everything necessary.

To see what's been done so far, checkout the [windows](https://github.com/nschlia/ffmpegfs/tree/windows) branch.

### Version 2.14 under development

**New in in 2.14 (2023-02-XX):**

- **Bugfix:** Closes [#141](https://github.com/nschlia/ffmpegfs/issues/141): Improved memory management by allocating several times the average size of allocations. This prevents obtaining tiny portions over and over again. Additionally, after the file is opened, grab the entire expected memory block rather than doing a tiny allocation initially, followed by a larger allocation.
- **Bugfix:** Avoid race conditions that cause the inter-process semaphore creation to fail for the second process.
- **Bugfix:** Issue [#119](https://github.com/nschlia/ffmpegfs/issues/119): If a seek request is still open after EOF, restart transcoding.
- **Bugfix:** Issue [#119](https://github.com/nschlia/ffmpegfs/issues/119): To prevent frame/segment creation errors, the frame set and HLS code has been updated.
- **Bugfix:** Avoid crashes during shutdown if cache objects have already been closed.
- **Bugfix:** Issue [#119](https://github.com/nschlia/ffmpegfs/issues/119): The AVSEEK_FLAG_FRAME set should be used to seek to frames when building frame sets. Otherwise, output images may vary if searched for or continuously decoded.
- **Bugfix:** The conversion of PTS to frame number and vice versa for frame sets was incorrect if TBR did not equal frames per second.
- **Bugfix:** Fixed seek requests that are being ignored with frame sets.
- **Bugfix:** When transferring from cache to the Fuse buffer, avoid a possible 1 byte overrun.
- **Bugfix:** Issue [#143](https://github.com/nschlia/ffmpegfs/issues/143): To avoid occasional EPERM failures, missing synchronisation objects were added.
- **Bugfix:** Issue [#144](https://github.com/nschlia/ffmpegfs/issues/144): To fix the crashes that may have been caused by them, the variables impacted by a potential threading issue were marked as "volatile."
- **Enhancement:** Record milliseconds for every log event.
- **Enhancement:** make check: added a file size check to frame set tests.
- **Optimisation:** When reopening after invalidating the cache, the size remained at 0. The original size is now once again reserved in order to prevent reallocations.
- **Optimisation:** To avoid reallocations, save enough space in the cache buffer to hold the entire frame set.
- **Optimisation:** Checking folders to see if they can be transcoded is completely pointless. Directories are now immediately skipped.
- To avoid problems with logfile viewers, renamed built-in logfiles to *_builtin.log (removing the double extension).

### Version 2.13 has been released

**New in in 2.13 (2023-01-15):**

- **Feature:** Added --prebuffer_time parameter. Files will be decoded until the  buffer contains the specified playing time, allowing playback to start
 smoothly without lags. Works similar to --prebuffer_size but gives better control because it does not depend on the bit rate. An example: when set to 25 seconds for HLS transcoding, this will make sure that at least 2 complete segments will be available once the file is released and visible.
- **Feature:** Closes [#140](https://github.com/nschlia/ffmpegfs/issues/140): Filtering the files that will be encoded has been added. A comma-separated list of extensions is specified by the *—include_extensions* parameter. These file extensions are the only ones that will be transcoded. The entries support shell wildcard patterns.
- **Feature:** The --hide_extensions parameter syntax has been extended. The entries now support shell wildcard patterns.
- **Bugfix:** Closes [#139](https://github.com/nschlia/ffmpegfs/issues/139): Additional files could be added using the *—extensions* parameter. However, this is no longer necessary; in the past, a file's extension determined whether or not it would be transcoded. Files with unknown extensions would be ignored. The extension is no longer important because FFmpegfs now examines all input files and recognises transcodable files by the format.
The outdated *—extensions* argument was removed without substitution.
- **Bugfix:** Fixed crash when implode() function was called with an empty string. Happened with Windows GCC 11.3.0 only.

## Full History

For more information, see [history](HISTORY.md).

Supported Formats
-----

### Input

Making a full list of the formats the FFmpeg API supports would be somewhat pointless. See [Demuxers](https://ffmpeg.org/ffmpeg-formats.html#Demuxers) on FFmpeg's home pages and [Supported Formats](https://en.wikipedia.org/wiki/FFmpeg#Supported_formats) on Wikipedia to get an idea.

Sadly, it also depends on the codecs that have been built into the Linux distribution's library. Some, like openSUSE, only include royalty-free codecs, while others, like Red Hat, completely omit FFmpeg. You can use the following command to find out:

```
ffmpeg -formats
```

A list of the available codecs will be created as a result. In the case of missing codecs or no FFmpeg at all, your only option is to build FFmpeg yourself.
Big fun... 

### Output

As output, only formats that can be read while being written to can be used. Whereas MP4 is not one of them, it can be supported through the use of format extensions. 

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

By choosing *HLS*, TS segments and an M3U playlist (master.m3u8 and index_0_av.m3u8) are created in a directory. A generated hls.html file that can be accessed in a browser can also be used to play the segments.

Please be aware that since most browsers cannot open the files from disc due to restrictions, they must be on a web server. For further information, see [problems](PROBLEMS.md#open-hlshtml-from-disk-to-play-hls-output).

Installation Instructions
-------------------------

A rather detailed description can be found under [install](INSTALL.md).

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

This fork with FFmpeg support copyright \(C) 2017-2023 Norbert Schlia (nschlia@oblivion-software.de).

Based on work Copyright \(C) 2006-2008 David Collett, 2008-2013 K. Henriksson.

This is free software: you are free to change and redistribute it under the terms of the GNU General Public License (GPL) version 3 or later.
