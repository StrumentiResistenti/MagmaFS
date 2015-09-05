/*
   MAGMA -- magma_flare.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Defines functions to operate on flare in an abstract and opaque mode.

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

#include "../magma.h"

void magma_single_add_flare_to_parent(const gchar *path, magma_volcano *parent)
{
	dbg(LOG_INFO, DEBUG_PFUSE, "Adding flare %s on %s", path, parent->node_name);

	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(parent->ip_addr, MAGMA_NODE_PORT, &peer);

	magma_node_response response;
	magma_pktqs_add_flare_to_parent(socket, peer, path, &response);
	magma_close_client_connection(socket, peer);
}

void magma_whole_add_flare_to_parent(const gchar *path)
{
	gchar *parent_path = g_strdup(path);
	gchar *filename = magma_point_filename_in_path(parent_path);
	if (filename is parent_path) {
		dbg(LOG_ERR, DEBUG_FLARE, "Trying to add %s to non existing parent", path);
		return;
	}
	filename--;
	*filename = '\0';

	/*
	 * add flare on parent main node
	 */
	magma_volcano *parent = magma_route_path(parent_path);
	magma_single_add_flare_to_parent(path, parent);

	/*
	 * add flare on parent redundant node
	 */
	magma_volcano *red_parent = parent->next ? parent->next : lava->first_node;
	if (red_parent is parent) {
		dbg(LOG_INFO, DEBUG_PFUSE, "%s red_parent is parent", path);
	} else {
		magma_single_add_flare_to_parent(path, red_parent);
	}

	/*
	 * add flare on joining node, if any
	 */
	if (myself.joining_node) magma_single_add_flare_to_parent(path, myself.joining_node);

	g_free(parent_path);
}

void magma_single_remove_flare_from_parent(const gchar *path, magma_volcano *parent)
{
	dbg(LOG_INFO, DEBUG_PFUSE, "Removing flare %s from %s", path, parent->node_name);

	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(parent->ip_addr, MAGMA_NODE_PORT, &peer);

	magma_node_response response;
	magma_pktqs_remove_flare_from_parent(socket, peer, path, &response);
	magma_close_client_connection(socket, peer);
}

void magma_whole_remove_flare_from_parent(const gchar *path)
{
	gchar *parent_path = g_strdup(path);
	gchar *filename = magma_point_filename_in_path(parent_path);
	if (filename is parent_path) {
		dbg(LOG_ERR, DEBUG_FLARE, "Trying to remove %s from non existing parent", path);
		return;
	}
	filename--;
	*filename = '\0';

	/*
	 * add flare on parent main node
	 */
	magma_volcano *parent = magma_route_path(parent_path);
	magma_single_remove_flare_from_parent(path, parent);

	/*
	 * add flare on parent redundant node
	 */
	magma_volcano *red_parent = parent->next ? parent->next : lava->first_node;
	if (red_parent is parent) {
		dbg(LOG_INFO, DEBUG_PFUSE, "%s red_parent is parent", path);
	} else {
		magma_single_remove_flare_from_parent(path, red_parent);
	}

	/*
	 * remove flare from joining node, if any
	 */
	if (myself.joining_node) magma_single_remove_flare_from_parent(path, myself.joining_node);

	g_free(parent_path);
}

int magma_getattr(uid_t uid, gid_t gid, const char *path, struct stat *stbuf)
{
	magma_flare_response response;
	response.body.getattr.stbuf = stbuf;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_getattr() called with NULL or zero-length path");
		return (-1);
	}

	/*
	 * get owner and redundant owner of the path
	 */
	magma_volcano *owner = magma_route_path(path);
	magma_volcano *red_owner = owner->next ? owner->next : lava->first_node; // magma_redundant_route_path(path);

	/*
	 * guess if this node is owner or redundant owner
	 */
	gboolean im_owner = magma_compare_nodes(owner, &myself);
	gboolean im_red_owner = im_owner ? FALSE : magma_compare_nodes(red_owner, &myself);

	/*
	 * do the local operation, if applies
	 */
	if (im_owner || im_red_owner) {
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "GETATTR(%s): no space for flare allocation", path);
		} else if (!flare->type) {
			response.header.res = -1;
			response.header.err_no = ENOENT;
			dbg(LOG_ERR, DEBUG_PFUSE, "GETATTR(%s): flare not upcasted", path);
		} else if (!magma_check_flare(flare)) {
			response.header.res = -1;
			response.header.err_no = ENOENT;
			dbg(LOG_ERR, DEBUG_PFUSE, "GETATTR(%s): flare is not on disk", path);
		} else {
			magma_flare_t *parent = magma_search_or_create(flare->parent_path);
			uint8_t perm_res = 0;
			if ((perm_res = magma_check_permission(parent, uid, gid, MAGMA_OPERATION_X)) != 0) {
				dbg(LOG_ERR, DEBUG_PFUSE, "GETATTR operation not permitted to %d.%d", uid, gid);
				magma_explain_permission(perm_res);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else if ((response.header.res = stat(flare->contents, stbuf)) is 0) {
				flare->st.st_size = stbuf->st_size;
				flare->st.st_blocks = stbuf->st_blocks;
				flare->st.st_blksize = stbuf->st_blksize;
				flare->st.st_nlink = stbuf->st_nlink;
				flare->st.st_atime = stbuf->st_atime;
				flare->st.st_ctime = stbuf->st_ctime;
				flare->st.st_mtime = stbuf->st_mtime;
				flare->st.st_ino = stbuf->st_ino;

				stbuf->st_dev = flare->st.st_dev;
				stbuf->st_mode = flare->st.st_mode;
				stbuf->st_uid = flare->st.st_uid;
				stbuf->st_gid = flare->st.st_gid;
				stbuf->st_rdev = flare->st.st_rdev;

				/*
				 * set flare type
				 */
				stbuf->st_mode &= ~S_IFMT;
				switch (flare->type) {
					case 'd': stbuf->st_mode |= S_IFDIR; break;
					case 'r': stbuf->st_mode |= S_IFREG; break;
					case 'l': stbuf->st_mode |= S_IFLNK; break;
					case 'c': stbuf->st_mode |= S_IFCHR; break;
					case 'b': stbuf->st_mode |= S_IFBLK; break;
					case 'p': stbuf->st_mode |= S_IFIFO; break;
					case 's': stbuf->st_mode |= S_IFSOCK; break;
				}
			} else {
				memcpy(stbuf, &(flare->st), sizeof(struct stat));
			}
			dbg(LOG_INFO, DEBUG_PFUSE, "GETATTR %s OK!", path);
			magma_dispose_flare(parent);
		}

		magma_dispose_flare(flare);
	}

	/*
	 * do a remote operation, if applies
	 */
	if ((!im_owner && !im_red_owner) || (im_red_owner && response.header.res is -1)) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
		magma_pktqs_getattr(socket, peer, uid, gid, path, &response);
		magma_close_client_connection(socket, peer);

	}

	errno = response.header.err_no;
	return (response.header.res);
}

