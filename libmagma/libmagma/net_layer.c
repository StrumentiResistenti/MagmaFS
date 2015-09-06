/*
   MAGMA -- net_layer.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Provide network basic operations like socket creation, data read()
   and send() using facilities which well behave in case of errors.

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

#include "magma.h"

#define MAGMA_CACHE_SOCKETS TRUE

#if MAGMA_CACHE_SOCKETS
GHashTable *magma_socket_cache;

typedef struct {
	GSocket *socket;
	GSocketAddress *peer;
} magma_socket_cacher;

gboolean magma_compare_cached_sockets(gchar *key1, gchar *key2)
{
	return ((g_strcmp0(key1, key2) == 0) ? TRUE : FALSE);
}

void magma_destroy_cached_sockets(magma_socket_cacher *sc)
{
	g_object_unref(sc->peer);
	g_object_unref(sc->socket);
	g_free(sc);
}
#endif

void magma_init_net_layer()
{
#if MAGMA_CACHE_SOCKETS
	magma_socket_cache = g_hash_table_new_full(
		g_str_hash,
		(GEqualFunc) magma_compare_cached_sockets,
		g_free,
		(GDestroyNotify) magma_destroy_cached_sockets);
#endif
}

/**
 * Open a connection to a magma server
 *
 * @param host remote host as a string
 * @param port remote port as an integer
 * ; if no port is present, default port is used; host can be an address or a host name
 * @return a magma_connection structure
 */
GSocket *magma_open_client_connection(const gchar *host, const uint16_t port, GSocketAddress **peer)
{
#if MAGMA_CACHE_SOCKETS
	/*
	 * lookup the socket in the cache
	 */
	gchar *key = g_strdup_printf("%s:%d", host, port);
	magma_socket_cacher *sc = g_hash_table_lookup(magma_socket_cache, key);
	if (sc) {
		g_free(key);
		*peer = sc->peer;
		return (sc->socket);
	}
#endif

	/*
	 * Create the socket
	 */
	GError *error = NULL;
	GSocket *socket = g_socket_new(
		G_SOCKET_FAMILY_IPV4,
		G_SOCKET_TYPE_DATAGRAM,
		G_SOCKET_PROTOCOL_UDP,
		&error);

	if (error) {
		fprintf(stderr, "Error on socket creation: %s\n", error->message);
		g_error_free(error);
		return (NULL);
	}

	/*
	 * Avoid blocking on incoming data
	 */
	g_socket_set_blocking(socket, FALSE);

	/*
	 * Create a GSocketAddress to represent the destination
	 */
	GInetAddress *address = g_inet_address_new_from_string(host);
	*peer = g_inet_socket_address_new(address, port);
	if (!*peer) {
		fprintf(stderr, "Error creating GSocketAddress\n");
		g_object_unref(socket);
		socket = NULL;
	}

	g_object_unref(address);

#if MAGMA_CACHE_SOCKETS
	/*
	 * save the socket in the cache
	 */
	if (!g_hash_table_contains(magma_socket_cache, key)) {
		magma_socket_cacher *sc = g_new0(magma_socket_cacher, 1);
		if (sc) {
			sc->peer = *peer;
			sc->socket = socket;
			g_hash_table_insert(magma_socket_cache, key, sc);
		}
	}
#endif

	return (socket);
}

/**
 * Close an open client connection, by first sending a graceful
 * request and then calling close_connection().
 *
 * @param con the open connection to be closed
 */
void magma_close_client_connection(GSocket *socket, GSocketAddress *peer)
{
#if MAGMA_SEND_EXPLICIT_CLOSE_CONNECTION
	magma_request request;
	request.flare_request.header.type = MAGMA_OP_TYPE_CLOSECONNECTION;
	request.flare_request.header.uid = 0;
	request.flare_request.header.gid = 0;

	guint16 packet_length = sizeof(magma_request_header);

	magma_send_buffer(socket, peer, (gchar *) &request, packet_length);
#endif

#if !MAGMA_CACHE_SOCKETS
	g_object_unref(socket);
	g_object_unref(peer);
#endif
}

