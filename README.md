FFmpegfs
========

| Branch | Compiler | Library | Build State |
| ------------- | ------------- | ------------- | ------------- |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | gcc 10.2.1   | FFmpeg 4.3.2-0+deb11u2  | ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs%20(github-ffmpeg)) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | clang 11.0.1 | FFmpeg 4.3.2-0+deb11u2  | ![Build Status](https://secure.oblivion-software.de/jenkins/buildStatus/icon?job=ffmpegfs%20(github-ffmpeg-clang)) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | gcc 9.3.0    | FFmpeg 4.2.4-1ubuntu0.1 | [![Build Status](https://github.com/nschlia/ffmpegfs/actions/workflows/make-gcc.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/make-gcc.yml) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | clang 11.0.1 | FFmpeg 4.2.4-1ubuntu0.1 | [![Build Status](https://github.com/nschlia/ffmpegfs/actions/workflows/make-clang.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/make-clang.yml) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | gcc 9.3.0    | FFmpeg 4.2.4-1ubuntu0.1 | [![Build Status](https://github.com/nschlia/ffmpegfs/actions/workflows/cmake-gcc.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/cmake-gcc.yml) |
| [master](https://github.com/nschlia/ffmpegfs/tree/master) |              |                         | [![CodeQL](https://github.com/nschlia/ffmpegfs/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/codeql-analysis.yml)|
| [master](https://github.com/nschlia/ffmpegfs/tree/master) | gcc 9.3.0    | FFmpeg 4.2.4-1ubuntu0.1 | [![Docker Image](https://github.com/nschlia/ffmpegfs/actions/workflows/docker-image.yml/badge.svg)](https://github.com/nschlia/ffmpegfs/actions/workflows/docker-image.yml)|

[![Packaging status](https://repology.org/badge/vertical-allrepos/ffmpegfs.svg?columns=4)](https://repology.org/project/ffmpegfs/versions)

News
----

### Version 2.11 is under development

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
* **Bugfix:** Add missing include headers to fix build with GCC 12 (closes: [#1012925](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1012925)).

### Version 2.10 has been released

**New in in 2.10 (2022-04-26):**

* **Feature:** [Issue #123](https://github.com/nschlia/ffmpegfs/issues/123): New command line option to hide files by extension. For example, --hide_extensions=jpg, png, cue would stop covers and cue sheets from showing up.
* **Feature:** [Issue #120](https://github.com/nschlia/ffmpegfs/issues/120): Added subtitle support. Subtitle streams can now also be transcoded to the output files. Separate SRT or VTT files will be incorporated as subtitle streams.
* **Bugfix:** Fixed memory leak in encode_audio_frame().
* **Bugfix:** [Issue #122](https://github.com/nschlia/ffmpegfs/issues/122): The last song was missing from the cuesheet files.
* **Bugfix:** [Issue #129](https://github.com/nschlia/ffmpegfs/issues/129): Files remained zero size when previously transcoded.
* **Bugfix:** [Issue #130](https://github.com/nschlia/ffmpegfs/issues/130): Fix file sizes can be incorrectly reported by ls but are correct when data is read.
* **Bugfix:** Duration was not saved in cache SQLite database.
* **Bugfix:** [Issue #131](https://github.com/nschlia/ffmpegfs/issues/131): Sometimes video parameters for some Blu-ray or DVD chapters cannot be detected by the FFmpeg API. Transcode then fails - fixed by using data from the Blu-ray directory or DVD IFO instead.
* The lowest supported FFmpeg API version has been raised to 4.1.8 "al-Khwarizmi".
* Dropped libavresample support, library was removed from the FFmpeg API after 3.4.9.
* Deprecated previous channel layout API based on uint64 bitmasks.
* Deprecated swr_alloc_set_opts() and swr_build_matrix().
* Going to C++17 now: The packet queue has been recoded in C++17 to support external subtitle files. As C++17 is required now, why not go all the way: Starting to replace legacy C++ and somewhat C-like parts with real C++.
* Using std::shared_ptr to ensure proper memory allocation/free.

Future Objectives
-----------------

*Any ideas or wishes?* Free to create [an issue](https://github.com/nschlia/ffmpegfs/issues) and let me know. Some great features started this way!

For more information, see [TODO](TODO).

## History

See [HISTORY](HISTORY.md) for details.

Installation Instructions
-------------------------

You'll find a rather extensive explanation in the [INSTALL](INSTALL.md) file.

About
-----

Web site: https://nschlia.github.io/ffmpegfs/

FFmpegfs is a read-only FUSE filesystem which transcodes between audio and video formats on the fly when opened and read.

### Audio Formats
| Format | Description | Audio |
| ------------- | ------------- | ------------- |
| AIFF | Audio Interchange File Format | PCM 16 bit BE |
| ALAC | Apple Lossless Audio Codec | ALAC |
| FLAC | Free Lossless Audio | FLAC |
| MP3 | MPEG-2 Audio Layer III | MP3 |
| Opus |Opus Audio| Opus |
| WAV | Waveform Audio File Format | PCM 16 bit LE |

### Video Formats
| Format | Description | Video | Audio |
| ------------- | ------------- | ------------- | ------------- |
| HLS | HTTP Live Streaming | H264 | AAC |
| MOV | QuickTime File Format | H264 | AAC |
| MP4 | MPEG-4 | H264 | AAC |
| OGG|Free Open Container Format| Theora | Vorbis |
| ProRes | Apple ProRes | ProRes | PCM 16 bit LE |
| TS | MPEG Transport Stream | H264 | AAC |
| WebM|Free Open Web Media Project| VP9 | Opus |

### Stills

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

Please note that the files must be on a web server because restrictions prevent most browsers from opening the files from disk. See [FIXING PROBLEMS](README.md#fixing-problems) for details.

Supported Linux Distributions
-----------------------------

Tested with:

| Distribution | FFmpeg Version | Remarks | Result |
|---|---|---|---|
| **Daily build** | N-99880-g8fbcc546b8 |  | OK |
| **Debian 9 Stretch** | 3.2.8-1~deb9u1 |  | OK |
| **Debian 10 Buster** | 4.1.6-1~deb10u1 |  | OK |
| **Debian 11 Bullseye** | 4.3.2-0+deb11u2 |  | OK |
| **Raspbian 10 Buster** | 4.1.6-1~deb10u1+rpt1 |  | OK |
| **Raspbian 11 Bullseye** | 4.3.2-0+rpt3+deb11u2 | Added subtitle support | OK |
| **Ubuntu 16.04.3 LTS** | .8.11-0ubuntu0.16.04.1 |  | OK |
| **Ubuntu 17.10** | 3.3.4-2 |  | OK |
| **Ubuntu 20.04** | 4.2.2-1ubuntu1 |  | OK |
| **Suse 42** | 3.3.4 | See notes below | not OK |
| **Red Hat 7**| FFmpeg must be compiled from sources |  | OK |
| **Funtoo 7.3.1** | 3.4.1 | FFmpeg needs to be installed with correct "USE flags", see [install](INSTALL.md) | OK |

**Suse** does not provide proprietary formats like AAC and H264, so the distribution of FFmpeg is crippled. FFmpegfs will not be able to encode to H264 and AAC. End of story.

See https://en.opensuse.org/Restricted_formats.

**Tips on other operating systems and distributions, such as Mac or other nixes, are welcome.**

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

Hardware Acceleration
---------------------
The hardware acceleration feature depends heavily on the hardware used. As this is a personal project, I cannot go out and buy and test all possible devices. So I'll have to rely on you to report your issues so we can iron them out. Even different hardware supporting the same API may behave differently. Sometimes the format range is not the same, sometimes subfeatures are missing, and so on.

### How It Works

Acceleration is done by specialised graphics adapters. The FFmpeg API can use several types using a range of APIs. As of today, even cheap on-board chips can do hardware acceleration.

Here is an incomplete list.

Hardware acceleration using hardware buffered frames:

* VAAPI: Intel, AMD (Decoders: H.264, MPEG-2, MPEG-4 part 2, VC-1. H.265, H.265 10-bit on recent devices. Encoder: H.264, H.265, MPJEPG, MPEG-2, VP8/9, MPEG-4 part 2 can probably be enabled.)
* VDPAU: Nividia, AMD (H.264, MPEG-1/2/4, and VC-1)
* CUDA: Compute Unified Device Architecture (Decoders: VP9, H.264, MPEG-2, MPEG-4. Encoding: H.264, H.265), see https://developer.nvidia.com/ffmpeg and https://en.wikipedia.org/wiki/CUDA
* QSV: QuickSync, see https://trac.ffmpeg.org/wiki/Hardware/QuickSync
* OPENCL: Open Standard for Parallel Programming of Heterogeneous Systems, see https://trac.ffmpeg.org/wiki/HWAccelIntro#OpenCL
* VULKAN: Low-overhead, cross-platform 3D graphics and computing API, requires Libavutil >= 56.30.100, see https://en.wikipedia.org/wiki/Vulkan_(API)

These use software frames:

* v4l2m2m: Intel (Encoders: H.263, H.264, H.265, MPEG-4, VP8). Decoders: H.263, H.264, H.265, MPEG-1, MPEG-2, MPEG-4, VC-1, VP8, VP9.)
* OpenMAX: Encoding on Raspberry (H.264, MPEG-4. Requires key to unlock.)
* MMAL: Decoding on Raspberry (H.264, MPEG-2, MPEG-4, VC-1. Requires key to unlock.)

More information can be found at https://trac.ffmpeg.org/wiki/HWAccelIntro.

### Current Implementation

#### Supported Hardware Acceleration APIs

These APIs are implemented and tested. VAAPI mostly targets Intel hardware, but there are other hardware vendors that offer support now. MMAL and OpenMAX are supported by Raspberry PI boards.

| API       | Decode | Encode | Description                                     | Details see                                                  |
| --------- | ------ | ------ | ----------------------------------------------- | ------------------------------------------------------------ |
| **VAAPI** | x      | x      | Video Acceleration API (VA-API), formerly Intel | https://en.wikipedia.org/wiki/Video_Acceleration_API<br />https://trac.ffmpeg.org/wiki/Hardware/VAAPI |
| **MMAL**  |        | x      | Multimedia Abstraction Layer by Broadcom        | https://github.com/techyian/MMALSharp/wiki/What-is-MMAL%3F<br />http://www.jvcref.com/files/PI/documentation/html/ |
| **OMX**   | x      |        | OpenMAX (Open Media Acceleration)               | https://en.wikipedia.org/wiki/OpenMAX                        |

#### Tested On

| System                                                       | CPU                                                          | GPU                             | APIs         |
| ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------- | ------------ |
| Debian 10                                                    | Intel Core i5-6500 CPU @ 3.20GHz                             | Intel HD Graphics 530 (rev 06)  | VAAPI        |
| Debian 11                                                    | Intel Core i5-8250U CPU @ 1.60GHz                            | Intel UHD Graphics 620 (rev 07) | VAAPI        |
| Debian 11                                                    | Intel(R) Core(TM) i7-1065G7 CPU @ 1.30GHz                    | NVIDIA GP108M, GeForce MX330    | VAAPI        |
| Raspbian 10<br />Raspberry Pi 2 Model B Rev 1.1<br />Raspberry Pi 3 Model B Plus Rev 1.3 | <br />ARMv7 Processor rev 5 (v7l)<br />ARMv7 Processor rev 4 (v7l) |                                 | OpenMAX/MMAL |

### Planned Hardware Acceleration APIs

There are several more APIs that could be added. Currently, this is not possible due to a lack of hardware.

| API         | Decode | Encode | Notes                                                        | Details see                                                  |
| ----------- | ------ | ------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| **CUDA**    |        |        | Compute Unified Device Architecture                          | https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html<br />https://en.wikipedia.org/wiki/CUDA<br />https://developer.nvidia.com/ffmpeg |
| **OPENCL**  |        |        | Open Standard for Parallel Programming of Heterogeneous Systems | https://trac.ffmpeg.org/wiki/HWAccelIntro#OpenCL             |
| **VDPAU**   |        |        | Video Decode and Presentation API for Unix                   | https://en.wikipedia.org/wiki/VDPAU                          |
| **QSV**     |        |        | QuickSync                                                    | https://trac.ffmpeg.org/wiki/Hardware/QuickSync              |
| **V4L2M2M** |        |        | v4l2 mem to mem (Video4linux)                                |                                                              |
| **VULKAN**  |        |        | Low-overhead, cross-platform 3D graphics and computing API, requires Libavutil >= 56.30.100 | https://en.wikipedia.org/wiki/Vulkan_(API)                   |

### Hardware Encoding

This version has been tested with VAAPI (Debian) and OpenMAX (Raspberry). It may be possible that other APIs work, but this has not been confirmed yet.

To enable hardware support, use these parameters respectively (of course, use only one):

```
-hwaccel_enc=VAAPI
-hwaccel_enc=OMX
```

If your system supports VAAPI:

* It could be possible that the rendering device on your system goes by a different name than the default "/dev/dri/renderD128". You can use the --hwaccel_enc_device parameter to set it.
* Depending on what your renderer supports, setting a bitrate will fail. On my test system, for example, CQG (Constant Quantisation parameter) is the only valid rendering control mode. The driver ignores bitrate settings and accepts the qp option only.

As found in libavcodec/vaapi_encode.c:

 Rate control mode selection:
 * If the user has set a mode explicitly with the rc_mode option, use it and fail if it is not available.
 * If an explicit QP option has been set, use CQP.
 * If the codec is CQ-only, use CQP.
 * If the QSCALE avcodec option is set, use CQP.
 * If bitrate and quality are both set, try QVBR.
 * If quality is set, try ICQ, then CQP.
 * If bitrate and maxrate are set and have the same value, try CBR.
 * If a bitrate is set, try AVBR, then VBR, then CBR.
 * If no bitrate is set, try ICQ, then CQP.

At the moment, this is hardwired into the code. None of these values can be controlled now by the command line. This is planned, of course, but not implemented in the current version yet.

### Hardware Decoding

This version has been tested with VAAPI (Debian) and MMAL (Raspberry). It may be possible that other APIs work, but this has not been confirmed yet.

To enable hardware support, use these parameters respectively (of course, use only one):

```
-hwaccel_dec=VAAPI
-hwaccel_dec=MMAL
```

If your system supports VAAPI:

- It could be possible that the rendering device on your system goes by a different name than the default "/dev/dri/renderD128". You can use the --hwaccel_enc_device parameter to set it.

On slow machines like the Raspberry, this should give an extra kick and also relieve the CPU from load. On faster machines, this impact may be smaller, yet noticeable. 

The decoding part is a bit tricky. If encoding is set to hardware, and this hardware is there and capable of encoding, it will work. If hardware decoding is possible, it depends on the source file. Therefore, the file needs to be checked first and then it needs to be decided if hardware acceleration can be used or if a fallback to software is required. FFmpeg requires that to be set via the command line, but FFmpegfs must be able to decide that automatically.

### TODOs

Doing both de- and encoding in hardware can make costly transfers of frames between software and hardware memory unneccessary.

It is not clear, at the moment, if it is possible to keep the frames in hardware as FFmpegfs does some processing with the frames (for example, rescaling or deinterlacing), which probably cannot be done without transferring buffers from hardware to software memory and vice versa. We'll see.

As seen above, selecting a target bitrate turns out to be a bit tricky. I'll have to work out a way to reach the desired bitrate in any case (no matter if the hardware supports CQP, VBR, CBR, ICQ, or AVBR).

On the other hand, everything seems to work and there are no show stoppers in sight. Sheesh, wiping the sweat off my chin :)

HTTP Live Streaming
-------------------

FFmpegfs supports HLS (HTTP Live Streaming). FFmpegfs will create transport stream (ts) segments and the required m3u8 playlists. For your convenience, it will also offer a virtual test.html file that can playback the segments using the hls.js library (see https://github.com/video-dev/hls.js/).

To use the new HLS feature, invoke FFmpegfs with:

     ffmpegfs -f $HOME/test/in $HOME/test/out -o allow_other,ro,desttype=hls

Please note that this will only work over http because most browsers refuse to load multimedia files from the local file system, so you need to publish the directory on a web server. Security restrictions prevent direct playback from a disk. Simply navigate to the directory and open test.html.

Cue Sheets
----------

Cue sheets, or cue sheet files, were first introduced for the CDRWIN CD/DVD burning software. Basically, they are used to define a CD or DVD track layout. Today, they are supported by a wide range of optical disk authoring applications and, moreover, media players.

When a media file is accompanied by a cue sheet, its contents are read and a virtual directory with separate tracks is created. The cue sheet file must have the same name but the extension ".cue" instead. It can also be embedded into the media file.

The directory is named after the source media, with an additional ".tracks" extension. If several media files with different extensions exist, for example, different formats, several ".tracks" directories will be visible.

Example:

     myfile.mp4
     myfile.ogv
     myfile.cue

If the destination type is TS, the following files and directories will appear:

     myfile.mp4
     myfile.mp4.ts
     myfile.ogv
     myfile.ogv.ts
     myfile.cue
     myfile.mp4.tracks/
     myfile.ogv.tracks/

Tracks defined in the cue sheet will show up in the *.tracks subdirectories.

Selecting Audio and Video Codecs
----------

Some new codec combinations are now possible (the default codecs are in bold):

| Formats | Audio Codecs      | Video Codecs                 |
| ------- | ----------------- | ---------------------------- |
| MP4     | **AAC**, MP3      | **H264**, H265, MPEG1, MPEG2 |
| WebM    | **OPUS**, VORBIS  | **VP9**, VP8, AV1            |
| MOV     | **AAC**, AC3, MP3 | **H264**, H265, MPEG1, MPEG2 |
| TS, HLS | **AAC**, AC3, MP3 | **H264**, H265, MPEG1, MPEG2 |

For audio, the codec can be selected with --audiocodec. For videos, use --videocodec. Without these parameters, FFmpegfs will use the codecs as before (no change).

Please note that hardware acceleration might not work, e.g., my hardware encoder supports H264 but not H265. So even though H265 creates much smaller files, it takes 10 times longer to transcode.

Building A Docker Container
----------

FFmpegfs can run under Docker. A Dockerfile is provided to build a container for FFmpegfs. Change to the "docker" directory and run

     docker build --build-arg BRANCH=master -t nschlia/ffmpegfs .

Depending on the machine speed, this will take quite a while. After the command is completed, the container can be started with

     docker run --rm \
          --cgroupns host \
          --name=ffmpegfs \
          --device /dev/fuse \
          --cap-add SYS_ADMIN \
          --security-opt apparmor:unconfined \
          -v /path/to/source:/src:ro \
          -v /path/to/output:/dst:rshared \
          nschlia/ffmpegfs \
          -f --log_stderr --audiobitrate=256K -o allow_other,ro,desttype=mp3,log_maxlevel=INFO

Of course, */path/to/source* must be changed to a directory with multi-media files and */path/to/output* to where the converted files should be visible. The output type may be changed to MP4 or whatever is desired. 

Auto Copy
---------

"Auto copy" performs intelligent stream copy. For example, if transcoding a transport stream that already represents a H264 video and/or AAC audio stream, it is possible to simply repackage it to an mp4 container without recoding.

This is very efficient because it does not require as much computing as de- and encoding, and it also does not degrade quality because the original file remains essentially unchanged.

The function detects if the target format supports the source codec and simply remuxes the stream even if recoding from one format (for example, TS) to another (for example, MOV, MP4).

There are three options:

|Option|Description|
| ------------- | ------------- |
|OFF|no auto copy.|
|LIMIT|only auto copy if the target file will not become significantly larger.|
|ALWAYS|auto copy whenever possible, even if the target file becomes larger.|

Smart Transcoding
-----------------

Smart transcoding can create different output formats for video and audio files. For example, video files can be converted to ProRes and audio files to AIFF. Of course, combinations like MP4/MP3 or WebM/WAV are possible but do not make sense, as MP4 or WebM work perfectly with audio-only content.

To use the new feature, simply specify a video and audio file type, separated by a "+" sign. For example, *--desttype=mov+aiff* will convert video files to Apple Quicktime MOV and audio only files to AIFF. This can be handy if the results are consumed, for example, by some Apple editing software, which is very picky about the input format.

*Notes*

1. The first format must be a video codec, and the second must be an audio codec. For example, *--desttype=wav+mp4* is invalid, and instead it should be *--desttype=mp4+wav*.
2. Smart transcoding currently determines the output format by taking the input format type into account, e.g., an MP3 would be recoded to AIFF, an MP4 to MOV even if the input MP4 does not contain a video stream.

The input format should be scanned for streams and the output  selected appropriately: An MP4 with video should be transcoded to MOV,  an MP4 with audio only to AIFF. See [Issue #86] (https://github.com/nschlia/ffmpegfs/issues/86) for details.

Transcode To Frame Images
-------------------------

To transcode a video to frame images, set the destination type to JPG, PNG, or BMP. This will convert videos into virtual folders with one image for each frame.

```
$ ls /storage/videos
  video1.mp4
  video2.mov

$ ffmpegfs /storage/videos /mnt/ffmpegfs
$ find /mnt/ffmpegfs
  /mnt/ffmpegfs/video1.mp4/00001.png
  /mnt/ffmpegfs/video1.mp4/00002.png
  ...
  /mnt/ffmpegfs/video1.mov/00001.png
  /mnt/ffmpegfs/video1.mov/00002.png
```

A Few Words On ProRes
---------------------

Apple's ProRes is a so-called intermediate format, intended for post-production editing. It combines the highest possible quality while still saving some disk space and not requiring high-performance disk systems. On the other hand, this means that ProRes encoded videos will become quite large—for example, a 60-minute video may require up to 25 GB.

It is not for target audience use, and certainly not suitable for internet streaming.

Also, please keep in mind that when using lossy source input formats, the quality will not get better, but the files can be fed into software like Final Cut Pro, which only accepts a small number of input formats.

Transcoding Subtitles
---------------------

Closed captions are converted to the output files, if possible. There are two general subtitle formats: text and bitmap. Subtitle transcoding is currently only possible from text to text or bitmap to bitmap. It may be relatively easy to convert text to bitmap, but not vice versa. This would require some sort of OCR and could become arbitrarily complex. That may work well for Latin alphabets, but there are others. Guess what would happen with Georgian, Indian, Chinese, or Arabic...

| Output Format    | Subtitle Codec                                               | Format |
| ---------------- | ------------------------------------------------------------ | ------ |
| MP4, MOV, ProRes | MOV Text (Apple Text Media Handler)                          | Text   |
| WebM             | WebVTT Subtitles (Web Video Text Tracks Format)              | Text   |
| TS, HLS          | DVB Subtitles                                                | Bitmap |
| MKV              | ASS (Advanced SSA), SubRip Subtitles, WebVTT Subtitles       | Text   |
| MKV              | DVB Subtitles                                                | Bitmap |

Matroska (MKV) supports a wide range of subtitle formats, both text and bitmap. FFmpegfs automatically selects the best matching output codec. MKV would be the best choice to cover all input subtitle formats.

### External Subtitle Files

Subtitles can reside in separate files. These must have the same filename but the extension "srt" for ASS/SubRip or "vtt" for WebVTT. The language can be defined with a second level extension, e.g. "mediafile.en.srt" would define the contents as "English". There is no convention for the language name, so it could even be the full language name like "mediafile.french.srt" or similar.

Example

| Filename | Contents |
| ---- | ---- |
| myvideo.mkv | Video |
| myvideo.de.srt | German Subtitles |
| myvideo.en.srt | English Subtitles     |
| myvideo.es.srt | Spanish Subtitles     |
| myvideo.fr.srt | French Subtitles     |
| myvideo.hu.srt | Hungarian Subtitles     |
| myvideo.it.srt | Italian Subtitles     |
| myvideo.jp.srt | Japanese Subtitles     |
| myvideo.srt | Unknown Language Subtitles     |

**TODO:**

* Maybe FFmpeg could convert text to bitmap for TS/HLS if the input format is not bitmapped. 

MP4 Format Profiles
------------------

The MP4 container has several derivative formats that are not compatible with all target audiences. To successfully feed the resulting files into, for example, MS Edge, the subformat must be different than for Firefox, unfortunately.

The --profile option allows you to select the format.

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
* An error message appears when the file is opened while transcoding.
* Must start again when the file is transcoded.
* It works fine when the file is loaded directly from the buffer.

This all boils down to the fact that Firefox and Edge are the only browsers that support the necessary extensions to start playback while still transcoding.

In most cases, files will not play if they are not properly optimised.

See [TODO](TODO) for details.

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

Fixing Problems
---------------

### General problems accessing a file

Due to the nature in which FFmpegfs works, if anything goes wrong, it can only report a general error. That means, when there is a problem accessing a file, copying or opening it, you will get an "Invalid argument" or "Operation not permitted". Not really informative. There is no way to pass the original result code through the file system. 

What you will have to do is refer to the syslog, system journal, or to the FFmpeg log itself, optionally at a higher verbosity. If you are unable to resolve the problem yourself, feel free to create [an issue](https://github.com/nschlia/ffmpegfs/issues), stating what you have done and what has happened. Do not forget to add logs (preferably at a higher verbosity) and, if possible, a description of how to recreate the issue. A demo file that causes the error would be helpful as well.

### Transcoding too slow

See [Building FFmpeg with optimisations](INSTALL.md#building-ffmpeg-with-optimisations)

### Lock ups when accessed through Samba

When accessed from a Samba drive, the pending read can lock the whole share, causing Windows Explorer and even KDE Dolphin to freeze. Any access from the same machine to that share is blocked. Even "ls" is not possible and blocks until the data is returned.

It seems others had the same problem:

http://samba.2283325.n4.nabble.com/Hangs-Accessing-fuse-filesystem-in-Windows-through-Samba-td4681904.html

Adding this to the [global] config in smb.conf fixes that:

 	oplocks = no
 	level2 oplocks = no
 	aio read size = 1

The "aio read size" parameter may be moved to the share config:

 	aio read size = 1

### rsync, Beyond Compare and other tools

Some copy tools do not get along very well with dynamically generated files, as in [Issue #23: Partial transcode of some files](https://github.com/nschlia/ffmpegfs/issues/22).

Under Linux, it is best to use (optionally with the -r parameter)

        cp -uv /path/to/source /path/to/target

This will copy all missing or changed files without missing parts. On the Windows side, Windows Explorer or copy/xcopy work. Tools like Beyond Compare may only copy the predicted size first and may not respond to size changes.

### Open hls.html from disk to play HLS output.

* Most browsers prevent the playback of files from disk. You may put them into a website directory, but sometimes https must be used or playback will be blocked.

  **To enable disk playback in Firefox:**

  \* Open about:config

  \* Set security.fileuri.strict_origin_policy to false.

### Songs get cut short

If songs do not play to the very end and you are using SAMBA or NFS, you're in trouble.

It happens when the files are transcoded on the fly, but never when the file comes from the cache. This is because the result is never exactly what was predicted.

SAMBA fills files with zeros if the result is smaller than predicted, or cuts off the rest if the file is larger than predicted.

NFS arbitrarily sends the correct file, or one that is cut or padded like SAMBA. This can be repeated as many times as one wants to—once the file is OK, once not.

As of yet, there seems to be no way around that. Maybe NFS or SAMBA can be configured to cope with that, but how to is unknown to me.

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

Demo Code
---------

See https://github.com/video-dev/hls.js/ for the HLS player and demo code.

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
