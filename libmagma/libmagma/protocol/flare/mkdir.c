/*
   MAGMA -- protocol_flare/mkdir.c
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
magma_pktqs_mkdir(
	GSocket *socket,
	GSocketAddress *peer,
	magma_ttl ttl,
	uid_t uid,
	gid_t gid,
	mode_t mode,
	const gchar *path,
	magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_MKDIR, uid, gid, &tid, ttl);
	ptr = magma_serialize_32(ptr, mode);
	ptr = magma_serialize_string(ptr, path);

	magma_log_transaction(MAGMA_OP_TYPE_MKDIR, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_mkdir, response);

	return (tid);
}

void magma_pktqr_mkdir(gchar *buffer, magma_flare_request *request)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_32(ptr, &request->body.mkdir.mode);
	ptr = magma_deserialize_string(ptr, request->body.mkdir.path);

	(void) ptr;
}

void magma_pktas_mkdir(
	GSocket *socket,
	GSocketAddress *peer,
	int res,
	int error,
	magma_transaction_id tid,
	magma_flags flags)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, error, tid, flags);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

GIOStatus magma_pktar_mkdir(GSocket *socket, GSocketAddress *peer, magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	return (!ptr ? G_IO_STATUS_AGAIN : G_IO_STATUS_NORMAL);
}
