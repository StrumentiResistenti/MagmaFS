#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_CONFIG_SRCDIR([libmagma/magma.h])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
# AC_CONFIG_AUX_DIR([build-aux])

AC_INIT([libmagma],[0.1.20150213],[tx0@strumentiresistenti.org])
AC_COPYRIGHT([MAGMA is (C) Tx0 <tx0@strumentiresistenti.org>])
CXXFLAGS="-O0"

AM_INIT_AUTOMAKE([1.11 subdir-objects -Wall -Werror])

# Checks for programs.
AC_PROG_CC
# AM_PROG_CC_C_O
AM_PROG_AR
AC_PROG_LIBTOOL

LT_INIT

# Checks for libraries.

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h stddef.h stdlib.h string.h sys/socket.h sys/statfs.h sys/vfs.h syslog.h unistd.h utime.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_rdev])

# Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC

AC_FUNC_STAT
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS([inet_ntoa lchown mempcpy memset mkdir rmdir setenv socket strdup strerror utime])

dnl AC_SUBST([CFLAGS],["${CFLAGS} -g -D_FILE_OFFSET_BITS=64 -Wall -W"])
AC_SUBST([CFLAGS],["${CFLAGS} -D_FILE_OFFSET_BITS=64 -DFUSE_VERSION=28 -Wall -W"])

AC_CHECK_SIZEOF(mode_t)
AC_CHECK_SIZEOF(nlink_t)
AC_CHECK_SIZEOF(time_t)
AC_CHECK_SIZEOF(dev_t)
AC_CHECK_SIZEOF(blksize_t)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(off_t)
AC_CHECK_SIZEOF(ino_t)
AC_CHECK_SIZEOF(blkcnt_t)
AC_CHECK_SIZEOF(uid_t)
AC_CHECK_SIZEOF(gid_t)

# check for NFS interface
nfs_interface=no
AC_ARG_ENABLE(
	[nfs-interface],
	[AS_HELP_STRING([--enable-nfs-interface], [support NFS server functionalities @<:@default=no@:>@])],
	[nfs_interface=yes]
)
if test [ "x$nfs_interface" == "xyes" ]; then
	AC_SUBST([CFLAGS],["-DMAGMA_ENABLE_NFS_INTERFACE ${CFLAGS}"])
fi

# check for packed dump facility
pkt_dump=no
AC_ARG_ENABLE(
	[pkt-dump],
	[AS_HELP_STRING([--enable-pkt-dump], [support packet dumps @<:@default=no@:>@])],
	[pkt_dump=yes]
)
if test [ "x$pkt_dump" == "xyes" ]; then
	AC_SUBST([CFLAGS],["-DMAGMA_ENABLE_DUMP_TO_FILE ${CFLAGS}"])
fi

# add option for debugging on console instead of through syslog facility
debug_stderr=no
AC_ARG_ENABLE(
	[debug-stderr],
	[AS_HELP_STRING([--enable-debug-stderr], [print debugging statements on stderr instead of using syslog @<:@default=syslog@:>@])],
	[debug_stderr=yes]
)
if test [ "x$debug_stderr" == "xyes" ]; then
	AC_SUBST([CFLAGS],["-DMAGMA_DEBUG_STDERR ${CFLAGS}"])
else
	AC_SUBST([CFLAGS],["-DMAGMA_DEBUG_SYSLOG ${CFLAGS}"])
fi

# enable garbage collector (otherwise, delete data as soon as possible)
garbage_collector=yes
AC_ARG_ENABLE(
	[garbage-collector],
	[AS_HELP_STRING([--disable-garbage-collector], [disable garbage collector allowing instant removing of unused data @<:@default=no@:>@])],
	[garbage_collector=no]
)
if test [ "x$garbage_collector" == "xyes" ]; then
	AC_SUBST([CFLAGS],["-DMAGMA_ENABLE_GARBAGE_COLLECTOR ${CFLAGS}"])
fi

### # add profiling with cprof --- BROKENNNNN!!!!
### # simply compiling with -lcprof -finstrument-functions makes
### # programs segv before executing main!
###
### cprofiler=no
### AC_ARG_ENABLE(
### 	[cprofiler],
### 	[AS_HELP_STRING([--enable-cprofiler], [enable profiling using cprof @<:@default=no@:>@])],
### 	[cprofiler=yes]
### )
### if test [ "x$cprofiler" == "xyes" ]; then
### 	AC_SUBST([CFLAGS],["-D_FILE_OFFSET_BITS=64 -lcprof -finstrument-functions"])
### fi

# add profiling using prof
profiler=no
AC_ARG_ENABLE(
	[profiler],
	[AS_HELP_STRING([--enable-profiler], [enable profiling using gprof and internal statistics @<:@default=no@:>@])],
	[profiler=yes]
)
if test [ "x$profiler" == "xyes" ]; then
	AC_SUBST([CFLAGS],["${CFLAGS} -g -pg -DMAGMA_TIME_PROFILE"])
	echo "To profile multithreaded apps like magmad,"
	echo "use LD_PRELOAD=t/libgprof-helper.so magmad [opts...]"
	echo "or run magmad in monothread mode using line option -T"
else
	AC_SUBST([CFLAGS],["${CFLAGS}"])
fi

debugger=no
AC_ARG_ENABLE(
	[debugger],
	[AS_HELP_STRING([--enable-debugger], [enable debugging symbols for gdb @<:@default=no@:>@])],
	[debugger=yes]
)
if test [ "x$debugger" == "xyes" ]; then
	AC_SUBST([CFLAGS],["${CFLAGS} -g"])
else
	AC_SUBST([CFLAGS],["${CFLAGS}"])
fi

AC_CHECK_LIB([fuse],[fuse_main],,[
	echo "FUSE (Filesystem in USErspace) it's a great user space library"
	echo "which allows linux kernel users to create filesystem implementations"
	echo "without playing with complex kernel internals. Magma is entirely based"
	echo "on FUSE. Please install libfuse-dev package (or the one which best"
	echo "fits your linux distribution) or download and compile sources at:"
	echo "http://fuse.sourceforge.net/"
	AC_MSG_FAILURE(["libfuse not found"])
	exit 1
])

AC_CHECK_LIB([dbi],[dbi_conn_new],,[
	echo "DBI is an generic database abstactio layer used to connect to MySQL"
	echo "and will probably used to connect to SQLite too in future releases"
	echo "More info at http://libdbi.sourceforge.net/"
	AC_MSG_FAILURE(["libdbi not found"])
])

PKG_CHECK_MODULES([GLIB], [glib-2.0 gthread-2.0 gobject-2.0 gio-2.0])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