/*
 * Send a request/response buffer over the wire. First the buffer
 * length is sent as a guint16 type. Then the buffer itself gets streamed.
 *
 * @param connection an open connection
 * @param request the buffer to be sent over the wire
 * @param size the buffer size
 */
GIOStatus magma_send_buffer(GSocket *socket, GSocketAddress *peer, const gchar *buffer, guint16 size)
{
	/* null size? then return */
	if (!size) return (G_IO_STATUS_NORMAL);

	GError *error = NULL;
	int sent = g_socket_send_to(socket, peer, buffer, size, NULL, &error);

	if (-1 == sent) {
		dbg(LOG_ERR, DEBUG_NET, "Error sending UDP datagram: %s", error ? error->message : "unknown reason");
		g_error_free(error);
		return (G_IO_STATUS_ERROR);
	}

	return (G_IO_STATUS_NORMAL);
}

/*
 * Receive a request/response buffer over the wire. First the buffer
 * length is received as a guint16 type. Then the buffer itself gets read.
 *
 * @param connection an open connection
 * @param request the buffer to be sent over the wire
 * @param size the buffer size
 */
GIOStatus magma_receive_buffer(GSocket *socket, GSocketAddress *peer, gchar *buffer, gsize max_size)
{
	int read = FALSE;

	do {
		int cycle = 1;
		while (!g_socket_condition_timed_wait(socket, G_IO_IN, cycle * MAGMA_WAIT_CYCLE_UNIT, NULL, NULL)) {
			cycle++;
			if (MAGMA_MAX_WAIT_CYCLE_ITERATIONS == cycle) return (G_IO_STATUS_AGAIN);
		}

		GError *error = NULL;
		gssize received = g_socket_receive_from(socket, &peer, buffer, max_size, NULL, &error);
		if (-1 == received) {
			if (G_IO_ERROR_WOULD_BLOCK == error->code) {
				dbg(LOG_ERR, DEBUG_NET, "Blocking operation will be retried");
			} else {
				dbg(LOG_ERR, DEBUG_NET, "Error receiving buffer: %s", error->message);
			}
			if (error) g_error_free(error);
			return (G_IO_STATUS_ERROR);
		} else {
			read = TRUE;
		}

		if (error) g_error_free(error);
	} while (!read);

	return (G_IO_STATUS_NORMAL);
}

#if 0

static gchar __G_IO_STATUS_ERROR[]  = "G_IO_STATUS_ERROR";
static gchar __G_IO_STATUS_NORMAL[] = "G_IO_STATUS_NORMAL";
static gchar __G_IO_STATUS_EOF[]    = "G_IO_STATUS_EOF";
static gchar __G_IO_STATUS_AGAIN[]  = "G_IO_STATUS_AGAIN";

/**
 * Transform a GIOStatus in its corresponding printable form
 */
static gchar *magma_explain_io_status(GIOStatus iostatus)
{
	switch (iostatus) {
		case G_IO_STATUS_ERROR:		return __G_IO_STATUS_ERROR;
		case G_IO_STATUS_NORMAL:	return __G_IO_STATUS_NORMAL;
		case G_IO_STATUS_EOF:		return __G_IO_STATUS_EOF;
		case G_IO_STATUS_AGAIN:		return __G_IO_STATUS_AGAIN;
	};

	return "";
}

/**
 * Open a connection to a magma server
 *
 * @param host_and_port remote host and port as a string; if no port is present, default port is used; host can be an address or a host name
 * @return a magma_connection structure
 */
