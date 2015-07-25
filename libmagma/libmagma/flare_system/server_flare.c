/*
   MAGMA -- protocol_flare.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Implement protocol for client mounts. File name is a bit misleading.
   protocol_mount.c should be more appropriate. Probably will change
   in future release to encourage writing pure kernel filesystems.

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

// #define INCLUDE_FLARE_INTERNALS
#include "../magma.h"

/*
 * if TRUE a GThreadPool is used to queue the replication
 * operations
 */
#define MAGMA_DUPLICATION_THREAD_POOL FALSE

/*
 * if TRUE the MAGMA_OP_TYPE_TRANSMIT_KEY operation is used to
 * replicate every operation
 *
 * if FALSE, each PFUSE operation is replicated as received
 * using the PFUSE protocol
 */
#define MAGMA_REPLICA_ON_PNODE TRUE

GHashTable *magma_operation_cache;

#if MAGMA_DUPLICATION_THREAD_POOL
GThreadPool *magma_duplication_thread_pool;
#else
GAsyncQueue *magma_duplication_queue;
GThread *magma_duplication_thread;
#endif

/**
 * Stores a duplication operation
 */
typedef void (*magma_replica_function)(magma_flare_request *, magma_volcano *);
typedef struct {
	magma_flare_request *request;
	magma_volcano *node;
	magma_replica_function replica_function;
} magma_replica_ops;

#if MAGMA_DUPLICATION_THREAD_POOL

void magma_replica_pool_kernel(magma_replica_ops *op, gpointer data)
{
	(void) data;

	/*
	 * execute it
	 */
	op->replica_function(op->request, op->node);

	/*
	 * free the request resources
	 */
	if (MAGMA_OP_TYPE_WRITE == op->request->header.type) g_free(op->request->body.write.buffer);
	g_free(op->request);
	g_free(op->node);
	g_free(op);
}

#else

gpointer magma_replica_kernel(gpointer data)
{
	(void) data;

	while (1) {
		/*
		 * pop a replica request from the queue
		 */
		magma_replica_ops *op = g_async_queue_pop(magma_duplication_queue);

		/*
		 * execute it
		 */
		op->replica_function(op->request, op->node);

		/*
		 * free the request resources
		 */
		if (op->request->header.type is MAGMA_OP_TYPE_WRITE) g_free(op->request->body.write.buffer);
		g_free(op->request);
		g_free(op->node);
		g_free(op);
	}

	return (NULL);
}

#endif

void magma_init_server_flare()
{
	magma_operation_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

#if MAGMA_DUPLICATION_THREAD_POOL

	GError *error = NULL;
	magma_duplication_thread_pool = g_thread_pool_new((GFunc) magma_replica_pool_kernel, NULL, -1, FALSE, &error);

#else

	magma_duplication_queue = g_async_queue_new();
	magma_duplication_thread = g_thread_new("Replica ops", magma_replica_kernel, NULL);

#endif
}

/**
 * Build the key to store the result of an operation
 *
 * @param peer the remote peer sending the request
 * @param tid the transaction id
 * @return the string to be used as key
 */
gchar *magma_operation_cache_make_key(GSocketAddress *peer, magma_transaction_id tid)
{
	GInetAddress *addr = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(peer));
	gchar *addr_string = g_inet_address_to_string(addr);
	gchar *key = g_strdup_printf("%s:%d:%d",
		addr_string,
		g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(peer)),
		tid);
	g_free(addr_string);
	return (key);
}

/**
 * Stores the result of writing operations
 */
typedef struct {
	guint32 res;
	guint16 err_no;
} magma_operation_result;

/**
 * Saves an operation result in the cache
 *
 * @param key a key obtained magma_operation_cache_make_key()
 * @param res the result to be cached
 * @err_no the error value to be cached
 */
void magma_operation_cache_save_result(gchar *key, guint32 res, guint16 err_no)
{
	magma_operation_result *result = g_new0(magma_operation_result, 1);
	result->res = res;
	result->err_no = err_no;

	g_hash_table_insert(magma_operation_cache, key, result);
}

#if 0
/**
 * Compares two replica operations by destination IP address and
 * transaction id to order the queue
 */
gint magma_queue_sorter(const magma_replica_ops *op1, const magma_replica_ops *op2, gpointer data)
{
	(void) data;

	/*
	 * first compare by IP address
	 */
	int strings = g_strcmp0(op1->node->ip_addr, op2->node->ip_addr);
	if (0 != strings) return (strings);

	/*
	 * then compare by transaction id
	 */
	if (op1->request->header.transaction_id < op2->request->header.transaction_id) return (-1);
	if (op1->request->header.transaction_id > op2->request->header.transaction_id) return (1);
	return (0);
}
#endif

/**
 * Enqueues a replica operation
 *
 * @param replica_function a pointer to a replica function (typical name: magma_redundant_<operation>)
 * @param request a magma_flare_request to be duplicated
 * @param node the destination volcano
 * @return TRUE if enqueued, FALSE otherwise
 */
gboolean magma_queue_replica(magma_replica_function replica_function, magma_flare_request *request, magma_volcano *node)
{
#if 0 // MAGMA_REPLICA_ON_PNODE
	/*
	 * call the replica function
	 */
	replica_function(request, node);
	return (TRUE);

#else

	/*
	 * allocate the magma_replica_ops object
	 */
	magma_replica_ops *op = g_new0(magma_replica_ops, 1);
	if (!op) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error allocating magma_replica_ops: %s", strerror(errno));
		return (FALSE);
	}

	/*
	 * allocate the magma_flare_request object
	 */
	magma_flare_request *r = g_new0(magma_flare_request, 1);
	if (!r) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error allocating magma_flare_request duplicate: %s", strerror(errno));
		g_free(op);
		return (FALSE);
	}

	/*
	 * duplicate the flare request content
	 */
	memcpy(r, request, sizeof(magma_flare_request));

	/*
	 * special case: MAGMA_OP_TYPE_WRITE has a buffer that requires ad-hoc duplication
	 */
	if (r->header.type is MAGMA_OP_TYPE_WRITE) {
		r->body.write.buffer = g_new0(gchar, r->body.write.size);
		if (!r->body.write.buffer) {
			dbg(LOG_ERR, DEBUG_PFUSE, "Error duplicating write buffer");
			g_free(op);
			g_free(r);
			return (FALSE);
		}

		memcpy(r->body.write.buffer, request->body.write.buffer, r->body.write.size);
	}

	/*
	 * duplicate the node in the magma_replica_ops struct,
	 * then link the function and the already duplicated
	 * magma_flare_request
	 */
	op->node = magma_volcano_clone(node);
	op->replica_function = replica_function;
	op->request = r;

	/*
	 * finally, push the request into the queue
	 */
