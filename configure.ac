# Process this file with autoconf to produce a configure script.

# Sets up package and initializes build system.
AC_PREREQ(2.57)
AC_INIT([FFMPEGFS], [1.95])
AC_CONFIG_SRCDIR([src/ffmpegfs.cc])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_AUX_DIR([config])

# Get configure arguments
configure_args="$*"
AC_DEFINE_UNQUOTED([CONFIGURE_ARGS], ["$configure_args"], [Arguments passed to configure])

AM_INIT_AUTOMAKE([foreign subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# AC_CANONICAL_HOST is needed to access the 'host_os' variable
AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED([HOST_OS], ["$host_os"], [Host operating system])

# compiler flags
CFLAGS=""
CXXFLAGS=""

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDC
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT32_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for header files.
#AC_CHECK_HEADERS([fcntl.h stdlib.h string.h unistd.h sstream climits cstring])

# Check integer size
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF([off_t])
#AC_CHECK_SIZEOF([size_t]

AS_IF([test "$ac_cv_sizeof_int" -lt 4],
    AC_MSG_ERROR([Integer type 'int' must be at least 4 bytes in size.]))

dnl Figure out which format string to use for time_t
AC_MSG_CHECKING([size of time_t type])
AC_RUN_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <time.h>]],
    [[return (sizeof (time_t) == sizeof (long) ? 0 : 1);]])],
  [AC_MSG_RESULT(long)
   AC_DEFINE([FFMPEGFS_FORMAT_TIME_T], ["ld"], [format string for time_t])],
  [AC_MSG_RESULT(int)
   AC_DEFINE([FFMPEGFS_FORMAT_TIME_T], ["d"], [format string for time_t])])

dnl Figure out which format string to use for pthread_t
AC_MSG_CHECKING([size of pthread_t type])
AC_RUN_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <pthread.h>]],
    [[return (sizeof (pthread_t) == sizeof (long) ? 0 : 1);]])],
  [AC_MSG_RESULT(long)
   AC_DEFINE([FFMPEGFS_FORMAT_PTHREAD_T], ["lx"], [format string for pthread_t])],
  [AC_MSG_RESULT(int)
   AC_DEFINE([FFMPEGFS_FORMAT_PTHREAD_T], ["x"], [format string for pthread_t])])

# Checks for packages which use pkg-config.
PKG_CHECK_MODULES([fuse], [fuse >= 2.6.0])

# Large file support
AC_SYS_LARGEFILE

# Define POSIX standard conformance
AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])

# This is because the first PKG_CHECK_MODULES call is inside a conditional.
PKG_PROG_PKG_CONFIG

# Checks for sqlite3
# API misses a lot of functionality if not fairly recent
PKG_CHECK_MODULES([sqlite3], [sqlite3 >= 3.7.13])
AC_SEARCH_LIBS([sqlite3_errstr], [sqlite3], [AC_DEFINE([HAVE_SQLITE_ERRSTR], [1], [libsqlite3 has sqlite3_errstr() function.])], [])
AC_SEARCH_LIBS([sqlite3_db_cacheflush], [sqlite3], [AC_DEFINE([HAVE_SQLITE_CACHEFLUSH], [1], [libsqlite3 has sqlite3_db_cacheflush() function.])], [])
AC_SEARCH_LIBS([sqlite3_expanded_sql], [sqlite3], [AC_DEFINE([HAVE_SQLITE_EXPANDED_SQL], [1], [libsqlite3 has sqlite3_expanded_sql() function.])], [])

#
# Check for programs used when building manpages and help.
#

# Check for a2x (convert asciidoc to another format)
AC_PATH_PROG(A2X, a2x)
AC_CHECK_PROG(HAVE_A2X, a2x, "yes", "no")
AS_IF([test "$HAVE_A2X" == "yes"], [], AC_MSG_ERROR([Missing a2x. Please install asciidoc to fix.]))

# Check for w3m (html -> text)
AC_PATH_PROG(W3M, w3m)
AC_CHECK_PROG(HAVE_W3M, w3m, "yes", "no")
AS_IF([test "$HAVE_W3M" == "yes"], [], AC_MSG_ERROR([Missing w3m. Please install w3m to fix.]))

