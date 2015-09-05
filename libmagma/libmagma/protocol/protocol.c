/*
   MAGMA -- protocol.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Implements server side protocol management. Single protocols are
   implemented separately in protocol_console.c, protocol_node.c
   and protocol_flare.c

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

#if 0
/*
 * when a new connection reaches magma, first byte declares the kind of
 * peer connecting
 */
const magma_peertype PEER_TYPE_NODE		= 1;		/**< identify a magma server exchanging data */
const magma_peertype PEER_TYPE_FLARE	= 2;		/**< identify a flare system client */
const magma_peertype PEER_TYPE_MOUNT	= 2;		/**< an alias for flare system client */
const magma_peertype PEER_TYPE_FUSE		= 2;		/**< an alias for flare system client */
const magma_peertype PEER_TYPE_CONSOLE	= 3;		/**< identify a console connection */
const magma_peertype PEER_TYPE_TELNET	= 13;		/**< identify a console connection */

#endif

/*
 * fuse protocol
 */
const magma_optype MAGMA_OP_TYPE_GETATTR	=  1;	/**< Operation type GETATTR. Need -D_FILE_OFFSET_BITS=64 to compile cleanly */
const magma_optype MAGMA_OP_TYPE_READLINK	=  2;	/**< Operation type READLINK*/ /* ok */
const magma_optype MAGMA_OP_TYPE_GETDIR		=  3;	/**< Operaiton type GETDIR. deprecated, see readdir */
const magma_optype MAGMA_OP_TYPE_MKNOD		=  4;	/**< Operation type MKNOD*/ /* ok */
const magma_optype MAGMA_OP_TYPE_MKDIR		=  5;	/**< Operation type MKDIR*/ /* ok */
const magma_optype MAGMA_OP_TYPE_UNLINK		=  6;	/**< Operation type UNLINK*/ /* ok */
const magma_optype MAGMA_OP_TYPE_RMDIR		=  7;	/**< Operation type RMDIR*/ /* ok */
const magma_optype MAGMA_OP_TYPE_SYMLINK	=  8;	/**< Operation type SYMLINK*/ /* ok */
const magma_optype MAGMA_OP_TYPE_RENAME		=  9;	/**< Operation type RENAME*/ /* ok */
const magma_optype MAGMA_OP_TYPE_LINK		= 10;	/**< Operation type LINK*/ /* ok */
const magma_optype MAGMA_OP_TYPE_CHMOD		= 11;	/**< Operation type CHMOD*/ /* ok */
const magma_optype MAGMA_OP_TYPE_CHOWN		= 12;	/**< Operation type CHOWN*/ /* ok */
const magma_optype MAGMA_OP_TYPE_TRUNCATE	= 13;	/**< Operation type TRUNCATE*/ /* ok */
const magma_optype MAGMA_OP_TYPE_UTIME		= 14;	/**< Operation type UTIME */
const magma_optype MAGMA_OP_TYPE_OPEN		= 15;	/**< Operation type OPEN */ /* optional */
const magma_optype MAGMA_OP_TYPE_READ		= 16;	/**< Operation type READ */
const magma_optype MAGMA_OP_TYPE_WRITE		= 17;	/**< Operation type WRITE */
const magma_optype MAGMA_OP_TYPE_STATFS		= 18;	/**< Operation type STATFS */
const magma_optype MAGMA_OP_TYPE_FLUSH		= 19;	/**< Operation type FLUSH optional */
const magma_optype MAGMA_OP_TYPE_RELEASE	= 20;	/**< Operation type RELEASE optional */
const magma_optype MAGMA_OP_TYPE_FSYNC		= 21;	/**< Operation type FSYNC optional */
#ifdef HAVE_SETXATTR
const magma_optype MAGMA_OP_TYPE_SETXATTR	= 22;	/**< Operation type SETXATTR optional */
const magma_optype MAGMA_OP_TYPE_GETXATTR	= 23;	/**< Operation type GETXATTR optional */
const magma_optype MAGMA_OP_TYPE_LISTXATTR	= 24;	/**< Operation type LISTXATTR optional */
const magma_optype MAGMA_OP_TYPE_REMOVEXATTR= 25;	/**< Operation type REMOVEXATTR optional */
#endif /**< HAVE_SETXATTR */
const magma_optype MAGMA_OP_TYPE_OPENDIR	= 26;	/**< Operation type OPENDIR optional */
const magma_optype MAGMA_OP_TYPE_READDIR	= 27;	/**< Operation type READDIR implemented */
const magma_optype MAGMA_OP_TYPE_RELEASEDIR = 28;	/**< Operation type RELEASEDIR optional */
const magma_optype MAGMA_OP_TYPE_FSYNCDIR	= 29;	/**< Operation type FSYNCDIR optional */
const magma_optype MAGMA_OP_TYPE_INIT		= 30;	/**< Operation type INIT optional */
const magma_optype MAGMA_OP_TYPE_DESTROY	= 31;	/**< Operation type DESTROY optional */
const magma_optype MAGMA_OP_TYPE_READDIR_EXTENDED = 32;
const magma_optype MAGMA_OP_TYPE_READDIR_OFFSET = 33;

