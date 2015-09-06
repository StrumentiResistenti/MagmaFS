/*
   MAGMA -- protocol_flare/readdir.c
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

#if !MAGMA_OPTIMIZE_READDIR

magma_transaction_id
magma_pktqs_readdir(
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
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_READDIR, uid, gid, &tid, MAGMA_TERMINAL_TTL);
	ptr = magma_serialize_string(ptr, path);

	magma_log_transaction(MAGMA_OP_TYPE_READDIR, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_readdir, response);

	return (tid);
}

void magma_pktqr_readdir(gchar *buffer, magma_flare_request *request)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_string(ptr, request->body.readdir.path);
}

void magma_pktas_readdir(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, error, tid);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

GIOStatus magma_pktar_readdir(GSocket *socket, GSocketAddress *peer, magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	return (!ptr ? G_IO_STATUS_AGAIN : G_IO_STATUS_NORMAL);
}

void magma_pkt_send_readdir_offset(
	GSocket *socket,
	GSocketAddress *peer,
	uid_t uid,
	gid_t gid,
	off_t offset,
	magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id in_tid = tid;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_READDIR_OFFSET, uid, gid, &in_tid, MAGMA_TERMINAL_TTL);
	ptr = magma_serialize_64(ptr, offset);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

GIOStatus magma_pkt_recv_readdir_offset(GSocket *socket, GSocketAddress *peer, magma_flare_request *request)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktqr(socket, peer, buffer, (magma_request *) request);
	if (!ptr) return (G_IO_STATUS_AGAIN);

	ptr = magma_deserialize_64(ptr, &request->body.readdir_offset.offset);

	return (G_IO_STATUS_NORMAL);
}

void magma_pkt_send_readdir_entry(GSocket *socket, GSocketAddress *peer, gchar *dirent, magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr;

	if (dirent) {
		ptr = magma_format_response_header(buffer, 0, 0, tid);
		ptr = magma_serialize_string(ptr, dirent);
	} else {
		ptr = magma_format_response_header(buffer, -1, ENOENT, tid);
	}

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

GIOStatus
magma_pkt_recv_readdir_entry(GSocket *socket, GSocketAddress *peer, magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (G_IO_STATUS_NORMAL != response->header.status || -1 == response->header.res || !ptr) return (response->header.status);

	ptr = magma_deserialize_string(ptr, response->body.f_readdir.dentry);

	return (G_IO_STATUS_NORMAL);
}

#else

magma_transaction_id
magma_pktqs_readdir_extended(
	GSocket *socket,
	GSocketAddress *peer,
	uid_t uid,
	gid_t gid,
	const gchar *path,
	off_t offset,
	magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_READDIR_EXTENDED, uid, gid, &tid, MAGMA_TERMINAL_TTL);
	ptr = magma_serialize_64(ptr, offset);
	ptr = magma_serialize_string(ptr, path);

	magma_log_transaction(MAGMA_OP_TYPE_READDIR_EXTENDED, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_readdir_extended, response);

	return (tid);
}

void magma_pktqr_readdir_extended(gchar *buffer, magma_flare_request *request)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_64(ptr, &request->body.readdir_extended.offset);
	ptr = magma_deserialize_string(ptr, request->body.readdir_extended.path);
}

void magma_pktas_readdir_extended(
	GSocket *socket,
	GSocketAddress *peer,
	magma_flare_response *response,
	magma_transaction_id tid,
	magma_flags flags)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, response->header.res, response->header.err_no, tid, flags);
	ptr = magma_serialize_64(ptr, response->body.readdir_extended.offset);
	ptr = magma_serialize_16(ptr, response->body.readdir_extended.entry_number);

	/*
	 * serialize each entry with its own struct stat
	 */
	int i = 0;
	for (i = 0; i < response->body.readdir_extended.entry_number; i++) {
		ptr = magma_serialize_string(ptr, response->body.readdir_extended.entries[i].path);
		ptr = magma_encode_magma_stat_struct(&response->body.readdir_extended.entries[i].st, ptr);
	}

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

GIOStatus
magma_pktar_readdir_extended(GSocket *socket, GSocketAddress *peer, magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (G_IO_STATUS_NORMAL != response->header.status || -1 == response->header.res || !ptr) return (response->header.status);

	ptr = magma_deserialize_64(ptr, &response->body.readdir_extended.offset);
	ptr = magma_deserialize_16(ptr, &response->body.readdir_extended.entry_number);

	int i = 0;
	for (i = 0; i < response->body.readdir_extended.entry_number; i++) {
		ptr = magma_deserialize_string(ptr,  response->body.readdir_extended.entries[i].path);
		ptr = magma_decode_magma_stat_struct(ptr, &response->body.readdir_extended.entries[i].st);
	}

	return (G_IO_STATUS_NORMAL);
}

#endif
