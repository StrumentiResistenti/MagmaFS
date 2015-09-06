/*
   MAGMA -- mount.magma.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   MAGMA mount client written using FUSE userspace library.

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

#define MAGMA_REUSE_SOCKET TRUE

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif

#include "../config.h"
#include <errno.h>

#ifdef HAVE_SETXATTR
#	include <sys/xattr.h>
#endif

#ifdef MAGMA_DEBUG_SYSLOG
#	include <syslog.h>
#endif

// #define FUSE_USE_VERSION 25

#include <fuse.h>
#include "compat/fuse_opt.h"

#include <libmagma/magma.h>

#ifndef MAGMA_OPTIMIZE_READDIR
#define MAGMA_OPTIMIZE_READDIR TRUE
#endif

#ifndef MAGMA_DEFAULT_TTL
#define MAGMA_DEFAULT_TTL 2
#endif

#define MAGMA_CACHE_SOCKETS TRUE

/**
 * Prepare the context on FUSE operations (gets uid and gid)
 */
#define MAGMA_CLIENT_SETUP_CONTEXT() \
	uid_t uid = 0;\
	gid_t gid = 0;\
	struct fuse_context *ctx = fuse_get_context();\
	if (ctx != NULL) { uid = ctx->uid; gid = ctx->gid; }

#define magma_unlock_client_connection(socket, peer) {}

/**
 * Store the environment mount.magma is working under
 */
magma_environment_t magma_environment;

/**
 * Save a filehandle received after opening a file
 */
typedef struct {
	gchar commit_url[2 * MAGMA_TERMINATED_PATH_LENGTH];
	gchar key[SHA_READABLE_DIGEST_LENGTH];
} magma_fd;

/**
 * The hash table used to save magma_fd file descriptors
 */
GHashTable *magma_fds;

GMutex magma_refresh_topology_mutex;

void magma_refresh_topology()
{
	GSocketAddress *peer;
	GSocket *socket = magma_open_client_connection(magma_environment.remoteip, MAGMA_NODE_PORT, &peer);

	/*
	 * if the mutex is already locked, another thread has
	 * started a topology refresh, so we can safely return here
	 */
	if (!g_mutex_trylock(&magma_refresh_topology_mutex)) return;

	magma_lava *new_lava = g_new0(magma_lava, 1);
	guint16 offset = 0;
	int i = 0;

	while (1) {
		magma_node_response topology_response;
		magma_pktqs_transmit_topology(socket, peer, offset, &topology_response);

		if (topology_response.header.status isNot G_IO_STATUS_NORMAL) return;
		if (topology_response.header.res isNot 0) return;

		for (; i < topology_response.body.send_topology.transmitted_nodes; i++) {
			magma_node_inside_topology *n = &(topology_response.body.send_topology.nodes[i]);

			/*
			 * failsafe check: port can't be zero, that means the
			 * struct has not been filled
			 */
			if (!n->port) {
				dbg(LOG_ERR, DEBUG_PNODE, "Null node received @%d/%d",
					i, topology_response.body.send_topology.transmitted_nodes);
				return;
			}

			/*
			 * create the new node
			 */
			magma_volcano *new_node = magma_volcano_new(
				n->node_name,
				n->fqdn_name,
				n->ip_addr,
				n->port,
				0, 0,
				n->start_key,
				n->stop_key,
				NULL);

			if (!new_node) {
				dbg(LOG_ERR, DEBUG_ERR, "Null new node: aborting lava topology refresh");
				return;
			}

			/*
			 * add the node to the lava ring
			 */
			new_lava->participants++;

			if (!new_lava->first_node) {
				new_lava->first_node = new_lava->last_node = new_node;
			} else {
				new_lava->last_node->next = new_node;
				new_node->prev = new_lava->last_node;
				new_lava->last_node = new_node;
			}

			/*
			 * if no more nodes are left to be received
			 * then end the cycle
			 */
			if (
				(i is topology_response.body.send_topology.transmitted_nodes - 1) &&
				(!topology_response.body.send_topology.more_nodes_waiting)) {
				break;
			}
		}

		if (!topology_response.body.send_topology.more_nodes_waiting) break;
	}

	/*
	 * replace registered topology
	 */
	magma_lava *old_lava = lava;
	lava = new_lava;
	if (old_lava) magma_lava_destroy(old_lava);

	dbg(LOG_INFO, DEBUG_PFUSE, "network topology refreshed");

	g_mutex_unlock(&magma_refresh_topology_mutex);
	g_object_unref(socket);
	g_object_unref(peer);
}

