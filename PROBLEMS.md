# Fixing Problems

## General problems accessing a file

Due to the nature in which FFmpegfs works, if anything goes wrong, it can only report a general error. That means, when there is a problem accessing a file, copying or opening it, you will get an "Invalid argument" or "Operation not permitted". Not really informative. There is no way to pass the original result code through the file system.

What you will have to do is refer to the syslog, system journal, or to the FFmpeg log itself, optionally at a higher verbosity. If you are unable to resolve the problem yourself, feel free to create [an issue](https://github.com/nschlia/ffmpegfs/issues), stating what you have done and what has happened. Do not forget to add logs (preferably at a higher verbosity) and, if possible, a description of how to recreate the issue. A demo file that causes the error would be helpful as well.

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

        cp -uv /path/to/source /path/to/target

This will copy all missing or changed files without missing parts. On the Windows side, Windows Explorer or copy/xcopy work. Tools like Beyond Compare may only copy the predicted size first and may not respond to size changes.

## Open hls.html from disk to play HLS output.

Most browsers prevent the playback of files from disk. You may put them into a website directory, but sometimes https must be used or playback will be blocked.

**To enable disk playback in Firefox:**

- Open about:config
- Set security.fileuri.strict_origin_policy to false.

## Songs get cut short

If songs do not play to the very end and you are using SAMBA or NFS, you're in trouble.

It happens when the files are transcoded on the fly, but never when the file comes from the cache. This is because the result is never exactly what was predicted.

SAMBA fills files with zeros if the result is smaller than predicted, or cuts off the rest if the file is larger than predicted.

NFS arbitrarily sends the correct file, or one that is cut or padded like SAMBA. This can be repeated as many times as one wants to—once the file is OK, once not.

As of yet, there seems to be no way around that. Maybe NFS or SAMBA can be configured to cope with that, but how to is unknown to me.


