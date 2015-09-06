/*
   MAGMA -- magma_flare.h
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Defines functions to operate on flares in an abstract and opaque mode.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
*/

#ifndef MAGMA_FLARE_H
#define MAGMA_FLARE_H

#include "../magma_setup.h"
#include "magma_flare_types.h"
#include "magma_flare_internals.h"

#define MAGMA_DEFAULT_HASHPATH		"/tmp/magma/"
#define MAGMA_DEFAULT_BANDWIDTH		1
#define MAGMA_DEFAULT_STORAGE		1
#define MAGMA_DEFAULT_SERVERNAME	"localhost.localdomain"
#define MAGMA_DEFAULT_NICKNAME		"localhost"
#define MAGMA_DEFAULT_IPADDR		"127.0.0.1"

#define MAGMA_DEFAULT_TTL  2
#define MAGMA_TERMINAL_TTL 1

extern void magma_init_server_flare();
extern void magma_flare_system_init();
extern void magma_build_network(int bootstrap, char *bootserver, int bootport, int start_balancer);

/**
 * Init the callback lookup table
 */
extern void magma_init_flare_callbacks();
extern void magma_init_node_callbacks();

/**
 * magma flare system interface mimics as closer
 * as possible libc function interface. every function
 * listed below set errno on its exit.
 *
 * each function gets a uid and gid to assign the
 * operation to a used and to chek permission
 */

/**
 * lstat() equivalent. Fills struct stat stbuf of informations
 * relating to flare path.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param path path of the flare requested
 * @param stbuf struct stat result buffer
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_getattr(uid_t uid, gid_t gid, const char *path, struct stat *stbuf);
#define magma_lstat(uid, gid, path, stbuf) magma_getattr(uid, gid, path, stbuf);

/**
 * stat() equivalent. Fills struct stat stbuf of informations
 * relating to flare path.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param path path of the flare requested
 * @param stbuf struct stat result buffer
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_stat(uid_t uid, gid_t gid, const char *path, struct stat *stbuf);

/**
 * statfs() equivalent. Fills struct statfs statbuf of informations
 * relating to share which holds flare path. Since magma preorganize
 * shares and since flare protocol predeclare the share involved in
 * current connection, passing a path is useless. But parameter has
 * been mantained so far.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param path path of the flare requested
 * @param statbuf struct stat result buffer
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_statfs(uid_t uid, gid_t gid, const char *path, struct statfs *statbuf);

/**
 * mknod() equivalent. Create new regular, FIFO char and block dev flares.
 * Returns an error if used to create a directory.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param ttl if greater than MAGMA_TERMINAL_TTL allows to proxy the request
 * @param path path of the flare to be created
 * @param mode permission and flare type (see mknod(2) for more info)
 * @param rdev major and minor number if flare is a char or block device
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_mknod(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, mode_t mode, dev_t rdev);

/**
 * unlink() equivalent. Delete a regular, FIFO, char or block dev flare.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param ttl if greater than MAGMA_TERMINAL_TTL allows to proxy the request
 * @param path path of the flare to be unlinked
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_unlink(uid_t uid, gid_t gid, magma_ttl ttl, const char *path);

/**
 * truncate() equivalent. Truncate a regular flare.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param ttl if greater than MAGMA_TERMINAL_TTL allows to proxy the request
 * @param path path of the flare requested
 * @param offset the truncation point
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_truncate(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, off_t offset);

/**
 * utime() equivalent. Update access and modification time of a flare.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param ttl if greater than MAGMA_TERMINAL_TTL allows to proxy the request
 * @param path path of the flare requested
 * @param atime access time
 * @param mtime modification time
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_utime(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, time_t atime, time_t mtime);

/**
 * rename() equivalent. Change name to a flare.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param from original flare path
 * @param to new flare path
 * @return 0 on success, -1 on error and sets errno
 *
 * TODO: So far magma uses a typical errno (EXDEV) which means cross link rename
 * to refuse renaming a flare, since that can involve up to four magma nodes. Can
 * be changed in the future...
 */
extern int magma_rename(uid_t uid, gid_t gid, const char *from, const char *to);