const magma_optype MAGMA_OP_TYPE_ADD_FLARE_TO_PARENT = 50;		/**< Operation type ADD_FLARE_TO_PARENT implemented */
const magma_optype MAGMA_OP_TYPE_REMOVE_FLARE_FROM_PARENT = 51;	/**< Operation type REMOVE_FLARE_FROM_PARENT implemented */

/* MAGMA_OP_TYPE_F_* are flare system internal equivalent */
const magma_optype MAGMA_OP_TYPE_F_OPENDIR	= 60;	/**< Operation type F_OPENDIR implemented */
const magma_optype MAGMA_OP_TYPE_F_CLOSEDIR	= 61;	/**< Operation type F_CLOSEDIR implemented */
const magma_optype MAGMA_OP_TYPE_F_TELLDIR	= 62;	/**< Operation type F_TELLDIR implemented */
const magma_optype MAGMA_OP_TYPE_F_SEEKDIR	= 63;	/**< Operation type F_SEEKDIR implemented */
const magma_optype MAGMA_OP_TYPE_F_READDIR	= 64;	/**< Operation type F_READDIR implemented */

const magma_optype MAGMA_OP_TYPE_F_OPEN		= 70;
const magma_optype MAGMA_OP_TYPE_F_READ		= 71;
const magma_optype MAGMA_OP_TYPE_F_WRITE	= 72;

/*
 * node protocol
*/
const magma_optype MAGMA_OP_TYPE_JOIN				= 100;	/**< Operation type JOIN (existing network)*/
const magma_optype MAGMA_OP_TYPE_FINISH_JOIN		= 101;	/**< Finish a JOIN procedure */
const magma_optype MAGMA_OP_TYPE_TRANSMIT_TOPOLOGY	= 105;	/**< Operation type TRANSMIT_TOPOLOGY (tell me about all the nodes) */
const magma_optype MAGMA_OP_TYPE_TRANSMIT_KEY		= 110;	/**< Operation type TRANSMIT_KEY (send a copy of a key usually to its redundant node */
const magma_optype MAGMA_OP_TYPE_TRANSMIT_NODE		= 112;	/**< Operation type TRANSMIT_NODE (tell me about another node) */
const magma_optype MAGMA_OP_TYPE_GET_KEY			= 113;	/**< Operation type GET_KEY (give me a key of your keyspace) */
const magma_optype MAGMA_OP_TYPE_PUT_KEY			= 114;	/**< Operation type PUT_KEY (take a key of my keyspace) */
const magma_optype MAGMA_OP_TYPE_DROP_KEY			= 115;	/**< Operation type DROP_KEY (I've received this key, forget it) */
const magma_optype MAGMA_OP_TYPE_GET_KEY_CONTENT	= 116;	/**< Operation type GET_KEY_CONTENT (Send me flare contents of this key) */
const magma_optype MAGMA_OP_TYPE_NETWORK_BUILT		= 117;	/**< Operation type NETWORK_BUILT (Lava network loaded from disk and ready to operate) */

