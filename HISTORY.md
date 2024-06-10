History
=======

**New in 2.16 (2024-06-XX):**

* Bugfix: Closes#1072412: Fix build with FFmpeg 7.0

**New in 2.15 (2024-02-03):**

- **Bugfix:** Issue [#151](https://github.com/nschlia/ffmpegfs/issues/151): Fixed autocopy STRICT never triggers for video streams
- **Bugfix:** Closes [#153](https://github.com/nschlia/ffmpegfs/issues/153): The --include_extensions parameter now contains a description, which was previously missing from the manual and online help.
- **Issue** [#149](https://github.com/nschlia/ffmpegfs/issues/149): 2023-05-04 - xxxxxxxxxx - lavu 58.7.100 - frame.h
  Deprecate AVFrame.interlaced_frame, AVFrame.top_field_first, and AVFrame.key_frame.
  Add AV_FRAME_FLAG_INTERLACED, AV_FRAME_FLAG_TOP_FIELD_FIRST, and AV_FRAME_FLAG_KEY flags as replacement.
- **Issue** [#149](https://github.com/nschlia/ffmpegfs/issues/149): 2023-05-04 - xxxxxxxxxx - lavu 58.7.100 - frame.h
  Deprecate AVFrame.interlaced_frame, AVFrame.top_field_first, and AVFrame.key_frame.
  Add AV_FRAME_FLAG_INTERLACED, AV_FRAME_FLAG_TOP_FIELD_FIRST, and AV_FRAME_FLAG_KEY flags as replacement.
- **Issue** [#149](https://github.com/nschlia/ffmpegfs/issues/149): 2021-09-20 - dd846bc4a91 - lavc 59.8.100 - avcodec.h codec.h
  Deprecate AV_CODEC_FLAG_TRUNCATED and AV_CODEC_CAP_TRUNCATED, as they are redundant with parsers.
- Closes [#136](https://github.com/nschlia/ffmpegfs/issues/136): The CMake build files have been removed. Support was never more than experimental, and CMake lacks a good uninstall option. Will stick to automake system from now on.

**New in 2.14 (2023-06-15):**

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
- **Bugfix:** [Closes#1037653:](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1037653) Fix build with GCC-13
- **Bugfix:** Update docker build for Debian Bookworm
- **Enhancement:** Record milliseconds for every log event.
- **Enhancement:** make check: added a file size check to frame set tests.
- **Optimisation:** When reopening after invalidating the cache, the size remained at 0. The original size is now once again reserved in order to prevent reallocations.
- **Optimisation:** To avoid reallocations, save enough space in the cache buffer to hold the entire frame set.
- **Optimisation:** Checking folders to see if they can be transcoded is completely pointless. Directories are now immediately skipped.
- To avoid problems with logfile viewers, renamed built-in logfiles to *_builtin.log (removing the double extension).

**New in 2.13 (2023-01-15):**

- **Feature:** Added --prebuffer_time parameter. Files will be decoded until the buffer contains the specified playing time, allowing playback to start
  smoothly without lags. Works similar to --prebuffer_size but gives better control because it does not depend on the bit rate. An example: when set to 25 seconds for HLS transcoding, this will make sure that at least 2 complete segments will be available once the file is released and visible.
- **Feature:** Closes [#140](https://github.com/nschlia/ffmpegfs/issues/140): Filtering the files that will be encoded has been added. A comma-separated list of extensions is specified by the *—include_extensions* parameter. These file extensions are the only ones that will be transcoded. The entries support shell wildcard patterns.
- **Feature:** The --hide_extensions parameter syntax has been extended. The entries now support shell wildcard patterns.
- **Bugfix:** Closes [#139](https://github.com/nschlia/ffmpegfs/issues/139): Additional files could be added using the *—extensions* parameter. However, this is no longer necessary; in the past, a file's extension determined whether or not it would be transcoded. Files with unknown extensions would be ignored. The extension is no longer important because FFmpegfs now examines all input files and recognises transcodable files by the format.
  The outdated *—extensions* argument was removed without substitution.
- **Bugfix:** Fixed crash when implode() function was called with an empty string. Happened with Windows GCC 11.3.0 only.

### Version 2.12 released

**New in 2.12 (2022-08-27):**

- The code has been run through clang-tidy to detect areas that could be updated to C++17 and to find areas that are prone to bugs or are inefficient. Many problems could be fixed. Sometimes a few lines of code can take the place of many. Some components function far more effectively than they did in the past. C++17 is cool! I must purchase a t-shirt.
- **Bugfix:** In get prores bitrate(), a crash that might have happened under unusual circumstances has been corrected. If the best match resolution could not be found, array access out-of-bounds could happen.
- **Bugfix:** Several unlikely, but potential problems that could have happened when subtitle decoding failed or delayed video/audio packets couldn't be decoded have been fixed.
- **Bugfix:** An internal problem could cause the application to crash. Should never happen, though. Fixed anyway.
- **Bugfix:** Sometimes, the last segment's estimated size was incredibly small - about 2 bytes. Each segment should have the same predicted size as is is calculated simply by dividing the projected size of the entire file by the number of segments. Following transcoding, the size was accurate.

### Version 2.11 released

**New in 2.11 (2022-06-16):**

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

### Version 2.10 released

**New in 2.10 (2022-04-26):**

* **Feature:** [Issue #123](https://github.com/nschlia/ffmpegfs/issues/123): New command line option to hide files by extension. Example: `--hide_extensions=jpg,png,cue` would stop covers and cue sheets from showing up.
* **Feature:** [Issue #120](https://github.com/nschlia/ffmpegfs/issues/120): Added subtitle support. Subtitle streams can now also be transcoded to the output files. Separate SRT or VTT files will be incorporated as subtitle streams.
* **Bugfix:** Fixed memory leak in encode_audio_frame().
* **Bugfix:** [Issue #122](https://github.com/nschlia/ffmpegfs/issues/122): Last song was missing from cuesheet files.
* **Bugfix:** [Issue #129](https://github.com/nschlia/ffmpegfs/issues/129): Files remained zero size when previously transcoded.
* **Bugfix:** [Issue #130](https://github.com/nschlia/ffmpegfs/issues/130): Fix file sizes can be incorrectly reported by ls but are correct when data is read.
* **Bugfix:** Duration was not saved in cache SQLite database.
* **Bugfix:** [Issue #131](https://github.com/nschlia/ffmpegfs/issues/131): Sometimes video parameters for some Blu-ray or DVD chapters cannot be detected by FFmpeg API. Transcode then fails - fixed by using data from the Blu-ray directory or DVD IFO instead.
* Lowest supported FFmpeg API version raised to 4.1.8 "al-Khwarizmi".
* Dropped libavresample support, library was removed from FFmpeg API after 3.4.9.
* Deprecated previous channel layout API based on uint64 bitmasks.
* Deprecated swr_alloc_set_opts() and swr_build_matrix().
* Going C++17 now: The packet queue has been recoded in C++17 to support external subtitles files. As C++17 is required now, why not go all the way: Starting to replace legacy C++ and somewhat C-like parts with real C++.
* Using std::shared_ptr to ensure proper memory allocation/free.

### Version 2.9 released

**New in 2.9 (2022-02-16):**

* **Feature:** [Issue #97](https://github.com/nschlia/ffmpegfs/issues/97): Added options to chose different codecs. The audio codec can be selected with --audiocodec, for videos use --videocodec.
* **Feature:** [Issue #109](https://github.com/nschlia/ffmpegfs/issues/109): Allow user defined file extensions for source files. By default, only standard extensions are accepted, i.e., mp4, ts, avi etc. Arbitrary file extensions can be defined now, e.g. using --extensions=xxx,abc,yxz,aaa to also convert files ending with .xxx, .abc and so on.
* **Feature:** [Issue #121](https://github.com/nschlia/ffmpegfs/issues/121): Added MKV support. New format can be selected with --desttype=mkv.
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

**New in 2.8 (2021-11-29):**

* **Bugfix:** [Issue #102](https://github.com/nschlia/ffmpegfs/issues/102): Not all SQL queries where case sensitive, causing cache confusion. Several database entries were created, but only one was updated. Made all queries case sensitive.
* **Bugfix:** [Issue #91](https://github.com/nschlia/ffmpegfs/issues/91): Fixed HLS problems with cache causing garbled videos and hick-ups in audio.
* **Enhancement**: [Issue #103](https://github.com/nschlia/ffmpegfs/issues/103): If requested HLS segment is less than X (adjustable) seconds away, discard seek request. Segment would be available very soon anyway, and that seek makes a re-transcode necessary. Can be set with *--min_seek_time_diff*. Defaults to 30 seconds.
* **Feature**: [Issue #105](https://github.com/nschlia/ffmpegfs/issues/105): Added Free Lossless Audio Codec (FLAC) support. Activate with *--desttype=FLAC*.
* **Feature:** [Issue #101](https://github.com/nschlia/ffmpegfs/issues/101): Sample format for audio files can be selected via command line with *--audiosamplefmt*. Possible values are 0 to use the predefined setting, 8, 16, 32, 64 for integer format, F16, F32, F64 for floating point.
  Not all formats are supported by all destination types, selecting an invalid format will be reported as error and a list of values printed.
  Defaults to 0 (Use same as source or the predefined format of the destination if source format is not possible).

### Version 2.7 released

**New in 2.7 (2021-11-08):**

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
* **Enhancement:** [Issue #81](https://github.com/nschlia/ffmpegfs/issues/81): If source format has no audio, and the target supports no video (e.g.WAV/MP3), the files have shown up zero sized. These will now not be visible when doing ls. When trying to open them "File not found" will be returned.
* **Added** "configure --enable-debug" to create binaries with debug symbols. Defaults to the optimised version.
* **Feature:** [Issue #73](https://github.com/nschlia/ffmpegfs/issues/73) Cue sheet tracks now play "gapless" if played in order. Whenever a track is started, the next track will automatically be transcoded as well.
* **Feature:** [Issue #66](https://github.com/nschlia/ffmpegfs/issues/66) and [issue #82](https://github.com/nschlia/ffmpegfs/issues/82): Added cue sheet support. If a file with cue extension is found with the same name as a media file or if a cue sheet is embedded into it (a tag named CUESHEET), tracks defined in it will show up in a virtual directory.
* **Feature:** [Issue #83](https://github.com/nschlia/ffmpegfs/issues/83): Character conversion for cue sheet files. Automatically detects the character encoding of the cue sheet. and converts as necessary.
* **Feature:** [Issue #78](https://github.com/nschlia/ffmpegfs/issues/78): Duplicate ARTIST to ALBUMARTIST tag if empty.
* **Feature:** [Issue #79](https://github.com/nschlia/ffmpegfs/issues/79): Added Docker support. See [Build A Docker Container](FEATURES.md#build-a-docker-container) how to use it.
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
* **Feature**: Implemented in 1.7, removed experimental state for --win_smb_fix now. Windows seems to access the files on Samba drives starting at the last 64K segment simply when the file is opened. Setting --win_smb_fix=1 will ignore these attempts (not decode the file up to this point).
* **Feature**: --win_smb_fix now defaults to 1 (fix on by default). Has no effect if the drive is accessed directly or via Samba from Linux.
* **Bugfix**: Fixed grammatical error in text: It's "access to", not "access at".
* **Bugfix**: Did not transcode some source files with invalid DTS.
* **Bugfix**: Cosmetical - No need to log date/time twice in syslog.
* **Bugfix**: Cosmetical - Fix man page/online help for --recodesame parameter.
* **Bugfix**: Report correct segment duration
* **Bugfix**: Avoid crash if opening next HLS segment failed. Should not ignore this, but report it instead and stop transcoding.
* **Cosmetical**: Log cache close action at trace level
* **Cosmetical**: Shorter log entry when opening cache files