# Check for xmllint
AC_PATH_PROG(XMLLINT, xmllint)
AC_CHECK_PROG(HAVE_XMLLINT, xmllint, "yes", "no")
AS_IF([test "$HAVE_XMLLINT" == "yes"], [], AC_MSG_ERROR([Missing xmlint. Please install libxml2-utils to fix.]))

# xmlto seems not to be required
# Check for xmlto
#AC_PATH_PROG(XMLLTO, xmlto)
#AC_CHECK_PROG(HAVE_XMLLTO, xmlto, "yes", "no")
#AS_IF([test "$HAVE_XMLLTO" == "yes"], [], AC_MSG_ERROR([Missing xmlto. Please install xmlto to fix.]))

dnl FFMPEG support checks

# Tested with...
#
# libavutil      55. 34.101		54.3.0 (required)
# libavcodec     57. 64.101		56.1.0 (required)
# libavformat    57. 56.101		56.1.0 (required)
# libavdevice    57.  6.100
# libavfilter     6. 82.100     5.40.0 (required)
# libavresample   3.  5.  0		2.1.0 (optional)
# libswscale      4.  2.100		3.0.0 (required)
# libswresample   2.  3.100     1.0.0 (optional)
# libpostproc    54.  5.10
#
# Either libswresample or libavresample must be present.
#
# May work with older versions but this is not guaranteed.

PKG_CHECK_MODULES([libavutil], [libavutil >= 54.3.0],
    [PKG_CHECK_MODULES([libavcodec], [libavcodec >= 56.1.0],
        [PKG_CHECK_MODULES([libavformat], [libavformat >= 56.1.0],
            [PKG_CHECK_MODULES([libswscale], [libswscale >= 3.0.0],
                [PKG_CHECK_MODULES([libavfilter], [libavfilter >= 5.40.0],
                    [AC_DEFINE([HAVE_FFMPEG], [1], [Use FFMPEG libraries.])]
                )
                ]
            )
			]
        )
		]
	)
	]
)

dnl Check for libswresample
AC_ARG_WITH([libswresample],
  [AS_HELP_STRING([--with-libswresample],
    [support using libswresample @<:@default=check@:>@])],
  [],
  [with_libswresample=check])
AS_CASE(["$with_libswresample"],
  [yes], [PKG_CHECK_MODULES([libswresample], [libswresample >= 1.0.0], [HAVE_LIBSWRESAMPLE=1])],
  [no], [],
  [PKG_CHECK_MODULES([libswresample], [libswresample >= 1.0.0], [HAVE_LIBSWRESAMPLE=1], [HAVE_LIBSWRESAMPLE=0])])
AM_CONDITIONAL([USE_LIBSWRESAMPLE], [test "$with_libswresample" != "no" -a "0$HAVE_LIBSWRESAMPLE" -eq 1])
AS_IF([test "0$HAVE_LIBSWRESAMPLE" -eq 1], [with_libavresample=no])

dnl Check for libavresample
AC_ARG_WITH([libavresample],
  [AS_HELP_STRING([--with-libavresample],
    [support using libavresample @<:@default=check@:>@])],
  [],
  [with_libavresample=check])
AS_CASE(["$with_libavresample"],
  [yes], [PKG_CHECK_MODULES([libavresample], [libavresample >= 2.1.0], [HAVE_LIBAVRESAMPLE=1])],
  [no], [],
  [PKG_CHECK_MODULES([libavresample], [libavresample >= 2.1.0], [HAVE_LIBAVRESAMPLE=1], [HAVE_LIBAVRESAMPLE=0])])
AM_CONDITIONAL([USE_LIBAVRESAMPLE], [test "$with_libavresample" != "no" -a "0$HAVE_LIBAVRESAMPLE" -eq 1])

# Error checks
AS_IF([test "$with_libswresample" != "no" -a "0$HAVE_LIBSWRESAMPLE" -eq 1], [],
    AS_IF([test "$with_libavresample" != "no" -a "0$HAVE_LIBAVRESAMPLE" -eq 1], [],
        AC_MSG_ERROR([Either libswresample or libavresample must be present and enabled. Please make sure its development modules are installed. Do not use --without-libswresample and --without-libavresample at once.])
    )
)

