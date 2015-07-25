## Place generated object files (.o) into the same directory as their source
## files, in order to avoid collisions when non-recursive make is used.
AUTOMAKE_OPTIONS = subdir-objects

## Additional flags to pass to aclocal when it is invoked automatically at
## make time. The ${ACLOCAL_FLAGS} variable is picked up from the environment
## to provide a way for the user to supply additional arguments.
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4 --install

## The libtool archive file (.la) will be installed into the directory named
## by the predefined variable $(bindir), along with the actual shared library
## file (.so).
lib_LTLIBRARIES = libmagma-1.0.la

libmagma_1_0_la_SOURCES = \
	libmagma/magma.h\
	libmagma/vulcano.c\
	libmagma/vulcano.h\
	libmagma/net_layer.c\
	libmagma/net_layer.h\
	libmagma/sha1.c\
	libmagma/sha1.h\
	libmagma/utils.c\
	libmagma/utils.h\
	libmagma/routing.c\
	libmagma/debug.h\
	libmagma/protocol/protocol.c\
	libmagma/protocol/protocol.h\
	libmagma/protocol/protocol_pkt.c\
	libmagma/protocol/protocol_pkt.h\
	libmagma/protocol/console/protocol_console.c\
	libmagma/protocol/flare/protocol_flare.h\
	libmagma/protocol/flare/chmod.c\
	libmagma/protocol/flare/chown.c\
	libmagma/protocol/flare/commons.c\
	libmagma/protocol/flare/getattr.c\
	libmagma/protocol/flare/mkdir.c\
	libmagma/protocol/flare/mknod.c\
	libmagma/protocol/flare/open.c\
	libmagma/protocol/flare/read.c\
	libmagma/protocol/flare/readdir.c\
	libmagma/protocol/flare/readlink.c\
	libmagma/protocol/flare/rename.c\
	libmagma/protocol/flare/rmdir.c\
	libmagma/protocol/flare/statfs.c\
	libmagma/protocol/flare/symlink.c\
	libmagma/protocol/flare/truncate.c\
	libmagma/protocol/flare/unlink.c\
	libmagma/protocol/flare/utime.c\
	libmagma/protocol/flare/write.c\
	libmagma/protocol/node/protocol_node.h\
	libmagma/protocol/node/protocol_node.c\
	libmagma/flare_system/magma_flare.c\
	libmagma/flare_system/magma_flare.h\
	libmagma/flare_system/magma_flare_internals.c\
	libmagma/flare_system/magma_flare_internals.h\
	libmagma/flare_system/server_flare.c\
	libmagma/flare_system/server_node.c\
	libmagma/flare_system/acl.c\
	libmagma/flare_system/sql.c\
	libmagma/flare_system/balance.c

libmagma_1_0_la_CFLAGS = $(GLIB_CFLAGS) -D_REENTRANT -D_NET_LAYER_INCLUDE_GET_SOCKET
libmagma_1_0_la_LIBADD = -lm $(GLIB_LIBS)

## Define the list of public header files and their install location.  The
## nobase_ prefix instructs Automake to not strip the directory part from each
## filename, in order to avoid the need to define separate file lists for each
## installation directory.  This only works if the directory hierarchy in the
## source tree matches the hierarchy at the install location, however.
library_includedir=$(includedir)/magma-1.0/
nobase_library_include_HEADERS = \
	libmagma/magma.h\
	libmagma/magma_setup.h\
	libmagma/net_layer.h\
	libmagma/sha1.h\
	libmagma/flare_system/magma_flare.h\
	libmagma/flare_system/magma_flare_internals.h\
	libmagma/flare_system/magma_flare_types.h\
	libmagma/vulcano.h\
	libmagma/protocol/protocol.h\
	libmagma/protocol/protocol_pkt.h\
	libmagma/protocol/flare/protocol_flare.h\
	libmagma/protocol/node/protocol_node.h\
	libmagma/protocol/console/protocol_console.h\
	libmagma/protocol/balancer/protocol_balancer.h\
	libmagma/debug.h\
	libmagma/utils.h

## The generated configuration header is installed in its own subdirectory of
## $(libdir).  The reason for this is that the configuration information put
## into this header file describes the target platform the installed library
## has been built for.  Thus the file must not be installed into a location
## intended for architecture-independent files, as defined by the Filesystem
## Hierarchy Standard (FHS).
## The nodist_ prefix instructs Automake to not generate rules for including
## the listed files in the distribution on 'make dist'.  Files that are listed
## in _HEADERS variables are normally included in the distribution, but the
## configuration header file is generated at configure time and should not be
## shipped with the source tarball.
libmagma_libincludedir = $(libdir)/libmagma-1.0/include
nodist_libmagma_libinclude_HEADERS = config.h libmagma.h

EXTRA_DIRS = m4/libtool.m4  m4/lt~obsolete.m4  m4/ltoptions.m4  m4/ltsugar.m4  m4/ltversion.m4
