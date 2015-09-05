/*
   MAGMA -- magma.h
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Defines a lot of macros and default values.

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

#ifndef _MAGMA_INCLUDE_FILE
#define _MAGMA_INCLUDE_FILE

#define MAGMA_DEBUGGING_BUILD

#include "magma_setup.h"

/*
 * just making C more readable and less error prone ;)
 */
#define equals ==
#define is ==
#define isNot !=

#define MAGMA_VERSION PACKAGE_VERSION

/**
 * if TRUE, readdir() is translated into extended readdir
 * operation. Every UDP packet contains several directory
 * entries paired with their struct stat.
 *
 * if FALSE, readdir() uses the old protocol which sends
 * a packet for each entry and does not include struct stat
 */
#define MAGMA_OPTIMIZE_READDIR TRUE

/**
 * the size of a message
 */
#define MAGMA_MESSAGE_MAX_SIZE 65536

/**
 * The size of the buffer sent with f_opendir() operations
 */
#define MAGMA_DIR_FRAGMENT_SIZE 50 * 1024

/**
 * Magma network possible states
 */
typedef enum {
	magma_network_loading	= 0,
	magma_network_ready		= 1,
	magma_network_shutdown	= 2,

	magma_key_mismatch		= 10,
	magma_dht_rebuild		= 20, // useful?

	magma_last_status
} magma_network_status;

/**
 * The struct holding all the environment information
 */
typedef struct {
	/*
	 * magmad section
	 */
	char *progname;		/** the name magmad was invoked with */
	gchar log[255];		/** debug mask expanded*/
	gchar *logstring;	/** debug mask */

	int port;			/** Internet port passed on command line */
	char *nickname;		/** Node short name */
	char *servername;	/** Fully Qualified Domain Name of this node */
	char *ipaddr;		/** String representation of Internet address of this node */
	char *hashpath;		/** Local storage directory */
	int bandwidth;		/** Declared bandwidth */
	int storage;		/** Declared storage */
	int bootstrap;		/** If true, this node should bootstrap a new network, if false this node should join an existing one */
	char *bootserver;	/** Remote boot server address used if bootstrap is false */
	int bootport;		/** Remote boot server port used if bootstrap is false */
	char *secretkey;	/** Secret key used to join a network */

	/*
	 * mount.magma section
	 */
	char *remoteip;		/** Remote IP address */
	char *remotehost;	/** Remote hostname */
	int remoteport;		/** Remote port */
	char *keyphrase;	/** Secret key phrase */
	int nothreads;		/** Use threads? */
	char *mountpoint;	/** File system mount point */

	int debug_all;		/** Enable all debug flags? */

	int foreground;		/** Run in foreground? */

	magma_network_status state; /** The state of the network */

	/** used by mount.magma */
	GSocket *socket;
	GSocketAddress *peer;
} magma_environment_t;

/**
 * One magma_environment_t struct is instantiated for all the
 * library, holding all the information
 */
extern magma_environment_t magma_environment;

extern void magma_start_nfs_interface();

#include "debug.h"
#include "sha1.h"
#include "net_layer.h"
#include "utils.h"

#include "protocol/protocol.h"
#include "flare_system/magma_flare.h"
#include "vulcano.h"

#ifdef MAGMA_ENABLE_NFS_INTERFACE
#	include <rpc/rpc.h>
#	include <rpc/pmap_clnt.h>
#	include "mount.h"
#	include "nfs_prot.h"
#endif

#endif

// vim:ts=4:nocindent:autoindent
