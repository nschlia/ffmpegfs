Hardware Acceleration
=====================

The hardware acceleration feature depends heavily on the hardware used. As this is a personal project, I cannot go out and buy and test all possible devices. So I'll have to rely on you to report your issues so we can iron them out. Even different hardware supporting the same API may behave differently. Sometimes the format range is not the same, sometimes subfeatures are missing, and so on.

## How It Works

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

## Current Implementation

### Supported Hardware Acceleration APIs

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

## Planned Hardware Acceleration APIs

There are several more APIs that could be added. Currently, this is not possible due to a lack of hardware.

| API         | Decode | Encode | Notes                                                        | Details see                                                  |
| ----------- | ------ | ------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| **CUDA**    |        |        | Compute Unified Device Architecture                          | https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html<br />https://en.wikipedia.org/wiki/CUDA<br />https://developer.nvidia.com/ffmpeg |
| **OPENCL**  |        |        | Open Standard for Parallel Programming of Heterogeneous Systems | https://trac.ffmpeg.org/wiki/HWAccelIntro#OpenCL             |
| **VDPAU**   |        |        | Video Decode and Presentation API for Unix                   | https://en.wikipedia.org/wiki/VDPAU                          |
| **QSV**     |        |        | QuickSync                                                    | https://trac.ffmpeg.org/wiki/Hardware/QuickSync              |
| **V4L2M2M** |        |        | v4l2 mem to mem (Video4linux)                                |                                                              |
| **VULKAN**  |        |        | Low-overhead, cross-platform 3D graphics and computing API, requires Libavutil >= 56.30.100 | https://en.wikipedia.org/wiki/Vulkan_(API)                   |

## Hardware Encoding

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

## Hardware Decoding

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
===================