int magma_stat(uid_t uid, gid_t gid, const char *path, struct stat *stbuf)
{
	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_stat() called with NULL or zero-length path");
		return (-1);
	}

	int res = magma_lstat(uid, gid, path, stbuf);

	if (res is -1) {
		return res;
	}

	if (S_ISLNK(stbuf->st_mode)) {
		char link_target[PATH_MAX];
		res = magma_readlink(uid, gid, path, link_target, PATH_MAX);

		if (res is -1) {
			return res;
		}

		res = magma_stat(uid, gid, link_target, stbuf);
		return res;
	}

	return res;
}

int magma_statfs(uid_t uid, gid_t gid, const char *path, struct statfs *statbuf)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_statfs() called with NULL or zero-length path");
		return (-1);
	}

	/*
	 * get owner and redundant owner of the path
	 */
	magma_volcano *owner = magma_route_path(path);
	magma_volcano *red_owner = owner->next ? owner->next : lava->first_node; // magma_redundant_route_path(path);

	/*
	 * guess if this node is owner or redundant owner
	 */
	gboolean im_owner = magma_compare_nodes(owner, &myself);
	gboolean im_red_owner = im_owner ? FALSE : magma_compare_nodes(red_owner, &myself);


	/*
	 * do the local operation, if applies
	 */
	if (im_owner || im_red_owner) {

		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "STATFS: Error allocating memory for flare");
		}

		memset(statbuf, 0, sizeof(struct statfs));
		response.header.res = statfs(flare->contents, statbuf);
		if (response.header.res is -1) {
			response.header.err_no = errno;
			dbg(LOG_ERR, DEBUG_PFUSE, "STATFS %s: %s", path, strerror(response.header.err_no));
		} else {
			dbg(LOG_INFO, DEBUG_PFUSE, "STATFS %s OK!", path);
		}

		magma_dispose_flare(flare);
	}

	/*
	 * do a remote operation, if applies
	 */
	if ((!im_owner && !im_red_owner) || (im_red_owner && response.header.res is -1)) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
		magma_pktqs_statfs(socket, peer, uid, gid, path, &response);
		magma_close_client_connection(socket, peer);

		memcpy(statbuf, &response.body.statfs.statbuf, sizeof response.body.statfs.statbuf);
	}

	errno = response.header.err_no;
	return response.header.res;
}

/* creating, deleting, updating, ... */
int magma_mknod(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, mode_t mode, dev_t rdev)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_mknod() called with NULL or zero-length path");
		return (-1);
	}

	/* check if mode contains flare type (regular, char, ...) */
	if (!(mode & S_IFMT)) {
		dbg(LOG_ERR, DEBUG_PFUSE, "magma_mknod() called with no flare type!");
		errno = EINVAL;
		return -1;
	}

	/* magma_mknod() is not allowed to create directories */
	if (mode & S_IFDIR) {
		dbg(LOG_ERR, DEBUG_PFUSE, "magma_mknod() requested to create a directory!");
		errno = EINVAL;
		return -1;
	}

	magma_volcano *owner = magma_route_path(path);
	if (!magma_compare_nodes(owner, &myself) && ttl > MAGMA_TERMINAL_TTL) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);

		magma_pktqs_mknod(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, mode, rdev, path, &response);
		magma_close_client_connection(socket, peer);

	} else {

		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_INFO, DEBUG_PFUSE, "MKNOD %s: %s", path, strerror(response.header.err_no));
		} else if (flare->type) {
			magma_dispose_flare(flare);
			response.header.res = -1;
			response.header.err_no = EEXIST;
			dbg(LOG_ERR, DEBUG_PFUSE, "MKNOD %s: %s", path, strerror(response.header.err_no));
		} else {
			uint8_t parent_perm = 0;
			magma_flare_t *parent = magma_search_or_create(flare->parent_path);
			if (!parent) {
				magma_dispose_flare(flare);
				response.header.res = -1;
				response.header.err_no = ENOMEM;
				dbg(LOG_INFO, DEBUG_PFUSE, "MKNOD %s: %s", path, strerror(response.header.err_no));
			} else if ((parent_perm = magma_check_permission(parent, uid, gid, MAGMA_OPERATION_R|MAGMA_OPERATION_W)) != 0) {
				magma_dispose_flare(parent);
				magma_dispose_flare(flare);
				dbg(LOG_ERR, DEBUG_PFUSE, "MKNOD operation not permitted to %d.%d", uid, gid);
				magma_explain_permission(parent_perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else  {
				magma_dispose_flare(parent);

				/* upcast this flare */
			         if ( S_ISREG(mode)) { magma_cast_to_file(flare);     }
				else if ( S_ISCHR(mode)) { magma_cast_to_chardev(flare);  }
				else if ( S_ISBLK(mode)) { magma_cast_to_blockdev(flare); }
				else if (S_ISFIFO(mode)) { magma_cast_to_fifo(flare);     }
				else if ( S_ISLNK(mode)) { magma_cast_to_symlink(flare);  }

				magma_touch_flare(flare, MAGMA_TOUCH_ATIME|MAGMA_TOUCH_MTIME|MAGMA_TOUCH_CTIME, 0, 0);
	
				/* setting permissions */
				flare->st.st_mode |= mode; /* |S_IRUSR|S_IWUSR; */
	
				/* setting major and minor */
				flare->st.st_rdev = rdev;
	
				/* setting ownership */
				flare->st.st_uid = uid;
				flare->st.st_gid = gid;

				/* save flare to disk */
				response.header.res = magma_save_flare(flare, TRUE);

				if (response.header.res is -1) {
					magma_dispose_flare(flare);
					response.header.err_no = errno;
					dbg(LOG_ERR, DEBUG_PFUSE, "MKNOD error saving flare %s: %s", path, strerror(response.header.err_no));
				} else {
					magma_flare_sql_save(flare);
					magma_dispose_flare(flare);

					/* add flare to parent */
					magma_whole_add_flare_to_parent(path);

					response.header.err_no = 0;
					dbg(LOG_INFO, DEBUG_PFUSE, "MKNOD %s OK!", path);
				}
			}
		}	
	}

	errno = response.header.err_no;
	return response.header.res;
}

