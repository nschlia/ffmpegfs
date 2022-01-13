#ifndef CONFIG_H
#define CONFIG_H

#define PROJECT_VENDOR              "@PROJECT_VENDOR@"
#define PROJECT_CONTACT             "@PROJECT_CONTACT@"
#define PROJECT_URL                 "@PROJECT_URL@"
#define PROJECT_DESCRIPTION         "@PROJECT_DESCRIPTION@"

/* Arguments passed to configure */
#cmakedefine CONFIGURE_ARGS "@CONFIGURE_ARGS@"

/* format string for pthread_t */
#cmakedefine FFMPEGFS_FORMAT_PTHREAD_T "@FFMPEGFS_FORMAT_PTHREAD_T@"

/* format string for time_t */
#cmakedefine FFMPEGFS_FORMAT_TIME_T "@FFMPEGFS_FORMAT_TIME_T@"

/* Define to 1 if you have the <inttypes.h> header file. */
#cmakedefine HAVE_INTTYPES_H @HAVE_INTTYPES_H@

/* Define to 1 if you have the <memory.h> header file. */
#cmakedefine HAVE_MEMORY_H @HAVE_MEMORY_H@

/* libsqlite3 has sqlite3_db_cacheflush() function. */
#cmakedefine HAVE_SQLITE_CACHEFLUSH @HAVE_SQLITE_CACHEFLUSH@

/* libsqlite3 has sqlite3_errstr() function. */
#cmakedefine HAVE_SQLITE_ERRSTR @HAVE_SQLITE_ERRSTR@

/* libsqlite3 has sqlite3_expanded_sql() function. */
#cmakedefine HAVE_SQLITE_EXPANDED_SQL @HAVE_SQLITE_EXPANDED_SQL@

/* Define to 1 if you have the <stdint.h> header file. */
#cmakedefine HAVE_STDINT_H @HAVE_STDINT_H@

/* Define to 1 if you have the <stdlib.h> header file. */
#cmakedefine HAVE_STDLIB_H @HAVE_STDLIB_H@

/* Define to 1 if you have the <strings.h> header file. */
#cmakedefine HAVE_STRINGS_H @HAVE_STRINGS_H@

/* Define to 1 if you have the <string.h> header file. */
#cmakedefine HAVE_STRING_H @HAVE_STRING_H@

/* Define to 1 if you have the <sys/stat.h> header file. */
#cmakedefine HAVE_SYS_STAT_H @HAVE_SYS_STAT_H@

/* Define to 1 if you have the <sys/types.h> header file. */
#cmakedefine HAVE_SYS_TYPES_H @HAVE_SYS_TYPES_H@

/* Define to 1 if you have the <unistd.h> header file. */
#cmakedefine HAVE_UNISTD_H @HAVE_UNISTD_H@

/* Host operating system */
#cmakedefine HOST_OS "@HOST_OS@"

/* Name of package */
#cmakedefine PACKAGE "@PACKAGE@"

/* Define to the address where bug reports for this package should be sent. */
#cmakedefine PACKAGE_BUGREPORT "@PACKAGE_BUGREPORT@"

/* Define to the full name of this package. */
#cmakedefine PACKAGE_NAME "@PACKAGE_NAME@"

/* Define to the full name and version of this package. */
#cmakedefine PACKAGE_STRING "@PACKAGE_STRING@"

/* Define to the one symbol short name of this package. */
#cmakedefine PACKAGE_TARNAME "@PACKAGE_TARNAME@"

/* Define to the home page for this package. */
#cmakedefine PACKAGE_URL "@PACKAGE_URL@"

/* Define to the version of this package. */
#cmakedefine PACKAGE_VERSION "@PACKAGE_VERSION@"

/* The size of `int', as computed by sizeof. */
#cmakedefine SIZEOF_INT @SIZEOF_INT@

/* The size of `long', as computed by sizeof. */
#cmakedefine SIZEOF_LONG @SIZEOF_LONG@

/* The size of `off_t', as computed by sizeof. */
#cmakedefine SIZEOF_OFF_T @SIZEOF_OFF_T@

/* The size of `pthread_t', as computed by sizeof. */
#cmakedefine SIZEOF_PTHREAD_T @SIZEOF_PTHREAD_T@

/* The size of `time_t', as computed by sizeof. */
#cmakedefine SIZEOF_TIME_T @SIZEOF_TIME_T@

/* Define to 1 if you have the ANSI C header files. */
#cmakedefine STDC_HEADERS @STDC_HEADERS@

/* Version number of package */
#cmakedefine VERSION "@VERSION@"

/* Define the POSIX version */
#cmakedefine _POSIX_C_SOURCE @_POSIX_C_SOURCE@

/* Library versions */
#cmakedefine SQLITE3_FOUND          @SQLITE3_FOUND@
#cmakedefine SQLITE3_VERSION        "@SQLITE3_VERSION@"
#cmakedefine FUSE_FOUND             @FUSE_FOUND@
#cmakedefine LIBCUE_FOUND           @LIBCUE_FOUND@
#cmakedefine LIBCUE_VERSION         "@LIBCUE_VERSION@"
#cmakedefine CHARDET_FOUND          @CHARDET_FOUND@
#cmakedefine CHARDET_VERSION        "@CHARDET_VERSION@"

/* Use FFMPEG libraries. */
#cmakedefine HAVE_FFMPEG            @HAVE_FFMPEG@

/* FFmpeg libraries and versions */
/* libavutil */
#cmakedefine LIBAVUTIL_FOUND        @LIBAVUTIL_FOUND@
#cmakedefine LIBAVUTIL_VERSION      "@LIBAVUTIL_VERSION@"
/* libacodec */
#cmakedefine LIBAVCODEC_FOUND       @LIBAVCODEC_FOUND@
#cmakedefine LIBAVCODEC_VERSION     "@LIBAVCODEC_VERSION@"
/* libavformt */
#cmakedefine LIBAVFORMAT_FOUND      @LIBAVFORMAT_FOUND@
#cmakedefine LIBAVFORMAT_VERSION    "@LIBAVFORMAT_VERSION@"
/* libswscale */
#cmakedefine LIBSWSCALE_FOUND       @LIBSWSCALE_FOUND@
#cmakedefine LIBSWSCALE_VERSION     "@LIBSWSCALE_VERSION@"
/* libavfilter */
#cmakedefine LIBAVFILTER_FOUND      @LIBAVFILTER_FOUND@
#cmakedefine LIBAVFILTER_VERSION    "@LIBAVFILTER_VERSION@"
/* libswresample */
#cmakedefine LIBSWRESAMPLE_FOUND    "@LIBSWRESAMPLE_FOUND@"
#cmakedefine LIBSWRESAMPLE_VERSION  "@LIBSWRESAMPLE_VERSION@"
/* libavresample (deprecated) */
#cmakedefine LIBAVRESAMPLE_FOUND    "@LIBAVRESAMPLE_FOUND@"
#cmakedefine LIBAVRESAMPLE_VERSION  "@LIBAVRESAMPLE_VERSION@"

/* DVD reader library */
#cmakedefine LIBDVDREAD_FOUND       @LIBDVDREAD_FOUND@
#cmakedefine LIBDVDREAD_VERSION     "@LIBDVDREAD_VERSION@"
/* Bluray reader library */
#cmakedefine LIBBLURAY_FOUND        @LIBBLURAY_FOUND@
#cmakedefine LIBBLURAY_VERSION      "@LIBBLURAY_VERSION@"

#endif
