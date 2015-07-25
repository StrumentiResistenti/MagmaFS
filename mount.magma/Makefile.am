ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = mount.magma

mount_magma_SOURCES = src/mount.magma.c src/compat/fuse_opt.c
mount_magma_CFLAGS = $(GLIB_CFLAGS) -DFUSE_USE_VERSION=22 -D_NET_LAYER_INCLUDE_GET_SOCKET -DMAGMA_MOUNT_PROGRAM -DMAGMA_EXCLUDE_XLATE -I/usr/local/include/magma-1.0
mount_magma_LDFLAGS = -L/usr/local/lib/
mount_magma_LDADD = -lfuse -lm $(GLIB_LIBS) -lmagma-1.0
