History
=======

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
