/*
   MAGMA -- protocol_flare/mknod.c
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
magma_pktqs_mknod(
	GSocket *socket,
	GSocketAddress *peer,
	magma_ttl ttl,
	uid_t uid,
	gid_t gid,
	mode_t mode,
	dev_t rdev,
	const gchar *path,
	magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_MKNOD, uid, gid, &tid, ttl);
	ptr = magma_serialize_32(ptr, mode);
	ptr = magma_serialize_64(ptr, rdev);
	ptr = magma_serialize_string(ptr, path);

	magma_log_transaction(MAGMA_OP_TYPE_MKNOD, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_mknod, response);

	return (tid);
}

void magma_pktqr_mknod(gchar *buffer, magma_flare_request *request)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_32(ptr, &request->body.mknod.mode);
	ptr = magma_deserialize_64(ptr, &request->body.mknod.rdev);
	ptr = magma_deserialize_string(ptr, request->body.mknod.path);
}

void magma_pktas_mknod(
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

GIOStatus magma_pktar_mknod(GSocket *socket, GSocketAddress *peer, magma_flare_response *response) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	return (!ptr ? G_IO_STATUS_AGAIN : G_IO_STATUS_NORMAL);
}