#if MAGMA_DUPLICATION_THREAD_POOL
	GError *error = NULL;
	g_thread_pool_push(magma_duplication_thread_pool, op, &error);
	if (error) {
		dbg(LOG_ERR, DEBUG_PFUSE, "Error pushing request to replication pool: %s", error->message);
		g_error_free(error);
	}
#else
	// g_async_queue_push_sorted(magma_duplication_queue, op, (GCompareDataFunc) magma_queue_sorter, NULL);
	g_async_queue_push(magma_duplication_queue, op);
#endif // MAGMA_DUPLICATION_THREAD_POOL

	return (TRUE);
#endif // MAGMA_REPLICA_ON_PNODE
}

/**************************************************************
 *                                                            *
 *                       READ OPERATIONS                      *
 *                                                            *
 **************************************************************/

/**************************************************************
 * GETATTR                                                    *
 **************************************************************/
int magma_server_manage_getattr(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	struct stat statbuf;
	int res = 0, server_errno = 0;

	magma_pktqr_getattr(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.getattr.path, 'r')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "GETATTR denied");
	} else {
		memset(&statbuf, 0, sizeof(struct stat));

		dbg(LOG_INFO, DEBUG_PFUSE, "GETATTR #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.getattr.path,
			request->header.uid,
			request->header.gid);

		res = magma_getattr(
			request->header.uid,
			request->header.gid,
			request->body.getattr.path,
			&statbuf);
		server_errno = errno;
	}

	magma_pktas_getattr(socket, peer, res, &statbuf, server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "getattr #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.getattr.path,
		res, server_errno);

	return (res);
}

/**************************************************************
 * READLINK                                                   *
 **************************************************************/
int magma_server_manage_readlink(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int res = 0, server_errno = 0;
	char nbuf[PATH_MAX];

	magma_pktqr_readlink(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.readlink.path, 'r')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "READLINK denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "READLINK #%05d on %s of %u bytes by %d.%d",
			request->header.transaction_id,
			request->body.readlink.path,
			request->body.readlink.buffer_length,
			request->header.uid,
			request->header.gid);

		res = magma_readlink(
			request->header.uid,
			request->header.gid,
			request->body.readlink.path,
			nbuf,
			request->body.readlink.buffer_length);

		server_errno = errno;
	}

	if (res != -1) res = strlen(nbuf);

	magma_pktas_readlink(socket, peer, res, nbuf, server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "readlink #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.readlink.path,
		res, server_errno);

	return (res);
}

/**************************************************************
 * READDIR                                                    *
 **************************************************************/
#if !MAGMA_OPTIMIZE_READDIR

int magma_server_manage_readdir(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	magma_pktqr_readdir(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.readdir.path, 'r')) {
		magma_pktas_readdir(socket, peer, MAGMA_DIR_IS_CLOSE, ECONNREFUSED, request->header.transaction_id);
		dbg(LOG_INFO, DEBUG_PFUSE, "READDIR denied");
		return (-1);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "READDIR #%05d on %s by %d.%d",
		request->header.transaction_id,
		request->body.readdir.path,
		request->header.uid,
		request->header.gid);

	magma_DIR_t *dirp = magma_opendir(
		request->header.uid,
		request->header.gid,
		request->body.readdir.path);

	// if dirp is NULL, the opendir() failed and we can return
	if (!dirp) {
		magma_pktas_readdir(socket, peer, MAGMA_DIR_IS_CLOSE, errno, request->header.transaction_id);
		dbg(LOG_ERR, DEBUG_ERR, "READDIR: %s", strerror(errno));
		return (-1);
	}

	magma_pktas_readdir(socket, peer, MAGMA_DIR_IS_OPEN, errno, request->header.transaction_id);

	// scan the directory and send each entry to the client
	char *de = NULL;
	off_t offset = 0;

	do {
		magma_flare_request entry_request;
		magma_pkt_recv_readdir_offset(socket, peer, &entry_request);

		offset = entry_request.body.readdir_offset.offset;
		magma_seekdir(dirp, offset);

		if ((de = magma_readdir(dirp)) == NULL) {
			/*
			 * signaling client that transmission is ended
			 */
			dbg(LOG_INFO, DEBUG_PFUSE, "READDIR: last entry read");
			magma_send_udp_failure(socket, peer, ENOENT, entry_request.header.transaction_id);
			break;
		}

		offset = magma_telldir(dirp);

		/*
		 * sending entry and position offset
		 */
		magma_pkt_send_readdir_entry(socket, peer, de, entry_request.header.transaction_id);
		dbg(LOG_INFO, DEBUG_PFUSE, "READDIR#%d: sending entry %s @%u", entry_request.header.transaction_id, de, (unsigned int) offset);

	} while (offset);

	magma_closedir(dirp);

	dbg(LOG_INFO, DEBUG_PFUSE, "READDIR#%d %s OK!", request->header.transaction_id, request->body.readdir.path);
	return 0;
}

#else

/**************************************************************
 * READDIR (extended version with entries struct stat)        *
 **************************************************************/
int magma_server_manage_readdir_extended(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	off_t offset = 0;
	magma_flare_response response;

	magma_pktqr_readdir_extended(buffer, request);

	/*
	 * ACL
	 */
	if (!magma_validate_connection(socket, peer, request->body.readdir.path, 'r')) {
		response.header.res = MAGMA_DIR_IS_CLOSE;
		magma_pktas_readdir_extended(socket, peer, &response, request->header.transaction_id);
		dbg(LOG_INFO, DEBUG_PFUSE, "READDIR denied");
		return (-1);
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "READDIR extended #%05d on %s by %d.%d",
		request->header.transaction_id,
		request->body.readdir_extended.path,
		request->header.uid,
		request->header.gid);

	/*
	 * open the directory
	 */
	magma_DIR_t *dirp = magma_opendir(
		request->header.uid,
		request->header.gid,
		request->body.readdir_extended.path);

	/*
	 * if dirp is NULL, the opendir() fails and we can return
	 */
	if (!dirp) {
		response.header.res = MAGMA_DIR_IS_CLOSE;
		magma_pktas_readdir_extended(socket, peer, &response, request->header.transaction_id);
		dbg(LOG_ERR, DEBUG_ERR, "READDIR#%d: %s", request->header.transaction_id, strerror(errno));
		return (-1);
	}

	dbg(LOG_ERR, DEBUG_FLARE, "Seeking directory @%ld", request->body.readdir_extended.offset);
	magma_seekdir(dirp, request->body.readdir_extended.offset);

	/*
	 * scan the directory and send each entry to the client
	 */
	response.header.res = MAGMA_DIR_IS_OPEN;
	response.body.readdir_extended.entry_number = 0;

	int e;
	for (e = 0; e < MAGMA_MAX_READDIR_ENTRIES; e++) {
		/*
		 * read the next entry
		 */
		char *de = NULL;
		if ((de = magma_readdir(dirp)) == NULL) {
			/*
			 * signaling client that transmission is ended by sending an empty dirent
			 */
			dbg(LOG_INFO, DEBUG_PFUSE, "READDIR: last entry read");
			response.header.res = MAGMA_DIR_IS_CLOSE;
			break;
		}

		/*
		 * increment the number of entries
		 */
		response.body.readdir_extended.entry_number++;

		/*
		 * save the entry path
		 */
		magma_copy_path(response.body.readdir_extended.entries[e].path, de);

		/*
		 * add the struct stat section
		 */
		gchar *entry_path = g_strdup_printf("%s/%s", request->body.readdir_extended.path, de);

		struct stat src;
		magma_stat(request->header.uid, request->header.gid, entry_path, &src);
		magma_stat_struct *dst = &(response.body.readdir_extended.entries[e].st);

		dst->dev		= src.st_dev;
		dst->ino		= src.st_ino;
		dst->size		= src.st_size;
		dst->blocks		= src.st_blocks;
		dst->atime		= src.st_atime;
		dst->ctime		= src.st_ctime;
		dst->mtime		= src.st_mtime;
		dst->mode		= src.st_mode;
		dst->nlink		= src.st_nlink;
		dst->uid		= src.st_uid;
		dst->gid		= src.st_gid;
		dst->rdev		= src.st_rdev;
		dst->blksize	= src.st_blksize;

		// magma_encode_stat_struct(&st, &(response.body.readdir_extended.entries[e].st));
		g_free(entry_path);
	}

	/*
	 * Send this chunk of entries
	 */
	response.body.readdir_extended.offset = magma_telldir(dirp);
	magma_pktas_readdir_extended(socket, peer, &response, request->header.transaction_id);

	dbg(LOG_INFO, DEBUG_PFUSE, "READDIR#%d: sending a chunk of entries @%u", request->header.transaction_id, (unsigned int) offset);

	magma_closedir(dirp);

	dbg(LOG_INFO, DEBUG_PFUSE, "READDIR#%d %s OK!", request->header.transaction_id, request->body.readdir.path);
	return 0;
}

