SUBDIRS = # t

lib_LTLIBRARIES = libmagma-1.0.la

libmagma_1_0_la_SOURCES = magma.h\
	vulcano.c\
	vulcano.h\
	net_layer.c\
	net_layer.h\
	sha1.c\
	sha1.h\
	utils.c\
	utils.h\
	routing.c\
	debug.h\
	protocol/protocol.c\
	protocol/protocol.h\
	protocol/protocol_pkt.c\
	protocol/protocol_pkt.h\
	protocol/console/protocol_console.c\
	protocol/flare/protocol_flare.h\
	protocol/flare/chmod.c\
	protocol/flare/chown.c\
	protocol/flare/commons.c\
	protocol/flare/getattr.c\
	protocol/flare/mkdir.c\
	protocol/flare/mknod.c\
	protocol/flare/open.c\
	protocol/flare/read.c\
	protocol/flare/readdir.c\
	protocol/flare/readlink.c\
	protocol/flare/rename.c\
	protocol/flare/rmdir.c\
	protocol/flare/statfs.c\
	protocol/flare/symlink.c\
	protocol/flare/truncate.c\
	protocol/flare/unlink.c\
	protocol/flare/utime.c\
	protocol/flare/write.c\
	protocol/node/protocol_node.h\
	flare_system/magma_flare.c\
	flare_system/magma_flare.h\
	flare_system/magma_flare_internals.c\
	flare_system/magma_flare_internals.h\
	flare_system/server_flare.c\
	flare_system/server_node.c\
	flare_system/acl.c\
	flare_system/balance.c


libmagma_1_0_la_CFLAGS = $(GLIB_CFLAGS) -D_REENTRANT -D_NET_LAYER_INCLUDE_GET_SOCKET
libmagma_1_0_la_LIBADD = -lm $(GLIB_LIBS)

library_includedir=$(includedir)/magma-1.0/magma
library_include_HEADERS = \
	magma/magma.h\
	magma/net_layer.h\
	magma/sha1.h\
	magma/magma_flare.h\
	magma/magma_flare_internals.h\
	magma/vulcano.h\
	magma/protocol.h\
	magma/protocol_pkt.h\
	magma/protocol_flare/protocol_flare.h\
	magma/protocol_node/protocol_node.h\
	magma/debug.h
