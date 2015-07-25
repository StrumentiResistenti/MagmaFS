/*
   MAGMA -- protocol_flare/statfs.c
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
magma_pktqs_statfs(
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
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_STATFS, uid, gid, &tid, MAGMA_TERMINAL_TTL);
	ptr = magma_serialize_string(ptr, path);

	magma_log_transaction(MAGMA_OP_TYPE_STATFS, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_statfs, response);

	return (tid);
}

void magma_pktqr_statfs(gchar *buffer, magma_flare_request *request)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_string(ptr, request->body.statfs.path);
}

void magma_pktas_statfs(GSocket *socket, GSocketAddress *peer, int res, struct statfs *statbuf, int error, magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, error, tid);

    ptr = magma_serialize_64(ptr, statbuf->f_blocks);
	ptr = magma_serialize_64(ptr, statbuf->f_bfree);
	ptr = magma_serialize_64(ptr, statbuf->f_bavail);
	ptr = magma_serialize_64(ptr, statbuf->f_files);
	ptr = magma_serialize_64(ptr, statbuf->f_ffree);

	ptr = magma_serialize_32(ptr, statbuf->f_fsid.__val[0]);
	ptr = magma_serialize_32(ptr, statbuf->f_fsid.__val[1]);

#if __WORDSIZE == 32
	ptr = magma_serialize_32(ptr, statbuf->f_type);
	ptr = magma_serialize_32(ptr, statbuf->f_bsize);
	ptr = magma_serialize_32(ptr, statbuf->f_namelen);
	ptr = magma_serialize_32(ptr, statbuf->f_frsize);
#else
	ptr = magma_serialize_64(ptr, statbuf->f_type);
	ptr = magma_serialize_64(ptr, statbuf->f_bsize);
	ptr = magma_serialize_64(ptr, statbuf->f_namelen);
	ptr = magma_serialize_64(ptr, statbuf->f_frsize);
#endif

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

GIOStatus magma_pktar_statfs(GSocket *socket, GSocketAddress *peer, magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (G_IO_STATUS_NORMAL != response->header.status || -1 == response->header.res || !ptr) return (G_IO_STATUS_AGAIN);

	ptr = magma_deserialize_64(ptr, &response->body.statfs.statbuf.f_blocks);
	ptr = magma_deserialize_64(ptr, &response->body.statfs.statbuf.f_bfree);
	ptr = magma_deserialize_64(ptr, &response->body.statfs.statbuf.f_bavail);
	ptr = magma_deserialize_64(ptr, &response->body.statfs.statbuf.f_files);
	ptr = magma_deserialize_64(ptr, &response->body.statfs.statbuf.f_ffree);

	ptr = magma_deserialize_32(ptr, (guint32 *) &response->body.statfs.statbuf.f_fsid.__val[0]);
	ptr = magma_deserialize_32(ptr, (guint32 *) &response->body.statfs.statbuf.f_fsid.__val[1]);

#if __WORDSIZE == 32
	ptr = magma_deserialize_32(ptr, &response->body.statfs.statbuf.f_type);
	ptr = magma_deserialize_32(ptr, &response->body.statfs.statbuf.f_bsize);
	ptr = magma_deserialize_32(ptr, &response->body.statfs.statbuf.f_namelen);
	ptr = magma_deserialize_32(ptr, &response->body.statfs.statbuf.f_frsize);
#else
	ptr = magma_deserialize_64(ptr, (guint64 *) &response->body.statfs.statbuf.f_type);
	ptr = magma_deserialize_64(ptr, (guint64 *) &response->body.statfs.statbuf.f_bsize);
	ptr = magma_deserialize_64(ptr, (guint64 *) &response->body.statfs.statbuf.f_namelen);
	ptr = magma_deserialize_64(ptr, (guint64 *) &response->body.statfs.statbuf.f_frsize);
#endif

	return (G_IO_STATUS_NORMAL);
}
