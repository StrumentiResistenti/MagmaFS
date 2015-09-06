/*
   MAGMA -- magma_flare_internals.h
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   You should never use this file. This file is for internal
   implementation only. If you want to play with magma contents
   use magma_flare.h! You've bee warned...

   * Defines objects stored inside magma, with constructors and
   destructors.
   
   * Defines a cache mechanism based on b-trees and a garbage
   collector to clean that b-tree.
   
   * Defines lava network and vulcano nodes, with functions that can
   manage load balancing.

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

#ifndef _MAGMA_FLARE_INTERNALS_H
#define _MAGMA_FLARE_INTERNALS_H

#include "magma_flare_types.h"

#define MAGMA_CONF "/etc/magma"

#define MAGMA_CONTENTS "/contents"
#define MAGMA_METADATA "/metadata"

#define MAGMA_ACL_FILE "/etc/magma/acl"
#define MAGMA_ACL_FILE_TEMP "/etc/magma/.acl"
#define MAGMA_EXPORTS_FILE "/etc/magma/exports"
#define MAGMA_EXPORTS_FILE_BACK "/etc/magma/exports.back"

/**
 * The path internal to the DHT where a Lavanet stores
 * its profile and metainformation
 */
#define MAGMA_DHT_PATH "/.dht"

/**
 * The local path where magmad stores it metainformation,
 * like PID and such
 */
#ifndef MAGMA_RUN_PATH
#define MAGMA_RUN_PATH "/tmp/magma/" /* "/var/run/magma/" */
#endif

extern const char magma_dht_path[];
extern const char magma_run_path[];

/**
 * The user id and group id magmad will run as
 */
extern uid_t magma_flare_system_uid;
extern gid_t magma_flare_system_gid;

/** used instead of calling getpagesize(), mainly in mmap() functions */
extern int magma_system_pagesize;

/* the GTree used as flare RAM storage */
GTree *magma_cache_gtree;

#define MAGMA_FLARE_TYPE_REGULAR 'r'
#define MAGMA_FLARE_TYPE_DIR     'd'
#define MAGMA_FLARE_TYPE_SYMLINK 'l'
#define MAGMA_FLARE_TYPE_SOCKET  's'
#define MAGMA_FLARE_TYPE_CHAR    'c'
#define MAGMA_FLARE_TYPE_BLOCK   'b'
#define MAGMA_FLARE_TYPE_FIFO    'p'
#define MAGMA_FLARE_TYPE_UNKNOWN  0

/**************************************************************\
 * FLARE CREATION AND UPCASTING (see magma_flare_internals.c) *
\**************************************************************/

/* generic flare creation */
#define magma_new_flare(path) magma_new_flare_(path, __FILE__, __LINE__)
extern magma_flare_t *magma_new_flare_(const char *path, char *file, int line);

/* Directories */
extern int magma_cast_to_dir(magma_flare_t *flare);
extern magma_flare_t *magma_new_dir_flare(const char *path);

/* Regular files */
extern int magma_cast_to_file(magma_flare_t *flare);
extern magma_flare_t *magma_new_file_flare(const char *path);

/* Block devices */
extern int magma_cast_to_blockdev(magma_flare_t *flare);
extern magma_flare_t *magma_new_blockdev_flare(const char *path);

/* Character devices */
extern int magma_cast_to_chardev(magma_flare_t *flare);
extern magma_flare_t *magma_new_chardev_flare(const char *path);

/* Symbolic links */
extern int magma_cast_to_symlink(magma_flare_t *flare);
extern magma_flare_t *magma_new_symlink_flare(const char *path);

/* FIFOs */
extern int magma_cast_to_fifo(magma_flare_t *flare);
extern magma_flare_t *magma_new_fifo_flare(const char *path);

extern int magma_flare_upcast(magma_flare_t *flare);
extern void magma_dispose_flare(magma_flare_t *flare);
extern magma_flare_t *magma_duplicate_flare(magma_flare_t *flare);