int magma_unlink(uid_t uid, gid_t gid, magma_ttl ttl, const char *path)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_unlink() called with NULL or zero-length path");
		return (-1);
	}

	magma_volcano *owner = magma_route_path(path);
	if (!magma_compare_nodes(owner, &myself) && ttl > MAGMA_DEFAULT_TTL) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
		magma_pktqs_unlink(socket, peer, ttl, uid, gid, path, &response);
		magma_close_client_connection(socket, peer);

	} else {

		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_INFO, DEBUG_PFUSE, "UNLINK %s: %s", path, strerror(response.header.err_no));
		} else if (!flare->type) {
			magma_dispose_flare(flare);
			response.header.res = -1;
			response.header.err_no = magma_check_flare(flare) ? EACCES : ENOENT;
			dbg(LOG_INFO, DEBUG_PFUSE, "UNLINK %s: %s", path, strerror(response.header.err_no));
		} else if (magma_isdir(flare)) {
			magma_dispose_flare(flare);
			response.header.res = -1;
			response.header.err_no = EACCES;
			dbg(LOG_INFO, DEBUG_PFUSE, "UNLINK on a directory is illegal!");
		} else {
			uint8_t parent_perm = 0;
			magma_flare_t *parent = magma_search_or_create(flare->parent_path);
			if (!parent) {
				magma_dispose_flare(flare);
				response.header.res = -1;
				response.header.err_no = ENOMEM;
				dbg(LOG_INFO, DEBUG_PFUSE, "UNLINK %s: %s", path, strerror(response.header.err_no));
			} else if ((parent_perm = magma_check_permission(parent, uid, gid, MAGMA_OPERATION_W)) != 0) {
				magma_dispose_flare(flare);
				magma_dispose_flare(parent);
				dbg(LOG_ERR, DEBUG_PFUSE, "UNLINK operation not permitted to %d.%d", uid, gid);
				magma_explain_permission(parent_perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else {
				magma_dispose_flare(parent);
				magma_erase_flare_from_disk(flare);
				magma_dispose_flare(flare);
				magma_whole_remove_flare_from_parent(path);

				response.header.res = 0;
				dbg(LOG_INFO, DEBUG_PFUSE, "UNLINK OK!");
			}
		}
	}

	errno = response.header.err_no;
	return response.header.res;
}

int magma_truncate(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, off_t offset)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_truncate() called with NULL or zero-length path");
		return (-1);
	}

	magma_volcano *owner = magma_route_path(path);

	if (!magma_compare_nodes(owner, &myself) && ttl > MAGMA_TERMINAL_TTL) {
		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);

		magma_pktqs_truncate(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, path, offset, &response);
		magma_close_client_connection(socket, peer);

	} else {

		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "TRUNCATE: Error allocating memory for flare");
		} else if (!flare->type) {
			magma_dispose_flare(flare);
			response.header.res = -1;
			response.header.err_no = magma_check_flare(flare) ? EIO : ENOENT;
			dbg(LOG_ERR, DEBUG_PFUSE, "TRUNCATE %s: %s", path, strerror(response.header.err_no));
		} else if (magma_isdir(flare)) {
			magma_dispose_flare(flare);
			response.header.res = -1;
			response.header.err_no = EISDIR;
			dbg(LOG_ERR, DEBUG_PFUSE, "TRUNCATE %s: is a directory", path);
		} else {
			uint8_t perm = 0;
			if ((perm = magma_check_permission(flare, uid, gid, MAGMA_OPERATION_W)) != 0) {
				magma_dispose_flare(flare);
				dbg(LOG_INFO, DEBUG_PFUSE, "TRUNCATE operation not permitted");
				magma_explain_permission(perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else {
				/* perform truncate on contents file */
				response.header.res = truncate(flare->contents, offset);
				if (response.header.res is -1) {
					response.header.err_no = errno;
					dbg(LOG_ERR, DEBUG_PFUSE, "TRUNCATE %s: %s", path, strerror(response.header.err_no));
				} else {
					magma_touch_flare(flare, MAGMA_TOUCH_MTIME, 0, 0);
					magma_flare_update_stat(flare);
					magma_dispose_flare(flare);
					dbg(LOG_INFO, DEBUG_PFUSE, "TRUNCATE OK!");
				}
			}
		}
	}

	errno = response.header.err_no;
	return response.header.res;
}

int magma_utime(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, time_t atime, time_t mtime)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_utime() called with NULL or zero-length path");
		return (-1);
	}

	magma_volcano *owner = magma_route_path(path);

	if (!magma_compare_nodes(owner, &myself) && ttl > MAGMA_DEFAULT_TTL) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);

		magma_pktqs_utime(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, atime, mtime, path, &response);
		magma_close_client_connection(socket, peer);

	} else {

		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "UTIME: Error allocating memory for flare");
		} else if (!flare->type) {
			magma_dispose_flare(flare);
			response.header.res = -1;
			response.header.err_no = ENOENT;
			dbg(LOG_ERR, DEBUG_PFUSE, "UTIME %s: %s", path, strerror(response.header.err_no));
		} else {
			uint8_t perm = 0;
			if ((perm = magma_check_permission(flare, uid, gid, MAGMA_OPERATION_W)) != 0) {
				magma_dispose_flare(flare);
				dbg(LOG_INFO, DEBUG_PFUSE, "UTIME operation not permitted");
				magma_explain_permission(perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else {
				magma_touch_flare(flare, MAGMA_TOUCH_ATIME|MAGMA_TOUCH_MTIME, atime, mtime);
				magma_save_flare(flare, FALSE);
				magma_dispose_flare(flare);
				dbg(LOG_INFO, DEBUG_FLARE, "UTIME %s OK!", path);
			}
		}
	}

	errno = response.header.err_no;
	return response.header.res;
}

//
// TODO: code this function! Now we can do it!
//
int magma_rename(uid_t uid, gid_t gid, const char *from, const char *to)
{
	int res = 0, server_errno = 0;
	magma_flare_response response;

	(void) uid;
	(void) gid;
	(void) from;
	(void) to;
	(void) response;

	res = -1;
	server_errno = errno = EXDEV;

	// suppress gcc warnings
	(void) server_errno;

	return res;
}