#endif

/**************************************************************
 * OPEN (for file creation see MKNOD)                         *
 **************************************************************/
int magma_server_manage_open(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int server_errno = 0, res = 0;

	char op = (request->body.open.flags & (O_WRONLY|O_RDWR)) ? 'w' : 'r';

	magma_pktqr_open(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.open.path, op)) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "OPEN denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "OPEN #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.open.path,
			request->header.uid,
			request->header.gid);

		res = magma_open(
			request->header.uid,
			request->header.gid,
			request->body.open.path,
			request->body.open.flags);
		server_errno = errno;
	}

	magma_flare_t *flare = NULL;
	if (res isNot -1) {
		flare = magma_search_or_create(request->body.open.path);
	}

	magma_pktas_open(socket, peer, res, server_errno, flare, request->header.transaction_id);

	dbg(LOG_INFO, DEBUG_PFUSE, "open #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.open.path,
		res, server_errno);

	magma_dispose_flare(flare);
	return 0;
}

/**************************************************************
 * READ                                                       *
 **************************************************************/
int magma_server_manage_read(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int server_errno = 0, res = 0;
	gchar read_buffer[MAGMA_READ_WRITE_BUFFER_SIZE];

	magma_pktqr_read(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.read.path, 'r')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "READ denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "READ #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.read.path,
			request->header.uid,
			request->header.gid);

		res = magma_read(
			request->header.uid, request->header.gid,
			request->body.read.path,
			request->body.read.size,
			request->body.read.offset,
			read_buffer);

		server_errno = errno;
	}

	magma_pktas_read(socket, peer, res, server_errno, read_buffer, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "read #%05d (%s) answered res: %d, errno: %d",
			request->header.transaction_id,
			request->body.read.path,
			res, server_errno);

    return (res);
}

/**************************************************************
 * STATFS                                                     *
 **************************************************************/
int magma_server_manage_statfs(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int res = 0, server_errno = 0;
	struct statfs statbuf;

	magma_pktqr_statfs(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.statfs.path, 'r')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "STATFS denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "STATFS #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.statfs.path,
			request->header.uid,
			request->header.gid);

		res = magma_statfs(
			request->header.uid,
			request->header.gid,
			request->body.statfs.path,
			&statbuf);
		server_errno = errno;
	}

	magma_pktas_statfs(socket, peer, res, &statbuf, server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "statfs #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.statfs.path,
		res, server_errno);

	return (res);
}

/**************************************************************
 *                                                            *
 *                      WRITE OPERATIONS                      *
 *                                                            *
 **************************************************************/

/**************************************************************
 * MKNOD                                                      *
 **************************************************************/
void magma_redundant_mknod(magma_flare_request *request, magma_volcano *node)
{
	magma_node_response response;
	GSocketAddress *peer;
	GSocket *socket;

	dbg(LOG_INFO, DEBUG_PFUSE, "Mirroring mknod with PNODE to %s (%s)", node->node_name, node->ip_addr);

	magma_flare_t *flare = magma_search_or_create(request->body.mknod.path);
	if (!flare) {
		dbg(LOG_ERR, DEBUG_ERR, "Unable to mirror %s: flare not found", request->body.mknod.path);
		return;
	}

	socket = magma_open_client_connection(node->ip_addr, MAGMA_NODE_PORT, &peer);
	magma_pktqs_transmit_key(socket, peer, 0, 0, NULL, flare, &response);
	magma_close_client_connection(socket, peer);
	magma_dispose_flare(flare);
}

int magma_server_manage_mknod(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int res = 0, server_errno = 0;
	magma_operation_result *result = NULL;

	magma_pktqr_mknod(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.mknod.path, 'w')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "MKNOD denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "MKNOD #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.mknod.path,
			request->header.uid,
			request->header.gid);

		gchar *key = magma_operation_cache_make_key(peer, request->header.transaction_id);
		result = g_hash_table_lookup(magma_operation_cache, key);

		if (result) {

			g_free(key);
			res = result->res;
			server_errno = result->err_no;

		} else {

			res = magma_mknod(
				request->header.uid,
				request->header.gid,
				request->header.ttl,
				request->body.mknod.path,
				request->body.mknod.mode,
				request->body.mknod.rdev);

			server_errno = errno;
			magma_operation_cache_save_result(key, res, server_errno);
		}
	}

	magma_pktas_mknod(socket, peer, res, server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "mknod #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.mknod.path,
		res, server_errno);

	if (result) return (res);

	/* Redundancy ops */
	if (-1 != res) {
		/* mirror on redundant node */
		if (request->header.ttl > MAGMA_TERMINAL_TTL) {
			magma_volcano *owner = magma_route_path(request->body.mknod.path);
			magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;

			if (!magma_compare_nodes(red_owner, &myself))
				magma_queue_replica(magma_redundant_mknod, request, red_owner);
	    } else {
	        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring mknod(%s) because TTL is %d",
	            request->body.mknod.path, request->header.ttl);
	    }

		/* mirror on joining node */
		if (myself.joining_node) magma_queue_replica(magma_redundant_mknod, request, myself.joining_node);
    } else {
        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring mknod(%s) because result is -1",
            request->body.mknod.path);
    }

	return (res);
}