GSocket *magma_open_client_connection(const gchar *host, const uint16_t port, GSocketAddress **peer)
{
	GError *error = NULL;

	/*
	 * Setup the connection
	 */
	magma_connection *con	= g_new0(magma_connection, 1);
	if (!con) {
		dbg(LOG_ERR, DEBUG_NET, "Error allocating connection structure");
		return (NULL);
	}

	con->client				= g_socket_client_new();
	con->connection			= g_socket_client_connect_to_host(con->client, host_and_port, MAGMA_PORT, NULL, &error);

	if (error) {
		dbg(LOG_ERR, DEBUG_NET, "Error opening connection: %s", error->message);
		g_error_free(error);
		g_free(con);
		return (NULL);
	}

	/* keep the connection open */
	g_object_ref(con->connection);

	/* get some objects from the connection */
	con->socket				= g_socket_connection_get_socket(con->connection);

    g_socket_set_blocking(con->socket, FALSE);

    con->sockaddr			= g_socket_connection_get_remote_address(con->connection, NULL);
    con->addr				= g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(con->sockaddr));
    con->port				= g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(con->sockaddr));
    con->printable_addr		= g_inet_address_to_string(con->addr);
    con->fd					= g_socket_get_fd(con->socket);
    con->channel			= g_io_channel_unix_new(con->fd);

	if (G_IO_STATUS_NORMAL != g_io_channel_set_encoding(con->channel, NULL, &error)) {
		dbg(LOG_ERR, DEBUG_NET, "Error setting I/O channel encoding to raw: %s", error ? error->message : "unknown");
		if (error) g_error_free(error);
		g_io_stream_close((GIOStream *) con->connection, NULL, NULL);
		g_object_unref(con->socket);
		g_free(con);
		return (NULL);
	}

	g_io_channel_set_buffered(con->channel, FALSE);

	return (con);
}

/**
 * Close an open connection
 */
void close_connection(magma_connection *connection)
{
	if (connection->channel) {
		GError *error = NULL;
		g_io_channel_shutdown(connection->channel, TRUE, &error);
		if (error) {
			dbg(LOG_ERR, DEBUG_NET, "Error closing IO channel: %s", error->message);
			g_error_free(error);
			g_free(connection);
			return;
		}

		g_io_channel_unref(connection->channel);
	}

	// g_input_stream_close(connection->in, NULL, NULL);
	// g_output_stream_close(connection->out, NULL, NULL);

	g_io_stream_close((GIOStream *) connection->connection, NULL, NULL);
	g_socket_shutdown(connection->socket, 1, 1, NULL);

	if (connection->socket)	g_object_unref(connection->socket);
	if (connection->client)	g_object_unref(connection->client);
	if (connection->sockaddr)	g_object_unref(connection->sockaddr);
	if (connection->addr)		g_object_unref(connection->addr);

	g_free(connection->printable_addr);
	g_free(connection);
}

/**
 * Send a whole buffer over the wire
 *
 * @param connection an established connection
 * @param buffer the data to be sent
 * @param size data size
 * @param flush if true (FLUSH_CHANNEL) the channel is flushed, if false (DONT_FLUSH_CHANNEL) it is not
 * @param caption the begin of the error message
 * @return a GIOStatus
 */
GIOStatus
magma_perfect_send(magma_connection *connection, const gchar *buffer, guint16 size, gboolean flush, const gchar *caption)
{
	/* null size, then return */
	if (!size) return (G_IO_STATUS_NORMAL);

	GError *error = NULL;
	GIOStatus status = G_IO_STATUS_NORMAL;

	/* point the buffer with a movable pointer */
	const gchar *buffer_pointer = buffer;

	/* keeping track of sending operations */
	gsize written;
	guint16 remaining_size = size;

    /* send content */
    do {
		status = g_io_channel_write_chars(connection->channel, buffer_pointer, remaining_size, &written, &error);

		switch (status) {
			case G_IO_STATUS_ERROR:
			case G_IO_STATUS_EOF:
				if (error) {
					dbg(LOG_ERR, DEBUG_NET, "%s: [%s] %s", caption, magma_explain_io_status(status), error->message);
					g_error_free(error);
				} else {
					dbg(LOG_ERR, DEBUG_NET, "%s", caption);
				}
				return (status);

			case G_IO_STATUS_AGAIN:
			case G_IO_STATUS_NORMAL:
			default:
				buffer_pointer += written;
				remaining_size -= written;
				break;
		}
	} while (remaining_size > 0);

	/* flush the channel if requested */
	if (flush) {
		status = g_io_channel_flush(connection->channel, &error);
		if (error) {
			dbg(LOG_ERR, DEBUG_NET, "%s: [%s] %s", caption, magma_explain_io_status(status), error->message);
			g_error_free(error);
		}
	}

    return (G_IO_STATUS_NORMAL);
}