int magma_chmod(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, mode_t mode)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_chmod() called with NULL or zero-length path");
		return (-1);
	}

	magma_volcano *owner = magma_route_path(path);
	if (!magma_compare_nodes(owner, &myself) && ttl > MAGMA_TERMINAL_TTL) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);

		magma_pktqs_chmod(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, path, mode, &response);
		magma_close_client_connection(socket, peer);

	} else {
		
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "CHMOD: Error allocating memory for flare");
		} else {
			uint8_t parent_perm = 0;
			magma_flare_t *parent = magma_search_or_create(flare->parent_path);
			if (!parent) {
				magma_dispose_flare(flare);
				response.header.res = -1;
				response.header.err_no = ENOMEM;
				dbg(LOG_INFO, DEBUG_PFUSE, "CHMOD %s: %s", path, strerror(response.header.err_no));
			} else if ((parent_perm = magma_check_permission(parent, uid, gid, MAGMA_OPERATION_X)) != 0) {
				magma_dispose_flare(flare);
				magma_dispose_flare(parent);
				dbg(LOG_ERR, DEBUG_PFUSE, "CHMOD operation not permitted to %d.%d", uid, gid);
				magma_explain_permission(parent_perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else {
				/*
				 * do the real chmod()
				 */
				response.header.res = chmod(flare->contents, mode);
				if (response.header.res isNot -1) {
					magma_flare_update_stat(flare);
					magma_dispose_flare(flare);
					dbg(LOG_INFO, DEBUG_PFUSE, "CHMOD %s OK!", path);
				}
			}
		}
	}

	errno = response.header.err_no;
	return response.header.res;
}

int magma_chown(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, uid_t newuid, gid_t newgid)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_chown() called with NULL or zero-length path");
		return (-1);
	}

	if (uid != 0) {
		dbg(LOG_INFO, DEBUG_FLARE, "Only root can chown");
		errno = EPERM;
		return -1;
	}

	magma_volcano *owner = magma_route_path(path);

	if (!magma_compare_nodes(owner, &myself) && ttl > MAGMA_DEFAULT_TTL) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);

		magma_pktqs_chown(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, path, newuid, newgid, &response);
		magma_close_client_connection(socket, peer);

	} else {

		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "CHOWN: Error allocating memory for flare");
		} else if (!flare->type) {
			magma_dispose_flare(flare);
			response.header.res = -1;
			response.header.err_no = magma_check_flare(flare) ? EACCES : ENOENT;
			dbg(LOG_ERR, DEBUG_PFUSE, "CHOWN %s: %s", path, strerror(response.header.err_no));
		} else {
			uint8_t parent_perm = 0;
			magma_flare_t *parent = magma_search_or_create(flare->parent_path);
			if (!parent) {
				magma_dispose_flare(flare);
				response.header.res = -1;
				response.header.err_no = ENOMEM;
				dbg(LOG_INFO, DEBUG_PFUSE, "UNLINK %s: %s", path, strerror(response.header.err_no));
			} else if ((parent_perm = magma_check_permission(parent, uid, gid, MAGMA_OPERATION_X)) != 0) {
				magma_dispose_flare(flare);
				magma_dispose_flare(parent);
				dbg(LOG_ERR, DEBUG_PFUSE, "UNLINK operation not permitted to %d.%d", uid, gid);
				magma_explain_permission(parent_perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else {
				flare->st.st_uid = newuid;
				flare->st.st_gid = newgid;

				response.header.res = magma_save_flare(flare, FALSE);
				if ( response.header.res is -1 ) {
					response.header.err_no = errno;
					dbg(LOG_ERR, DEBUG_PFUSE, "CHOWN %s: %s", path, strerror(response.header.err_no));
				} else {
					dbg(LOG_INFO, DEBUG_PFUSE, "CHOWN %s OK!", path);
				}
				magma_dispose_flare(flare);
				magma_dispose_flare(parent);
			}
		}
	}

	errno = response.header.err_no;
	return response.header.res;
}

/* working with files */
int magma_open(uid_t uid, gid_t gid, const char *path, int flags)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_open() called with NULL or zero-length path");
		return (-1);
	}

	/*
	 * get owner and redundant owner of the path
	 */
	magma_volcano *owner = magma_route_path(path);
	magma_volcano *red_owner = owner->next ? owner->next : lava->first_node; // magma_redundant_route_path(path);

	/*
	 * guess if this node is owner or redundant owner
	 */
	gboolean im_owner = magma_compare_nodes(owner, &myself);
	gboolean im_red_owner = im_owner ? FALSE : magma_compare_nodes(red_owner, &myself);

	/*
	 * do the local operation, if applies
	 */
	if (im_owner || im_red_owner) {

		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "OPEN(%s): no space for flare allocation", path);
		} else if (!flare->type) {
			response.header.res = -1;
			response.header.err_no = errno;
			dbg(LOG_ERR, DEBUG_PFUSE, "OPEN(%s): flare not upcasted", path);
		} else if (!magma_check_flare(flare)) {
			response.header.res = -1;
			response.header.err_no = ENOENT;
			dbg(LOG_ERR, DEBUG_PFUSE, "OPEN(%s): flare is not on disk", path);
		} else if (magma_isdir(flare)) {
			response.header.res = -1;
			response.header.err_no = EISDIR;
			dbg(LOG_ERR, DEBUG_PFUSE, "OPEN(%s): flare is a directory", path);
		} else {
			uint8_t perm = 0;
			if (flags & (O_RDONLY|O_RDWR)) {
				perm |= MAGMA_OPERATION_R;
			}
			if (flags & (O_WRONLY|O_RDWR)) {
				perm |= MAGMA_OPERATION_W;
			}
			if ((perm = magma_check_permission(flare, uid, gid, perm)) != 0) {
				dbg(LOG_INFO, DEBUG_PFUSE, "OPEN operation not permitted");
				magma_explain_permission(perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else {
				int contentsfd = magma_open_flare_contents(flare);
				if (contentsfd != -1) {
					dbg(LOG_INFO, DEBUG_PFUSE, "OPEN %s OK!", path);
					close(contentsfd);
				} else {
					response.header.err_no = errno;
					response.header.res = -1;
					dbg(LOG_ERR, DEBUG_PFUSE, "OPEN %s: %s", path, strerror(response.header.err_no));
				}
			}
		}
		magma_dispose_flare(flare);
	}

	/*
	 * do a remote operation, if applies
	 */
	if ((!im_owner && !im_red_owner) || (im_red_owner && response.header.res is -1)) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);

		magma_pktqs_open(socket, peer, uid, gid, flags, path, &response);
		magma_close_client_connection(socket, peer);

	}

	errno = response.header.err_no;
	return response.header.res;
}

