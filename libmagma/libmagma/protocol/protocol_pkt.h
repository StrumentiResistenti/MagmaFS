/*
   MAGMA -- protocol_pkt.h
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

#include "../flare_system/magma_flare_types.h"

#define MAX_IP_LENGTH 16
#define MAX_HOSTNAME_LENGTH 1024
#define MAX_PATH_LENGTH 1024

/*
 * defining magma_optype of unsigned 8 bits will
 * avoid the need to use hton() and ntoh() functions
 */
typedef guint8 magma_optype;	     /**< define the next operation requested to a server */
typedef guint16 magma_operation_id;

#if 0
/*
 * when a new connection reaches magma, first byte declares the kind of
 * peer connecting
 */
typedef guint8  magma_peertype;                 /**< define the type of a connection if sent as first information */
extern const magma_peertype PEER_TYPE_NODE;		/* a magma server exchanging data */
extern const magma_peertype PEER_TYPE_MOUNT;	/* a mounted filesystem */
extern const magma_peertype PEER_TYPE_FUSE;		/* an alias */
extern const magma_peertype PEER_TYPE_CLIENT;	/* a console connection */
#endif

/*
 * second field is the operation type
 */

/*
 * fuse protocol
 */
extern const magma_optype MAGMA_OP_TYPE_GETATTR;	/* implemented */ /* NEED -D_FILE_OFFSET_BITS=64 to compile cleanly */
extern const magma_optype MAGMA_OP_TYPE_READLINK;	/* implemented */ /* ok */
extern const magma_optype MAGMA_OP_TYPE_GETDIR;		/* deprecated, see readdir */
extern const magma_optype MAGMA_OP_TYPE_MKNOD;		/* implemented */ /* ok */
extern const magma_optype MAGMA_OP_TYPE_MKDIR;		/* implemented */ /* ok */
extern const magma_optype MAGMA_OP_TYPE_UNLINK;		/* implemented */ /* ok */
extern const magma_optype MAGMA_OP_TYPE_RMDIR;		/* implemented */ /* ok */
extern const magma_optype MAGMA_OP_TYPE_SYMLINK;	/* implemented */ /* ok */
extern const magma_optype MAGMA_OP_TYPE_RENAME;		/* implemented */ /* ok */
extern const magma_optype MAGMA_OP_TYPE_LINK;		/* implemented */ /* ok */
extern const magma_optype MAGMA_OP_TYPE_CHMOD;		/* implemented */ /* ok */
extern const magma_optype MAGMA_OP_TYPE_CHOWN;		/* implemented */ /* ok */
extern const magma_optype MAGMA_OP_TYPE_TRUNCATE;	/* implemented */ /* ok */
extern const magma_optype MAGMA_OP_TYPE_UTIME;		/* implemented but untested */
extern const magma_optype MAGMA_OP_TYPE_OPEN;		/* implemented */ /* optional */
extern const magma_optype MAGMA_OP_TYPE_READ;		/* implemented */
extern const magma_optype MAGMA_OP_TYPE_WRITE;		/* implemented */
extern const magma_optype MAGMA_OP_TYPE_STATFS;		/* implemented */
extern const magma_optype MAGMA_OP_TYPE_FLUSH;		/* optional */
extern const magma_optype MAGMA_OP_TYPE_RELEASE;	/* optional */
extern const magma_optype MAGMA_OP_TYPE_FSYNC;		/* optional */
#ifdef HAVE_SETXATTR
extern const magma_optype MAGMA_OP_TYPE_SETXATTR;	/* optional */
extern const magma_optype MAGMA_OP_TYPE_GETXATTR;	/* optional */
extern const magma_optype MAGMA_OP_TYPE_LISTXATTR;	/* optional */
extern const magma_optype MAGMA_OP_TYPE_REMOVEXATTR;/* optional */
#endif /* HAVE_SETXATTR */
extern const magma_optype MAGMA_OP_TYPE_OPENDIR;	/* optional */
extern const magma_optype MAGMA_OP_TYPE_READDIR;	/* implemented */
extern const magma_optype MAGMA_OP_TYPE_RELEASEDIR;	/* optional */
extern const magma_optype MAGMA_OP_TYPE_FSYNCDIR;	/* optional */
extern const magma_optype MAGMA_OP_TYPE_INIT;		/* optional */
extern const magma_optype MAGMA_OP_TYPE_DESTROY;	/* optional */
extern const magma_optype MAGMA_OP_TYPE_READDIR_EXTENDED; /* implemented */
extern const magma_optype MAGMA_OP_TYPE_READDIR_OFFSET; /* implemented */