/**************************************************************
 * MKDIR                                                      *
 **************************************************************/
void magma_redundant_mkdir(magma_flare_request *request, magma_volcano *node)
{
	magma_node_response response;
	GSocketAddress *peer;
	GSocket *socket;

	dbg(LOG_INFO, DEBUG_PFUSE, "Mirroring mkdir with PNODE to %s (%s)", node->node_name, node->ip_addr);

	magma_flare_t *flare = magma_search_or_create(request->body.mkdir.path);
	if (!flare) {
		dbg(LOG_ERR, DEBUG_ERR, "Unable to mirror %s: flare not found", request->body.mkdir.path);
		return;
	}

	socket = magma_open_client_connection(node->ip_addr, MAGMA_NODE_PORT, &peer);
	magma_pktqs_transmit_key(socket, peer, 0, 0, NULL, flare, &response);
	magma_close_client_connection(socket, peer);
	magma_dispose_flare(flare);
}

int magma_server_manage_mkdir(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int server_errno = 0, res = 0;
	magma_operation_result *result = NULL;

	magma_pktqr_mkdir(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.mkdir.path, 'w')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "MKDIR denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "MKDIR #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.mkdir.path,
			request->header.uid,
			request->header.gid);

		gchar *key = magma_operation_cache_make_key(peer, request->header.transaction_id);
		result = g_hash_table_lookup(magma_operation_cache, key);
		if (result) {

			g_free(key);
			res = result->res;
			server_errno = result->err_no;

		} else {

			res = magma_mkdir(
				request->header.uid,
				request->header.gid,
				request->header.ttl,
				request->body.mkdir.path,
				request->body.mkdir.mode);

			server_errno = errno;
			magma_operation_cache_save_result(key, res, server_errno);

		}
	}

	magma_pktas_mkdir(socket, peer, res, server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "mkdir #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.mkdir.path,
		res, server_errno);

	if (result) return (res);

	/* Redundancy ops */
	if (-1 != res) {
		/* mirror on redundant node */
		if (request->header.ttl > MAGMA_TERMINAL_TTL) {
			magma_volcano *owner = magma_route_path(request->body.mkdir.path);
			magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;

			if (!magma_compare_nodes(red_owner, &myself)) magma_queue_replica(magma_redundant_mkdir, request, red_owner);
		} else {
			dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring mkdir(%s) because TTL is %d",
				request->body.mkdir.path, request->header.ttl);
		}

		/* mirror on joining node */
		if (myself.joining_node) magma_queue_replica(magma_redundant_mkdir, request, myself.joining_node);
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring mkdir(%s) because result is -1", request->body.mkdir.path);
	}

	return (res);
}

/**************************************************************
 * UNLINK                                                     *
 **************************************************************/
void magma_redundant_unlink(magma_flare_request *request, magma_volcano *node)
{
	magma_flare_response response;
	GSocketAddress *peer;
	GSocket *socket;

	dbg(LOG_INFO, DEBUG_PFUSE, "Mirroring unlink to %s (%s)", node->node_name, node->ip_addr);

	socket = magma_open_client_connection(node->ip_addr, node->port, &peer);

	magma_pktqs_unlink(socket, peer, request->header.ttl - 1,
		request->header.uid,
		request->header.gid,
		request->body.unlink.path,
		&response);
	magma_close_client_connection(socket, peer);
}

int magma_server_manage_unlink(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int server_errno = 0, res = 0;
	magma_operation_result *result = NULL;

	magma_pktqr_unlink(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.unlink.path, 'w')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "UNLINK denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "UNLINK #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.unlink.path,
			request->header.uid,
			request->header.gid);

		gchar *key = magma_operation_cache_make_key(peer, request->header.transaction_id);
		result = g_hash_table_lookup(magma_operation_cache, key);

		if (result) {

			g_free(key);
			res = result->res;
			server_errno = result->err_no;

		} else {

			res = magma_unlink(
				request->header.uid,
				request->header.gid,
				request->header.ttl,
				request->body.unlink.path);

			server_errno = errno;
			magma_operation_cache_save_result(key, res, server_errno);

		}
	}

	magma_pktas_unlink(socket, peer,res,server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "unlink #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.unlink.path,
		res, server_errno);

	if (result) return (res);

	/* Redundancy ops */
	if (-1 != res) {
		/* mirror on redundant node */
		if (request->header.ttl > MAGMA_TERMINAL_TTL) {
			magma_volcano *owner = magma_route_path(request->body.unlink.path);
			magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;

			if (!magma_compare_nodes(&myself, red_owner)) magma_queue_replica(magma_redundant_unlink, request, red_owner);
	    } else {
	        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring unlink(%s) because TTL is %d",
	            request->body.unlink.path, request->header.ttl);
	    }

		/* mirror on joining node */
		if (myself.joining_node) magma_queue_replica(magma_redundant_unlink, request, myself.joining_node);
    } else {
        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring unlink(%s) because result is -1",
            request->body.unlink.path);
    }

	return (res);
}

/**************************************************************
 * RMDIR                                                      *
 **************************************************************/
void magma_redundant_rmdir(magma_flare_request *request, magma_volcano *node)
{
	magma_flare_response response;
	GSocketAddress *peer;
	GSocket *socket;

	dbg(LOG_INFO, DEBUG_PFUSE, "Mirroring rmdir to %s (%s)", node->node_name, node->ip_addr);

	socket = magma_open_client_connection(node->ip_addr, node->port, &peer);

	magma_pktqs_rmdir(socket, peer, request->header.ttl - 1,
		request->header.uid,
		request->header.gid,
		request->body.rmdir.path,
		&response);
	magma_close_client_connection(socket, peer);
}

int magma_server_manage_rmdir(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int server_errno = 0, res = 0;
	magma_operation_result *result = NULL;

	magma_pktqr_rmdir(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.rmdir.path, 'w')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "RMDIR denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "RMDIR #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.rmdir.path,
			request->header.uid,
			request->header.gid);

		gchar *key = magma_operation_cache_make_key(peer, request->header.transaction_id);
		result = g_hash_table_lookup(magma_operation_cache, key);
		if (result) {

			g_free(key);
			res = result->res;
			server_errno = result->err_no;

		} else {

			res = magma_rmdir(
				request->header.uid,
				request->header.gid,
				request->header.ttl,
				request->body.rmdir.path);

			server_errno = errno;
			magma_operation_cache_save_result(key, res, server_errno);

		}
	}

	magma_pktas_rmdir(socket, peer, res, server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "rmdir #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.rmdir.path,
		res, server_errno);

	if (result) return (res);

	/* Redundancy ops */
	if (-1 != res) {
		/* mirror on redundant node */
		if (request->header.ttl > MAGMA_TERMINAL_TTL) {
			magma_volcano *owner = magma_route_path(request->body.unlink.path);
			magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;

			if (!magma_compare_nodes(&myself, red_owner)) magma_queue_replica(magma_redundant_rmdir, request, red_owner);
	    } else {
	        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring rmdir(%s) because TTL is %d",
	            request->body.rmdir.path, request->header.ttl);
	    }

		/* mirror on joining node */
		if (myself.joining_node) magma_queue_replica(magma_redundant_rmdir, request, myself.joining_node);
    } else {
        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring rmdir(%s) because result is -1",
            request->body.rmdir.path);
    }
	return (res);
}