int magma_read(uid_t uid, gid_t gid, const char *path, size_t size, off_t offset, char *buf)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_read() called with NULL or zero-length path");
		return (-1);
	}

	/*
	 * get owner and redundant owner of the path
	 */
	magma_volcano *owner = magma_route_path(path);
	magma_volcano *red_owner = owner->next ? owner->next : lava->first_node; // magma_redundant_route_path(path);

	/*
	 * guess if this node is owner or redundant owner
	 */
	gboolean im_owner = magma_compare_nodes(owner, &myself);
	gboolean im_red_owner = im_owner ? FALSE : magma_compare_nodes(red_owner, &myself);

	/*
	 * do the local operation, if applies
	 */
	if (im_owner || im_red_owner) {
	
		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "READ(%s): Error allocating memory for flare", path);
		} else if (magma_isdir(flare)) {
			response.header.res = -1;
			response.header.err_no = EINVAL;
			dbg(LOG_ERR, DEBUG_PFUSE, "READ(%s): flare is a directory", path);
		} else if (!magma_check_flare(flare)) {
			response.header.res = -1;
			response.header.err_no = ENOENT;
			dbg(LOG_ERR, DEBUG_PFUSE, "READ(%s): flare is not on disk", path);
		} else {
			uint8_t perm = 0;
			if ((perm = magma_check_permission(flare, uid, gid, MAGMA_OPERATION_R)) != 0) {
				dbg(LOG_INFO, DEBUG_PFUSE, "READ operation not permitted");
				magma_explain_permission(perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else {
				int fd = magma_open_flare_contents(flare);
				if (fd is -1) {
					response.header.err_no = errno;
					response.header.res = -1;
					dbg(LOG_ERR, DEBUG_PFUSE, "READ can't open() %s: %s", path, strerror(response.header.err_no));
				} else {
					memset(buf, 0, size);
					response.header.res = pread(fd, buf, size, offset);
					if ( response.header.res is -1 ) {
						response.header.err_no = errno;
						dbg(LOG_ERR, DEBUG_PFUSE, "READ: can't pread() %s: %s", path, strerror(response.header.err_no));
					} else {
						dbg(LOG_INFO, DEBUG_PFUSE, "READ: pread() returned %d/%lu bytes from %s", response.header.res, size, path);
					}
					close(fd);
				}
			}
		}
		magma_dispose_flare(flare);
	}

	/*
	 * do a remote operation, if applies
	 */
	if ((!im_owner && !im_red_owner) || (im_red_owner && response.header.res is -1)) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
		magma_pktqs_read(socket, peer, uid, gid, size, offset, path, &response);
		magma_close_client_connection(socket, peer);

	}

	errno = response.header.err_no;
	return response.header.res;
}

int magma_write(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, size_t size, off_t offset, char *buf)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_write() called with NULL or zero-length path");
		return (-1);
	}

	magma_volcano *owner = magma_route_path(path);

	if (!magma_compare_nodes(owner, &myself) && ttl > MAGMA_TERMINAL_TTL) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);

		magma_pktqs_write(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, size, offset, path, buf, &response);
		magma_close_client_connection(socket, peer);
	
	} else {

		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "WRITE: Error allocating memory for flare");
		} else if (flare->type && magma_isdir(flare)) {
			response.header.res = -1;
			response.header.err_no = EISDIR;
			dbg(LOG_ERR, DEBUG_PFUSE, "WRITE %s: is a directory", path);
		} else {
			if (!flare->type) {
				dbg(LOG_ERR, DEBUG_PFUSE, "WRITE: flare %s wasn't casted", path);
				magma_cast_to_file(flare);
			}

			uint8_t perm = 0;
			if ((perm = magma_check_permission(flare, uid, gid, MAGMA_OPERATION_W)) != 0) {
				dbg(LOG_INFO, DEBUG_PFUSE, "WRITE operation not permitted");
				magma_explain_permission(perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else {

				int contentsfd = magma_open_flare_contents(flare);
				if (contentsfd != -1) {
					response.header.res = pwrite(contentsfd, buf, size, offset);
					if ( response.header.res is -1 ) {
						response.header.err_no = errno;
						dbg(LOG_ERR, DEBUG_PFUSE, "WRITE can't pwrite() %s: %s", path, strerror(response.header.err_no));
					} else {
						magma_touch_flare(flare, MAGMA_TOUCH_MTIME, 0, 0);
						dbg(LOG_INFO, DEBUG_PFUSE, "WRITE %s OK! (%d bytes)", path, response.header.res);
					}
					close(contentsfd);
	
				} else {
					response.header.err_no = errno;
					response.header.res = -1;
				}
			}
		}
		magma_dispose_flare(flare);
	}

	errno = response.header.err_no;
	return response.header.res;
}