/* 
 * generic utilities
*/
const magma_optype MAGMA_OP_TYPE_CLOSECONNECTION 	= 252;	/**< Operation type CLOSECONNECTION (close connection to MAGMA server) */
const magma_optype MAGMA_OP_TYPE_SHUTDOWN			= 253;	/**< Operation type SHUTDOWN (shutdown the lava network) */
const magma_optype MAGMA_OP_TYPE_HEARTBEAT			= 254;	/**< Operation type HEARTBEAT (see if server is still available) */

gchar *magma_explain_optype(magma_optype optype)
{
	static gchar *explanation[256];
	static int initialized = 0;

	if (!initialized) {
		explanation[MAGMA_OP_TYPE_GETATTR] = g_strdup("MAGMA_OP_TYPE_GETATTR");
		explanation[MAGMA_OP_TYPE_READLINK] = g_strdup("MAGMA_OP_TYPE_READLINK");
		explanation[MAGMA_OP_TYPE_GETDIR] = g_strdup("MAGMA_OP_TYPE_GETDIR");
		explanation[MAGMA_OP_TYPE_MKNOD] = g_strdup("MAGMA_OP_TYPE_MKNOD");
		explanation[MAGMA_OP_TYPE_MKDIR] = g_strdup("MAGMA_OP_TYPE_MKDIR");
		explanation[MAGMA_OP_TYPE_UNLINK] = g_strdup("MAGMA_OP_TYPE_UNLINK");
		explanation[MAGMA_OP_TYPE_RMDIR] = g_strdup("MAGMA_OP_TYPE_RMDIR");
		explanation[MAGMA_OP_TYPE_SYMLINK] = g_strdup("MAGMA_OP_TYPE_SYMLINK");
		explanation[MAGMA_OP_TYPE_RENAME] = g_strdup("MAGMA_OP_TYPE_RENAME");
		explanation[MAGMA_OP_TYPE_LINK] = g_strdup("MAGMA_OP_TYPE_LINK");
		explanation[MAGMA_OP_TYPE_CHMOD] = g_strdup("MAGMA_OP_TYPE_CHMOD");
		explanation[MAGMA_OP_TYPE_CHOWN] = g_strdup("MAGMA_OP_TYPE_CHOWN");
		explanation[MAGMA_OP_TYPE_TRUNCATE] = g_strdup("MAGMA_OP_TYPE_TRUNCATE");
		explanation[MAGMA_OP_TYPE_UTIME] = g_strdup("MAGMA_OP_TYPE_UTIME");
		explanation[MAGMA_OP_TYPE_OPEN] = g_strdup("MAGMA_OP_TYPE_OPEN");
		explanation[MAGMA_OP_TYPE_READ] = g_strdup("MAGMA_OP_TYPE_READ");
		explanation[MAGMA_OP_TYPE_WRITE] = g_strdup("MAGMA_OP_TYPE_WRITE");
		explanation[MAGMA_OP_TYPE_STATFS] = g_strdup("MAGMA_OP_TYPE_STATFS");
		explanation[MAGMA_OP_TYPE_FLUSH] = g_strdup("MAGMA_OP_TYPE_FLUSH");
		explanation[MAGMA_OP_TYPE_RELEASE] = g_strdup("MAGMA_OP_TYPE_RELEASE");
		explanation[MAGMA_OP_TYPE_FSYNC] = g_strdup("MAGMA_OP_TYPE_FSYNC");
#ifdef HAVE_SETXATTR
		explanation[MAGMA_OP_TYPE_SETXATTR] = g_strdup("MAGMA_OP_TYPE_SETXATTR");
		explanation[MAGMA_OP_TYPE_GETXATTR] = g_strdup("MAGMA_OP_TYPE_GETXATTR");
		explanation[MAGMA_OP_TYPE_LISTXATTR] = g_strdup("MAGMA_OP_TYPE_LISTXATTR");
		explanation[MAGMA_OP_TYPE_REMOVEXATTR=] = g_strdup("MAGMA_OP_TYPE_REMOVEXATTR=");
#endif
		explanation[MAGMA_OP_TYPE_OPENDIR] = g_strdup("MAGMA_OP_TYPE_OPENDIR");
		explanation[MAGMA_OP_TYPE_READDIR] = g_strdup("MAGMA_OP_TYPE_READDIR");
		explanation[MAGMA_OP_TYPE_RELEASEDIR] = g_strdup("MAGMA_OP_TYPE_RELEASEDIR");
		explanation[MAGMA_OP_TYPE_FSYNCDIR] = g_strdup("MAGMA_OP_TYPE_FSYNCDIR");
		explanation[MAGMA_OP_TYPE_INIT] = g_strdup("MAGMA_OP_TYPE_INIT");
		explanation[MAGMA_OP_TYPE_DESTROY] = g_strdup("MAGMA_OP_TYPE_DESTROY");
		explanation[MAGMA_OP_TYPE_ADD_FLARE_TO_PARENT] = g_strdup("MAGMA_OP_TYPE_ADD_FLARE_TO_PARENT");
		explanation[MAGMA_OP_TYPE_REMOVE_FLARE_FROM_PARENT] = g_strdup("MAGMA_OP_TYPE_REMOVE_FLARE_FROM_PARENT");
		explanation[MAGMA_OP_TYPE_F_OPENDIR] = g_strdup("MAGMA_OP_TYPE_F_OPENDIR");
		explanation[MAGMA_OP_TYPE_F_CLOSEDIR] = g_strdup("MAGMA_OP_TYPE_F_CLOSEDIR");
		explanation[MAGMA_OP_TYPE_F_TELLDIR] = g_strdup("MAGMA_OP_TYPE_F_TELLDIR");
		explanation[MAGMA_OP_TYPE_F_SEEKDIR] = g_strdup("MAGMA_OP_TYPE_F_SEEKDIR");
		explanation[MAGMA_OP_TYPE_F_READDIR] = g_strdup("MAGMA_OP_TYPE_F_READDIR");
		explanation[MAGMA_OP_TYPE_F_OPEN] = g_strdup("MAGMA_OP_TYPE_F_OPEN");
		explanation[MAGMA_OP_TYPE_F_READ] = g_strdup("MAGMA_OP_TYPE_F_READ");
		explanation[MAGMA_OP_TYPE_F_WRITE] = g_strdup("MAGMA_OP_TYPE_F_WRITE");
		explanation[MAGMA_OP_TYPE_JOIN] = g_strdup("MAGMA_OP_TYPE_JOIN");
		explanation[MAGMA_OP_TYPE_TRANSMIT_NODE] = g_strdup("MAGMA_OP_TYPE_TRANSMIT_NODE");
		explanation[MAGMA_OP_TYPE_GET_KEY] = g_strdup("MAGMA_OP_TYPE_GET_KEY");
		explanation[MAGMA_OP_TYPE_PUT_KEY] = g_strdup("MAGMA_OP_TYPE_PUT_KEY");
		explanation[MAGMA_OP_TYPE_DROP_KEY] = g_strdup("MAGMA_OP_TYPE_DROP_KEY");
		explanation[MAGMA_OP_TYPE_GET_KEY_CONTENT] = g_strdup("MAGMA_OP_TYPE_GET_KEY_CONTENT");
		explanation[MAGMA_OP_TYPE_CLOSECONNECTION] = g_strdup("MAGMA_OP_TYPE_CLOSECONNECTION");
		explanation[MAGMA_OP_TYPE_HEARTBEAT] = g_strdup("MAGMA_OP_TYPE_HEARTBEAT");

		initialized++;
	}

	return explanation[optype];
}