/**************************************************************
 * SYMLINK                                                    *
 **************************************************************/
void magma_redundant_symlink(magma_flare_request *request, magma_volcano *node)
{
	magma_node_response response;
	GSocketAddress *peer;
	GSocket *socket;

	dbg(LOG_INFO, DEBUG_PFUSE, "Mirroring symlink with PNODE to %s (%s)", node->node_name, node->ip_addr);

	magma_flare_t *flare = magma_search_or_create(request->body.symlink.from);
	if (!flare) {
		dbg(LOG_ERR, DEBUG_ERR, "Unable to mirror %s: flare not found", request->body.symlink.from);
		return;
	}

	socket = magma_open_client_connection(node->ip_addr, MAGMA_NODE_PORT, &peer);
	magma_pktqs_transmit_key(socket, peer, 0, strlen(request->body.symlink.to), request->body.symlink.to, flare, &response);
	magma_close_client_connection(socket, peer);
	magma_dispose_flare(flare);
}

int magma_server_manage_symlink(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int server_errno = 0, res = 0;
	magma_operation_result *result = NULL;

	magma_pktqr_symlink(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.symlink.from, 'w')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "SYMLINK denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "SYMLINK #%d from %s to %s by %d.%d",
			request->header.transaction_id,
			request->body.symlink.from,
			request->body.symlink.to,
			request->header.uid,
			request->header.gid);

		gchar *key = magma_operation_cache_make_key(peer, request->header.transaction_id);
		result = g_hash_table_lookup(magma_operation_cache, key);
		if (result) {

			g_free(key);
			res = result->res;
			server_errno = result->err_no;

		} else {

			res = magma_symlink(
				request->header.uid,
				request->header.gid,
				request->header.ttl,
				request->body.symlink.from,
				request->body.symlink.to);

			server_errno = errno;
			magma_operation_cache_save_result(key, res, server_errno);
		}
	}

	magma_pktas_symlink(socket, peer,res,server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "symlink #%05d (%s, %s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.symlink.from,
		request->body.symlink.to,
		res, server_errno);

	if (result) return (res);

	/* Redundancy ops */
	if (-1 != res) {
		/* mirror on redundant node */
		if (request->header.ttl > MAGMA_TERMINAL_TTL) {
			magma_volcano *owner = magma_route_path(request->body.unlink.path);
			magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;

			if (!magma_compare_nodes(&myself, red_owner)) magma_queue_replica(magma_redundant_symlink, request, red_owner);
	    } else {
	        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring symlink(%s, %s) because TTL is %d",
	            request->body.symlink.from, request->body.symlink.to, request->header.ttl);
	    }

		/* mirror on joining node */
		if (myself.joining_node) magma_queue_replica(magma_redundant_symlink, request, myself.joining_node);
    } else {
        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring symlink(%s, %s) because result is -1",
            request->body.symlink.from, request->body.symlink.to);
    }
	return (res);
}

/**************************************************************
 * LINK                                                       *
 **************************************************************/
int magma_server_manage_link(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	return magma_server_manage_symlink(socket, peer, buffer, request);
}

/**
 * rename a flare. renaming is a perfect example of magma
 * internal complexity (which is solved kindly by POSIX standard).
 *
 * rename()ing a flare can involve from 1 to 4(!) nodes. the
 * most complex case is when flare "/path1/original", hosted on
 * node "n1", has to be moved to "/path2/newname", hosted on
 * node "n2", while flare "/path1/" is on node n3 and "/path2/" is
 * on node n4.
 *
 * that is pure pain in the ass! magma should.
 * 1. add entry in /path2/ on node n4
 * 2. create flare "/path2/newname" on n2 and sync contents.
 * 3. remove entry from /path1/ on node n3
 * 4. delete flare from itself on node n1
 *
 * everything should be managed with a chain of fallback
 * tests which should recover previous state if a step fail!
 *
 * BUT! POSIX standard states that when a rename() is performed
 * on two paths on different filesystems, should return -1 and
 * set errno = EXDEV (Invalid cross-device link).
 *
 * take mv(1) behaviour as an example, while renaming /tmp/file
 * to /home/user/file (assuming that /tmp and /home are on
 * different filesystems):
 *
 * [..cut..]
 * rename("/home/tx0/xyz", "/tmp/xyz")     = -1 EXDEV (Invalid cross-device link)
 * unlink("/tmp/xyz")                      = -1 ENOENT (No such file or directory)
 * open("/home/tx0/xyz", O_RDONLY|O_LARGEFILE) = 3
 * fstat64(3, {st_mode=S_IFREG|0644, st_size=0, ...}) = 0
 * open("/tmp/xyz", O_WRONLY|O_CREAT|O_LARGEFILE, 0100644) = 4
 * [..cut..]
 *
 * as you can see, rename failed with EXDEV errno. so mv assumes
 * that a normal copy (read/write pair) plus unlink should be
 * performed! and do it. but magma already has operations for
 * opening, reading and writing a flare (which also add and
 * remove flares from parent flares)
 *
 * so, that's the plan: if source and destination is both locally
 * we implement rename for speed reasons (it's simply a matter of
 * create new destination flare, save it, copy contents, case by
 * case -- is a directory? a symlink? what? -- and than erasing
 * the original one). for all other cases, -1 EXDEV will be
 * returned!
 *
 * @param s network socket
 * @param sa struct in_addr of connecting peer
 */
int magma_server_manage_rename(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	magma_pktqr_rmdir(buffer, request);

	int server_errno = 0, res = 0;

	if (!magma_validate_connection(socket, peer, request->body.rename.from, 'w')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "RENAME denied");
	} else {
		res = -1;
		server_errno = EXDEV;
	}

	magma_pktas_rename(socket, peer, res, server_errno, request->header.transaction_id);
	return (res);
}

/**************************************************************
 * CHMOD                                                      *
 **************************************************************/