int magma_mkdir(uid_t uid, gid_t gid, magma_ttl ttl, const char *path, mode_t mode)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_mkdir() called with NULL or zero-length path");
		return (-1);
	}

	magma_volcano *owner = magma_route_path(path);
	if (!magma_compare_nodes(owner, &myself) && ttl > MAGMA_TERMINAL_TTL) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
		dbg(LOG_INFO, DEBUG_PFUSE, "Forwarding mkdir(%s) to %s", path, owner->node_name);
		magma_pktqs_mkdir(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, mode, path, &response);
		magma_close_client_connection(socket, peer);

	} else {
		
		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_INFO, DEBUG_PFUSE, "MKDIR %s: %s", path, strerror(response.header.err_no));
		} else if (flare->type) {
			magma_dispose_flare(flare);
			response.header.res = -1;
			response.header.err_no = EEXIST;
			dbg(LOG_INFO, DEBUG_DIR, "MKDIR flare %s already exists", flare->path);
		} else {
			uint8_t parent_perm = 0;
			magma_flare_t *parent = magma_search_or_create(flare->parent_path);
			if (!parent) {
				magma_dispose_flare(flare);
				response.header.res = -1;
				response.header.err_no = ENOMEM;
				dbg(LOG_INFO, DEBUG_PFUSE, "MKDIR %s: %s", path, strerror(response.header.err_no));
			} else if ((parent_perm = magma_check_permission(parent, uid, gid, MAGMA_OPERATION_R|MAGMA_OPERATION_W)) != 0) {
				magma_dispose_flare(flare);
				magma_dispose_flare(parent);
				dbg(LOG_ERR, DEBUG_PFUSE, "MKDIR operation not permitted to %d.%d", uid, gid);
				magma_explain_permission(parent_perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else {
				magma_dispose_flare(parent);

				/* save requested permission mode */
				flare->st.st_mode |= mode; /* |S_IRUSR|S_IWUSR; */
	
				/* setting ownership */
				flare->st.st_uid = uid;
				flare->st.st_gid = gid;
	
				/* turn flare into a directory */
				magma_cast_to_dir(flare);

				/* save flare to disk */
				response.header.res = magma_save_flare(flare, TRUE);
				magma_dispose_flare(flare);
	
				if (response.header.res is -1) {
					response.header.err_no = errno;
					dbg(LOG_ERR, DEBUG_PFUSE, "MKDIR error saving dir %s: %s", path, strerror(errno));
				} else {
					/* add flare to parent */
					magma_whole_add_flare_to_parent(path);

					response.header.err_no = 0;
					dbg(LOG_INFO, DEBUG_PFUSE, "MKDIR %s OK!", path);
				}
			}
		}
	}

	errno = response.header.err_no;
	return response.header.res;
}

int magma_rmdir(uid_t uid, gid_t gid, magma_ttl ttl, const char *path)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_rmdir() called with NULL or zero-length path");
		return (-1);
	}

	magma_volcano *owner = magma_route_path(path);
	if (!magma_compare_nodes(owner, &myself) && ttl > MAGMA_DEFAULT_TTL) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);

		magma_pktqs_rmdir(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, path, &response);
		magma_close_client_connection(socket, peer);

	} else {

		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "RMDIR: Error allocating memory for flare");
		} else if (!flare->type) {
			response.header.res = -1;
			if (magma_check_flare(flare)) {
				response.header.err_no = EACCES;
				dbg(LOG_ERR, DEBUG_PFUSE, "RMDIR: Permission denied on %s", path);
			} else {
				response.header.err_no = ENOENT;
				dbg(LOG_ERR, DEBUG_PFUSE, "RMDIR: Directory %s does not exists", path);
			}
			magma_dispose_flare(flare);
		} else if (!magma_isdir(flare)) {
			magma_dispose_flare(flare);
			response.header.res = -1;
			response.header.err_no = ENOTDIR;
			dbg(LOG_ERR, DEBUG_PFUSE, "RMDIR: %s is not a directory", path);
		} else if (!magma_dir_is_empty(flare)) {
			magma_dispose_flare(flare);
			response.header.res = -1;
			response.header.err_no = ENOTEMPTY;
			dbg(LOG_ERR, DEBUG_PFUSE, "RMDIR: Directory %s is not empty", path);
		} else  {
			uint8_t parent_perm = 0;
			magma_flare_t *parent = magma_search_or_create(flare->parent_path);
			if (!parent) {
				magma_dispose_flare(flare);
				response.header.res = -1;
				response.header.err_no = ENOMEM;
				dbg(LOG_INFO, DEBUG_PFUSE, "RMDIR %s: %s", path, strerror(response.header.err_no));
			} else if ((parent_perm = magma_check_permission(parent, uid, gid, MAGMA_OPERATION_W)) != 0) {
				magma_dispose_flare(flare);
				magma_dispose_flare(parent);
				dbg(LOG_ERR, DEBUG_PFUSE, "RMDIR operation not permitted to %d.%d", uid, gid);
				magma_explain_permission(parent_perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else {
				magma_dispose_flare(flare);
				magma_dispose_flare(parent);
				magma_whole_remove_flare_from_parent(path);
				response.header.res = 0;
				dbg(LOG_ERR, DEBUG_PFUSE, "RMDIR: OK!");
			}
		}
	}

	errno = response.header.err_no;
	return response.header.res;
}


/* working with links */
int magma_readlink(uid_t uid, gid_t gid, const char *path, char *buf, size_t size)
{
	magma_flare_response response;

	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_readlink() called with NULL or zero-length path");
		return (-1);
	}

	/*
	 * get owner and redundant owner of the path
	 */
	magma_volcano *owner = magma_route_path(path);
	magma_volcano *red_owner = owner->next ? owner->next : lava->first_node; // magma_redundant_route_path(path);

	/*
	 * guess if this node is owner or redundant owner
	 */
	gboolean im_owner = magma_compare_nodes(owner, &myself);
	gboolean im_red_owner = im_owner ? FALSE : magma_compare_nodes(red_owner, &myself);

	/*
	 * do the local operation, if applies
	 */
	if (im_owner || im_red_owner) {
	
		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(path);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "READLINK(%s): Error allocating memory for flare", path);
		} else if (!flare->type) {
			response.header.res = -1;
			response.header.err_no = ENOENT;
			dbg(LOG_ERR, DEBUG_PFUSE, "READLINK(%s): flare not upcasted", path);
		} else if (!magma_check_flare(flare)) {
			response.header.res = -1;
			response.header.err_no = ENOENT;
			dbg(LOG_ERR, DEBUG_PFUSE, "READLINK(%s): flare is not on disk", path);
		} else if (!magma_islnk(flare)) {
			response.header.res = -1;
			response.header.err_no = EINVAL;
			dbg(LOG_ERR, DEBUG_PFUSE, "READLINK(%s): %s", path, strerror(response.header.err_no));
		} else  {
			memset(buf, 0, size);

			GError *error = NULL;
			gchar *contents;
			gsize length;
			g_file_get_contents(flare->contents, &contents, &length, &error);
			if (error) {
				dbg(LOG_ERR, DEBUG_PFUSE, "Error reading symlink %s: %s", flare->path, error->message);
				g_error_free(error);
			} else {
				memcpy(buf, contents, length);
				dbg(LOG_INFO, DEBUG_PFUSE, "READLINK %s -> %s OK!", path, buf);
			}
		}
		magma_dispose_flare(flare);
	}
	
	/*
	 * do a remote operation, if applies
	 */
	if ((!im_owner && !im_red_owner) || (im_red_owner && response.header.res is -1)) {

		gchar buffer[MAGMA_MAX_BUFFER_SIZE];
		memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);
		magma_pktqs_readlink(socket, peer, uid, gid, path, &response);
		magma_close_client_connection(socket, peer);

		memcpy(buf, response.body.readlink.path, size);

	}

	errno = response.header.err_no;
	return response.header.res;
}