/**
 * Cycles to receive new packets from the wire and call the proper
 * callack to service them.
 *
 * @param context the context the thread is running in
 */
gpointer magma_manage_udp_service(magma_udp_service_context *context)
{
	GError *error = NULL;

	while (1) {
		/*
		 * allocate the incoming buffer
		 */
		magma_incoming_request *incoming = g_new0(magma_incoming_request, 1);
		if (!incoming) {
			g_usleep(100000);
			continue;
		}

		g_strlcpy(incoming->ip_addr, context->ip_addr, MAX_IP_LENGTH);
		incoming->port = context->port;
		incoming->socket = context->socket;

		/*
		 * Wait for available data
		 */
		while (!g_socket_condition_timed_wait(context->socket, G_IO_IN, 100000, NULL, NULL)) { g_thread_yield(); }

		/*
		 * Receive a datagram from the net
		 */
		gssize length = g_socket_receive_from(
			context->socket,
			&(incoming->peer),
			incoming->buffer,
			MAGMA_MESSAGE_MAX_SIZE,
			NULL,
			&error);

		if (length is -1) {
			dbg(LOG_INFO, DEBUG_NET, "Error receiving next datagram: %s\n", error->message);
			g_error_free(error);
			g_object_unref(incoming->peer);
			g_free(incoming);
			continue;
		}

		if (length is 0) {
			dbg(LOG_INFO, DEBUG_NET, "Connection closed by peer\n");
			g_error_free(error);
			g_object_unref(incoming->peer);
			g_free(incoming);
			continue;
		}

		/*
		 * if the datagram has been received, call the passed
		 * callback to manage it
		 */
		dbg(LOG_INFO, DEBUG_NET, "Datagram received");
		if (context->tp) {
			/* pass the request to the thread pool */
			error = NULL;
			g_thread_pool_push(context->tp, incoming, &error);
			if (error) {
				dbg(LOG_ERR, DEBUG_NET, "Error pushing new request into thread pool: %s", error->message);
				g_error_free(error);
			} else {
				dbg(LOG_INFO, DEBUG_NET, "New request pushed into thread pool");
			}
		} else {
			/* directly call the callback to manage the request */
			context->callback(context->socket, incoming->peer, incoming->buffer);

			/* unref GLib objects to allow their disposal */
			g_object_unref(incoming->peer);
			g_free(incoming);
		}
		dbg(LOG_INFO, DEBUG_NET, "Ready for receiving another datagram");
	}

	return 0;
}

