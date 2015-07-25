ACLOCAL_AMFLAGS = -I m4

sbin_PROGRAMS = magmad

magmad_SOURCES = \
	src/magmad.c\
	src/parseargs.c\
	src/magmad.h\
	src/protocol_nfs.c\
	src/nfs.c\
	src/nfs_prot.h\
	src/nfs_prot_xdr.c\
	src/nfs_prot_svc.c\
	src/protocol_mount.c\
	src/mount.h\
	src/mount_xdr.c\
	src/mount_svc.c

magmad_CFLAGS = $(GLIB_CFLAGS) -D_REENTRANT -D_NET_LAYER_INCLUDE_GET_SOCKET -DMAGMA_SERVER_NODE -I/usr/local/include/magma-1.0
magmad_LDADD = -lm $(GLIB_LIBS) -lmagma-1.0