int magma_symlink(uid_t uid, gid_t gid, magma_ttl ttl, const char *from, const char *to)
{
	magma_flare_response response;

	if (!from || !strlen(from)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_symlink() called with NULL or zero-length from path");
		return (-1);
	}

	if (!to || !strlen(to)) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_symlink() called with NULL or zero-length to path");
		return (-1);
	}

	magma_volcano *owner = magma_route_path(to);
	if (!magma_compare_nodes(owner, &myself) && ttl > MAGMA_TERMINAL_TTL) {

		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);

		magma_pktqs_symlink(socket, peer, MAGMA_DEFAULT_TTL, uid, gid, from, to, &response);
		magma_close_client_connection(socket, peer);
	
	} else {

		/* get flare from cache or from disk */
		magma_flare_t *flare = magma_search_or_create(to);
		if (!flare) {
			response.header.res = -1;
			response.header.err_no = ENOMEM;
			dbg(LOG_ERR, DEBUG_PFUSE, "SYMLINK: Error allocating memory for flare");
		} else if (flare->type) {
			magma_dispose_flare(flare);
			response.header.res = -1;
			response.header.err_no = EEXIST;
			dbg(LOG_ERR, DEBUG_PFUSE, "SYMLINK flare %s already exists", flare->path);
		} else {
			uint8_t parent_perm = 0;
			magma_flare_t *parent = magma_search_or_create(flare->parent_path);
			if (!parent) {
				magma_dispose_flare(flare);
				response.header.res = -1;
				response.header.err_no = ENOMEM;
				dbg(LOG_INFO, DEBUG_PFUSE, "UNLINK %s: %s", to, strerror(response.header.err_no));
			} else if ((parent_perm = magma_check_permission(parent, uid, gid, MAGMA_OPERATION_W)) != 0) {
				magma_dispose_flare(flare);
				magma_dispose_flare(parent);
				dbg(LOG_ERR, DEBUG_PFUSE, "UNLINK operation not permitted to %d.%d", uid, gid);
				magma_explain_permission(parent_perm);
				response.header.res = -1;
				response.header.err_no = EACCES;
			} else {
				magma_dispose_flare(parent);

				/* upcast flare to symbolic link */
				magma_cast_to_symlink(flare);

				if (!flare->type) {
					magma_dispose_flare(flare);
					response.header.res = -1;
					response.header.err_no = ENOMEM;
					dbg(LOG_ERR, DEBUG_PFUSE, "SYMLINK error upcasting flare: %s", strerror(response.header.err_no));
				} else {
					/* updating flare timestamps */
					magma_touch_flare(flare, MAGMA_TOUCH_ATIME|MAGMA_TOUCH_MTIME|MAGMA_TOUCH_CTIME, 0, 0);
			
					/* apply symlink standard mode */
					flare->st.st_mode |= S_IRWXU|S_IRWXG|S_IRWXO;
			
					/* setting ownership */
					flare->st.st_uid = 0;
					flare->st.st_gid = 0;

					/* save link contents to disk */
					GError *error = NULL;
					g_file_set_contents(flare->contents, from, strlen(from), &error);
					if (error) {
						dbg(LOG_ERR, DEBUG_PFUSE, "Error setting symlink %s to %s: %s", from, to, error->message);
						g_error_free(error);
					}
	
					if (!magma_save_flare(flare, TRUE)) {
						magma_dispose_flare(flare);
						response.header.res = -1;
						response.header.err_no = EIO;
						dbg(LOG_ERR, DEBUG_PFUSE, "SYMLINK error saving to disk: %s", strerror(response.header.err_no));
					} else {
						magma_flare_sql_save(flare);
						magma_dispose_flare(flare);

						/* add flare to parent */
						magma_whole_add_flare_to_parent(to);

						response.header.err_no = 0;
						dbg(LOG_INFO, DEBUG_PFUSE, "MKNOD %s OK!", to);
					}
				}
			}
		}
	}

	errno = response.header.err_no;
	return response.header.res;
}

int magma_link(uid_t uid, gid_t gid, magma_ttl ttl, const char *from, const char *to)
{
	return magma_symlink(uid, gid, ttl, from, to);
}

/**
 * sync to disk a cache node. if remove is true, node is also deleted
 * from volatile memory
 *
 * @param key the node key, passed by g_tree_foreach(), not used
 * @param flare the node of the cache to sync
 * @param free_flare if true, the node is also destroyed
 */
gboolean magma_save_cache_node(guint *key, magma_flare_t *flare, gboolean *free_flare)
{
	(void) key;

	if (!flare) return FALSE;

	dbg(LOG_ERR, DEBUG_PFUSE, "Saving flare %s", flare->path);

	/* save this node */
	magma_save_flare(flare, FALSE);

	/* this is just to check for memory leaks with mtrace */
	if (*free_flare && magma_remove_from_cache(flare)) {
		magma_dispose_flare(flare);
	}

	return FALSE;
}

void magma_sync_cache()
{
	int free_flare = 0;
	g_tree_foreach(magma_cache_gtree, (GTraverseFunc) magma_save_cache_node, &free_flare);
}

void magma_flush_cache()
{
	int free_flare = 1;
	g_tree_foreach(magma_cache_gtree, (GTraverseFunc) magma_save_cache_node, &free_flare);
}

/**
 * open a magma directory returning a magma_DIR_t directory
 * pointer. set offset to zero.
 *
 * @param path the path to be opened
 * @param uid
 * @param gid
 * @return a magma_DIR_t handler or NULL on error
 */