/**
 * Start a UDP service
 */
magma_udp_service_context *magma_start_udp_service(
	const gchar *service_name,
	const gchar *address,
	guint16 port,
	magma_udp_service_callback callback,
	GFunc pool_callback)
{
	GError * error = NULL;

	/*
	 * Create the socket
	 */
	GSocket *socket = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &error);
	if (error) {
		dbg(LOG_INFO, DEBUG_NET, "Error creating the socket: %s\n", error->message);
		g_error_free(error);
		exit (1);
	}

	/*
	 * Parse the address into a GInetAddress
	 */
	GInetAddress *inet_address = g_inet_address_new_from_string(address);
	if (!inet_address) {
		dbg(LOG_INFO, DEBUG_NET, "Address %s could not be parsed\n", address);
		g_object_unref(socket);
	}

	/*
	 * Create the GSocketAddress object representing the local end point
	 */
	GSocketAddress *saddr = g_inet_socket_address_new(inet_address, port);
	if (!saddr) {
		dbg(LOG_INFO, DEBUG_NET, "Error creating local end point GSocketAddress\n");
		g_object_unref(socket);
		g_object_unref(inet_address);
		exit (1);
	}

	/*
	 * Bind the socket to the local end point
	 */
	if (!g_socket_bind(socket, saddr, FALSE, &error)) {
		dbg(LOG_INFO, DEBUG_NET, "Error binding the socket: %s", error->message);
		g_error_free(error);
		g_object_unref(socket);
		g_object_unref(inet_address);
		g_object_unref(saddr);
		exit (1);
	}

	/*
	 * Create a context for this service
	 */
	magma_udp_service_context *context = g_new0(magma_udp_service_context, 1);
	if (!context) {
		dbg(LOG_INFO, DEBUG_NET, "Internal error allocating UDP context\n");
		g_object_unref(socket);
		g_object_unref(inet_address);
		g_object_unref(saddr);
		exit (1);
	}

	/*
	 * Create a thread pool to manage flare requests
	 */
	if (pool_callback) {
		dbg(LOG_INFO, DEBUG_NET, "Starting thread pool for %s", service_name);
		context->tp = g_thread_pool_new(pool_callback, NULL, -1, FALSE, &error);
		if (!context->tp) {
			dbg(LOG_ERR, DEBUG_NET, "Error spawning thread pool for %s: %s", service_name, error->message);
			g_error_free(error);
			g_object_unref(socket);
			g_object_unref(inet_address);
			g_object_unref(saddr);
			exit (1);
		}
	}

	/*
	 * fill the rest of the context
	 */
	context->callback = callback;
	context->socket = socket;
	context->description = g_strdup(service_name);
	g_strlcpy(context->ip_addr, address, MAX_IP_LENGTH);
	context->port = port;

	/*
	 * Spawn a thread to manage this service
	 */
	GThread *thread = g_thread_try_new(service_name, (GThreadFunc) magma_manage_udp_service, context, &error);
	if (!thread) {
		dbg(LOG_INFO, DEBUG_NET, "Error spawning service thread: %s", error->message);
		g_error_free(error);
		g_object_unref(socket);
		g_object_unref(inet_address);
		g_object_unref(saddr);
		g_thread_pool_free(context->tp, TRUE, TRUE);
		exit (1);
	}

	return (context);
}