void magma_redundant_chmod(magma_flare_request *request, magma_volcano *node)
{
	magma_node_response response;
	GSocketAddress *peer;
	GSocket *socket;

	dbg(LOG_INFO, DEBUG_PFUSE, "Mirroring chmod with PNODE to %s (%s)", node->node_name, node->ip_addr);

	magma_flare_t *flare = magma_search_or_create(request->body.chmod.path);
	if (!flare) {
		dbg(LOG_ERR, DEBUG_ERR, "Unable to mirror %s: flare not found", request->body.chmod.path);
		return;
	}

	socket = magma_open_client_connection(node->ip_addr, MAGMA_NODE_PORT, &peer);
	magma_pktqs_transmit_key(socket, peer, 0, 0, 0, flare, &response);
	magma_close_client_connection(socket, peer);
	magma_dispose_flare(flare);
}

int magma_server_manage_chmod(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int server_errno = 0, res = 0;
	magma_operation_result *result = NULL;

	magma_pktqr_chmod(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.chmod.path, 'w')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "CHMOD denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "CHMOD #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.chmod.path,
			request->header.uid,
			request->header.gid);

		gchar *key = magma_operation_cache_make_key(peer, request->header.transaction_id);
		result = g_hash_table_lookup(magma_operation_cache, key);
		if (result) {

			g_free(key);
			res = result->res;
			server_errno = result->err_no;

		} else {

			res = magma_chmod(
				request->header.uid,
				request->header.gid,
				request->header.ttl,
				request->body.chmod.path,
				request->body.chmod.mode);

			server_errno = errno;
			magma_operation_cache_save_result(key, res, server_errno);

		}
	}

	magma_pktas_chmod(socket, peer,res,server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "chmod #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.chmod.path,
		res, server_errno);

	if (result) return (res);

	/* Redundancy ops */
	if (-1 != res) {
		/* mirror on redundant node */
		if (request->header.ttl > MAGMA_TERMINAL_TTL) {
			magma_volcano *owner = magma_route_path(request->body.unlink.path);
			magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;

			if (!magma_compare_nodes(&myself, red_owner)) magma_queue_replica(magma_redundant_chmod, request, red_owner);
	    } else {
	        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring chmod(%s) because TTL is %d",
	            request->body.chmod.path, request->header.ttl);
	    }

		/* mirror on joining node */
		if (myself.joining_node) magma_queue_replica(magma_redundant_chmod, request, myself.joining_node);
    } else {
        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring chmod(%s) because result is -1",
            request->body.chmod.path);
    }

	return (res);
}

/**************************************************************
 * CHOWN                                                      *
 **************************************************************/
void magma_redundant_chown(magma_flare_request *request, magma_volcano *node)
{
	magma_node_response response;
	GSocketAddress *peer;
	GSocket *socket;

	dbg(LOG_INFO, DEBUG_PFUSE, "Mirroring chown with PNODE to %s (%s)", node->node_name, node->ip_addr);

	magma_flare_t *flare = magma_search_or_create(request->body.chown.path);
	if (!flare) {
		dbg(LOG_ERR, DEBUG_ERR, "Unable to mirror %s: flare not found", request->body.chown.path);
		return;
	}

	socket = magma_open_client_connection(node->ip_addr, MAGMA_NODE_PORT, &peer);
	magma_pktqs_transmit_key(socket, peer, 0, 0, 0, flare, &response);
	magma_close_client_connection(socket, peer);
	magma_dispose_flare(flare);
}

int magma_server_manage_chown(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int server_errno = 0, res = 0;
	magma_operation_result *result = NULL;

	magma_pktqr_chown(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.chown.path, 'w')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "CHOWN denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "CHOWN #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.chown.path,
			request->header.uid,
			request->header.gid);

		gchar *key = magma_operation_cache_make_key(peer, request->header.transaction_id);
		result = g_hash_table_lookup(magma_operation_cache, key);
		if (result) {

			g_free(key);
			res = result->res;
			server_errno = result->err_no;

		} else {

			res = magma_chown(
				request->header.uid,
				request->header.gid,
				request->header.ttl,
				request->body.chown.path,
				request->body.chown.new_uid,
				request->body.chown.new_gid);

			server_errno = errno;
			magma_operation_cache_save_result(key, res, server_errno);

		}
	}

	magma_pktas_chown(socket, peer,res,server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "chown #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.chown.path,
		res, server_errno);

	if (result) return (res);

	/* Redundancy ops */
	if (-1 != res) {
		/* mirror on redundant node */
		if (request->header.ttl > MAGMA_TERMINAL_TTL) {
			magma_volcano *owner = magma_route_path(request->body.unlink.path);
			magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;

			if (!magma_compare_nodes(&myself, red_owner)) magma_queue_replica(magma_redundant_chown, request, red_owner);
	    } else {
	        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring chown(%s) because TTL is %d",
	            request->body.chown.path, request->header.ttl);
	    }

		/* mirror on joining node */
		if (myself.joining_node) magma_queue_replica(magma_redundant_chown, request, myself.joining_node);
    } else {
        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring chown(%s) because result is -1",
            request->body.chown.path);
    }
	return (res);
}

/**************************************************************
 * TRUNCATE                                                   *
 **************************************************************/
void magma_redundant_truncate(magma_flare_request *request, magma_volcano *node)
{
	magma_flare_response response;
	GSocketAddress *peer;
	GSocket *socket;

	dbg(LOG_INFO, DEBUG_PFUSE, "Mirroring truncate to %s (%s)", node->node_name, node->ip_addr);

	socket = magma_open_client_connection(node->ip_addr, node->port, &peer);

	magma_pktqs_truncate(socket, peer, request->header.ttl - 1,
		request->header.uid,
		request->header.gid,
		request->body.truncate.path,
		request->body.truncate.offset,
		&response);
	magma_close_client_connection(socket, peer);
}

int magma_server_manage_truncate(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int res = 0, server_errno = 0;
	magma_operation_result *result = NULL;

	magma_pktqr_truncate(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.truncate.path, 'w')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "TRUNCATE denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "TRUNCATE #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.truncate.path,
			request->header.uid,
			request->header.gid);

		gchar *key = magma_operation_cache_make_key(peer, request->header.transaction_id);
		result = g_hash_table_lookup(magma_operation_cache, key);

		if (result) {

			g_free(key);
			res = result->res;
			server_errno = result->err_no;

		} else {

			res = magma_truncate(
				request->header.uid,
				request->header.gid,
				request->header.ttl,
				request->body.truncate.path,
				request->body.truncate.offset);

			server_errno = errno;
			magma_operation_cache_save_result(key, res, server_errno);

		}
	}

	magma_pktas_truncate(socket, peer, res, server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "truncate #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.truncate.path,
		res, server_errno);

	if (result) return (res);

	/* Redundancy ops */
	if (-1 != res) {
		/* redundant node */
		if (request->header.ttl > MAGMA_TERMINAL_TTL) {
			magma_volcano *owner = magma_route_path(request->body.truncate.path);
			magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;

			if (!magma_compare_nodes(red_owner, &myself)) magma_queue_replica(magma_redundant_truncate, request, red_owner);
	    } else {
	        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring truncate(%s) because TTL is %d",
	            request->body.truncate.path, request->header.ttl);
	    }

		/* joining node */
		if (myself.joining_node) magma_queue_replica(magma_redundant_truncate, request, myself.joining_node);
    } else {
        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring truncate(%s) because result is -1",
            request->body.truncate.path);
    }
	return (res);
}

