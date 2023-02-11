# Fixing Problems

## General problems accessing a file

Due to the nature in which FFmpegfs works, if anything goes wrong, it can only report a general error. That means, when there is a problem accessing a file, copying or opening it, you will get an "Invalid argument" or "Operation not permitted". Not really informative. There is no way to pass the original result code through the file system.

What you will have to do is refer to the syslog, system journal, or to the FFmpeg log itself, optionally at a higher verbosity. If you are unable to resolve the problem yourself, feel free to create [an issue](https://github.com/nschlia/ffmpegfs/issues), stating what you have done and what has happened. Do not forget to add logs (preferably at a higher verbosity) and, if possible, a description of how to recreate the issue. It would also be helpful to have an example of the media file that is generating the error. Copyrighted material should never be sent, nevertheless.

## Transcoding too slow

Slow transcoding can be caused by a variety of factors, including a slow CPU or network issues. This might help with decoding and encoding speed: [Building FFmpeg with optimisations](INSTALL.md#building-ffmpeg-with-optimisations).

## Lock ups when accessed through Samba

When accessed from a Samba drive, the pending read can lock the whole share, causing Windows Explorer and even KDE Dolphin to freeze. Any access from the same machine to that share is blocked. Even "ls" is not possible and blocks until the data is returned.

It seems others had the same problem:

http://samba.2283325.n4.nabble.com/Hangs-Accessing-fuse-filesystem-in-Windows-through-Samba-td4681904.html

Adding this to the [global] config in smb.conf fixes that:

 	oplocks = no
 	level2 oplocks = no
 	aio read size = 1

The "aio read size" parameter may be moved to the share config:

 	aio read size = 1

## rsync, Beyond Compare and other tools

Some copy tools do not get along very well with dynamically generated files, as in [Issue #23: Partial transcode of some files](https://github.com/nschlia/ffmpegfs/issues/22).

Under Linux, it is best to use (optionally with the -r parameter)

        cp -upv /path/to/source /path/to/target

This will copy all missing or changed files without missing parts. On the Windows side, Windows Explorer or copy/xcopy work. Beyond Compare and comparable tools might only duplicate the expected size without responding to changes in size.

Please take note of the "-p" parameter, which means "—preserve=mode,ownership,timestamps." It appears that files may occasionally be copied with zero size if it is absent.

## Open hls.html from disk to play HLS output.

The majority of browsers forbid playing back files from disc. You may put them into a website directory, however sometimes https must be used in order to allow playback.

**To enable disk playback in Firefox:**

- Open about:config
- Set security.fileuri.strict_origin_policy to false.

## Songs get cut short

If you are using SAMBA or NFS and songs don't play all the way through, you're in trouble.

It occurs when files are transcoded live, but never when a file is retrieved from cache. This is because the final size seldom comes out exactly as expected. Size information is only available up front when a cached file is returned.

If the outcome is smaller than expected, SAMBA fills files with zeros; if the outcome is larger than expected, SAMBA chops off the remaining data.

Unlike SAMBA, NFS sends either the right file or one that has been chopped or padded randomly. You can do this as many times as you like, with a different result each time.

There doesn't appear to be a solution to that as of now. Perhaps NFS or SAMBA can be set up to handle that, but I'm not sure how.
