/*
   MAGMA -- protocol.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Implements server side protocol management. Single protocols are
   implemented separately in protocol_console.c, protocol_node.c
   and protocol_flare.c

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

#include "../../flare_system/magma_flare_types.h"
#include "../protocol_pkt.h"

/**
 * holds informations about an active session
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_connection connection;
} magma_flare_environment;

/**
 * The maximum length of a path (1024) plus one for '\0'
 */
#define MAGMA_TERMINATED_PATH_LENGTH 1025

/**
 * The maximum lenfgth of a direntry (255) plus one for '\0'
 */
#define MAGMA_TERMINATED_DIRENTRY_LENGTH 257

/**
 * struct stat replacement
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_dev dev;
	magma_ino ino;
	magma_size size;
	magma_blocks blocks;
	magma_atime atime;
	magma_ctime ctime;
	magma_mtime mtime;
	magma_mode mode;
	magma_nlink nlink;
	magma_uid uid;
	magma_gid gid;
	magma_rdev rdev;
	magma_blksize blksize;
} magma_stat_struct;

/**
 * GETATTR request body
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_getattr_body;

/**
 * GETATTR response body
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	struct stat *stbuf;
} magma_response_getattr_body;

/**
 * READLINK request body
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_datagram_size buffer_length;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_readlink_body;

/**
 * READLINK response body
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_response_readlink_body;

/**
 * READDIR request body
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_readdir_body;

/**
 * READDIR response body
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_readdir_body;

/**
 * extended READDIR request body
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_offset offset;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_readdir_extended_body;

/**
 * extended READDIR entry struct (each response
 * carries MAGMA_MAX_READDIR_ENTRIES of these entries)
 */
typedef struct {
	magma_stat_struct st;
	gchar path[MAGMA_TERMINATED_DIRENTRY_LENGTH];
} magma_extended_readdir_entry;

#define MAGMA_MAX_READDIR_ENTRIES 50

/**
 * extended READDIR response body
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_readdir_entry entry_number;
	magma_offset offset;
	magma_extended_readdir_entry entries[MAGMA_MAX_READDIR_ENTRIES];
} magma_response_readdir_extended_body;

/**
 * READDIR offset request body
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_offset offset;
} magma_request_readdir_offset_body;

/**
 * READDIR offset response body
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar entry[MAGMA_TERMINATED_DIRENTRY_LENGTH];
} magma_response_readdir_offset_body;

/**
 * MKNOD
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_mode mode;
	magma_rdev rdev;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_mknod_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_mknod_body;

/**
 * MKDIR
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_mode mode;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_mkdir_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_mkdir_body;

/**
 * UNLINK
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_unlink_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_unlink_body;

/**
 * RMDIR
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_rmdir_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_rmdir_body;

/**
 * SYMLINK
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar from[MAGMA_TERMINATED_PATH_LENGTH];
	gchar to[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_symlink_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_symlink_body;

/**
 * RENAME
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar from[MAGMA_TERMINATED_PATH_LENGTH];
	gchar to[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_rename_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_rename_body;

/**
 * CHMOD
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_mode mode;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_chmod_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_chmod_body;

/**
 * CHOWN
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_uid new_uid;
	magma_gid new_gid;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_chown_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_chown_body;

/**
 * TRUNCATE
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_offset offset;
	char path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_truncate_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_truncate_body;

/**
 * UTIME
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_atime atime;
	magma_mtime mtime;
	char path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_utime_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// emtpy
} magma_response_utime_body;

/**
 * OPEN
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_flags flags;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_open_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar commit_url[2 * MAGMA_TERMINATED_PATH_LENGTH];
} magma_response_open_body;

/**
 * READ
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_offset offset;
	magma_size32 size;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_read_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar buffer[MAGMA_READ_WRITE_BUFFER_SIZE];
} magma_response_read_body;

/**
 * WRITE
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_offset offset;
	magma_size32 size;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
	// gchar buffer[MAGMA_READ_WRITE_BUFFER_SIZE];
	gchar *buffer;
} magma_request_write_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_write_body;

/**
 * STATFS
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_statfs_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// TODO FLARE: declare a magma portable alternative of struct statfs
	struct statfs statbuf;
} magma_response_statfs_body;

#define MAGMA_F_READDIR_BUFFER_SIZE 50 * 1024

/**
 * F_OPENDIR
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_offset offset;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_f_opendir_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_offset offset;
	magma_size size;
	magma_size buffer_size;
	gchar *buffer;
} magma_response_f_opendir_body;

/**
 * ADD_FLARE_TO_PARENT
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	guint8 just_write;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_add_flare_to_parent_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_add_flare_to_parent_body;

/**
 * REMOVE_FLARE_FROM_PARENT
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	guint8 just_write;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_request_remove_flare_from_parent_body;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	// empty
} magma_response_remove_flare_from_parent_body;

/**
 * FLARE REQUEST packet
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_request_header header;
	union {
		magma_request_getattr_body getattr;
		magma_request_readlink_body readlink;
		magma_request_readdir_body readdir;
		magma_request_readdir_extended_body readdir_extended;
		magma_request_readdir_offset_body readdir_offset;
		magma_request_mknod_body mknod;
		magma_request_mkdir_body mkdir;
		magma_request_unlink_body unlink;
		magma_request_rmdir_body rmdir;
		magma_request_symlink_body symlink;
		magma_request_rename_body rename;
		magma_request_chmod_body chmod;
		magma_request_chown_body chown;
		magma_request_truncate_body truncate;
		magma_request_utime_body utime;
		magma_request_open_body open;
		magma_request_read_body read;
		magma_request_write_body write;
		magma_request_statfs_body statfs;

		magma_request_f_opendir_body f_opendir;

		magma_request_add_flare_to_parent_body add_flare_to_parent;
		magma_request_remove_flare_from_parent_body remove_flare_from_parent;
	} body;
} magma_flare_request;

/**
 * FLARE RESPONSE packet
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_response_header header;
	union {
		magma_response_getattr_body getattr;
		magma_response_readlink_body readlink;
		magma_response_readdir_body readdir;
		magma_response_readdir_extended_body readdir_extended;
		magma_response_readdir_offset_body readdir_offset;
		magma_response_mknod_body mknod;
		magma_response_mkdir_body mkdir;
		magma_response_unlink_body unlink;
		magma_response_rmdir_body rmdir;
		magma_response_symlink_body symlink;
		magma_response_rename_body rename;
		magma_response_chmod_body chmod;
		magma_response_chown_body chown;
		magma_response_truncate_body truncate;
		magma_response_utime_body utime;
		magma_response_open_body open;
		magma_response_read_body read;
		magma_response_write_body write;
		magma_response_statfs_body statfs;

		magma_response_f_opendir_body f_opendir;

		magma_response_add_flare_to_parent_body add_flare_to_parent;
		magma_response_remove_flare_from_parent_body remove_flare_from_parent;
	} body;
} magma_flare_response;

extern void magma_manage_udp_flare_protocol(GSocket *socket, GSocketAddress *peer, gchar buffer[MAGMA_MESSAGE_MAX_SIZE]);
extern void magma_manage_udp_flare_protocol_pool(magma_incoming_request *item, gpointer user_data);

/***************************************************************************************
 ***                                                                                 ***
 *** OPERATION DISPATCHING FUNCTIONS                                                 ***
 ***                                                                                 ***
 ***************************************************************************************/