/**************************************************************
 * UTIME                                                      *
 **************************************************************/
void magma_redundant_utime(magma_flare_request *request, magma_volcano *node)
{
	magma_node_response response;
	GSocketAddress *peer;
	GSocket *socket;

	dbg(LOG_INFO, DEBUG_PFUSE, "Mirroring utime with PNODE to %s (%s)", node->node_name, node->ip_addr);

	magma_flare_t *flare = magma_search_or_create(request->body.utime.path);
	if (!flare) {
		dbg(LOG_ERR, DEBUG_ERR, "Unable to mirror %s: flare not found", request->body.utime.path);
		return;
	}

	socket = magma_open_client_connection(node->ip_addr, MAGMA_NODE_PORT, &peer);
	magma_pktqs_transmit_key(socket, peer, 0, 0, 0, flare, &response);
	magma_close_client_connection(socket, peer);
	magma_dispose_flare(flare);
}

int magma_server_manage_utime(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int server_errno = 0, res = 0;
	magma_operation_result *result = NULL;

	magma_pktqr_utime(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.utime.path, 'w')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "UTIME denied");
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "UTIME #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.utime.path,
			request->header.uid,
			request->header.gid);

		gchar *key = magma_operation_cache_make_key(peer, request->header.transaction_id);
		result = g_hash_table_lookup(magma_operation_cache, key);
		if (result) {

			g_free(key);
			res = result->res;
			server_errno = result->err_no;

		} else {

			res = magma_utime(
				request->header.uid,
				request->header.gid,
				request->header.ttl,
				request->body.utime.path,
				request->body.utime.atime,
				request->body.utime.mtime);

			server_errno = errno;
			magma_operation_cache_save_result(key, res, server_errno);

		}
	}

	magma_pktas_utime(socket, peer, res, server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "utime #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.utime.path,
		res, server_errno);

	if (result) return (res);

	/* Redundancy ops */
	if (-1 != res) {
		/* mirror on redundant node */
		if (request->header.ttl > MAGMA_TERMINAL_TTL) {
			magma_volcano *owner = magma_route_path(request->body.utime.path);
			magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;

			if (!magma_compare_nodes(red_owner, &myself)) magma_queue_replica(magma_redundant_utime, request, red_owner);
	    } else {
	        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring utime(%s) because TTL is %d",
	            request->body.utime.path, request->header.ttl);
	    }

		/* mirror on joining node */
		if (myself.joining_node) magma_queue_replica(magma_redundant_utime, request, myself.joining_node);
    } else {
        dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring utime(%s) because result is -1",
            request->body.utime.path);
    }

	return (res);
}

/**************************************************************
 * WRITE                                                      *
 **************************************************************/
void magma_redundant_write(magma_flare_request *request, magma_volcano *node)
{
	magma_node_response response;
	GSocketAddress *peer;
	GSocket *socket;

	dbg(LOG_INFO, DEBUG_PFUSE, "Mirroring write with PNODE to %s (%s)", node->node_name, node->ip_addr);

	magma_flare_t *flare = magma_search_or_create(request->body.write.path);
	if (!flare) {
		dbg(LOG_ERR, DEBUG_ERR, "Unable to mirror %s: flare not found", request->body.write.path);
		return;
	}

	socket = magma_open_client_connection(node->ip_addr, MAGMA_NODE_PORT, &peer);
	magma_pktqs_transmit_key(socket, peer,
		request->body.write.offset,
		request->body.write.size,
		request->body.write.buffer,
		flare, &response);
	magma_close_client_connection(socket, peer);
	magma_dispose_flare(flare);
}

int magma_server_manage_write(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request)
{
	int server_errno = 0, res = 0;
	magma_operation_result *result = NULL;

	magma_pktqr_write(buffer, request);

	if (!magma_validate_connection(socket, peer, request->body.write.path, 'w')) {
		res = -1;
		errno = server_errno = ECONNREFUSED;
		dbg(LOG_INFO, DEBUG_PFUSE, "WRITE denied");
	} else {

		dbg(LOG_INFO, DEBUG_PFUSE, "WRITE #%05d on %s by %d.%d",
			request->header.transaction_id,
			request->body.write.path,
			request->header.uid,
			request->header.gid);

		gchar *key = magma_operation_cache_make_key(peer, request->header.transaction_id);
		result = g_hash_table_lookup(magma_operation_cache, key);

		if (result) {

			g_free(key);
			res = result->res;
			server_errno = result->err_no;

		} else {

			res = magma_write(
				request->header.uid,
				request->header.gid,
				request->header.ttl,
				request->body.write.path,
				request->body.write.size,
				request->body.write.offset,
				request->body.write.buffer);

			server_errno = errno;
			magma_operation_cache_save_result(key, res, server_errno);

		}
	}
	
	magma_pktas_write(socket, peer, res, server_errno, request->header.transaction_id);
	dbg(LOG_INFO, DEBUG_PFUSE, "write #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.write.path,
		res, server_errno);

	if (result) return (res);

	/* Redundancy ops */
	if (res isNot -1) {
		/* mirror on redundant node */
		if (request->header.ttl > MAGMA_TERMINAL_TTL) {
			magma_volcano *owner = magma_route_path(request->body.write.path);
			magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;

			if (!magma_compare_nodes(red_owner, &myself)) magma_queue_replica(magma_redundant_write, request, red_owner);
		} else {
			dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring write(%s) because TTL is %d",
				request->body.write.path, request->header.ttl);
		}

		/* mirror on joining node */
		if (myself.joining_node) magma_queue_replica(magma_redundant_write, request, myself.joining_node);
	} else {
		dbg(LOG_INFO, DEBUG_PFUSE, "Not mirroring write(%s) because result is -1",
			request->body.write.path);
	}

    return (res);
}

/**************************************************************
 *                                                            *
 *             FLARE-SYSTEM INTERNAL OPERATIONS               *
 *                                                            *
 **************************************************************/