extern const magma_optype MAGMA_OP_TYPE_ADD_FLARE_TO_PARENT;
extern const magma_optype MAGMA_OP_TYPE_REMOVE_FLARE_FROM_PARENT;

extern const magma_optype MAGMA_OP_TYPE_F_OPENDIR;
extern const magma_optype MAGMA_OP_TYPE_F_CLOSEDIR;
extern const magma_optype MAGMA_OP_TYPE_F_TELLDIR;
extern const magma_optype MAGMA_OP_TYPE_F_SEEKDIR;
extern const magma_optype MAGMA_OP_TYPE_F_READDIR;

/*
 * node protocol
 */
extern const magma_optype MAGMA_OP_TYPE_JOIN;				/* join an existing network */
extern const magma_optype MAGMA_OP_TYPE_FINISH_JOIN;		/* finish a join procedure */
extern const magma_optype MAGMA_OP_TYPE_TRANSMIT_TOPOLOGY;	/* transmit the whole topology */
extern const magma_optype MAGMA_OP_TYPE_TRANSMIT_KEY;		/* transmit a key to its redundant node */
extern const magma_optype MAGMA_OP_TYPE_TRANSMIT_NODE;		/* transmit profile of a node */
extern const magma_optype MAGMA_OP_TYPE_GET_KEY;			/* obtain a key from sibling node */
extern const magma_optype MAGMA_OP_TYPE_PUT_KEY;			/* release a key to sibling node */
extern const magma_optype MAGMA_OP_TYPE_DROP_KEY;			/* drop key, because it has been assigned to another node */
extern const magma_optype MAGMA_OP_TYPE_GET_KEY_CONTENT;	/* get key content */
extern const magma_optype MAGMA_OP_TYPE_NETWORK_BUILT;		/* network loaded and ready to operate */

/*
 * generic utilities
 */
extern const magma_optype MAGMA_OP_TYPE_CLOSECONNECTION;	/* close connection to MAGMA server */
extern const magma_optype MAGMA_OP_TYPE_SHUTDOWN;			/* shutdown the lava network */
extern const magma_optype MAGMA_OP_TYPE_HEARTBEAT;			/* see if server is still available */

#define MAGMA_OP_TYPE_MAX 255

#include "../magma_setup.h"

extern const int8_t MAGMA_DIR_IS_CLOSE;
extern const int8_t MAGMA_DIR_IS_OPEN;

#ifndef _MAGMA_PROTOCOL_PKT_H
#define _MAGMA_PROTOCOL_PKT_H

#ifndef PATH_LENGTH
#	ifdef PATH_MAX
#		define PATH_LENGTH PATH_MAX
#	else
#		define PATH_LENGTH 1024
#	endif
#endif

extern const uint8_t magma_ok;
extern const uint8_t magma_error;

/************* NODE OPERATIONS ***************/

extern const int16_t NO_MORE_NODES_TO_BE_SENT;
extern const int16_t MORE_NODES_TO_BE_SENT;

/* use PREVIOUS_NODE or NEXT_NODE as direction */
extern const int16_t PREVIOUS_NODE;
extern const int16_t NEXT_NODE;

#ifndef MAGMA_MESSAGE_MAX_SIZE
#define MAGMA_MESSAGE_MAX_SIZE 65507 // the size of the UDP packet minus 8 bytes of UDP header and 20 bytes of IP header
#endif

#if 0
#define MAGMA_PROTOCOL_ALIGNMENT __attribute__ ((packed,aligned(1)))
#else
#define MAGMA_PROTOCOL_ALIGNMENT
#endif

/**
 * request header
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_optype type;
	magma_transaction_id transaction_id;
	magma_ttl ttl;
	magma_uid uid;
	magma_gid gid;
} magma_request_header;

/**
 * FLARE RESPONSE HEADER
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_result res;
	magma_errno err_no;
	magma_transaction_id transaction_id;

	/** used to report magma_pktar() status and to check for retransmission */
	GIOStatus status;
} magma_response_header;

#endif /* _MAGMA_PROTOCOL_PKT_H */

// vim:ts=4:nocindent:autoindent