GMutex magma_transaction_mutex;
guint16 magma_transaction = 0;

/**
 * Set request header fields
 *
 * @param request the request struct
 * @param req_type the MAGMA_OP_TYPE_* type of the request
 * @param uid the UID placing the call
 * @param gid the GID placing the call
 */
gchar *magma_format_request_header(
	gchar *request,
	magma_optype req_type,
	uid_t uid,
	gid_t gid,
	magma_transaction_id *tid,
	magma_ttl ttl)
{
	if (!request) {
		dbg(LOG_ERR, DEBUG_ERR, "NULL request passed");
		return (NULL);
	}

	gchar *ptr = request;

	/* serialize request type and TTL */
	ptr = magma_serialize_8(ptr, req_type);
	ptr = magma_serialize_8(ptr, ttl);

	/*
	 * Serialize the transaction ID which has been passed or create
	 * a new ID and serialize it
	 */
	if (! *tid) {
		/* lock the transaction mutex to synchronize its access */
		g_mutex_lock(&magma_transaction_mutex);

		/* increment the transaction, jumping the 0 ID */
		magma_transaction++;
		if (!magma_transaction) magma_transaction++; // no zero transaction allowed

		/* save the transaction id to return it to the caller */
		*tid = magma_transaction;
		g_mutex_unlock(&magma_transaction_mutex);
	}

	/* serialize the transaction ID */
	ptr = magma_serialize_16(ptr, *tid);

	/* serialize UID and GID */
	ptr = magma_serialize_32(ptr, uid);
	ptr = magma_serialize_32(ptr, gid);

	return (ptr);
}

