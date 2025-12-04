# FFmpegfs

FFmpegfs is a read-only FUSE filesystem that transcodes various audio and video formats to MP4, WebM, and many more **on the fly** using the FFmpeg library. It thus supports a multitude of input formats and provides access in a variety of common output formats.

This enables seamless access to a multimedia file collection with software and/or hardware that only supports one of the output formats, or allows transcoding files simply via drag-and-drop in a file browser.

📌 Website: [https://nschlia.github.io/ffmpegfs/](https://nschlia.github.io/ffmpegfs/)

---

## Code Status

| Branch                                                    | Build State                                                  |
| --------------------------------------------------------- | ------------------------------------------------------------ |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | [![Build Status](https://github.com/nschlia/ffmpegfs/actions/workflows/make-gcc.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/make-gcc.yml) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | [![Build Status](https://github.com/nschlia/ffmpegfs/actions/workflows/make-clang.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/make-clang.yml) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | [![CodeQL](https://github.com/nschlia/ffmpegfs/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/codeql-analysis.yml) |

## Packaging Status

[![Packaging status](https://repology.org/badge/vertical-allrepos/ffmpegfs.svg?columns=4)](https://repology.org/project/ffmpegfs/versions)

---

## News

### Windows Version

A Windows version of FFmpegfs has frequently been requested; see issue [#76](https://github.com/nschlia/ffmpegfs/issues/76) for more information. In essence, this failed because Windows doesn't support FUSE. I discovered [WinFSP](https://winfsp.dev/), which offers everything necessary.

To see what's been done so far, checkout the [windows](https://github.com/nschlia/ffmpegfs/tree/windows) branch.

---

### New in 2.18

- **Feature:** Added ALAC profile for iTunes (`--desttype=ALAC --profile=ITUNES`). Playback of the file will not commence until it is fully recoded; however, it can be played in iTunes.
- **Feature:** Implemented a validation check for the combination of TYPE and PROFILE in `--desttype=TYPE --profile=PROFILE`.
- Updated Dockerfile to include FUSE 3.
- **Bugfix:** Fix error with new FFmpeg API: *"Option 'pix_fmts' is not a runtime option and so cannot be set after the object has been initialized"*.
- **Fixed deprecation:** Replace `avcodec_get_supported_config()`.
- **Fixed deprecation:** Remove `avcodec_close()`.
- **Fixed deprecation:** Remove `av_format_inject_global_side_data()`.
- **Fixed deprecation:** Replace `std::codecvt` with `iconv` in `read_file`.
- **Bugfix:** `reserve()` only guarantees capacity, not size → writing via `.data()` is undefined behaviour. Using `resize()` makes the memory usable.
- As `strerror()` is not thread-safe, use `strerror_r()` where available.
- `strncpy` likes to copy without NUL → terminate explicitly.
- **Bugfix:** Issue [#173](https://github.com/nschlia/ffmpegfs/issues/173): Fixed output directory no showing complete list of files under Debian 13.

---

### New in 2.17 (2024-11-10)

- **Bugfix:** Issue [#164](https://github.com/nschlia/ffmpegfs/issues/164): Fixed incorrectly discarded HLS seek requests.
- **Bugfix:** Wrong error message fixed when an invalid audio/video codec was selected. The message should rather say "unsupported codec" instead of talking about "sample format not supported.".
- **Bugfix:** Issue [#162](https://github.com/nschlia/ffmpegfs/issues/162): If not present, add time stamps to the copied streams.
- Changed quality from 34 to 40 for hardware encoded video streams to create slightly smaller files.
- [Closes#1084487:](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1084487): Moved from the FUSE 2 to the FUSE 3 API.

📜 See [HISTORY.md](HISTORY.md) for the complete changelog.

---

## Supported Formats

### Input

Making a full list of the formats the FFmpeg API supports would be somewhat pointless. See [Demuxers](https://ffmpeg.org/ffmpeg-formats.html#Demuxers) on FFmpeg's home pages and [Supported Formats](https://en.wikipedia.org/wiki/FFmpeg#Supported_formats) on Wikipedia to get an idea.

Sadly, it also depends on the codecs that have been built into the Linux distribution's library. Some, like openSUSE, only include royalty-free codecs, while others, like Red Hat, completely omit FFmpeg. You can use the following command to find out:

```bash
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

---

## Installation Instructions

A rather detailed description can be found under [install](INSTALL.md).

---

## Fixing Problems

This part has been transferred to a different file because it has gotten too big. Details can be found in [problems](PROBLEMS.md).

---

## Usage

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

Keep in mind that only root can, by default, use the "allow_other" option. Either use the "user_allow_other" key in `/etc/fuse.conf` or run FFmpegfs as root.

Any user must have "allow_other" enabled in order to access the mount. By default, only the user who initiated FFmpegfs has access to this.

Examples:

    ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache

Transcode files using FFmpegfs from `test/in` to `test/out` while logging to stderr at a noisy DEBUG level. The cache resides in `test/cache`. All directories are under the current user's home directory.

     ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache,videowidth=640

Similar to the previous, but with a 640-pixel maximum video width. The aspect ratio will be maintained when scaling down larger videos. Videos that are smaller won't be scaled up.

     ffmpegfs -f $HOME/test/in $HOME/test/out --log_stderr --log_maxlevel=DEBUG -o allow_other,ro,desttype=mp4,cachepath=$HOME/test/cache,deinterlace

Deinterlacing can be enabled for better image quality.

## More About Features

There is a [feature list](FEATURES.md) with detailed explanations.

---

## How It Works

The decoder and encoder are initialised when a file is opened, and the file's metadata is also read. At this point, a rough estimate of the total file size can be made. Because the actual size greatly depends on the material encoded, this technique works fair-to-good for MP4 or WebM output files but works well for MP3, AIFF, or WAV output files.

The file is transcoded as it is being read and stored in a private per-file buffer. This buffer keeps expanding as the file is read until the entire file has been transcoded. After being decoded, the file is stored in a disc buffer and is readily accessible.

Other processes will share the same transcoded data if they access the same file because transcoding is done in a single additional thread, which saves CPU time. Transcoding will continue for a while if all processes close the file before it is finished. Transcoding will resume if the file is viewed once more before the timer expires. If not, it will halt and delete the current chunk to free up storage space.

A file will be transcoded up to the seek point when you seek within it (if not already done). Since the majority of programmes will read a file from beginning to end, this is typically not a problem. Future upgrades might offer actual random seeking (but if this is feasible, it is not yet clear due to restrictions to positioning inside compressed streams). When HLS streaming is chosen, this already functions. The requested segment is immediately skipped to by FFmpegfs.

**MP3:** The source file's comments are used to generate ID3 version 2.4 and 1.1 tags. They are correspondingly at the beginning and the end of the file.

**MP4:** The same is true for meta atoms contained in MP4 containers.

**WAV:** The estimated size of the WAV file will be included in a pro forma WAV header. When the file is complete, this header will be changed. Though most current players apparently disregard this information and continue to play the file, it does not seem required.

Only for MP3 targets: A particular optimization has been done so that programmes that look for id3v1 tags don't have to wait for the entire file to be transcoded before reading the tag. This accelerates these apps *dramatically*.

---

## About Output Formats

A few remarks regarding the output formats that are supported:

Since these are plain vanilla constant bitrate (CBR) MP3 files, there isn't much to say about the MP3 output. Any modern player should be able to play them well.

However, MP4 files are unique because standard MP4s aren't really ideal for live broadcasting. The start block of an MP4 has a field with the size of the compressed data section, which is the cause. It suffices to say that until the size is known, compression must be finished, a file seek must be performed to the beginning, and the size atom updated.

That size is unknown for a live stream that is ongoing. To obtain that value for our transcoded files, one would need to wait for the entire file to be recoded. As if that weren't enough, the file's final section contains some crucial details, such as meta tags for the artist, album, etc. Additionally, the fact that there is just one enormous data block makes it difficult to do random searches among the contents without access to the entire data section.

Many programmes will then read the crucial information from the end of an MP4 before returning to the file's head and beginning playback. This will destroy FFmpegfs' entire transcode-on-demand concept.

Several extensions have been created to work around the restriction, including "faststart," which moves the aforementioned meta data from the end to the beginning of the MP4 file. Additionally, it is possible to omit the size field (0). An further plugin is isml (smooth live streaming).

Older versions of FFmpeg do not support several new MP4 features that are required for direct-to-stream transcoding, like ISMV, faststart, separate moof/empty moov, to mention a few (or if available, not working properly).

Faststart files are produced by default with an empty size field so that the file can be started to be written out at once rather than having to be encoded as a complete first. It would take some time before playback could begin if it were fully encoded. The data part is divided into chunks of about 1 second each, all with their own header, so it is possible to fill in the size fields early enough.

One disadvantage is that not all players agree with the format, or they play it with odd side effects. VLC only refreshes the time display every several seconds while playing the file. There may not always be a complete duration displayed while streaming using HTML5 video tags, but that is fine as long as the content plays. Playback can only move backwards from the current playback position. However, that is the cost of commencing playback quickly.

---

## Development

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

---

## Future Objectives

*Any ideas or wishes?* Free to create [an issue](https://github.com/nschlia/ffmpegfs/issues) and let me know. Some great features started this way!

For more information, see [TODO](TODO).

---

## Authors

- **Current maintainer (2017–present):** Norbert Schlia (nschlia@oblivion-software.de)  
- **Earlier maintainer (2008–2017):** K. Henriksson  
- **Original author (2006–2008):** David Collett  

Thanks to all for their foundational work.

---

## License

- FFmpegfs: **GPLv3 or later**  
- Documentation: **GFDL 1.3 or later** (or alternatively GPLv3+)  
- FFmpeg itself: **LGPL 2.1+**, with some optional GPL components. See [FFmpeg Legal](https://www.ffmpeg.org/legal.html).