FFmpegfs supports HLS (HTTP Live Streaming). FFmpegfs will create transport stream (ts) segments and the required m3u8 playlists. For your convenience, it will also offer a virtual test.html file that can playback the segments using the hls.js library (see https://github.com/video-dev/hls.js/).

To use the new HLS feature, invoke FFmpegfs with:

     ffmpegfs -f $HOME/test/in $HOME/test/out -o allow_other,ro,desttype=hls

Please note that this will only work over http because most browsers refuse to load multimedia files from the local file system, so you need to publish the directory on a web server. Security restrictions prevent direct playback from a disk. Simply navigate to the directory and open test.html.

Cue Sheets
==========

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
==========

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
==========

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
=========

"Auto copy" performs intelligent stream copy. For example, if transcoding a transport stream that already represents a H264 video and/or AAC audio stream, it is possible to simply repackage it to an mp4 container without recoding.

This is very efficient because it does not require as much computing as de- and encoding, and it also does not degrade quality because the original file remains essentially unchanged.

The function detects if the target format supports the source codec and simply remuxes the stream even if recoding from one format (for example, TS) to another (for example, MOV, MP4).

There are three options:

| Option | Description                                                  |
| ------ | ------------------------------------------------------------ |
| OFF    | no auto copy.                                                |
| LIMIT  | only auto copy if the target file will not become significantly larger. |
| ALWAYS | auto copy whenever possible, even if the target file becomes larger. |

Smart Transcoding
=================

Smart transcoding can create different output formats for video and audio files. For example, video files can be converted to ProRes and audio files to AIFF. Of course, combinations like MP4/MP3 or WebM/WAV are possible but do not make sense, as MP4 or WebM work perfectly with audio-only content.

To use the new feature, simply specify a video and audio file type, separated by a "+" sign. For example, *--desttype=mov+aiff* will convert video files to Apple Quicktime MOV and audio only files to AIFF. This can be handy if the results are consumed, for example, by some Apple editing software, which is very picky about the input format.

*Notes*

1. The first format must be a video codec, and the second must be an audio codec. For example, *--desttype=wav+mp4* is invalid, and instead it should be *--desttype=mp4+wav*.
2. Smart transcoding currently determines the output format by taking the input format type into account, e.g., an MP3 would be recoded to AIFF, an MP4 to MOV even if the input MP4 does not contain a video stream.

The input format should be scanned for streams and the output  selected appropriately: An MP4 with video should be transcoded to MOV,  an MP4 with audio only to AIFF. See [Issue #86] (https://github.com/nschlia/ffmpegfs/issues/86) for details.

Transcoding To Frame Images
=========================

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
=====================

Apple's ProRes is a so-called intermediate format, intended for post-production editing. It combines the highest possible quality while still saving some disk space and not requiring high-performance disk systems. On the other hand, this means that ProRes encoded videos will become quite large—for example, a 60-minute video may require up to 25 GB.

It is not for target audience use, and certainly not suitable for internet streaming.

Also, please keep in mind that when using lossy source input formats, the quality will not get better, but the files can be fed into software like Final Cut Pro, which only accepts a small number of input formats.

Transcoding Subtitles
=====================

Closed captions are converted to the output files, if possible. There are two general subtitle formats: text and bitmap. Subtitle transcoding is currently only possible from text to text or bitmap to bitmap. It may be relatively easy to convert text to bitmap, but not vice versa. This would require some sort of OCR and could become arbitrarily complex. That may work well for Latin alphabets, but there are others. Guess what would happen with Georgian, Indian, Chinese, or Arabic...

| Output Format    | Subtitle Codec                                         | Format |
| ---------------- | ------------------------------------------------------ | ------ |
| MP4, MOV, ProRes | MOV Text (Apple Text Media Handler)                    | Text   |
| WebM             | WebVTT Subtitles (Web Video Text Tracks Format)        | Text   |
| TS, HLS          | DVB Subtitles                                          | Bitmap |
| MKV              | ASS (Advanced SSA), SubRip Subtitles, WebVTT Subtitles | Text   |
| MKV              | DVB Subtitles                                          | Bitmap |

Matroska (MKV) supports a wide range of subtitle formats, both text and bitmap. FFmpegfs automatically selects the best matching output codec. MKV would be the best choice to cover all input subtitle formats.

## External Subtitle Files

Subtitles can reside in separate files. These must have the same filename but the extension "srt" for ASS/SubRip or "vtt" for WebVTT. The language can be defined with a second level extension, e.g. "mediafile.en.srt" would define the contents as "English". There is no convention for the language name, so it could even be the full language name like "mediafile.french.srt" or similar.

Example

| Filename       | Contents                   |
| -------------- | -------------------------- |
| myvideo.mkv    | Video                      |
| myvideo.de.srt | German Subtitles           |
| myvideo.en.srt | English Subtitles          |
| myvideo.es.srt | Spanish Subtitles          |
| myvideo.fr.srt | French Subtitles           |
| myvideo.hu.srt | Hungarian Subtitles        |
| myvideo.it.srt | Italian Subtitles          |
| myvideo.jp.srt | Japanese Subtitles         |
| myvideo.srt    | Unknown Language Subtitles |

**TODO:**

* Maybe FFmpeg could convert text to bitmap for TS/HLS if the input format is not bitmapped.

MP4 Format Profiles
==================

The MP4 container has several derivative formats that are not compatible with all target audiences. To successfully feed the resulting files into, for example, MS Edge, the subformat must be different than for Firefox, unfortunately.

The --profile option allows you to select the format.

| Profile | OS                    | Target                         | Remarks                        |
| ------- | --------------------- | ------------------------------ | ------------------------------ |
| NONE    | all                   | VLC, Windows Media Player etc. | Playback (default)             |
| FF      | Linux, Win10, Android | Firefox                        | OK: Playback while transcoding |
|         | Win7                  | Firefox                        | OK: Playback while transcoding |
| EDGE    | Win10                 | MS Edge, IE > 11               | OK: Playback while transcoding |
|         | Win10 Mobile          |                                | OK: Playback while transcoding |
| IE      | Win10                 | MS IE <= 11                    | OK: Playback while transcoding |
|         | Win7                  |                                | Must decode first (1)          |
| CHROME  | all                   | Google Chrome                  | Must decode first (1)          |
| SAFARI  | Win                   | Apple Safari                   | Must decode first (1)          |
| OPERA   | All                   | Opera                          | Must decode first (1)          |
| MAXTHON | Win                   | Maxthon                        | Must decode first (1)          |

(1)

* An error message appears when the file is opened while transcoding.
* Must start again when the file is transcoded.
* It works fine when the file is loaded directly from the buffer.

This all boils down to the fact that Firefox and Edge are the only browsers that support the necessary extensions to start playback while still transcoding.

In most cases, files will not play if they are not properly optimised.