/**
 * Deserialize the request header.
 *
 * @param buffer the network buffer to deserialize
 * @param request the magma_request struct to save the headers
 * @return a pointer to the buffer still to be deserialized
 */
gchar *magma_parse_request_header(gchar *buffer, magma_request *request)
{
	if (!buffer) {
		dbg(LOG_ERR, DEBUG_ERR, "NULL buffer passed");
		return (NULL);
	}

	gchar *ptr = buffer;

	ptr = magma_deserialize_8 (ptr, &request->generic_request.header.type);
	ptr = magma_deserialize_8 (ptr, &request->generic_request.header.ttl);
	ptr = magma_deserialize_16(ptr, &request->generic_request.header.transaction_id);
	ptr = magma_deserialize_32(ptr, &request->generic_request.header.uid);
	ptr = magma_deserialize_32(ptr, &request->generic_request.header.gid);

	return (ptr);
}

/**
 * Serialize a response header
 *
 * @param response the gchar * buffer that holds the response
 * @param res the operation result
 * @param err_no the operation errno, if any
 * @param transaction_id the ID assigned to this transaction, as received from the remote peer
 * @return a pointer to the buffer after the header to keep serializing more payload
 */
gchar *magma_format_response_header(
	gchar *response,
	magma_result res,
	magma_errno err_no,
	magma_transaction_id transaction_id)
{
	if (!response) {
		dbg(LOG_ERR, DEBUG_ERR, "NULL response passed");
		return (NULL);
	}

	gchar *ptr = response;
	ptr = magma_serialize_16(ptr, err_no);
	ptr = magma_serialize_32(ptr, res);
	ptr = magma_serialize_16(ptr, transaction_id);
	return (ptr);
}

/**
 * Deserialize a response header
 *
 * @param buffer the gchar * buffer received from the wire
 * @param response the response structure
 * @return a pointer to the buffer still to be deserialized
 */
gchar *magma_parse_response_header(gchar *buffer, magma_response *response)
{
	if (!buffer) {
		dbg(LOG_ERR, DEBUG_ERR, "NULL buffer passed");
		return (NULL);
	}

	/* force err_no to 0 if res is not -1 */
	if (response->generic_response.header.res is -1)
		response->generic_response.header.err_no = 0;

	gchar *ptr = buffer;

	ptr = magma_deserialize_16(ptr, (guint16 *) &response->generic_response.header.err_no);
	ptr = magma_deserialize_32(ptr, (guint32 *) &response->generic_response.header.res);
	ptr = magma_deserialize_16(ptr, &response->generic_response.header.transaction_id);

	return (ptr);

}

/**
 * Append src content to buffer and returns a pointer to the
 * next byte available in buffer
 *
 * @param buffer the destination buffer
 * @param src the source buffer
 * @size the amount of src to be copied
 * @return a pointer inside buffer to the next available location for insertion
 */
gchar *magma_append_to_buffer(gchar *buffer, const gchar *src, guint16 size) {
	memcpy(buffer, src, size);
	return (buffer + size);
}

gchar *magma_serialize_64(gchar *buffer, guint64 src) {
	guint64 base = src;
	guint64 tmp = magma_htonll(base);
	return magma_append_to_buffer(buffer, (gchar *) &tmp, sizeof(guint64));
}

gchar *magma_serialize_32(gchar *buffer, guint32 src) {
	guint32 base = src;
	guint32 tmp = htonl(base);
	return magma_append_to_buffer(buffer, (gchar *) &tmp, sizeof(guint32));
}

gchar *magma_serialize_16(gchar *buffer, guint16 src) {
	guint16 base = src;
	guint16 tmp = htons(base);
	return magma_append_to_buffer(buffer, (gchar *) &tmp, sizeof(guint16));
}

gchar *magma_serialize_8(gchar *buffer, guint8 src) {
	guint8 tmp = src;
	return magma_append_to_buffer(buffer, (gchar *) &tmp, sizeof(guint8));
}

