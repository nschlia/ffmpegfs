Building For Windows
====================

A Windows version of FFmpegfs has frequently been requested; see Issue [#76](https://github.com/nschlia/ffmpegfs/issues/76) for more information. In essence, this failed because Windows doesn't support Fuse. I attempted to create my own virtual file system solution, but this proved to be a challenging task that required building a system driver using the Windows DDK (Driver Development Kit) and would need buying  a software signing certificate, which is an expensive endeavor. That much time and money is not what I wanted to invest.

Finally, I discovered [WinFSP](https://winfsp.dev/), which offers everything necessary, and for free. It needs Cygwin, which was once merely a crutch but is today extremely powerful.

FFmpegfs can be built and operated under Windows, however doing so is not recommended. On the same hardware as if transcoding were being done under Linux, the performance is heartbreaking.

However, you may now use it under Windows if you wish to.

## Building with Cygwin

### Install Windows Prequisites

In order to build FFmpegfs under Windows, various steps must be taken. but it is possible with Cygwin's assistance.

Download Cygwin from https://www.cygwin.com/ to start with. and install. Later we will add the necessary packages.

From https://winfsp.dev/rel, get Windows File Proxy (WinFSP). Install it after making the recommended selections for all of the development parameters. To make it accessible, the subsequent steps must be carried out in a Cygwin shell.

### Update Cygwin

#### Install WinFSP Support For Cygwin

In order to activate Fuse support please follow the instructions  on https://winfsp.dev/rel/ under "For Cygwin users". This adds the Fuse package, which Cygwin does not provide.

#### Install Packages From Cygwin Setup

The necessary build environment will be made available by installing via Cygwin setup.
```
git, gcc-core, gcc-g++, make, pkg-config, asciidoc, docbook-xml, xxd, libtool, libiconv-devel, libcue-devel,  libsqlite3-devel, wget
```
If there are multiple versions available, choose the highest. Additionally make sure to install the **automake** wrapper for various versions. This will save you from the headache of choosing the right bundles.
To get DVD support:
```
libdvdread-devel
```
To get Blu-ray support see "Install Libraries Missing From Setup".

To use "make doxy" (build Doxygen documentation):
```
doxygen, graphviz, curl
```
To use "make check" (run test suite):
```
libchromaprint-devel, bc
```
**Cygserver** is required to execute FFmpegfs. See https://cygwin.com/cygwin-ug-net/using-cygserver.html for further information.

* You need to install ***cygrunsrv***. You will find it under the "Admin" category in the Cygwin setup utility.
* Launch the Cygwin terminal as administrator.
* Run /usr/bin/cygserver-config and select "yes" to all of the prompts.
* In order to access the WinX Menu, right-click the Start button.
* Choose Run.
* When the Run box appears, type services.msc.
* Windows Services Manager will open.
* Select and start the "CYGWIN cygserver".
* When Windows is restarted, it will automatically start, so you won't have to do this again.

#### Install Libraries Missing From Setup

Libchardet is also not part of Cygwin, therefore go to https://github.com/Joungkyun/libchardet/releases to download and build libchardet. You may use a newer version if one becomes available. At the time of writing, that was V1.0.6.

```
$ mkdir install
$ cd install
~/install $ wget https://github.com/Joungkyun/libchardet/releases/download/1.0.6/libchardet-1.0.6.tar.bz2
Length: 435028 (425K) [application/octet-stream]
Saving to: 'libchardet-1.0.6.tar.bz2'
2023-01-07 12:19:06 (6.41 MB/s) - 'libchardet-1.0.6.tar.bz2' saved [435028/435028]
~/install $ tar -xf libchardet-1.0.6.tar.bz2
~/install $ cd libchardet-1.0.6
~/install $ ./configure --prefix=/usr
~/install $ make install
```

Regrettably, Cygwin does not offer *libbluray*, which is optionally necessary to enable Bluray support.

As of yet I could not successfully build it. Bluray support will not be available.

#### Get The Windows FFmpeg Binaries

Look at the download page of the [FFmpeg Homepages](https://ffmpeg.org/download.html#build-windows) to obtain pre-built Windows binaries.

Download it, for example, from [Github](https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip), place it in win/ffmpeg/win64/
and then unpack it. This will automatically place all files in the appropriate directories.

Should you feel lucky and want to build the binaries yourself, you may also use FFmpeg Windows Build Helpers, which can be obtained [here](https://github.com/rdp/ffmpeg-windows-build-helpers).

| Directory                | Bit Width | To Copy There       |
| ------------------------ | --------- | ------------------- |
| win/ffmpeg/win64/bin     |        64 | Binaries            |
| win/ffmpeg/win64/include |        64 | Development headers |
| win/ffmpeg/win64/lib     |        64 | Libraries           |

#### Build The Binaries For FFmpegfs

After then, all else is just running...

```
$ ./configure
$ make
```

Call to generate a "export" directory with the necessary binaries.

```
$ make export
```

By doing this, all DLLs needed to launch ffmpegfs.exe without first installing Cygwin will be exported.

You will be able to run ffmpegfs.exe from the command prompt after running the setenv script.

```
$ . setenv
```

To take advantage of bash's built-in script command, make sure to utilise the "dot space" syntax as demonstrated above; otherwise, the environment will remain untouched.

#### A 32 Bit Version?
A 32-bit version is probably possible, but Cygwin 32 is no longer supported on Windows 10 and later. As a result, 32-bit builds are not implemented in the make system.

#### Troubleshooting

When ffmepgfs.exe refuses to run with the following message:

```
 "link_up(): shmget error (88) Function not implemented" -> Cygserver not running
```

**Cygserver** is not running or not installed. See "Install WinFSP Support For Cygwin" above how to fix that.