/**
 * chmod() equivalent. Changes mode to a flare.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param ttl if greater than MAGMA_TERMINAL_TTL allows to proxy the request
 * @param path path of the flare requested
 * @param mode new flare mode
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_chmod(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, mode_t mode);

/**
 * chown() equivalent. Changes ownership to a flare.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param ttl if greater than MAGMA_TERMINAL_TTL allows to proxy the request
 * @param path path of the flare requested
 * @param newuid new UID to be assigned to the flare
 * @param newgid new GID to be assigned to the flare
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_chown(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, uid_t newuid, gid_t newgid);

/**
 * open() equivalent. 
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param path path of the flare requested
 * @param flags describe the type of open (read, write, ...) (see open(2) for more informations)
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_open(uid_t uid, gid_t gid, const char *path, int flags);

/**
 * read() equivalent. Read some data from a regular flare.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param path path of the flare requested
 * @param size requested data length
 * @param offset requested data position inside the flare
 * @param buf memory region to store read data
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_read(uid_t uid, gid_t gid, const char *path, size_t size, off_t offset, char *buf);

/**
 * write() equivalent. Write some data to a regular file.
 * relating to flare path.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param ttl if greater than MAGMA_TERMINAL_TTL allows to proxy the operation
 * @param path path of the flare requested
 * @param size data length
 * @param offset data position inside the flare
 * @param buf memory region to store read data
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_write(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, size_t size, off_t offset, char *buf);

/**
 * mkdir() equivalent. Create a new directory flare.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param ttl if greater than MAGMA_TERMINAL_TTL allows to proxy the request
 * @param path path of the directory
 * @param mode directory permission
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_mkdir(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, mode_t mode);

/**
 * rmdir() equivalent. Delete a directory flare.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param ttl if greater than MAGMA_TERMINAL_TTL allows to proxy the request
 * @param path path of the flare requested
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_rmdir(uid_t uid, gid_t gid, magma_ttl ttl, const char *path);

/**
 * readlink() equivalent. Read target of a link.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param path path of the flare requested
 * @param buf memory location to store link target
 * @param size dimension of buffer buf
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_readlink(uid_t uid, gid_t gid, const char *path, char *buf, size_t size);

/**
 * symlink() equivalent. Create a symbolic link.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param ttl if greater than MAGMA_TERMINAL_TTL allows to proxy the request
 * @param from path of the symbolic link
 * @param to target path of this link
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_symlink(uid_t uid, gid_t gid, magma_ttl ttl, const char *from, const char *to);

/**
 * link() equivalent. Since magma use crosslink devices to store
 * flares, doing hardlink is not always possible. Hard link are
 * transformed in symlink transparently.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param ttl if greater than MAGMA_TERMINAL_TTL allows to proxy the request
 * @param from path of the symbolic link
 * @param to target path of this link
 * @return 0 on success, -1 on error and sets errno
 */
extern int magma_link(uid_t uid, gid_t gid, magma_ttl ttl, const char *from, const char *to);

/*
 * magma need a re-implementation of directory because every flare
 * should be thought like a ready-to-ship box, which can be transferred
 * between nodes at any time. real directory (of the underlying
 * filesystem) can't be transferred this way. they need file presence too.
 */

/**
 * opendir() equivalent. Opens a directory flare for reading
 * and inserting/removing data.
 *
 * @param uid UID requesting the operation
 * @param gid GID requesting the operation
 * @param path path of the flare requested
 * @return 0 on success, -1 on error and sets errno
 */
extern magma_DIR_t *magma_opendir(uid_t uid, gid_t gid, const char *path);

/**
 * readdir() equivalent. Read next entry from and opened directory
 * flare given as magma_DIR_t handle.
 *
 * @param dirp handle to open directory flare
 * @return 0 on success, -1 on error and sets errno
 */
extern char *magma_readdir(magma_DIR_t *dirp);

/**
 * seekdir() equivalent. Reposition directory pointer to
 * another location.
 *
 * @param dirp handle to open directory flare
 * @param offset new handle position
 * @return nothing. Always succeed.
 */
extern void magma_seekdir(magma_DIR_t *dirp, off_t offset);

/**
 * telldir() equivalent. Tell current directory handle position.
 *
 * @param dirp handle to open directory flare
 * @return current directory handle position
 */
extern off_t magma_telldir(magma_DIR_t *dirp);

/**
 * closedir() equivalent. Close an open directory flare handle.
 * If directory has been modified, reflect changes on disk.
 *
 * @param dirp handle to open directory flare
 * @return nothing. Always succeed.
 */
extern void magma_closedir(magma_DIR_t *dirp);

/**
 * write cache on disk
 */
extern void magma_sync_cache();

/**
 * write cache on disk and delete it from memory
 */
extern void magma_flush_cache();

/**
 * hook for signal()
 *
 * @param s signal received
 */
extern void magma_cleanup(int s);

/**
 * perform clean shutdown, what else?
 */
extern void magma_clean_shutdown();

extern void magma_send_udp_failure(GSocket *socket, GSocketAddress *peer, int err_no, magma_transaction_id tid, magma_flags flags);

extern void magma_node_transmit_key(GSocket *socket, GSocketAddress *peer, const gchar *flare_key);

extern void magma_update_myself_from_lava(magma_lava *new_lava);

#endif /* MAGMA_FLARE_H */

// vim:ts=4:nocindent:autoindent
