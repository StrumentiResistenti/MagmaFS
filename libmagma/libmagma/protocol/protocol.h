/*
   MAGMA -- protocol.h
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Defines protocol constants and opcodes.

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

#ifndef _MAGMA_PROTOCOL_H
#define _MAGMA_PROTOCOL_H

#include "protocol_pkt.h"

/**
 * Describes an incoming UDP request
 */
typedef struct {
	GSocket *socket;
	GSocketAddress *peer;
	gchar buffer[MAGMA_MESSAGE_MAX_SIZE];
	gchar ip_addr[MAX_IP_LENGTH];
	guint16 port;
} magma_incoming_request;

#include "balancer/protocol_balancer.h"
#include "console/protocol_console.h"
#include "flare/protocol_flare.h"
#include "node/protocol_node.h"

/**
 * Used to access the request header independently from
 * its inner nature (node, flare, ...)
 */
typedef struct {
	magma_request_header header;
} magma_generic_request;

/*
 * Every possible incoming request type must be added to this union
 */
typedef union {
	magma_generic_request generic_request;
	magma_flare_request flare_request;
	magma_node_request node_request;
} magma_request;

extern gchar *magma_format_request_header(
	gchar *request, magma_optype req_type, uid_t uid, gid_t gid, magma_transaction_id *tid, magma_ttl ttl);

extern gchar *magma_parse_request_header(gchar *buffer, magma_request *request);

/* receive a request */
extern gchar *magma_pktqr(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_request *request);

#if 0
/**
 * Quick macro to set response header fields
 *
 * @param response the response struct
 * @param res the response result code
 * @param error the response error code (libc errno equivalent)
 */
#define magma_format_response_header(response, rescode, error) {\
	memset(&response, '\0', sizeof(response));\
	response.header.res = htonl(rescode);\
	response.header.err_no = htons(error);\
}
#endif

/**
 * Used to access the response header independently from
 * its inner nature (node, flare, ...)
 */
typedef struct {
	magma_response_header header;
} magma_generic_response;

/*
 * Every possible outgoing response type must be added to this union
 */
typedef union {
	magma_generic_response generic_response;
	magma_flare_response flare_response;
	magma_node_response node_response;
} magma_response;

extern gchar *magma_format_response_header(
	gchar *response,
	magma_result res,
	magma_errno err_no,
	magma_transaction_id transaction_id,
	magma_flags flags);

extern gchar *magma_parse_response_header(gchar *buffer, magma_response *response);

/* receive a response */
extern gchar *magma_pktar(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_response *response);

/**
 * the callback called by magma_manage_udp_service on
 * every UDP message received
 */
typedef void (*magma_udp_service_callback)(
	GSocket *socket,
	GSocketAddress *peer,
	gchar buffer[MAGMA_MESSAGE_MAX_SIZE]);

/**
 * the context of a UDP service, including its callback
 * function of type magma_udp_service_callback and the
 * socket the service is bound to
 */
typedef struct {
	/*
	 * A string describing the purpose of this pool
	 */
	gchar *description;

	/*
	 * local address and port
	 */
	gchar ip_addr[MAX_IP_LENGTH];
	guint16 port;

	/*
	 * the callback the pool will use
	 */
	magma_udp_service_callback callback;

	/*
	 * local socket
	 */
	GSocket *socket;

	/*
	 * remote peer
	 */
	GSocketAddress *peer;

	/*
	 * A pointer to the pool
	 */
	GThreadPool *tp;
} magma_udp_service_context;

extern magma_udp_service_context *magma_start_udp_service(
	const gchar *service_name,
	const gchar *address,
	guint16 port,
	magma_udp_service_callback callback,
	GFunc pool_callback);

/**
 * like strlen but +1, to host the ending '\0'.
 * Useful in calculating packet sizes.
 *
 * @param path the path to be measured
 * @return the path length + 1
 */
#define magma_path_length(path) strlen(path) + 1

/**
 * Copy one path into a destination, including its '\0' terminator
 *
 * @param destination the destination buffer
 * @param path the original path
 */
#define magma_copy_path(destination, path)\
	strcpy(destination, path);