/**
 * lstat equivalent
 */
static int magma_client_getattr(const char *path, struct stat *stbuf)
{
    magma_flare_response response;
    response.body.getattr.stbuf = stbuf;

	dbg(LOG_INFO, DEBUG_PFUSE, "GETATTR(%s)", path);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending GETATTR(%s)", path);
	magma_pktqs_getattr(socket, peer, uid, gid, path, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received GETATTR(%s)", path);

	if ((response.header.res is -1) && (response.header.err_no != ENOENT)) {
		dbg(LOG_ERR, DEBUG_ERR, "GETATTR(%s) error: %s", path, strerror(response.header.err_no));
		magma_close_client_connection(socket, peer);
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "GETATTR on %s OK!", path);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

    return ((response.header.res is -1) ? -response.header.err_no : 0);
}

static int magma_client_readlink(const char *path, char *buf, size_t size)
{
	magma_flare_response response;
	memset(buf, '\0', size);

	dbg(LOG_INFO, DEBUG_PFUSE, "READLINK on %s of size %lu", path, size);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending READLINK(%s)", path);
	magma_pktqs_readlink(socket, peer, uid, gid, path, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received READLINK(%s)", path);

	if ( response.header.res is -1 ) {
		dbg(LOG_ERR, DEBUG_ERR, "READLINK error: %s", strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "readlink is waiting %d bytes... got %lu bytes", response.header.res, strlen(buf));

		strcpy(buf, response.body.readlink.path);

		/*
		 * According to readlink() manpage, readlink() does not set '\0' at
		 * the end of the string. But FUSE API seems to requires that, as
		 * shown in fusexmp.c example filesystem.
		 */
		buf[response.header.res] = '\0';

		dbg(LOG_INFO, DEBUG_PFUSE, "READLINK OK! (%s)", buf);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	/*
	 * Also is strange that in case of success the API mandates that 0
	 * is returned instead of res byte read!
	 */
    return ((response.header.res is -1) ? -response.header.err_no : 0);
}

#if MAGMA_OPTIMIZE_READDIR

static int magma_client_readdir(
	const char *path,
	void *buf,
	fuse_fill_dir_t filler,
	off_t offset,
	struct fuse_file_info *fi)
{
	(void) fi;

	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "READDIR on %s", path);
	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	off_t looping_offset = offset;
	gboolean refresh_topology = FALSE;

	while (1) {
		magma_pktqs_readdir_extended(socket, peer, uid, gid, path, looping_offset, &response);

		/*
		 * check for network problems
		 */
		if (G_IO_STATUS_NORMAL != response.header.status) continue;

		/*
		 * decode all the entries
		 */
		int i;
		for (i = 0; i < response.body.readdir_extended.entry_number; i++) {
			magma_extended_readdir_entry *entry = &(response.body.readdir_extended.entries[i]);

			struct stat st;
			st.st_dev     = entry->st.dev;
			st.st_ino     = entry->st.ino;
			st.st_size    = entry->st.size;
			st.st_blocks  = entry->st.blocks;
			st.st_atime   = entry->st.atime;
			st.st_ctime   = entry->st.ctime;
			st.st_mtime   = entry->st.mtime;
			st.st_mode    = entry->st.mode;
			st.st_nlink   = entry->st.nlink;
			st.st_uid     = entry->st.uid;
			st.st_gid     = entry->st.gid;
			st.st_rdev    = entry->st.rdev;
			st.st_blksize = entry->st.blksize;

			if (filler(buf, entry->path, &st, 0)) {
				return (0);
			}
		}

		if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
			refresh_topology = TRUE;
		}

		/*
		 * exit the loop if all the directory has been read
		 */
		if (MAGMA_DIR_IS_CLOSE is response.header.res) break;

		/*
		 * update loop offset
		 */
		looping_offset = response.body.readdir_extended.offset;
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (refresh_topology) {
		magma_refresh_topology();
	}

	return (0);
}

#else

static int magma_client_readdir(
	const char *path,
	void *buf,
	fuse_fill_dir_t filler,
	off_t offset,
	struct fuse_file_info *fi)
{
	int exit_cycle = 0;
	int count = 0;
	(void) fi;

	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "READDIR on %s", path);
	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	magma_pktqs_readdir(socket, peer, uid, gid, path, &response);

	if ( response.header.res is MAGMA_DIR_IS_CLOSE ) {
		magma_close_client_connection(socket, peer);

		dbg(LOG_ERR, DEBUG_ERR, "Server returned %d: %s", response.header.res, strerror(response.header.err_no));
		return -response.header.err_no;
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Server returned ok: can readdir...");
	off_t running_offset = offset;

	do {
		magma_flare_response response;
		memset(&(response.body.readdir_offset.entry), 0, MAGMA_TERMINATED_DIRENTRY_LENGTH);

		magma_pkt_send_readdir_offset(socket, peer, uid, gid, running_offset);
		magma_pkt_recv_readdir_entry(socket, peer, &response);

		if (-1 != response.header.res) {
			dbg(LOG_INFO, DEBUG_PFUSE, "[%.3d] Read regular answer on %s", count++, response.body.readdir_offset.entry);
			exit_cycle = filler(buf, response.body.readdir_offset.entry, NULL, 0 /* offset */);
			running_offset += strlen(response.body.readdir_offset.entry) + 1;
		} else break;
	} while (!exit_cycle);

	if (exit_cycle is -1) {
		dbg(LOG_INFO, DEBUG_PFUSE, "READDIR OK!");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "READDIR OK! [but filler returned %d]", exit_cycle);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

    return 0;
}

#endif // MAGMA_OPTIMIZE_READDIR

static int magma_client_mknod(const char *path, mode_t mode, dev_t rdev)
{
	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "MKNOD(%s, %u, %u)", path, (unsigned int) mode, (unsigned int) rdev);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending MKNOD(%s)", path);
	magma_pktqs_mknod(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, mode, rdev, path, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received MKNOD(%s)", path);

	if ( response.header.res is -1 ) {
		dbg(LOG_ERR, DEBUG_ERR, "MKNOD(%s) error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "MKNOD(%s) OK!", path);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no: 0);
}

static int magma_client_mkdir(const char *path, mode_t mode)
{
	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "MKDIR(%s, %u)", path, mode);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending MKDIR(%s)", path);
	magma_pktqs_mkdir(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, mode, path, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received MKDIR(%s)", path);

	if (response.header.res is -1) {
		dbg(LOG_ERR, DEBUG_ERR, "MKDIR(%s) error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "MKDIR(%s) OK!", path);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : 0);
}

static int magma_client_unlink(const char *path)
{
    magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "UNLINK(%s)", path);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending UNLINK(%s)", path);
	magma_pktqs_unlink(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, path, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received UNLINK(%s)", path);

	if (response.header.res is -1) {
		dbg(LOG_ERR, DEBUG_ERR, "UNLINK(%s) error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "UNLINK(%s) OK!", path);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : 0);
}

static int magma_client_rmdir(const char *path)
{
	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "RMDIR(%s)", path);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending RMDIR(%s)", path);
	magma_pktqs_rmdir(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, path, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received RMDIR(%s)", path);

	if ( response.header.res is -1 ) {
		dbg(LOG_ERR, DEBUG_ERR, "RMDIR(%s) error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "RMDIR(%s) OK!", path);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : 0);
}

static int magma_client_symlink(const char *from, const char *to)
{
	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "SYMLINK(%s, %s)", from, to);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(from);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", from);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", from);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending SYMLINK(%s, %s)", from, to);
	magma_pktqs_symlink(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, from, to, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received SYMLINK(%s, %s)", from, to);

	if (response.header.res is -1) {
		dbg(LOG_ERR, DEBUG_ERR, "SYMLINK(%s, %s) error: %s", from, to, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "SYMLINK(%s, %s) OK!", from, to);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : 0);
}

static int magma_client_rename(const char *from, const char *to)
{
	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "RENAME(%s, %s)", from, to);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(from);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", from);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", from);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending RENAME(%s, %s)", from, to);
	magma_pktqs_rename(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, from, to, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received RENAME(%s, %s)", from, to);

	if ( response.header.res is -1 ) {
		dbg(LOG_ERR, DEBUG_ERR, "RENAME(%s, %s) error: %s", from, to, strerror(response.header.err_no));
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "RENAME(%s, %s) OK!", from, to);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : 0);
}

static int magma_client_link(const char *from, const char *to)
{
	return magma_client_symlink(from, to);
}

static int magma_client_chmod(const char *path, mode_t mode)
{
	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "CHMOD(%s, %u)", path, mode);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending CHMOD(%s)", path);
	magma_pktqs_chmod(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, path, mode, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received CHMOD(%s)", path);

	if ( response.header.res is -1 ) {
		dbg(LOG_ERR, DEBUG_ERR, "CHMOD(%s) error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "CHMOD(%s) performed!", path);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : 0);
}

static int magma_client_chown(const char *path, uid_t new_uid, gid_t new_gid)
{
	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "CHOWN(%s, %u, %u)", path, new_uid, new_gid);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending CHOWN(%s)", path);
	magma_pktqs_chown(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, path, new_uid, new_gid, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received CHOWN(%s)", path);

	if (response.header.res is -1) {
		dbg(LOG_ERR, DEBUG_ERR, "CHOWN(%s): error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "CHOWN(%s): performed ok!", path);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : 0);
}

static int magma_client_truncate(const char *path, off_t size)
{
	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "TRUNCATE(%s, %lu)", path, size);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending TRUNCATE(%s)", path);
	magma_pktqs_truncate(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, path, size, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received TRUNCATE(%s)", path);

	if ( response.header.res is -1 ) {
		dbg(LOG_ERR, DEBUG_ERR, "TRUNCATE(%s) error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "TRUNCATE(%s) OK!", path);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : 0);
}

static int magma_client_utime(const char *path, struct utimbuf *buf)
{
	magma_flare_response response;

    dbg(LOG_INFO, DEBUG_PFUSE, "UTIME(%s)", path);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending UTIME(%s)", path);
	magma_pktqs_utime(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, buf->actime, buf->modtime, path, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received UTIME(%s)", path);

	if ( response.header.res is -1 ) {
		dbg(LOG_ERR, DEBUG_ERR, "UTIME(%s) error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		response.header.err_no = 0;
		dbg(LOG_INFO, DEBUG_PFUSE, "UTIME(%s) OK!", path);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : 0);
}

static int magma_client_open(const char *path, struct fuse_file_info *fi)
{
	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "OPEN(%s)", path);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending OPEN(%s)", path);
	magma_pktqs_open(socket, peer, uid, gid, (fi->flags), path, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received OPEN(%s)", path);

	if ( response.header.res is -1 ) {
		dbg(LOG_ERR, DEBUG_ERR, "OPEN(%s) error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "OPEN(%s) OK!", path);

		/*
		 * save the file information in the global store
		 */
		magma_fd *fd = g_new0(magma_fd, 1);
		if (fd) {
			g_strlcpy(fd->commit_url, response.body.open.commit_url, 2 * MAGMA_TERMINATED_PATH_LENGTH);
			unsigned char* binhash = magma_sha1_data(fd->commit_url, strlen(fd->commit_url));
			gchar *armour = magma_armour_hash(binhash);
			strcpy(fd->key, armour);
			g_free(armour);
			g_free(binhash);

			g_hash_table_insert(magma_fds, g_strdup(path), fd);
		} else {
			return (-ENOMEM);
		}
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : 0);
}

GMutex magma_read_mutex;
GHashTable *magma_read_hash_table;

static int magma_client_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	magma_flare_response response;
	(void) fi;

	dbg(LOG_INFO, DEBUG_PFUSE, "READ(%s, %lu, %lu)", path, size, offset);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending READ(%s)", path);
	magma_pktqs_read(socket, peer, uid, gid, size, offset, path, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received READ(%s)", path);

	if ( response.header.res is -1 ) {
		dbg(LOG_ERR, DEBUG_ERR, "READ(%s) error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "READ(%s) OK! [%d bytes]", path, response.header.res);
	}

	memcpy(buf, response.body.read.buffer, response.header.res);

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : response.header.res);
}

static int magma_client_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    magma_flare_response response;
	(void) fi;

	dbg(LOG_INFO, DEBUG_PFUSE, "WRITE(%s, %lu, %lu)", path, size, offset);

	MAGMA_CLIENT_SETUP_CONTEXT();

	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending WRITE(%s)", path);
	magma_pktqs_write(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, size, offset, path, buf, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received WRITE(%s)", path);

	if ( response.header.res is -1 ) {
		dbg(LOG_ERR, DEBUG_ERR, "WRITE(%s) error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "WRITE(%s) OK! [%d bytes]", path, response.header.res);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : response.header.res);
}

static int magma_client_statfs(const char *path, struct statfs *statbuf)
{
	magma_flare_response response;

	dbg(LOG_INFO, DEBUG_PFUSE, "STATFS(%s)", path);

	MAGMA_CLIENT_SETUP_CONTEXT();
	
	magma_volcano *owner = magma_route_path(path);
	if (!owner) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': no owner found", path);
		return (-EPROTO);
	}
	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error routing path '%s': can't create socket", path);
		return (-EPROTO);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "Sending STATFS(%s)", path);
	magma_pktqs_statfs(socket, peer, uid, gid, path, &response);
	dbg(LOG_INFO, DEBUG_PFUSE, "Received STATFS(%s)", path);

	memcpy(statbuf, &response.body.statfs.statbuf, sizeof response.body.statfs.statbuf);

	if ( response.header.res is -1 ) {
		dbg(LOG_ERR, DEBUG_ERR, "STATFS(%s) error: %s", path, strerror(response.header.err_no));
		if (response.header.err_no != ENOENT) {
			magma_close_client_connection(socket, peer);
		}
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "STATFS(%s) OK!", path);
	}

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif

	if (magma_flag_is_raised(response.header.flags, MAGMA_FLAG_REFRESH_TOPOLOGY)) {
		magma_refresh_topology();
	}

	return ((response.header.res is -1) ? -response.header.err_no : 0);
}

static int magma_client_release(const char *path, struct fuse_file_info *fi)
{
    /* Just a stub.  This method is optional and can safely be left
       unimplemented */

    (void) path;
    (void) fi;

	dbg(LOG_INFO, DEBUG_PFUSE, "--- UNIMPLEMENTED --- RELEASE on %s", path);

	return 0; /* REMOVE ME AFTER CODING */
}

static int magma_client_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
    /* Just a stub.  This method is optional and can safely be left
       unimplemented */

    (void) path;
    (void) isdatasync;
    (void) fi;

	dbg(LOG_INFO, DEBUG_PFUSE, "--- UNIMPLEMENTED --- FSYNC on %s", path);

	return 0; /* REMOVE ME AFTER CODING */
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int magma_client_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags)
{
    int res = 0;

	dbg(LOG_INFO, DEBUG_PFUSE, "--- UNIMPLEMENTED --- SETXATTR on %s", path);

	return 0; /* REMOVE ME AFTER CODING */
}

static int magma_client_getxattr(const char *path, const char *name, char *value,
                    size_t size)
{
    int res = 0;

	dbg(LOG_INFO, DEBUG_PFUSE, "--- UNIMPLEMENTED --- GETXATTR on %s", path);

	return 0; /* REMOVE ME AFTER CODING */
}

static int magma_client_listxattr(const char *path, char *list, size_t size)
{
    int res = 0;

	dbg(LOG_INFO, DEBUG_PFUSE, "--- UNIMPLEMENTED --- LISTXATTR on %s", path);

	return 0; /* REMOVE ME AFTER CODING */
}

static int magma_client_removexattr(const char *path, const char *name)
{
    int res = 0;

	dbg(LOG_INFO, DEBUG_PFUSE, "--- UNIMPLEMENTED --- REMOVEXATTR on %s", path);

	return 0; /* REMOVE ME AFTER CODING */
}
#endif /* HAVE_SETXATTR */

static void *magma_init(void)
{
	return 0;
}

static struct fuse_operations magma_oper = {
    .getattr	= magma_client_getattr,
    .readlink	= magma_client_readlink,
    .readdir	= magma_client_readdir,
    .mknod		= magma_client_mknod,
    .mkdir		= magma_client_mkdir,
    .symlink	= magma_client_symlink,
    .unlink		= magma_client_unlink,
    .rmdir		= magma_client_rmdir,
    .rename		= magma_client_rename,
    .link		= magma_client_link,
    .chmod		= magma_client_chmod,
    .chown		= magma_client_chown,
    .truncate	= magma_client_truncate,
    .utime		= magma_client_utime,
    .open		= magma_client_open,
    .read		= magma_client_read,
    .write		= magma_client_write,
    .statfs		= magma_client_statfs,
    .release	= magma_client_release,
    .fsync		= magma_client_fsync,
#ifdef HAVE_SETXATTR
    .setxattr	= magma_client_setxattr,
    .getxattr	= magma_client_getxattr,
    .listxattr	= magma_client_listxattr,
    .removexattr= magma_client_removexattr,
#endif
	.init		= magma_init,
};

enum {
    KEY_HELP,
    KEY_VERSION,
};

/* following code got from SSHfs sources */
#define MAGMA_OPT(pattern, field, v) { pattern, offsetof(magma_environment_t, field), v}

static struct fuse_opt magma_opts[] = {
	MAGMA_OPT("-p %u",			remoteport,	0),
	MAGMA_OPT("--host=%s",		remotehost,	0),
	MAGMA_OPT("--ip=%s",		remoteip,	0),
	MAGMA_OPT("--debug=%s",		logstring,	0),
	MAGMA_OPT("--debug-all",	debug_all,	1),
	MAGMA_OPT("-s",				nothreads,	0),
	MAGMA_OPT("--key=%s",		keyphrase,	0),
	MAGMA_OPT("--keyphrase=%s",	keyphrase,	0),
	MAGMA_OPT("-f",				foreground,	0),

    FUSE_OPT_KEY("-V",			KEY_VERSION),
    FUSE_OPT_KEY("--version",	KEY_VERSION),
    FUSE_OPT_KEY("-h",			KEY_HELP),
    FUSE_OPT_KEY("--help",		KEY_HELP),
    FUSE_OPT_END
};

int usage_already_printed = 0;
void magma_usage(char *progname)
{
	if (usage_already_printed++)
		return;

	fprintf(stderr, "\n"
		"  MAGMA v.0.1 (build date: %s)\n"
		"  Distributed Hash Table based file system for Linux kernels\n"
		"  (c) 2006-2013 Tx0 <tx0@strumentiresistenti.org>\n"
		"  This is free software released under GPL\n"
		"  THIS SOFTWARE IS PROVIDED AS IS!\n"
		"  \n"
		"  Usage: %s [OPTIONS] /mountpoint\n"
		"  \n"
		"  -p <PORT>           Connect using port <PORT> (default 12000)\n"
		"  --host=<HOST>       Connect to remote server <HOST>\n"
		"  --ip=<IPADDR>       Connect to remote ip addr server <IPADDR>\n"
		"  --debug=<MASK>      Enables debug\n"
		"  --debug-all         Enables full debug\n"
		"  -s                  Run in single thread mode\n"
		"  --key=<TEXT>        Use <TEXT> as keyphrase to join network on <HOST>\n"
		"  --keyphrase=<TEXT>  alias for --key option\n"
		"  -u                  unmount a mounted filesystem\n"
		"  -q                  be quiet\n"
		"  -z                  lazy unmount (can be dangerous!)\n"
		"\n"  /*fuse options will follow... */
		"  Debug mask can contain:\n\n",
		__DATE__, progname
	);
	magma_print_log_levels();
	fprintf(stderr, "\n\n");
}

static int magma_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
    (void) data;
	(void) arg;

    switch (key) {
		case FUSE_OPT_KEY_NONOPT:
			if (!magma_environment.mountpoint) {
				magma_environment.mountpoint = g_strdup(arg);
				return 1;
			}
			return 0;

	    case KEY_HELP:
	        magma_usage(outargs->argv[0]);
	        fuse_opt_add_arg(outargs, "-ho");
	        fuse_main(outargs->argc, outargs->argv, &magma_oper);
	        exit(1);
	
	    case KEY_VERSION:
	        fprintf(stderr, "Magma client for Linux 0.1 (prerelease %s)\n", VERSION);
#if FUSE_VERSION >= 25
	        fuse_opt_add_arg(outargs, "--version");
	        fuse_main(outargs->argc, outargs->argv, &magma_oper);
#endif
	        exit(0);
	
	    default:
	        fprintf(stderr, "Extra parameter provided\n");
	        magma_usage(outargs->argv[0]);
    }

	return 0;
}

gboolean magma_str_cmp(gchar *a, gchar *b)
{
	if (strcmp(a, b) is 0) return (TRUE);
	return (FALSE);
}

int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	char *fsname;
	int res = 0;

	/* setting everything to zero */
	magma_environment.progname = argv[0];
	magma_environment.remotehost = NULL;
	magma_environment.remoteip = NULL;
	magma_environment.remoteport = 0;
	magma_environment.debug_all = 0;
	magma_environment.nothreads = 0;
	magma_environment.keyphrase = NULL;
	magma_environment.mountpoint = NULL;
	magma_environment.foreground = 0;

	memset(magma_environment.log, 0, 255);
	magma_init_log();

	if (fuse_opt_parse(&args, &magma_environment, magma_opts, magma_opt_proc) is -1)
		exit(1);

	magma_expand_log_channels(&magma_environment);

	fuse_opt_add_arg(&args, "-obig_writes");
	fuse_opt_add_arg(&args, "-ofsname=magma");
	fuse_opt_add_arg(&args, "-omax_write=32768");
	fuse_opt_add_arg(&args, "-omax_read=32768");

	/* saving debug state */
	if (magma_environment.debug_all) memset(magma_environment.log, 1, 255);

	gchar *mask = magma_print_log_mask(magma_environment.log);
	dbg(LOG_INFO, DEBUG_BOOT, "Dmask: [%s] %s", magma_environment.logstring, mask);
	g_free_null(mask);
	fuse_opt_add_arg(&args, "-f");

	magma_environment.log[DEBUG_ERR] = 1;
	
	/*
	 * if debug mask includes DEBUG_MALLOC,
	 * memory debugging is enabled. ENV variable MALLOC_TRACE should
	 * contain the name of stack trace file to be later analized with
	 * mtrace command line utility. filename is set to failsafe value
	 * here but can be overwritten on program start
	 */
	if (magma_environment.log[DEBUG_MALLOC]) {
		if (getenv("MALLOC_TRACE") is NULL)
			setenv("MALLOC_TRACE", "/tmp/mount.magma.memorytrace", 1);
		dbg(LOG_INFO, DEBUG_BOOT, "Enabling memory trace in file %s", getenv("MALLOC_TRACE"));
		mtrace();
	}

	if (!magma_environment.mountpoint) {
		magma_usage(magma_environment.progname);
		fprintf(stderr, "  ERROR: No mountpoint provided \n\n");
		exit(2);
	}

	fprintf(stderr, "\n");

	fprintf(stderr,
		"  MAGMA v.0.1 (build date: %s)\n"
		"  Distributed Hash Table based file system for Linux kernels\n"
		"  (c) 2006-2015 Tx0 <tx0@strumentiresistenti.org>\n"
		"  This is free software released under GPL\n"
		"  THIS SOFTWARE IS PROVIDED AS IS!\n",
		__DATE__
	);

	if (!magma_environment.remoteport) magma_environment.remoteport = 12000;

	if ((!magma_environment.remotehost) || (strcmp(magma_environment.remotehost, "") is 0))
		magma_environment.remotehost = g_strdup("localhost");

	if (magma_environment.foreground) {
		fprintf(stderr, " INFO: will run in foreground\n");
		fuse_opt_add_arg(&args, "-f");
	}

	if ((!magma_environment.keyphrase) || (strcmp(magma_environment.keyphrase, "") is 0)) {
		fprintf(stderr, "  Enter Keyphrase: ");
		magma_environment.keyphrase = g_strdup("");
	}

	// getting remote ip from remote host name
	if ((magma_environment.remoteip is NULL) && strlen(magma_environment.remotehost)) {
		dbg(LOG_INFO, DEBUG_NET, "Resolving %s to its IP address", magma_environment.remotehost);
		struct hostent *he;
		if ((he = gethostbyname(magma_environment.remotehost)) != NULL) {
			struct in_addr inaddr;
			memcpy(&(inaddr.s_addr), he->h_addr, he->h_length);
			magma_environment.remoteip = inet_ntoa(inaddr);
			fprintf(stderr, "  Domain name is %s (%s)\n", he->h_name, magma_environment.remoteip);
		}
	}
	
	if (magma_environment.remotehost)
		fsname = g_strdup(magma_environment.remotehost);
	else
		fsname = g_strdup(magma_environment.remoteip);

	gchar *extra_args = g_strdup_printf("-oallow_other,use_ino,readdir_ino,fsname=magma-%s", fsname);
	fuse_opt_add_arg(&args, extra_args);
	g_free_null(extra_args);

	if (magma_environment.nothreads) fuse_opt_add_arg(&args, "-s");

	// TODO remove after debugging
	fuse_opt_add_arg(&args, "-s");
	fuse_opt_add_arg(&args, "-f");
//	fuse_opt_add_arg(&args, "-d");

	fprintf(stderr, "\n    Mounting %s:%d on %s using key %s...\n\n",
		(magma_environment.remotehost) ? magma_environment.remotehost : magma_environment.remoteip,
		magma_environment.remoteport, magma_environment.mountpoint, magma_environment.keyphrase);

	umask(0);

#ifdef MAGMA_DEBUG_SYSLOG
	openlog("mount.magma", LOG_PID, LOG_DAEMON);
#endif

#ifdef MAGMA_ENABLE_DUMP_TO_FILE
	init_dump_to_file("/tmp/mount.magma.dump");
#endif /* MAGMA_ENABLE_DUMP_TO_FILE */

#if GLIB_MAJOR_VERSION < 2 || (GLIB_MAJOR_VERSION is 2 && GLIB_MINOR_VERSION < 34)
	/* init GLib thread and type system */
	g_type_init();
#endif

	/*
	 * initialize the file descriptor hash table
	 */
	magma_fds = g_hash_table_new_full(g_str_hash, (GEqualFunc) magma_str_cmp, g_free, g_free);

	/*
	 * init the read hash table
	 */
	magma_read_hash_table = g_hash_table_new(g_str_hash, g_str_equal);

	magma_init_net_layer();

#if 0
	/*
	 * open the socket
	 */
	magma_environment.socket = magma_open_client_connection(
		magma_environment.remoteip,
		magma_environment.remoteport,
		&(magma_environment.peer));
#endif

	dbg(LOG_INFO, DEBUG_BOOT, "MAGMA: mounting %s:%d on %s",
		magma_environment.remoteip,
		magma_environment.remoteport,
		magma_environment.mountpoint);
	
	/*
	 * reading network topology
	 */
	magma_refresh_topology();

	dbg(LOG_INFO, DEBUG_BOOT, "Fuse options:");
	int fargc = 0;
	while (fargc < argc - 1) {
		dbg(LOG_INFO, DEBUG_BOOT, "%.2d: %s", fargc, args.argv[fargc]);
		fargc++;
	}

#if FUSE_VERSION >= 26
	res = fuse_main(args.argc, args.argv, &magma_oper, NULL);
#else
	res = fuse_main(args.argc, args.argv, &magma_oper);
#endif

    fuse_opt_free_args(&args);

	return res;
}

// vim:ts=4:autoindent:nocindent:syntax=c