magma_DIR_t *magma_opendir(uid_t uid, gid_t gid, const char *path)
{
	if (!path || !strlen(path)) {
		dbg(LOG_ERR, DEBUG_DIR, "magma_opendir() called on NULL of zero length path");
		return (NULL);
	}

	dbg(LOG_INFO, DEBUG_DIR, "magma_opendir(%s)", path);

	magma_DIR_t *dirp = g_new0(magma_DIR_t, 1);
	if (!dirp) {
		dbg(LOG_ERR, DEBUG_DIR, "magma_opendir(): error allocating memory for magma_DIR_t");
		return (NULL);
	}

	magma_volcano *owner = magma_route_path(path);
	magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;

	if (magma_compare_nodes(owner, &myself) || magma_compare_nodes(red_owner, &myself)) {
		/*
		 * open a local directory
		 */
		dirp->dir = magma_search_or_create(path);
		if (!dirp->dir) {
			dbg(LOG_ERR, DEBUG_DIR, "magma_opendir(%s) can't get corresponding flare", path);
			g_free(dirp);
			return (NULL);
		}

		magma_flare_read_lock(dirp->dir);

		GError *error = NULL;
		gboolean ok = g_file_get_contents(dirp->dir->contents, &dirp->content, &dirp->length, &error);

		magma_flare_read_unlock(dirp->dir);

		if (!ok) {
			if (error) {
				dbg(LOG_ERR, DEBUG_DIR, "Error loading %s dir: %s", dirp->dir->path, error->message);
				g_error_free(error);
			} else {
				dbg(LOG_ERR, DEBUG_DIR, "Error loading %s dir", dirp->dir->path);
			}

			magma_dispose_flare(dirp->dir);
			g_free(dirp);
			return (NULL);
		}

	} else {
		/*
		 * open a remote directory
		 */
		magma_flare_response response;
		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(owner->ip_addr, owner->port, &peer);

		magma_size received_size = 0;
		magma_offset offset = 0;

		dirp->content = NULL;
		do {
			magma_pktqs_f_opendir(socket, peer, uid, gid, path, offset, &response);

			/*
			 * check network status
			 */
			if ((G_IO_STATUS_NORMAL != response.header.status) || (response.header.res is -1)) break;

			/*
			 * allocate the buffer, if NULL
			 */
			if (!dirp->content) dirp->content = g_new0(gchar, response.body.f_opendir.size);

			/*
			 * fill the buffer
			 */
			if (!dirp->content) {
				dbg(LOG_ERR, DEBUG_DIR, "magma_opendir(%s): Error allocating memory", path);
				break;
			}

			if (offset >= response.body.f_opendir.size) {
				dbg(LOG_ERR, DEBUG_DIR, "magma_opendir(%s): Buffer overflow", path);
				break;
			}

			/*
			 * copy the buffer
			 */
			gchar *ptr = dirp->content + offset;
			memcpy(ptr, response.body.f_opendir.buffer, response.body.f_opendir.buffer_size);
			g_free(response.body.f_opendir.buffer);

			/*
			 * update received_size and offset
			 */
			received_size += response.body.f_opendir.buffer_size;
			offset = received_size + 1;

		} while (received_size < response.body.f_opendir.size);

		magma_close_client_connection(socket, peer);
	}

	return (dirp);
}

/**
 * return next entry in opened directory
 *
 * @param dirp a magma_DIR_t pointer
 */
gchar *magma_readdir(magma_DIR_t *dirp)
{
	if (!dirp || !(dirp->dir)) return (NULL);

	dbg(LOG_INFO, DEBUG_DIR, "magma_readdir() on %s", dirp->dir->path);

	/* locate the last valid byte */
	gchar *border = dirp->content + dirp->length - 1;

	/* point the first valid entry */
	gchar *entry = dirp->content + dirp->offset;

	/* skip all the '\0' */
	while ((entry < border) && (*entry is '\0')) {
		entry++;
		dirp->offset += 1;
	}

	/* get its length */
	int length = strlen(entry);

	/* move the offset pointer */
	dirp->offset += length + 1;

	/* return the entry only if it's not zero bytes long, otherwise return NULL */
	return (length ? entry : NULL);
}

/**
 * seek inside a directory (only if a local one)
 *
 * @param dirp magma_DIR_t pointer of opened directory
 * @param offset new off_t offset
 */
void magma_seekdir(magma_DIR_t *dirp, off_t offset)
{
	dirp->offset = offset;
}

/**
 * return current offset in opened directory
 *
 * @param dirp magma_DIR_t pointer of opened directory
 */
off_t magma_telldir(magma_DIR_t *dirp)
{
	return dirp->offset;
}

/**
 * close opened directory.
 *
 * @param dirp magma_DIR_t pointer of opened directory
 */
void magma_closedir(magma_DIR_t *dirp)
{
	g_free(dirp->content);
	g_free(dirp);
}

/**
 * Cleanup handler called when a SIGSEGV is received
 */
void magma_cleanup(int s)
{
	dbg(LOG_ERR, DEBUG_ERR, "Got signal %d from outer space!", s);
	dbg(LOG_ERR, DEBUG_ERR, "Cleaning everything and preparing to die...");

	/* let threads terminate safely */
	// TODO magma_join_all_threads();

	/* flush cache on disk */
	magma_flush_cache();

#ifdef MAGMA_ENABLE_NFS_INTERFACE
	/* unregister NFS services */
	unset_rpcservices();
#endif /* MAGMA_ENABLE_NFS_INTERFACE */

	switch (s) {
		case 0:
			dbg(LOG_INFO, DEBUG_BOOT, "Normal shutdown requested...");
			break;

		case SIGSEGV:
		case SIGBUS:
			magma_log_stacktrace(20);
			dbg(LOG_ERR, DEBUG_ERR, "Got Signal %d, cleaning and exiting now!", s);
			break;

		case SIGTERM:
		case SIGKILL:
		case SIGINT:
		default:
			dbg(LOG_ERR, DEBUG_ERR, "Got Signal %d, cleaning and exiting now!", s);
	}

	// shutdown(myself.socket, SHUT_RDWR);	
	exit(s);
}

/**
 * perform a clean server shutdown and exits main process
 */
void magma_clean_shutdown()
{
	magma_environment.state = magma_network_shutdown;

	/* let threads terminate safely */
	// TODO magma_join_all_threads();

	/* flush cache on disk */
	magma_flush_cache();

#ifdef MAGMA_ENABLE_NFS_INTERFACE
	/* unregister NFS services */
	unset_rpcservices();
#endif

	// shutdown(myself.socket, SHUT_RDWR);	
	exit(0);
}

/**
 * Send failure back to the request originating host
 *
 * @param socket a GSocket object
 * @param peer a GInetSocketAddress to save remote peer information
 * @transaction_id the ID of the failed transaction
 */
void magma_send_udp_failure(GSocket *socket, GSocketAddress *peer, int err_no, magma_transaction_id transaction_id) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, -1, err_no, transaction_id);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

// vim:ts=4:nocindent:autoindent
