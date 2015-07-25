/*
   MAGMA -- protocol_flare/commons.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Defines packets exchanged between clients and nodes and between nodes.

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

#include "../../magma.h"

/**
 * receive a generic magma_request.
 *
 * @param socket the GSocket used to receive
 * @param req the magma_request buffer to write in (must be allocated)
 * @return TRUE in case of success, FALSE otherwise
 */
gchar *magma_pktqr(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_request *request)
{
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	if (G_IO_STATUS_NORMAL != magma_receive_buffer(socket, peer, buffer, MAGMA_MAX_BUFFER_SIZE))
		return (NULL);

	gchar *ptr = magma_parse_request_header(buffer, request);

	return (ptr);
}

/**
 * receive a generic magma_response.
 *
 * Since the response is always preceded by a guint16 number
 * expressing its length, this function can read any type of
 * magma_response (see its union body) without wasting network
 * bandwidth and cpu time.
 *
 * @param socket the GSocket used to receive
 * @param response the magma_response buffer to write in (must be allocated)
 * @return TRUE in case of success, FALSE otherwise
 */
gchar *magma_pktar(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_response *response)
{
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	/* get the packet from the wire */
	response->generic_response.header.status = magma_receive_buffer(socket, peer, buffer, MAGMA_MAX_BUFFER_SIZE);

	/* if the GIOStatus is not normal, return NULL */
	if (G_IO_STATUS_NORMAL != response->generic_response.header.status) return (NULL);

	/* parse the response header */
	gchar *ptr = magma_parse_response_header(buffer, response);

	return (ptr);
}

magma_transaction_id
magma_pktqs_f_opendir(
	GSocket *socket,
	GSocketAddress *peer,
	uid_t uid,
	gid_t gid,
	const gchar *path,
	magma_offset offset,
	magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_F_OPENDIR, uid, gid, &tid, MAGMA_TERMINAL_TTL);

	ptr = magma_serialize_64(ptr, offset);
	ptr = magma_serialize_string(ptr, path);

	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_f_opendir, response);

	return (tid);
}

void magma_pktqr_f_opendir(gchar *buffer, magma_flare_request *request)
{
	gchar *ptr = buffer;

	ptr = magma_deserialize_64(ptr, &request->body.f_opendir.offset);
	ptr = magma_deserialize_string(ptr, request->body.f_opendir.path);
}

void magma_pktas_f_opendir(GSocket *socket, GSocketAddress *peer, int res, int error,
	gchar *payload, magma_offset offset, magma_size buffer_size, magma_size size, magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, error, tid);

	if (0 == res) {
		ptr = magma_serialize_64(ptr, offset);
		ptr = magma_serialize_64(ptr, size);
		ptr = magma_serialize_64(ptr, buffer_size);

		ptr = magma_append_to_buffer(ptr, payload, buffer_size);
		ptr += size;
	}

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

GIOStatus magma_pktar_f_opendir(GSocket *socket, GSocketAddress *peer, magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (!ptr) return (G_IO_STATUS_AGAIN);
	if (G_IO_STATUS_NORMAL != response->header.status) return (response->header.status);

	ptr = magma_deserialize_64(ptr, &response->body.f_opendir.offset);
	ptr = magma_deserialize_64(ptr, &response->body.f_opendir.size);
	ptr = magma_deserialize_64(ptr, &response->body.f_opendir.buffer_size);

	response->body.f_opendir.buffer = g_new0(gchar, response->body.f_opendir.buffer_size);
	if (response->body.f_opendir.buffer) memcpy(response->body.f_opendir.buffer, ptr, response->body.f_opendir.size);

	return (G_IO_STATUS_NORMAL);
}

#if 0
/************************************************\
 * FLARE_ADD_TO_PARENT                          *
\************************************************/

magma_transaction_id
magma_pktqs_add_flare_to_parent(
	GSocket *socket,
	GSocketAddress *peer,
	magma_ttl ttl,
	uid_t uid,
	gid_t gid,
	const gchar *path,
	magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_ADD_FLARE_TO_PARENT, uid, gid, &tid, ttl);

	ptr = magma_serialize_string(ptr, path);

	magma_log_transaction(MAGMA_OP_TYPE_ADD_FLARE_TO_PARENT, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_add_flare_to_parent, response);

	return (tid);
}

void magma_pktqr_add_flare_to_parent(gchar *buffer, magma_flare_request *request)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_string(ptr, request->body.add_flare_to_parent.path);
}

void magma_pktas_add_flare_to_parent(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, error, tid);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

GIOStatus magma_pktar_add_flare_to_parent(GSocket *socket, GSocketAddress *peer, magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	return (!ptr ? G_IO_STATUS_AGAIN : G_IO_STATUS_NORMAL);
}

/************************************************\
 * FLARE_REMOVE_FROM_PARENT                     *
\************************************************/

magma_transaction_id
magma_pktqs_remove_flare_from_parent(
	GSocket *socket,
	GSocketAddress *peer,
	magma_ttl ttl,
	uid_t uid,
	gid_t gid,
	const gchar *path,
	magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_REMOVE_FLARE_FROM_PARENT, uid, gid, &tid, ttl);

	ptr = magma_serialize_string(ptr, path);

	magma_log_transaction(MAGMA_OP_TYPE_REMOVE_FLARE_FROM_PARENT, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_remove_flare_from_parent, response);

	return (tid);
}

void magma_pktqr_remove_flare_from_parent(gchar *buffer, magma_flare_request *request)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_string(ptr, request->body.remove_flare_from_parent.path);
}

void magma_pktas_remove_flare_from_parent(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, error, tid);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);

	(void) ptr;
}

GIOStatus magma_pktar_remove_flare_from_parent(GSocket *socket, GSocketAddress *peer, magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	return (!ptr ? G_IO_STATUS_AGAIN : G_IO_STATUS_NORMAL);
}

#endif