#define magma_print_flare_cast(flare) {\
	     if (magma_isreg(flare))	{ dbg(LOG_INFO, DEBUG_FLARE, "Flare is a regular file"); }\
	else if (magma_isdir(flare))	{ dbg(LOG_INFO, DEBUG_FLARE, "Flare is a directory"); }\
	else if (magma_isblk(flare))	{ dbg(LOG_INFO, DEBUG_FLARE, "Flare is a block dev"); }\
	else if (magma_ischr(flare))	{ dbg(LOG_INFO, DEBUG_FLARE, "Flare is a char dev"); }\
	else if (magma_isfifo(flare))	{ dbg(LOG_INFO, DEBUG_FLARE, "Flare is a FIFO"); }\
	else if (magma_islnk(flare))	{ dbg(LOG_INFO, DEBUG_FLARE, "Flare is a symlink"); }\
	else                      		{ dbg(LOG_INFO, DEBUG_FLARE, "Flare is not casted?"); }\
}

extern void magma_flare_update_stat(magma_flare_t *flare);

#define magma_open_flare_contents(flare) open(flare->contents, O_RDWR|O_CREAT, S_IRWXU)

/* some macro just to shorten typing and clarify the code */
#if MAGMA_STRUCT_STAT_TYPE_MACROS

#define magma_isreg(flare)  S_ISREG(flare->st.st_mode)
#define magma_isdir(flare)  S_ISDIR(flare->st.st_mode)
#define magma_ischr(flare)  S_ISCHR(flare->st.st_mode)
#define magma_isblk(flare)  S_ISBLK(flare->st.st_mode)
#define magma_isfifo(flare) S_ISFIFO(flare->st.st_mode)
#define magma_issock(flare) S_ISSOCK(flare->st.st_mode)
#define magma_islnk(flare)  S_ISLNK(flare->st.st_mode)

#else

#define magma_isreg(flare)  (flare->type is MAGMA_FLARE_TYPE_REGULAR ? TRUE : FALSE)
#define magma_isdir(flare)  (flare->type is MAGMA_FLARE_TYPE_DIR ? TRUE : FALSE)
#define magma_ischr(flare)  (flare->type is MAGMA_FLARE_TYPE_CHAR ? TRUE : FALSE)
#define magma_isblk(flare)  (flare->type is MAGMA_FLARE_TYPE_BLOCK ? TRUE : FALSE)
#define magma_isfifo(flare) (flare->type is MAGMA_FLARE_TYPE_FIFO ? TRUE : FALSE)
#define magma_issock(flare) (flare->type is MAGMA_FLARE_TYPE_SOCKET ? TRUE : FALSE)
#define magma_islnk(flare)  (flare->type is MAGMA_FLARE_TYPE_SYMLINK ? TRUE : FALSE)

#endif

extern int magma_check_flare(magma_flare_t *flare);
extern int magma_erase_flare_from_disk(magma_flare_t *flare);
extern int magma_erase_flare(magma_flare_t *flare);

extern int magma_init_flare_on_disk(magma_flare_t *flare);
extern int magma_save_flare(magma_flare_t *flare, gboolean save_on_sql);
extern void magma_load_flare(magma_flare_t *flare);

typedef enum magma_touch_mode {
	MAGMA_TOUCH_ATIME = 1,
	MAGMA_TOUCH_CTIME = 2,
	MAGMA_TOUCH_MTIME = 4
} magma_touch_mode_t;

extern int magma_touch_flare(magma_flare_t *flare, magma_touch_mode_t mode, time_t atime, time_t mtime);

extern unsigned int magma_get_flare_inode(magma_flare_t *flare);

extern void magma_print_flare(magma_flare_t *flare);

/******************************************************\
 * LOCKING MACROS FOR READ/WRITE OPERATIONS ON FLARES *
\******************************************************/

extern GTree *magma_cache_gtree;
extern GTree *magma_lock_table;
extern GMutex magma_lookup_mutex;

extern GRWLock *magma_create_flare_lock(magma_flare_t *flare);

#define magma_flare_read_lock(flare) {\
	g_mutex_lock(&magma_lookup_mutex);\
	GRWLock *flare_lock = g_tree_lookup(magma_lock_table, flare->binhash);\
	if (!flare_lock) flare_lock = magma_create_flare_lock(flare);\
	g_mutex_unlock(&magma_lookup_mutex);\
	if (!flare_lock) {\
		dbg(LOG_INFO, DEBUG_MUTEX, "[R+] lock FAILED on \"%s\": no flare lock found @ %s:%d", flare->path, __FILE__, __LINE__);\
	} else {\
		g_rw_lock_reader_lock(flare_lock);\
		dbg(LOG_INFO, DEBUG_MUTEX, "[R+] lock on \"%s\" @ %s:%d", flare->path, __FILE__, __LINE__);\
	}\
}

