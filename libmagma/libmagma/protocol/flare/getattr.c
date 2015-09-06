/*
   MAGMA -- protocol_flare/getattr.c
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
magma_pktqs_getattr(
	GSocket *socket,
	GSocketAddress *peer,
	uid_t uid,
	gid_t gid,
	const gchar *path,
	magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_GETATTR, uid, gid, &tid, MAGMA_TERMINAL_TTL);
	ptr = magma_serialize_string(ptr, path);

	magma_log_transaction(MAGMA_OP_TYPE_GETATTR, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_getattr, response);

	return (tid);
}

void magma_pktqr_getattr(gchar *buffer, magma_flare_request *request)
{
	magma_deserialize_string(buffer, request->body.getattr.path);
}

gchar *magma_encode_stat_struct(struct stat *statbuf, gchar *buffer)
{
	gchar *ptr = buffer;
	ptr = magma_serialize_64(ptr, statbuf->st_dev);
	ptr = magma_serialize_64(ptr, statbuf->st_ino);
	ptr = magma_serialize_64(ptr, statbuf->st_nlink);
	ptr = magma_serialize_64(ptr, statbuf->st_rdev);

	ptr = magma_serialize_64(ptr, statbuf->st_size);
	ptr = magma_serialize_64(ptr, statbuf->st_blksize);
	ptr = magma_serialize_64(ptr, statbuf->st_blocks);

	ptr = magma_serialize_64(ptr, statbuf->st_atime);
	ptr = magma_serialize_64(ptr, statbuf->st_ctime);
	ptr = magma_serialize_64(ptr, statbuf->st_mtime);

	ptr = magma_serialize_32(ptr, statbuf->st_mode);
	ptr = magma_serialize_32(ptr, statbuf->st_uid);
	ptr = magma_serialize_32(ptr, statbuf->st_gid);

	return (ptr);
}

gchar *magma_decode_stat_struct(gchar *buffer, struct stat *statbuf)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_64(ptr, &statbuf->st_dev);
	ptr = magma_deserialize_64(ptr, &statbuf->st_ino);
	ptr = magma_deserialize_64(ptr, &statbuf->st_nlink);
	ptr = magma_deserialize_64(ptr, &statbuf->st_rdev);

	ptr = magma_deserialize_64(ptr, (guint64 *) &statbuf->st_size);
	ptr = magma_deserialize_64(ptr, (guint64 *) &statbuf->st_blksize);
	ptr = magma_deserialize_64(ptr, (guint64 *) &statbuf->st_blocks);

	ptr = magma_deserialize_64(ptr, (guint64 *) &statbuf->st_atime);
	ptr = magma_deserialize_64(ptr, (guint64 *) &statbuf->st_ctime);
	ptr = magma_deserialize_64(ptr, (guint64 *) &statbuf->st_mtime);

	ptr = magma_deserialize_32(ptr, &statbuf->st_mode);
	ptr = magma_deserialize_32(ptr, &statbuf->st_uid);
	ptr = magma_deserialize_32(ptr, &statbuf->st_gid);

	return (ptr);
}

gchar *magma_encode_magma_stat_struct(magma_stat_struct *statbuf, gchar *buffer)
{
	gchar *ptr = buffer;
	ptr = magma_serialize_64(ptr, statbuf->dev);
	ptr = magma_serialize_64(ptr, statbuf->ino);
	ptr = magma_serialize_64(ptr, statbuf->size);
	ptr = magma_serialize_64(ptr, statbuf->blocks);
	ptr = magma_serialize_64(ptr, statbuf->atime);
	ptr = magma_serialize_64(ptr, statbuf->ctime);
	ptr = magma_serialize_64(ptr, statbuf->mtime);
	ptr = magma_serialize_32(ptr, statbuf->mode);
	ptr = magma_serialize_64(ptr, statbuf->nlink);
	ptr = magma_serialize_32(ptr, statbuf->uid);
	ptr = magma_serialize_32(ptr, statbuf->gid);
	ptr = magma_serialize_64(ptr, statbuf->rdev);
	ptr = magma_serialize_32(ptr, statbuf->blksize);
	return (ptr);
}

gchar *magma_decode_magma_stat_struct(gchar *buffer, magma_stat_struct *statbuf)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_64(ptr, &statbuf->dev);
	ptr = magma_deserialize_64(ptr, &statbuf->ino);
	ptr = magma_deserialize_64(ptr, &statbuf->size);
	ptr = magma_deserialize_64(ptr, &statbuf->blocks);
	ptr = magma_deserialize_64(ptr, &statbuf->atime);
	ptr = magma_deserialize_64(ptr, &statbuf->ctime);
	ptr = magma_deserialize_64(ptr, &statbuf->mtime);
	ptr = magma_deserialize_32(ptr, &statbuf->mode);
	ptr = magma_deserialize_64(ptr, &statbuf->nlink);
	ptr = magma_deserialize_32(ptr, &statbuf->uid);
	ptr = magma_deserialize_32(ptr, &statbuf->gid);
	ptr = magma_deserialize_64(ptr, &statbuf->rdev);
	ptr = magma_deserialize_32(ptr, &statbuf->blksize);
	return (ptr);
}

void magma_pktas_getattr(
	GSocket *socket,
	GSocketAddress *peer,
	int res,
	struct stat *statbuf,
	int error,
	magma_transaction_id tid,
	magma_flags flags)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];

	gchar *ptr = magma_format_response_header(buffer, res, error, tid, flags);

	if (-1 != res) {
		ptr = magma_encode_stat_struct(statbuf, ptr);
	}

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

GIOStatus magma_pktar_getattr(GSocket *socket, GSocketAddress *peer, magma_flare_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (
		response->header.status isNot G_IO_STATUS_NORMAL ||
		response->header.res is -1 ||
		!ptr
	) return (G_IO_STATUS_AGAIN);

	ptr = magma_decode_stat_struct(ptr, response->body.getattr.stbuf);

	return (G_IO_STATUS_NORMAL);
}
