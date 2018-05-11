# Process this file with autoconf to produce a configure script.

# Sets up package and initializes build system.
AC_INIT([FFMPEGFS], [1.4])
AC_CONFIG_SRCDIR([src/ffmpegfs.c])
AC_CONFIG_AUX_DIR([config])

# Get configure arguments
configure_args="$*"
AC_DEFINE_UNQUOTED([CONFIGURE_ARGS], ["$configure_args"], [Arguments passed to configure])

AM_INIT_AUTOMAKE([foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# AC_CANONICAL_HOST is needed to access the 'host_os' variable
AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED([HOST_OS], ["$host_os"], [Host operating system])

# Optimisation
CXXFLAGS="-Ofast -g -Wextra -Wconversion -Wall -W"
CFLAGS="-Ofast -g -Wextra -Wconversion -Wall -W"

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB

# Check integer size
AC_CHECK_SIZEOF(int)

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

# FFMPEG support checks

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

# This is because the first PKG_CHECK_MODULES call is inside a conditional.
PKG_PROG_PKG_CONFIG

# Check for libswresample
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

# Check for libavresample
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

# Checks for sqlite3
# API misses a lot of functionality if not fairly recent
PKG_CHECK_MODULES([sqlite3], [sqlite3 >= 3.7.13])
AC_SEARCH_LIBS([sqlite3_errstr], [sqlite3], [AC_DEFINE([HAVE_SQLITE_ERRSTR], [1], [libsqlite3 has sqlite3_errstr() function.])], [])
AC_SEARCH_LIBS([sqlite3_db_cacheflush], [sqlite3], [AC_DEFINE([HAVE_SQLITE_CACHEFLUSH], [1], [libsqlite3 has sqlite3_db_cacheflush() function.])], [])
AC_SEARCH_LIBS([sqlite3_expanded_sql], [sqlite3], [AC_DEFINE([HAVE_SQLITE_EXPANDED_SQL], [1], [libsqlite3 has sqlite3_expanded_sql() function.])], [])

# Checks for packages which use pkg-config.
PKG_CHECK_MODULES([fuse], [fuse >= 2.6.0])

# Large file support
AC_SYS_LARGEFILE

# Define POSIX standard conformance
AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])

# Outputs resulting files.
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 test/Makefile])
AC_OUTPUT