#define magma_flare_read_unlock(flare) {\
	g_mutex_lock(&magma_lookup_mutex);\
	GRWLock *flare_lock = g_tree_lookup(magma_lock_table, flare->binhash);\
	g_mutex_unlock(&magma_lookup_mutex);\
	if (flare_lock) {\
		g_rw_lock_reader_unlock(flare_lock);\
		dbg(LOG_INFO, DEBUG_MUTEX, "[R-] lock on \"%s\" @ %s:%d", flare->path, __FILE__, __LINE__);\
	} else {\
		dbg(LOG_INFO, DEBUG_MUTEX, "[R-] lock FAILED on \"%s\": no flare lock found @ %s:%d", flare->path, __FILE__, __LINE__);\
	}\
}

#define magma_flare_write_lock(flare) {\
	g_mutex_lock(&magma_lookup_mutex);\
	GRWLock *flare_lock = g_tree_lookup(magma_lock_table, flare->binhash);\
	if (!flare_lock) flare_lock = magma_create_flare_lock(flare);\
	g_mutex_unlock(&magma_lookup_mutex);\
	if (!flare_lock) {\
		dbg(LOG_INFO, DEBUG_MUTEX, "[W+] lock FAILED on \"%s\": no flare lock found @ %s:%d", flare->path, __FILE__, __LINE__);\
	} else {\
		g_rw_lock_writer_lock(flare_lock);\
		dbg(LOG_INFO, DEBUG_MUTEX, "[W+] lock on \"%s\" @ %s:%d", flare->path, __FILE__, __LINE__);\
	}\
}

#define magma_flare_write_unlock(flare) {\
	g_mutex_lock(&magma_lookup_mutex);\
	GRWLock *flare_lock = g_tree_lookup(magma_lock_table, flare->binhash);\
	g_mutex_unlock(&magma_lookup_mutex);\
	if (flare_lock) {\
		g_rw_lock_writer_unlock(flare_lock);\
		dbg(LOG_INFO, DEBUG_MUTEX, "[W-] lock on \"%s\" @ %s:%d", flare->path, __FILE__, __LINE__);\
	} else {\
		dbg(LOG_INFO, DEBUG_MUTEX, "[W-] lock FAILED on \"%s\": no flare lock found @ %s:%d", flare->path, __FILE__, __LINE__);\
	}\
}

/***********************\
 * PERMISSION CHECKING *
\***********************/

/*
 * to check permission use check_permission() on test flare
 * and pass a ORed mask as "operations" argument
 */

typedef enum magma_permissions {
	MAGMA_OPERATION_R = 1,
	MAGMA_OPERATION_W = 2,
	MAGMA_OPERATION_X = 4
} magma_permissions_t;

extern uint8_t magma_check_permission(magma_flare_t *flare, uid_t uid, gid_t gid, magma_permissions_t operations);

/* explain result of check_permission() printing debug statements */
#define magma_explain_permission(optype) {\
	if (optype & MAGMA_OPERATION_R) {\
		dbg(LOG_ERR, DEBUG_ERR, "Reading not allowed");\
	}\
	if (optype & MAGMA_OPERATION_W) {\
		dbg(LOG_ERR, DEBUG_ERR, "Writing not allowed");\
	}\
	if (optype & MAGMA_OPERATION_X) {\
		dbg(LOG_ERR, DEBUG_ERR, "Executing not allowed");\
	}\
}

/********************************************\
 * DIRECTORY IMPLEMENTATION                 *
\********************************************/

/**
 * Check if a directory contains an element
 */
/*
#define magma_dir_contains_entry(flare, entry) \
	((entry) && (flare) && (flare->item.dir) && (flare->item.dir->entry_list) && \
	g_list_find_custom(flare->item.dir->entry_list, entry, (GCompareFunc) strcmp))
*/

extern gboolean magma_dir_contains_entry(magma_flare_t *flare, const gchar *entry);

extern int magma_add_flare_to_parent(magma_flare_t *flare);
extern int magma_remove_flare_from_parent(magma_flare_t *flare);
extern gboolean magma_dir_is_empty(magma_flare_t *dir);
// extern int magma_pop_dirent(magma_DIR_t *dirp, char *dirent); /* very internal use only */

#ifdef MAGMA_ENABLE_GARBAGE_COLLECTOR
extern void magma_schedule_destroy_flare(magma_flare_t *flare);
#endif

/******************************************************\
 * CACHE IMPLEMENTATION (see magma_flare_internals.c) *
\******************************************************/