int magma_server_manage_f_opendir(GSocket *socket, GSocketAddress *peer, magma_flare_request *request)
{
	int res = 0, server_errno = 0;

	dbg(LOG_INFO, DEBUG_PFUSE, "F_OPENDIR #%05d on %s@%lu by %d.%d",
		request->header.transaction_id,
		request->body.f_opendir.path,
		request->body.f_opendir.offset,
		request->header.uid,
		request->header.gid);

	magma_flare_t *flare = magma_search_or_create(request->body.f_opendir.path);
	if (!flare) {
		magma_pktas_f_opendir(socket, peer, -1, ENOENT, NULL, 0, 0, 0, request->header.transaction_id);
		dbg(LOG_ERR, DEBUG_DIR, "f_opendir(%s): no such flare", request->body.f_opendir.path);
		return (-1);
	}

	/*
	 * open directory contents
	 */
	int fd = magma_open_flare_contents(flare);
	if (fd is -1) {
		int server_error = errno;
		magma_pktas_f_opendir(socket, peer, -1, ENOENT, NULL, 0, 0, 0, request->header.transaction_id);
		dbg(LOG_ERR, DEBUG_DIR, "f_opendir(%s): error opening contents, %s", request->body.f_opendir.path, strerror(server_error));
		magma_dispose_flare(flare);
		return (-1);
	}

	/*
	 * seek at requested offset
	 */
	lseek(fd, request->body.f_opendir.offset, SEEK_SET);

	/*
	 * read the portion of the directory contents
	 */
	gchar buffer[MAGMA_DIR_FRAGMENT_SIZE];
	ssize_t read = magma_perfect_read(fd, buffer, MAGMA_DIR_FRAGMENT_SIZE);

	magma_pktas_f_opendir(socket, peer, res, server_errno, buffer,
		request->body.f_opendir.offset + read, read,
		flare->st.st_size, request->header.transaction_id);

	dbg(LOG_INFO, DEBUG_PFUSE, "f_opendir #%05d (%s) answered res: %d, errno: %d",
		request->header.transaction_id,
		request->body.f_opendir.path,
		res, server_errno);

	magma_dispose_flare(flare);
	return (res);
}

/*
 * The prototype of the flare callback used to answer
 * to flare requests
 */
typedef void (*magma_optype_callback) (GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_flare_request *request);

/*
 * A callback lookup table used to answer to flare request without
 * walking through a long if/switch cycle
 */
static magma_optype_callback magma_optype_callbacks[MAGMA_OP_TYPE_MAX] = {};
static gchar *magma_optype_callbacks_desc[MAGMA_OP_TYPE_MAX] = {};

/*
 * Initialize flare callbacks lookup table
 */
void magma_init_flare_callbacks()
{
	/*
	 * Set all the slots to NULL
	 */
	magma_optype i = 0;
	for (; i < MAGMA_OP_TYPE_MAX; i++)
		magma_optype_callbacks[i] = NULL;

	/*
	 * Register flare callbacks
	 */
	magma_register_callback(MAGMA_OP_TYPE_GETATTR,			magma_server_manage_getattr			);
	magma_register_callback(MAGMA_OP_TYPE_READLINK,			magma_server_manage_readlink		);
#if !MAGMA_OPTIMIZE_READDIR
	magma_register_callback(MAGMA_OP_TYPE_READDIR,			magma_server_manage_readdir			);
#else
	magma_register_callback(MAGMA_OP_TYPE_READDIR_EXTENDED,	magma_server_manage_readdir_extended);
#endif
	magma_register_callback(MAGMA_OP_TYPE_MKNOD,			magma_server_manage_mknod			);
	magma_register_callback(MAGMA_OP_TYPE_MKDIR,			magma_server_manage_mkdir			);
	magma_register_callback(MAGMA_OP_TYPE_UNLINK,			magma_server_manage_unlink			);
	magma_register_callback(MAGMA_OP_TYPE_RMDIR,			magma_server_manage_rmdir			);
	magma_register_callback(MAGMA_OP_TYPE_SYMLINK,			magma_server_manage_symlink			);
	magma_register_callback(MAGMA_OP_TYPE_RENAME,			magma_server_manage_rename			);
	magma_register_callback(MAGMA_OP_TYPE_LINK,				magma_server_manage_link			);
	magma_register_callback(MAGMA_OP_TYPE_CHMOD,			magma_server_manage_chmod			);
	magma_register_callback(MAGMA_OP_TYPE_CHOWN,			magma_server_manage_chown			);
	magma_register_callback(MAGMA_OP_TYPE_TRUNCATE,			magma_server_manage_truncate		);
	magma_register_callback(MAGMA_OP_TYPE_UTIME,			magma_server_manage_utime			);
	magma_register_callback(MAGMA_OP_TYPE_OPEN,				magma_server_manage_open			);
	magma_register_callback(MAGMA_OP_TYPE_READ,				magma_server_manage_read			);
	magma_register_callback(MAGMA_OP_TYPE_WRITE,			magma_server_manage_write			);
	magma_register_callback(MAGMA_OP_TYPE_STATFS,			magma_server_manage_statfs			);

	magma_register_callback(MAGMA_OP_TYPE_F_OPENDIR,		magma_server_manage_f_opendir		);
}

/**
 * Called by the thread pool to manage a request
 */
void magma_manage_udp_flare_protocol_pool(magma_incoming_request *incoming, gpointer user_data) {
	(void) user_data;

	/*
	 * just call the original callback with proper parameters
	 */
	magma_manage_udp_flare_protocol(incoming->socket, incoming->peer, incoming->buffer);

	/*
	 * unref GLib objects to allow their disposal
	 */
	g_object_unref(incoming->peer);
	g_free(incoming);
}

/**
 * Answer to a FLARE request
 */
void magma_manage_udp_flare_protocol(
	GSocket *socket,
	GSocketAddress *peer,
	gchar buffer[MAGMA_MESSAGE_MAX_SIZE])
{
	magma_flare_request request;
	memset(&request, 0, sizeof(magma_flare_request));

	/*
	 * parse request headers and complain on errors
	 */
	gchar *ptr = magma_parse_request_header(buffer, (magma_request *) &request);
	if (!ptr) {
		dbg(LOG_WARNING, DEBUG_ERR, "error deserializing request headers");
		magma_send_udp_failure(socket, peer, 0, request.header.transaction_id);
		return;
	}

	/*
	 * if requested to close the connection, do it
	 */
	if (MAGMA_OP_TYPE_CLOSECONNECTION == request.header.type) return;

	/*
	 * A request can be:
	 *   1. a duplicate of an already processed request, resent because
	 *      no answer were received
	 *
	 *   2. a brand new request to be processed
	 *
	 *   3. a malformed bunch of bytes
	 *
	 * First this function must check if there's already a response in the
	 * cache. If one is found, its sent back and the function returns.
	 * If no cached answer is found and the request is valid, it's answered.
	 */
	if (!magma_optype_callbacks[request.header.type]) {
		dbg(LOG_WARNING, DEBUG_ERR, "operation type unknown: %d", request.header.type);
		magma_send_udp_failure(socket, peer, 0, request.header.transaction_id);
		return;
	}

	if (request.header.type is MAGMA_OP_TYPE_ADD_FLARE_TO_PARENT)
	dbg(LOG_ERR, DEBUG_DIR, "Incoming request #%05d: %s",
		request.header.transaction_id,
		magma_optype_callbacks_desc[request.header.type]);

	(magma_optype_callbacks[request.header.type])(socket, peer, ptr, &request);
}
