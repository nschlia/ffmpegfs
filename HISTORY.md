History
=======

### Version 2.9 released

**New in in 2.9 (2022-02-16):**

* **Feature:** [Issue #97](https://github.com/nschlia/ffmpegfs/issues/97):  Added options to chose different codecs. The audio codec can be selected with --audiocodec, for videos use --videocodec.
* **Feature:** [Issue #109](https://github.com/nschlia/ffmpegfs/issues/109):  Allow user defined file extensions for source files. By default, only standard extensions are accepted, i.e., mp4, ts, avi etc. Arbitrary file extensions can be defined now, e.g. using --extensions=xxx,abc,yxz,aaa to also convert files ending with .xxx, .abc and so on.
* **Feature:** [Issue #121](https://github.com/nschlia/ffmpegfs/issues/121): Added MKV support.  New format can be selected with --desttype=mkv.
* **Bugfix:** [Issue #112](https://github.com/nschlia/ffmpegfs/issues/112): Fixed Docker detection.
* **Bugfix:** [Issue #110](https://github.com/nschlia/ffmpegfs/issues/110): Docker build command failed, added missing libchardet and allow libdvdread4 or *8 to be used, whatever available.
* **Bugfix:** Fixed crash when video had no audio.
* **Bugfix:** [Issue #112](https://github.com/nschlia/ffmpegfs/issues/112): Fixed access problems with frame sets and HLS. 
* **Bugfix:** Issue #119: Fixed problem that caused frame set generation to sometimes fail.
* **Bugfix:** Fixed JPG frame set generation. Suddenly FF_COMPLIANCE_UNOFFICIAL was required to have FFmpeg API accept the codec.
* **Enhancement:** [Issue #67](https://github.com/nschlia/ffmpegfs/issues/67): Enhanced file size prediction.
* **Bugfix:** Need to synchronise screen log. Concurrent entries by separate threads produced garbled output.
* **Bugfix:** Avoid creating an HLS segment number which is out of bounds (higher than the expected number of segments).
* **Bugfix:** Removed QMake support, replaced with CMake.

### Version 2.8 released

**New in in 2.8 (2021-11-29):**

* **Bugfix:** [Issue #102](https://github.com/nschlia/ffmpegfs/issues/102): Not all SQL queries where case sensitive, causing cache confusion. Several database entries were created, but only one was updated. Made all queries case sensitive.
* **Bugfix:** [Issue #91](https://github.com/nschlia/ffmpegfs/issues/91): Fixed HLS problems with cache causing garbled videos and hick-ups in audio.
* **Enhancement**: [Issue #103](https://github.com/nschlia/ffmpegfs/issues/103): If requested HLS segment is less than X (adjustable) seconds away, discard seek request. Segment would be available very soon anyway, and that seek makes a re-transcode necessary. Can be set with *--min_seek_time_diff*. Defaults to 30 seconds.
* **Feature**: [Issue #105](https://github.com/nschlia/ffmpegfs/issues/105): Added Free Lossless Audio Codec (FLAC) support. Activate with *--desttype=FLAC*.
* **Feature:** [Issue #101](https://github.com/nschlia/ffmpegfs/issues/101): Sample format for audio files can be selected via command line with *--audiosamplefmt*. Possible values are 0 to use the predefined setting, 8, 16, 32, 64 for integer format, F16, F32, F64 for floating point.
  Not all formats are supported by all destination types, selecting an invalid format will be reported as error and a list of values printed.
  Defaults to 0 (Use same as source or the predefined format of the destination if source format is not possible).

### Version 2.7 released

**New in in 2.7 (2021-11-08):**

* **Bugfix:** [Issue #92](https://github.com/nschlia/ffmpegfs/issues/92): Fixed crash when hardware decoding failed. The problem is that the FFmpeg API very late reports that it cannot decode the file in hardware. To find out about that, the source file must be decoded until the first video frame is encountered.
  It would be very time consuming to do this on every file (decode until it is clear that the file is supported, then actually start transcoding it from scratch). There is no feasible way to automatically handle the situation. To get around this a --hwaccel_dec_blocked parameter has been added.
  If hardware decoding fails, check the log for a message similar this:
  "[vp9 @ 0x7fe910016080] No support for codec vp9 profile 0."
  If VP9 profile 0 is not supported, the parameter would be:
  --hwaccel_dec_blocked=VP9:0
  This will tell FFmpegfs to decode the file in software. To block VP9 as a whole, the parameter would be --hwaccel_dec_blocked=VP9. To block both profile 0 and 1, use --hwaccel_dec_blocked=VP9:0:1. The parameter can be repeated to block even more codecs.
* **Bugfix:** [Issue #96](https://github.com/nschlia/ffmpegfs/issues/96): Fixed potential buffer overrun and crash when reading corrupted input files.
* **Enhancement:** [Issue #99](https://github.com/nschlia/ffmpegfs/issues/99): Report command line error if --desttype specifies audio format first, or if the second format is not audio only. Avoid misinterpretations. For example, --desttype=aiff+mov would create MOV files out of any input. Correct would be --desttype=mov+aiff which will create MOV files out of videos and AIFF from audio files, as expected.

### Version 2.6 released

**New in 2.6 (2021-09-04):**

* No new features, just a bugfix. See V2.4 for details.

### Version 2.5 released

**New in 2.5 (2021-06-18):**

* **Feature**: [Issue #63](https://github.com/nschlia/ffmpegfs/issues/63) - Hardware acceleration for encoding/decoding is partly implemented, VAAPI/MMAL/OMX/V4L2 are currently available only.
  - Supported hardware: V4L2/VAAPI (Intel) and V4L2/MMAL/OMX (Raspberry).
  - VAAPI: H264, H265/HEVC, MPEG-2 and VP-8 decoding and H264 encoding.
  - VAAPI: MJPEG and VC-9 do not work (yet).
  - MMAL: H264, MPEG-2, MPEG-4 and VC1 decoding.
  - OMX: H264 encoding.
  - V4L2: H263, H264, H265, MPEG1/2/4, VC-1, VP8/9 encoding/decoding.
* **Feature**: Added unit tests for hardware acceleration. Failing tests will report as *SKIPPED* and not fail the whole test.
* **Note**: Which hardware en/decoder actually works depends on what your hardware supports.
* **Call for testers**: Have a CUDA capable graphics adapter and interested in testing? Please write me an e-mail.

### Version 2.4 released

**New in 2.4 (2021-09-04):**

* **Bugfix:** [Issue #90](https://github.com/nschlia/ffmpegfs/issues/90): Make sure that one keyframe gets inserted at the start of each HLS segment.

### Version 2.3 released

**New in 2.3 (2021-06-11):**

* **Enhancement:** [Issue #80](https://github.com/nschlia/ffmpegfs/issues/80): Open input video codec only if target supports video. Saves resources: no need to decode video frames if not used.
* **Enhancement:**  [Issue #81](https://github.com/nschlia/ffmpegfs/issues/81): If source format has no audio, and the target supports no video (e.g.WAV/MP3), the files have shown up zero sized. These will now not be visible when doing ls. When trying to open them "File not found" will be returned.
* **Added** "configure --enable-debug" to create binaries with debug symbols. Defaults to the optimised version.
* **Feature:** [Issue #73](https://github.com/nschlia/ffmpegfs/issues/73) Cue sheet tracks now play "gapless" if played in order. Whenever a track is started, the next track will automatically be transcoded as well.
* **Feature:** [Issue #66](https://github.com/nschlia/ffmpegfs/issues/66) and [issue #82](https://github.com/nschlia/ffmpegfs/issues/82): Added cue sheet support. If a file with cue extension is found with the same name as a media file or if a cue sheet is embedded into it (a tag named CUESHEET), tracks defined in it will show up in a virtual directory.
* **Feature:** [Issue #83](https://github.com/nschlia/ffmpegfs/issues/83): Character conversion for cue sheet files. Automatically detects the character encoding of the cue sheet. and converts as necessary.
* **Feature:** [Issue #78](https://github.com/nschlia/ffmpegfs/issues/78): Duplicate ARTIST to ALBUMARTIST tag if empty.
* **Feature:** [Issue #79](https://github.com/nschlia/ffmpegfs/issues/79): Added Docker support. See [Build A Docker Container](README.md#build-a-docker-container) how to use it.
* **Fixed deprecation:** 2021-03-17 - f7db77bd87 - lavc 58.133.100 - codec.h
  Deprecated av_init_packet()
* **Fixed API compatitibility:** Many pointers made const as of 2021-04-27. Although reasonable, this breaks API compatibility with versions older than 59.0.100,
* **Bugfix:** find_original "fallback" method did not correctly handle the new filename format (extension added, not the original one replaced).
* **Bugfix:** [Issue #87](https://github.com/nschlia/ffmpegfs/issues/87): Segments are now properly separated, making sure that e.g. segment 3 only goes from 30 seconds up to 40 (including 30, but not 40 seconds). 
* **Bugfix:** [Issue #88](https://github.com/nschlia/ffmpegfs/issues/88): HLS audio and video now stay in sync after longer playback (more than 30 minutes) or after seek operations. 

### Version 2.2 released

**New in 2.2 (2021-02-06):**

* **Note**: This is planned as a maintenance version, no new features but bug fixes only. 
* **Bugfix:** [Issue #75](https://github.com/nschlia/ffmpegfs/issues/75): Fix crash when opening mp3 output with Dolphin.
* **Bugfix**: Possible crash in transcoder_thread: Decoder object could have been used after being freed.
* **Bugfix:** Stupid blooper. WAV and AIFF size was always calculated for a mono file, thus for stereo files only half the correct size.
* **Bugfix:** [Issue #70](https://github.com/nschlia/ffmpegfs/issues/70): Possible crash in Buffer::init: Should not assert if duration is 0 (and thus segment count 0). Report internal error and go on.
* **Bugfix:** [Issue #70](https://github.com/nschlia/ffmpegfs/issues/70): Do not set duration to 0 from cache but leave unchanged. Caused HLS transcoding to fail if more than one transcoder was concurrently started.
* **Bugfix:** Corrected documentation, "make checks" should read "make check", funny this went unnoticed for over 3 years...
* **Bugfix:** [Issue #74](https://github.com/nschlia/ffmpegfs/issues/74): Album arts were only copied from MP3/4 sources. Removed restriction, if the input file contains an album art it will be copied to the target (if supported, of course, e.g., to mp3 or mp4. Ogg is not yet supported because embedding album arts in Ogg can only be done by an unofficial workaround).
* **Bugfix:** [Issue #71](https://github.com/nschlia/ffmpegfs/issues/71): Virtual directories were missing dot and dot-dot nodes.

### Version 2.1 released

**New in 2.1 (2020-12-14):**

* **Feature**: Add BLURAY_VIDEO_FORMAT_2160P (UHD)
* **Feature**: Implemented in 1.7, removed experimental state for --win_smb_fix now.  Windows seems to access the files on Samba drives starting at the last 64K segment simply when the file is opened. Setting --win_smb_fix=1 will ignore these attempts (not decode the file up to this point).
* **Feature**: --win_smb_fix now defaults to 1 (fix on by default). Has no effect if the drive is accessed directly or via Samba from Linux.
* **Bugfix**: Fixed grammatical error in text: It's "access to", not "access at".
* **Bugfix**: Did not transcode some source files with invalid DTS.
* **Bugfix**: Cosmetical - No need to log date/time twice in syslog.
* **Bugfix**: Cosmetical - Fix man page/online help for --recodesame parameter.
* **Bugfix**: Report correct segment duration
* **Bugfix**: Avoid crash if opening next HLS segment failed. Should not ignore this, but report it instead and stop transcoding.
* **Cosmetical**: Log cache close action at trace level
* **Cosmetical**: Shorter log entry when opening cache files
