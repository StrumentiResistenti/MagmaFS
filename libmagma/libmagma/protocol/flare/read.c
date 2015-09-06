/*
   MAGMA -- protocol_flare/read.c
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

magma_transaction_id
magma_pktqs_read(
	GSocket *socket,
	GSocketAddress *peer,
	uid_t uid,
	gid_t gid,
	guint32 size,
	guint64 offset,
	const gchar *path,
	magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_READ, uid, gid, &tid, MAGMA_TERMINAL_TTL);
	ptr = magma_serialize_32(ptr, size);
	ptr = magma_serialize_64(ptr, offset);
	ptr = magma_serialize_string(ptr, path);

	magma_log_transaction(MAGMA_OP_TYPE_READ, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_read, response);

	return (tid);
}

void magma_pktqr_read(gchar *buffer, magma_flare_request *request)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_32(ptr, &request->body.read.size);
	ptr = magma_deserialize_64(ptr, &request->body.read.offset);
	ptr = magma_deserialize_string(ptr, request->body.read.path);
}

void magma_pktas_read(
	GSocket *socket,
	GSocketAddress *peer,
	gint32 res,
	int error,
	gchar *read_buffer,
	magma_transaction_id tid,
	magma_flags flags)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, error, tid, flags);
	memcpy(ptr, read_buffer, res);
	ptr += res;

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

GIOStatus magma_pktar_read(GSocket *socket, GSocketAddress *peer, magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (G_IO_STATUS_NORMAL != response->header.status || -1 == response->header.res || !ptr) return (G_IO_STATUS_AGAIN);

	memcpy(response->body.read.buffer, ptr, response->header.res);

	return (G_IO_STATUS_NORMAL);
}
