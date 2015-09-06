/*
   MAGMA -- vulcano.h
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Defines nodes as structures and implements node's operations.

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

#ifndef _MAGMA_NODE_H
#define _MAGMA_NODE_H

extern char *mypath;

/**
 * next magma_struct rappresents this node and is publicly available
 * to all code
 */
typedef struct magma_volcano {
	char ip_addr[MAX_IP_LENGTH];					/* node IP address as printable string */
	uint16_t port;									/* node port number */
	struct in_addr inaddr;							/* node IP address in binary format */

	char node_name[MAX_HOSTNAME_LENGTH];			/* node nick name */
	char fqdn_name[MAX_HOSTNAME_LENGTH];			/* node fully qualified host name */
	char bootserver[MAX_HOSTNAME_LENGTH];			/* remote node used to join the network */
	char secretkey[MAX_HOSTNAME_LENGTH];			/* secret key used to join the network (unused?) */
	char hashpath[MAX_PATH_LENGTH];					/* local directory used as flare repository */

	guint32 bandwidth;								/* node bandwidth in Kb/s */
	guint32 storage;								/* node storage in MB */

	gboolean alive;									/* the node answered last ping */

	GThreadedSocketService *service; 				/* GLib socket service */
	GSocket *connection;							/* GSocket already opened to this node */

	float load;										/* actual server load */

	guint32 total_keys;								/* the number of keys hosted on this server */
	guint32 free_storage;							/* the available megabytes */

	struct magma_volcano *prev;						/* fields for list double linking */
	struct magma_volcano *next;

	struct magma_volcano *joining_node; 			/* if a node is joining on this one, it's recorded here */

	GRecMutex mutex;								/* locking for this node */

	char start_key[SHA_READABLE_DIGEST_LENGTH];		/* limit for this node keyspace */
	char stop_key[SHA_READABLE_DIGEST_LENGTH];
} magma_volcano;

#define MAGMA_ENABLE_NODE_LOCKING 0

#if MAGMA_ENABLE_NODE_LOCKING

/**
 * lock node N mutex
 *
 * @param N the node
 */
#define magma_lock_node(N) {\
	if (N != NULL) {\
		dbg(LOG_INFO, DEBUG_MUTEX, "[N+] Lock on \"%s\" @ %s:%d", N->node_name, __FILE__, __LINE__);\
		g_rec_mutex_lock(&(N->mutex));\
		dbg(LOG_INFO, DEBUG_MUTEX, "[N+] Locked!");\
	}\
}

/**
 * unlock node N mutex
 *
 * @param N the node
 */
#define magma_unlock_node(N) {\
	if (N != NULL) {\
		g_rec_mutex_unlock(&(N->mutex));\
		dbg(LOG_INFO, DEBUG_MUTEX, "[N-] Lock on \"%s\" @ %s:%d", N->node_name, __FILE__, __LINE__);\
	}\
}

/**
 * locks this node
 */
#define magma_lock_me() {\
	dbg(LOG_INFO, DEBUG_MUTEX, " [+] Locking myself at %s:%d", __FILE__, __LINE__);\
	g_rec_mutex_lock(&(myself.mutex));\
	dbg(LOG_INFO, DEBUG_MUTEX, " [+] Locked!");\
}

/**
 * unlocks this node
 */
#define magma_unlock_me() {\
	g_rec_mutex_unlock(&(myself.mutex));\
	dbg(LOG_INFO, DEBUG_MUTEX, " [-] Unlocking myself at %s:%d", __FILE__, __LINE__);\
}

#else

#define magma_lock_node(N) {}
#define magma_unlock_node(N) {}
#define magma_lock_me() {}
#define magma_unlock_me() {}

#endif

/**
 * The current lava topology is described here
 */
typedef struct {
	guint32 participants;			/* the number of nodes participating the network */

	magma_volcano *first_node;		/* the first node of the network */
	magma_volcano *last_node;		/* the last node of the network */

	GRWLock lock;					/* RW lock used to serialize DHT topology updates */
} magma_lava;

/**
 * magma_volcano structure holding information related to this node
 */
extern magma_volcano myself;

/**
 * this is the lava ring pointer that will be used by all routing
 * functions and internode operations (load balancing, key exchange, ...)
 */
extern magma_lava *lava;

/**
 * setup network ring structures
 */
extern magma_lava *magma_lava_new();
extern magma_lava *magma_lava_clone();
extern void magma_lava_destroy(magma_lava *lava);

extern magma_volcano *magma_volcano_new(
	char *node_name, char *fqdn_name, char *ipaddr, int ipport, int bandwidth,
	int storage, char *start_key, char *stop_key, const gchar *dht_path);

#define magma_create_node_empty() magma_volcano_new(NULL,NULL,NULL,0,0,0,NULL,NULL,NULL,NULL,NULL)

extern magma_volcano *magma_config_myself(char *node_name, char *fqdn_name, char *ipaddr, int ipport, int bandwidth, int storage, const gchar *dht_path);
extern int magma_vulcano_destroy(magma_volcano *node);

#define magma_add_node_to_ring(n) magma_add_node_to_ring_near(n,lava->last_node)
extern int magma_remove_node_from_ring(magma_volcano *newnode);
extern void magma_parse_node_line(char *buf, magma_volcano *node);
extern int magma_load_node_from_dht(magma_volcano *node);

extern int magma_check_keyspace_continuity();

#define magma_get_next_node(node) (node->next ? node->next : lava->first_node)
#define magma_get_previous_node(node) (node->prev ? node->prev : lava->last_node)

extern int magma_save_node_profile();

/* check for node duplicates, return 0 if all is ok */
extern uint8_t magma_node_exists(magma_volcano *node, magma_lava *lava);

#define magma_explain_join_error(error) {\
	if (error is MAGMA_NODE_NAME_COLLISION) {\
		dbg(LOG_ERR, DEBUG_ERR, "Another node with same nick name exists in lava ring");\
	} if (error is MAGMA_FQDN_NAME_COLLISION) {\
		dbg(LOG_ERR, DEBUG_ERR, "Another node with same FQDN name exists in lava ring");\
	} else {\
		dbg(LOG_ERR, DEBUG_ERR, "Unknow error from");\
	}\
}

extern void magma_save_status();
extern void magma_load_status();

/**
 * DHT locking macros
 */
#define magma_dht_reader_lock() g_rw_lock_reader_lock(&lava->lock)
#define magma_dht_reader_unlock() g_rw_lock_reader_unlock(&lava->lock)
#define magma_dht_writer_lock() g_rw_lock_writer_lock(&lava->lock)
#define magma_dht_writer_unlock() g_rw_lock_writer_unlock(&lava->lock)

extern gchar *magma_get_highest_used_key();

// extern magma_volcano *magma_volcano_clone(magma_volcano *src);
extern void magma_lava_add_volcano(magma_lava *lava, magma_volcano *volcano);

extern gboolean magma_this_node_is_coordinator();
extern magma_volcano *magma_get_myself_from_lava(magma_lava *lava);

extern magma_volcano *magma_volcano_clone(magma_volcano *src);

#endif /* _MAGMA_NODE_H */

// vim:ts=4:nocindent:autoindent