gchar *magma_serialize_string(gchar *buffer, const gchar *string) {
	const gchar *_string = string ? string : "";

	/*
	 * First serialize the string length
	 */
	guint16 length = strlen(_string);
	gchar *ptr = magma_serialize_16(buffer, length);

	/*
	 * Then serialize the string itself
	 */
	ptr = magma_append_to_buffer(ptr, _string, length);
	return (ptr);
}

/**
 * Extracts size byted from buffer and copy them into dst.
 * Returns a pointer to next available byte for copy
 *
 * @param buffer the source buffer
 * @param dst the destination buffer
 * @size the amount of src to be copied
 * @return a pointer inside buffer to the next available location for extraction
 */
gchar *magma_extract_from_buffer(gchar *buffer, gchar *dst, guint16 size) {
	memcpy(dst, buffer, size);
	return (buffer + size);
}

gchar *magma_deserialize_64(gchar *buffer, guint64 *dst) {
	gchar *position = magma_extract_from_buffer(buffer, (gchar *) dst, sizeof(guint64));
	*dst = magma_ntohll(*dst);
	return (position);
}

gchar *magma_deserialize_32(gchar *buffer, guint32 *dst) {
	gchar *position = magma_extract_from_buffer(buffer, (gchar *) dst, sizeof(guint32));
	*dst = ntohl(*dst);
	return (position);
}

gchar *magma_deserialize_16(gchar *buffer, guint16 *dst) {
	gchar *position = magma_extract_from_buffer(buffer, (gchar *) dst, sizeof(guint16));
	*dst = ntohs(*dst);
	return (position);
}

gchar *magma_deserialize_8(gchar *buffer, guint8 *dst) {
	return magma_extract_from_buffer(buffer, (gchar *) dst, sizeof(guint8));
}

gchar *magma_deserialize_string(gchar *buffer, gchar *dst) {
	/*
	 * First deserialize the string length
	 */
	guint16 size;
	gchar *ptr = magma_deserialize_16(buffer, &size);

	/*
	 * Then deserialize the string itself into dst, null-terminating it.
	 * The string buffer is cleared for the required size + 1 to allocate
	 * the '\0' terminator
	 */
	memset(dst, 0, size + 1);
	ptr = magma_extract_from_buffer(ptr, dst, size);

	return (ptr);
}

/**
 * Performs a perfect send and receive loop. The buffer is sent
 * MAGMA_RETRY_LIMIT times until a response is received. After every
 * transmission, the response is waited for MAGMA_AGAIN_LIMIT times
 * before sending the request again.
 *
 * @param socket a GSocket object
 * @param peer a GSocketAddress object
 * @param request_buffer the serialized request
 * @param buffer_length the length in bytes of request_buffer
 * @param receiver a callback function to receive the response (usually a magma_pktar_* function)
 * @param response a pointer to a magma_response struct to hold the response
 */
void magma_send_and_receive_base(
	GSocket *socket,
	GSocketAddress *peer,
	gchar *request_buffer,
	guint16 buffer_length,
	magma_receiver_func receiver,
	magma_response *response)
{
	int retry_counter = 0;
	for (; retry_counter < MAGMA_RETRY_LIMIT; retry_counter++) {
		magma_send_buffer(socket, peer, request_buffer, buffer_length);

		int again_counter = 0;
		for (; again_counter < MAGMA_AGAIN_LIMIT; again_counter++) {
			receiver(socket, peer, response);
			if (response->generic_response.header.status isNot G_IO_STATUS_AGAIN) {
				// response->generic_response.header.err_no = EIO; // ENOTCONN - ECONNREFUSED - EHOSTDOWN - EHOSTUNREACH
				return;
			}
		}

		if (response->generic_response.header.status is G_IO_STATUS_NORMAL) return;
	}
}

// vim:ts=4:nocindent:autoindent