/*
 * Send a request/response buffer over the wire. First the buffer
 * length is sent as a guint16 type. Then the buffer itself gets streamed.
 *
 * @param connection an open connection
 * @param request the buffer to be sent over the wire
 * @param size the buffer size
 */
GIOStatus magma_send_buffer(GSocket *socket, GSocketAddress *peer, const gchar *buffer, guint16 size)
{
	/* prepare the size for the wire */
	guint16 wire_size = htons(size);

	/* sending the buffer size */
	GIOStatus status = magma_perfect_send(
		connection,
		(gchar *) &wire_size,
		MAGMA_BUFFER_SIZE,
		DONT_FLUSH_CHANNEL,
		"Error sending buffer size");

	/* sending the buffer content */
	if (G_IO_STATUS_NORMAL == status) {
		status = magma_perfect_send(
			connection, buffer, size,
			FLUSH_CHANNEL,
			"Error sending buffer content");
	}

	return (status);
}

/**
 * Receive a whole buffer of data
 *
 * @param connection an established connection
 * @param buffer the allocated room for received data
 * @param size buffer's dimension
 * @param caption the beginning of the error message
 * @return a GIOStatus
 */
GIOStatus
perfect_receive(magma_connection *connection, gchar *buffer, guint16 size, const gchar *caption)
{
	/* null size, then return */
	if (!size) return (G_IO_STATUS_NORMAL);

	GError *error = NULL;
	GIOStatus status = G_IO_STATUS_NORMAL;

	/* point the buffer with a movable pointer */
	gchar *buffer_pointer = buffer;

	/* keeping track of sending operations */
	gsize read;
	guint16 remaining_size = size;

    /* send content */
    do {
		status = g_io_channel_read_chars(connection->channel, buffer_pointer, size, &read, &error);

		switch (status) {
			case G_IO_STATUS_ERROR:
			case G_IO_STATUS_EOF:
				if (error) {
					dbg(LOG_ERR, DEBUG_NET, "%s: %s", caption, error->message);
					g_error_free(error);
				} else {
					dbg(LOG_ERR, DEBUG_NET, "%s", caption);
				}
    			g_io_channel_flush(connection->channel, NULL);
				return (status);

			case G_IO_STATUS_AGAIN:
			case G_IO_STATUS_NORMAL:
			default:
				buffer_pointer += read;
				remaining_size -= read;
				break;
		}
	} while (remaining_size > 0);

    g_io_channel_flush(connection->channel, NULL);
    return (G_IO_STATUS_NORMAL);
}

/*
 * Receive a request/response buffer over the wire. First the buffer
 * length is received as a guint16 type. Then the buffer itself gets read.
 *
 * @param connection an open connection
 * @param request the buffer to be sent over the wire
 * @param size the buffer size
 */
GIOStatus magma_receive_buffer(GSocket *socket, GSocketAddress *peer, gchar *buffer, gsize max_size)
{
	/* receiving buffer length */
	guint16 wire_size;
	GIOStatus status = perfect_receive(connection, (gchar *) &wire_size, MAGMA_BUFFER_SIZE, "Error receiving data size from the wire");

	/* receiving buffer content */
	if ((G_IO_STATUS_NORMAL == status) && (wire_size)) {
		guint16 size = ntohs(wire_size);

		if (size > max_size) {
			dbg(LOG_ERR, DEBUG_ERR, "Error receiving buffer: size %u is greater than max_size (%lu)", size, max_size);
			status = G_IO_STATUS_ERROR;
		} else {
			status = perfect_receive(connection, buffer, size, "Error receiving data content from the wire");
		}
	}

	return (status);
}
#endif

// vim:ts=4:nocindent:autoindent