/**
 * Copy two paths into a destination.
 * Used by two-paths functions like rename, link, symlink, ...
 * The destination buffer is assumed to be at least two times
 * MAGMA_TERMINATED_PATH_LENGTH long.
 *
 * @param destination the destination buffer
 * @param path1 the first path
 * @param path2 the second path
 */
#define magma_copy_paths(destination, path1, path2) \
	g_snprintf(destination, MAGMA_TERMINATED_PATH_LENGTH * 2, "%s%c%s", path1, '\0', path2)

/**
 * Points two variable to the paths contained in a request
 * paths[] field.
 *
 * @param paths a magma_request.body.optype.paths field holding the two paths
 * @param path1 a gchar* variable that will point to the first path
 * @param path2 a gchar* variable that will point to the second path
 */
#define magma_get_request_paths(paths, path1, path2) {\
	path1 = paths;\
	path2 = paths + strlen(path1) + 1;\
}

/*
 * function to initialize console commands
 */
extern void magma_init_console();

/*
 * if a node is not the first of the net, a join operation
 * should be performed.
 */
extern int magma_join_network(char *ipaddr, int port);

extern void magma_protocol_init();

extern gchar *magma_append_to_buffer(gchar *buffer, const gchar *src, guint16 size);
extern gchar *magma_serialize_64(gchar *buffer, guint64 src);
extern gchar *magma_serialize_32(gchar *buffer, guint32 src);
extern gchar *magma_serialize_16(gchar *buffer, guint16 src);
extern gchar *magma_serialize_8(gchar *buffer, guint8 src);
extern gchar *magma_serialize_string(gchar *buffer, const gchar *string);

extern gchar *magma_extract_from_buffer(gchar *buffer, gchar *dst, guint16 size);
extern gchar *magma_deserialize_64(gchar *buffer, guint64 *dst);
extern gchar *magma_deserialize_32(gchar *buffer, guint32 *dst);
extern gchar *magma_deserialize_16(gchar *buffer, guint16 *dst);
extern gchar *magma_deserialize_8(gchar *buffer, guint8 *dst);
extern gchar *magma_deserialize_string(gchar *buffer, gchar *dst);

/**
 * This function prototype must be conformed to by function
 * called to receive a response after sending a request. All
 * the magma_pktar_* functions must adhere to this prototype.
 */
typedef void (*magma_receiver_func) (GSocket *socket, GSocketAddress *peer, magma_response *response);

/**
 * A macro to call magma_send_and_receive_base with some
 * useful typecasting for free
 */
#define magma_send_and_receive(socket, peer, request_buffer, buffer_length, receiver, response) \
	magma_send_and_receive_base(socket, peer, request_buffer, buffer_length, \
		(magma_receiver_func) receiver, (magma_response *) response)

extern void magma_send_and_receive_base(
	GSocket *socket,
	GSocketAddress *peer,
	gchar *request_buffer,
	guint16 buffer_length,
	magma_receiver_func receiver,
	magma_response *response);

#define magma_log_transaction(opstring, tid, peer) {\
	gchar *remote = g_inet_address_to_string(g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(peer)));\
	guint16 port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(peer));\
	dbg(LOG_INFO, DEBUG_NET, "Sending " #opstring "#%05d to %s:%d", tid, remote, port);\
	g_free(remote);\
}

/**
 * When magma_send_and_received is called, the answer is
 * waited for MAGMA_AGAIN_LIMIT times, then the request is
 * sent again. This cycle will last for MAGMA_RETRY_LIMIT
 * cycles, after which magma_send_and_receive() just returns.
 */
#define MAGMA_RETRY_LIMIT 9
#define MAGMA_AGAIN_LIMIT 3

/*
 * A useful macro to simplify the syntax of flare callbacks registration
 */
#define magma_register_callback(optype, callback) {\
	magma_optype_callbacks[optype] = (magma_optype_callback) callback;\
	magma_optype_callbacks_desc[optype] = #optype;\
	dbg(LOG_INFO, DEBUG_FLARE, "Registering %s as flare optype %d (%s)", #callback, optype, #optype);\
}

#endif /* _MAGMA_PROTOCOL_H */

// vim:ts=4:nocindent