/* GETATTR OK */
extern gchar *magma_encode_stat_struct(struct stat *statbuf, gchar *buffer);
extern gchar *magma_decode_stat_struct(gchar *buffer, struct stat *statbuf);

extern gchar *magma_encode_magma_stat_struct(magma_stat_struct *statbuf, gchar *buffer);
extern gchar *magma_decode_magma_stat_struct(gchar *buffer, magma_stat_struct *statbuf);

extern magma_transaction_id magma_pktqs_getattr(GSocket *socket, GSocketAddress *peer, uid_t uid, gid_t gid, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_getattr(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_getattr(GSocket *socket, GSocketAddress *peer, int res, struct stat *statbuf, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_getattr(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* READLINK OK */
extern magma_transaction_id magma_pktqs_readlink(GSocket *socket, GSocketAddress *peer, uid_t uid, gid_t gid, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_readlink(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_readlink(GSocket *socket, GSocketAddress *peer, int res, const gchar *path, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_readlink(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* MKNOD OK */
extern magma_transaction_id magma_pktqs_mknod(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, mode_t mode, dev_t rdev, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_mknod(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_mknod(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_mknod(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* MKDIR OK */
extern magma_transaction_id magma_pktqs_mkdir(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, mode_t mode, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_mkdir(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_mkdir(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_mkdir(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* UNLINK OK */
extern magma_transaction_id magma_pktqs_unlink(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_unlink(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_unlink(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_unlink(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* RMDIR OK */
extern magma_transaction_id magma_pktqs_rmdir(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_rmdir(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_rmdir(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_rmdir(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* SYMLINK OK */
extern magma_transaction_id magma_pktqs_symlink(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, const gchar *from, const gchar *to, magma_flare_response *response);
extern void magma_pktqr_symlink(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_symlink(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_symlink(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* RENAME OK */
extern magma_transaction_id magma_pktqs_rename(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, const gchar *from, const gchar *to, magma_flare_response *response);
extern void magma_pktqr_rename(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_rename(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_rename(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* CHMOD OK */
extern magma_transaction_id magma_pktqs_chmod(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, const gchar *path, mode_t mode, magma_flare_response *response);
extern void magma_pktqr_chmod(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_chmod(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_chmod(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* CHOWN OK */
extern magma_transaction_id magma_pktqs_chown(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, const gchar *path, uid_t new_uid, gid_t new_gid, magma_flare_response *response);
extern void magma_pktqr_chown(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_chown(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_chown(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* TRUNCATE OK */
extern magma_transaction_id magma_pktqs_truncate(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, const gchar *path, off_t offset, magma_flare_response *response);
extern void magma_pktqr_truncate(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_truncate(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_truncate(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* UTIME OK */
extern magma_transaction_id magma_pktqs_utime(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, time_t atime, time_t mtime, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_utime(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_utime(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_utime(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* OPEN OK */
extern magma_transaction_id magma_pktqs_open(GSocket *socket, GSocketAddress *peer, uid_t uid, gid_t gid, guint32 flags, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_open(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_open(GSocket *socket, GSocketAddress *peer, int res, int error, magma_flare_t *flare, magma_transaction_id tid);
extern GIOStatus magma_pktar_open(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* READ OK */
extern magma_transaction_id magma_pktqs_read(GSocket *socket, GSocketAddress *peer, uid_t uid, gid_t gid, guint32 size, guint64 offset, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_read(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_read(GSocket *socket, GSocketAddress *peer, int res, int error, gchar *read_buffer, magma_transaction_id tid);
extern GIOStatus magma_pktar_read(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* WRITE OK */
extern magma_transaction_id magma_pktqs_write(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, guint32 size, guint64 offset, const gchar *path, const gchar *write_buffer, magma_flare_response *response);
extern void magma_pktqr_write(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_write(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_write(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* STATFS OK */
extern magma_transaction_id magma_pktqs_statfs(GSocket *socket, GSocketAddress *peer,  uid_t uid, gid_t gid, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_statfs(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_statfs(GSocket *socket, GSocketAddress *peer, int res, struct statfs *statbuf, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_statfs(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* F_OPENDIR OK */
extern magma_transaction_id magma_pktqs_f_opendir(GSocket *socket, GSocketAddress *peer, uid_t uid, gid_t gid, const gchar *path, magma_offset offset, magma_flare_response *response);
extern void magma_pktqr_f_opendir(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_f_opendir(GSocket *socket, GSocketAddress *peer, int res, int error, gchar *payload, magma_offset offset, magma_size buffer_size, magma_size size, magma_transaction_id tid);
extern GIOStatus magma_pktar_f_opendir(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

#if 0
/* ADD_FLARE_TO_PARENT OK */
extern magma_transaction_id magma_pktqs_add_flare_to_parent(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_add_flare_to_parent(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_add_flare_to_parent(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_add_flare_to_parent(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* REMOVE_FLARE_FROM_PARENT OK */
extern magma_transaction_id magma_pktqs_remove_flare_from_parent(GSocket *socket, GSocketAddress *peer, magma_ttl ttl, uid_t uid, gid_t gid, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_remove_flare_from_parent(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_remove_flare_from_parent(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_remove_flare_from_parent(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);
#endif

/* READDIR OK */
extern magma_transaction_id magma_pktqs_readdir(GSocket *socket, GSocketAddress *peer, uid_t uid, gid_t gid, const gchar *path, magma_flare_response *response);
extern void magma_pktqr_readdir(gchar *buffer, magma_flare_request *request);
extern void magma_pktas_readdir(GSocket *socket, GSocketAddress *peer, int res, int error, magma_transaction_id tid);
extern GIOStatus magma_pktar_readdir(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

extern void magma_pkt_send_readdir_offset(GSocket *socket, GSocketAddress *peer, uid_t uid, gid_t gid, off_t offset, magma_transaction_id tid);
extern GIOStatus magma_pkt_recv_readdir_offset(GSocket *socket, GSocketAddress *peer, magma_flare_request *request);

extern void magma_pkt_send_readdir_entry(GSocket *socket, GSocketAddress *peer, gchar *dirent, magma_transaction_id tid);
extern GIOStatus magma_pkt_recv_readdir_entry(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

/* READDIR extended OK */
extern magma_transaction_id magma_pktqs_readdir_extended(GSocket *socket, GSocketAddress *peer, uid_t uid, gid_t gid, const gchar *path, off_t offset, magma_flare_response *response);
extern void magma_pktqr_readdir_extended(gchar *buffer, magma_flare_request *request);

extern void magma_pktas_readdir_extended(GSocket *socket, GSocketAddress *peer, magma_flare_response *response, magma_transaction_id tid);
extern GIOStatus magma_pktar_readdir_extended(GSocket *socket, GSocketAddress *peer, magma_flare_response *response);

