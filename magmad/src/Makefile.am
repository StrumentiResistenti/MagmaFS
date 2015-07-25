sbin_PROGRAMS = magmad

magmad_SOURCES = magmad.c parseargs.c magmad.h\
	protocol_nfs.c nfs.c nfs_prot.h nfs_prot_xdr.c nfs_prot_svc.c\
	protocol_mount.c mount.h mount_xdr.c mount_svc.c
magmad_CFLAGS = $(GLIB_CFLAGS) -D_REENTRANT -D_NET_LAYER_INCLUDE_GET_SOCKET -DMAGMA_SERVER_NODE
magmad_LDADD = -lm $(GLIB_LIBS) -lmagma
