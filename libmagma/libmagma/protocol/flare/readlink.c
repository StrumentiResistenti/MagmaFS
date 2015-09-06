/*
   MAGMA -- protocol_flare/readlink.c
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
magma_pktqs_readlink(
	GSocket *socket,
	GSocketAddress *peer,
	uid_t uid,
	gid_t gid,
	const gchar *path,
	magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_READLINK, uid, gid, &tid, MAGMA_TERMINAL_TTL);
	ptr = magma_serialize_string(ptr, path);

	magma_log_transaction(MAGMA_OP_TYPE_READLINK, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_readlink, response);

	return (tid);
}

void magma_pktqr_readlink(gchar *buffer, magma_flare_request *request)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_string(ptr, request->body.readlink.path);
}

void magma_pktas_readlink(
	GSocket *socket,
	GSocketAddress *peer,
	int res,
	const gchar *path,
	int error,
	magma_transaction_id tid,
	magma_flags flags)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, error, tid, flags);
	ptr = magma_serialize_string(ptr, path);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

GIOStatus magma_pktar_readlink(GSocket *socket, GSocketAddress *peer, magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (G_IO_STATUS_NORMAL != response->header.status || -1 == response->header.res || !ptr) return (G_IO_STATUS_AGAIN);

	ptr = magma_deserialize_string(ptr, response->body.readlink.path);

	return (G_IO_STATUS_NORMAL);
}
