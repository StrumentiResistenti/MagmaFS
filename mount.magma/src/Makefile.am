bin_PROGRAMS = mount.magma

mount_magma_SOURCES = mount.magma.c compat/fuse_opt.c
mount_magma_CFLAGS = $(GLIB_CFLAGS) -DFUSE_USE_VERSION=22 -D_NET_LAYER_INCLUDE_GET_SOCKET -DMAGMA_MOUNT_PROGRAM -DMAGMA_EXCLUDE_XLATE
mount_magma_LDADD = -lfuse -lm $(GLIB_LIBS) -lmagma