# Optional libraries
dnl Check for libdvdread
AC_ARG_WITH([libdvd],
  [AS_HELP_STRING([--with-libdvd],
    [support using libdvd @<:@default=check@:>@])],
  [],
  [with_libdvd=check])

# Removed dependency, library not used
#AS_CASE(["$with_libdvd"],
#  [yes], [PKG_CHECK_MODULES([libdvdnav], [dvdnav >= 5.0.0], [HAVE_LIBDVDNAV=1])],
#  [no], [],
#  [PKG_CHECK_MODULES([libdvdnav], [dvdnav >= 5.0.0], [HAVE_LIBDVDNAV=1], [HAVE_LIBDVDNAV=0])])

AS_CASE(["$with_libdvd"],
  [yes], [PKG_CHECK_MODULES([libdvdread], [dvdread >= 5.0.0], [HAVE_LIBDVDREAD=1])],
  [no], [],
  [PKG_CHECK_MODULES([libdvdread], [dvdread >= 5.0.0], [HAVE_LIBDVDREAD=1], [HAVE_LIBDVDREAD=0])])
#AM_CONDITIONAL([USE_LIBDVD], [test "$with_libdvd" != "no" -a "0$HAVE_LIBDVDREAD" -eq 1 -a "0$HAVE_LIBDVDNAV" -eq 1])
AM_CONDITIONAL([USE_LIBDVD], [test "$with_libdvd" != "no" -a "0$HAVE_LIBDVDREAD" -eq 1])
#AM_CONDITIONAL([HINT_LIBDVD], [test "$with_libdvd" != "no" -a "0$HAVE_LIBDVDNAV" -eq 0])
AM_CONDITIONAL([HINT_LIBDVD], [test "$with_libdvd" != "no" -a "0$HAVE_LIBDVDREAD" -eq 0])

dnl Check for libbluray
AC_ARG_WITH([libbluray],
  [AS_HELP_STRING([--with-libbluray],
    [support using libbluray @<:@default=check@:>@])],
  [],
  [with_libbluray=check])

AS_CASE(["$with_libbluray"],
  [yes], [PKG_CHECK_MODULES([libbluray], [libbluray >= 0.6.2], [HAVE_LIBBLURAY=1])],
  [no], [],
  [PKG_CHECK_MODULES([libbluray], [libbluray >= 0.6.2], [HAVE_LIBBLURAY=1], [HAVE_LIBBLURAY=0])])
AM_CONDITIONAL([USE_LIBBLURAY], [test "$with_libbluray" != "no" -a "0$HAVE_LIBBLURAY" -eq 1])
AM_CONDITIONAL([HINT_LIBBLURAY], [test "$with_libbluray" != "no" -a "0$HAVE_LIBBLURAY" -eq 0])

# Check for libvcd
AC_ARG_WITH([libvcd],
  [AS_HELP_STRING([--with-libvcd],
    [support using libvcd @<:@default=check@:>@])],
  [],
  [with_libvcd=check])
AM_CONDITIONAL([USE_LIBVCD], [test "$with_libvcd" != "no"])
AM_COND_IF([USE_LIBVCD],
           [AC_MSG_RESULT([Internal S/VCD support enabled... yes])],
           [AC_MSG_RESULT([Internal S/VCD support enabled... no])])

# Check for doxygen. If not installed, go on, but make doxy won't work
AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN";
   then AC_MSG_WARN([Doxygen not found - continuing without Doxygen support. If you intend to use "make doxy" you should install it.])
fi

# Check for curl. If not installed, go on, but make doxy won't work
AC_CHECK_PROGS([CURL], [curl])
if test -z "$CURL";
   then AC_MSG_WARN([curl not found - continuing without curl support. If you intend to use "make doxy" you should install it.])
fi

# Outputs resulting files.
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 test/Makefile])
AC_OUTPUT

# Hints on options
AM_COND_IF([HINT_LIBDVD],
           [AC_MSG_NOTICE([HINT: To enable DVD suport install the libdvdread development packages. See INSTALL.md for details.])])
AM_COND_IF([HINT_LIBBLURAY],
           [AC_MSG_NOTICE([HINT: To enable bluray suport install the libbluray development package. See INSTALL.md for details.])])