/**
 * Caching is implemented using a b-tree of magma_flare_t
 * objects. Three methods are provided for b-tree access:
 *
 * magma_search() will search an entry from the cache
 * and return corresponding magma_flare_t object.
 *
 * magma_search_or_create() will use magma_search() to find
 * requested flare and will use magma_new_flare() if flare
 * is not in cache. if flare can be loaded from disk, will
 * also be upcasted. if flare is returned not upcasted,
 * probably don't exists on disk (or bad things happened!)
 *
 * magma_flare_t object is returned by address, not copied!
 * So any operation on its data will modify cache internal copy.
 * That's because magma_flare_t object also contain a mutex
 * and a read/write lock for object access control.
 *
 * magma_add_to_cache will add a magma_flare_t object to cache
 * performing placement operations recursively.
 *
 * magma_remove_from_cache will remove a magma_tree_node_t
 * object from the cache, relinking the tree to remain consistent.
 */
/*
extern magma_flare_t *magma_cache;
*/

extern magma_flare_t *magma_search(const char *path);
extern magma_flare_t *magma_search_by_hash(const unsigned char *hash);
extern magma_flare_t *magma_search_or_create(const char *hash);

extern magma_flare_t *magma_add_to_cache(magma_flare_t *flare);
extern gboolean magma_remove_from_cache(magma_flare_t *flare);

/******************************\
 * BALANCER (see balancer.c) *
\******************************/

/* every CHECK_LOAD_TIMEOUT microseconds a node re-calibrate its load */
#define MAGMA_CHECK_LOAD_TIMEOUT 300 * 1000 * 1000

/* how many key magmad should request from sibling node per each balancer turn? */
#define MAGMA_BALANCER_KEY_DELTA 10

// extern uint64_t actual_load(magma_volcano *node); /* used only inside balance.c? */
extern void *magma_balance(void *data);

/*********************************************************************\
 * BOOTSTRAP FUNCTION TO CREATE A NEW LAVA NETWORK (see bootstrap.c) *
\*********************************************************************/

/* this is the linked list of nodes participants */
extern magma_lava *lava;
extern int magma_check_filesystem(void);
extern int magma_bootstrap_network();

/**************************************************************************\
 * ROUTING FUNCTIONS TO ROUTE A KEY INSIDE A LAVA NETWORK (see routing.c) *
\**************************************************************************/

/* route a key inside main key space or redundant key space */
extern magma_volcano *magma_route_key(const char *key, magma_volcano *node);

/* route a file path inside main key space or redundant key space */
extern magma_volcano *magma_route_path(const char *path);

/* return true if nodes are equal, false otherwise */
extern int magma_compare_nodes(const magma_volcano *n1, const magma_volcano *n2);

extern void magma_parse_acl_file();
extern int magma_validate_connection(GSocket *socket, GSocketAddress *peer, const char *path, char optype);
extern gchar **magma_acl_get_paths();
extern gchar **magma_acl_get_netblocks(const gchar *path);
extern gchar magma_acl_get_policy(const gchar *path, const gchar *netblock);

extern gboolean magma_save_cache_node(guint *key, magma_flare_t *flare, int *remove);

extern char *magma_simplify_path(const char *path);

// data is unused, pass it NULL
extern int magma_compare_keys(const unsigned char *a, const unsigned char *b, gpointer data);

extern gchar *magma_point_filename_in_path(gchar *path);

/****************************************************\
 * SQL                                              *
\****************************************************/

extern void magma_init_sql();

extern void magma_flare_sql_save(magma_flare_t *flare);
extern void magma_flare_sql_delete(magma_flare_t *flare);
extern void magma_flare_sql_load(magma_flare_t *flare);
extern void magma_sql_save_volcano(magma_volcano *v);
extern void magma_sql_delete_volcano(magma_volcano *v);

extern dbi_result magma_sql_query(gchar *query);
extern dbi_result magma_sql_query_on_connection(gchar *query, dbi_conn dbi, GMutex *mutex);

extern guint32 magma_sql_fetch_integer(dbi_result result, int index);
extern double magma_sql_fetch_double(dbi_result result, int index);
extern gchar *magma_sql_fetch_string(dbi_result result, int index);

extern GAsyncQueue *magma_add_flare_queue;
extern GAsyncQueue *magma_remove_flare_queue;

#endif /* _MAGMA_FLARE_INTERNALS_H */

// vim:ts=4:nocindent
