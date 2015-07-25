/*
   MAGMA -- magma_flare_internals.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

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

#include "../magma.h"

/**
 * The GLib GTree used to store flares in memory
 */
GTree *magma_cache_gtree;

/**
 * The lock table used to synchronize magma_cache_gtree access
 */
GTree *magma_lock_table;

/**
 * The global mutex used to synchronize magma_lock_table access
 */
GMutex magma_lookup_mutex;

uid_t magma_flare_system_uid = 0; /**< UID of user magma got from system files scanning */
gid_t magma_flare_system_gid = 0; /**< GID of group magma got from system files scanning */

const char magma_dht_path[] = MAGMA_DHT_PATH; /**< the path of the distributed hashtable profile directory */
const char magma_run_path[] = MAGMA_RUN_PATH; /**< the path on real filesystem of the directory in which running data (pid, status) are saved */

/**
 * This is a pointer to a pagesize wide memory region filled with 0.
 * Is used mainly by mmap() operations to grow directory size, but
 * also to init a directory on disk.
 */
static char *magma_blank_page;

/**
 * Preallocate a blank page of pagesize bytes
 * used for directory mmap() operations
 *
 * @return 1 on success, 0 on failure
 */
int magma_allocate_blank_page()
{
	if (!magma_blank_page) {
		magma_blank_page = calloc(sizeof(char), magma_system_pagesize);
		if (!magma_blank_page) return (0);
	}
	return (1);
}

/*
 * a flare is really remove only if its in_use field
 * is equal or less than this value
 */
#define MAGMA_GC_USAGE_THRESHOLD 0 // was -20

/*
 * new version! a flare is removed from cache when its
 * time stamp is GC_TIMESTAMP_THRESHOLD second lower than
 * current time; that's:
 *
 * flare->timestamp + GC_TIMESTAMP_THRESHOLD < time(NULL)
 */
#define MAGMA_GC_TIMESTAMP_THRESHOLD 60

/*
 * garbage collector starts only if scheduled flares
 * are at least this value
 */
#define MAGMA_GC_START_THRESHOLD 0 /* was 100 */

#define magma_more_groups(groups, index) (index <= NGROUPS_MAX && (groups[index] || groups[index+1]))

/**
 * Check if a user with specified uid and gid is
 * allowed to perform "operations" operation on flare
 *
 * @param flare the flare to be accessed
 * @param uid UID accessing the flare
 * @param gid GID accessing the flare
 * @param operations bitmask field of required operations
 *
 * @return 0 if success, a uint8_t with failing operations otherwise
 */
uint8_t magma_check_permission(magma_flare_t *flare, uid_t uid, gid_t gid, magma_permissions_t operations)
{
	return (0); // TODO remove after debugging

	if (uid is 0) return (0);
	if (flare is NULL) return MAGMA_OPERATION_R|MAGMA_OPERATION_W|MAGMA_OPERATION_X;
	if (!flare->is_upcasted) return MAGMA_OPERATION_R|MAGMA_OPERATION_W|MAGMA_OPERATION_X;

	gid_t *groups = magma_get_user_groups(uid);

	/* check for reading */
	if (operations & MAGMA_OPERATION_R) {
		if (
			(flare->st.st_uid equals uid && (flare->st.st_mode & S_IRUSR)) /* is readable by the user */
			||
			(flare->st.st_gid equals gid && (flare->st.st_mode & S_IRGRP)) /* is readable by the group */
			||
			(flare->st.st_mode & S_IROTH) /* is readable by others */
		) {
			operations ^= MAGMA_OPERATION_R;
		}

		/* try with other groups */
		if (operations & MAGMA_OPERATION_R) {
			int gidx = 0;
			while (magma_more_groups(groups, gidx)) {
				if (flare->st.st_gid equals groups[gidx] && (flare->st.st_mode & S_IRGRP)) {
					operations ^= MAGMA_OPERATION_R;
					break;
				}
				gidx++;
			}
		}
	}

	/* check for writing */
	if (operations & MAGMA_OPERATION_W) {
		if (
			(flare->st.st_uid equals uid && (flare->st.st_mode & S_IWUSR)) /* is writable by the user */
			||
			(flare->st.st_gid equals gid && (flare->st.st_mode & S_IWGRP)) /* is writable by the group */
			||
			(flare->st.st_mode & S_IWOTH) /* is writable by others */
		) {
			operations ^= MAGMA_OPERATION_W;
		}

		/* try with other groups */
		if (operations & MAGMA_OPERATION_W) {
			int gidx = 0;
			while (magma_more_groups(groups, groups[gidx])) {
				if (flare->st.st_gid equals gid && (flare->st.st_mode & S_IWGRP)) {
					operations ^= MAGMA_OPERATION_W;
					break;
				}
				gidx++;
			}
		}
	}

	/* check for executing */
	if (operations & MAGMA_OPERATION_X) {
		if (
			(flare->st.st_uid equals uid && (flare->st.st_mode & S_IXUSR)) /* is executable by the user */
			||
			(flare->st.st_gid equals gid && (flare->st.st_mode & S_IXGRP)) /* is executable by the group */
			||
			(flare->st.st_mode & S_IXOTH) /* is executable by others */
		) {
			operations ^= MAGMA_OPERATION_X;
		}

		/* try with other groups */
		if (operations & MAGMA_OPERATION_X) {
			int gidx = 0;
			while (magma_more_groups(groups, gidx)) {
				if (flare->st.st_gid equals groups[gidx] && (flare->st.st_mode & S_IXGRP)) {
					operations ^= MAGMA_OPERATION_X;
					break;
				}
				gidx++;
			}
		}
	}

	return operations;
}

/**
 * instead of calling getpagesize() every time page size is needed,
 * flare system stores this value one here and than refers to
 * pagesize later in code.
 */
int magma_system_pagesize = 0;

/**
 * Performs prechecking of filesystem, creating storage directory.
 *
 * @return always 0. In case of failure, will exit(1) the program.
 */
int magma_check_filesystem()
{
	/* checking repository path */
	struct stat buf;

	if ( stat(magma_environment.hashpath, &buf) is -1 ) {
		if (errno is ENOENT) {
			if (mkdir(magma_environment.hashpath, S_ISGID|S_IRWXU|S_IRWXG) is -1) {
				dbg(LOG_ERR, DEBUG_ERR, "Can't mkdir(%s): %s", magma_environment.hashpath, strerror(errno));
				exit(1);
			}
			if (chown(magma_environment.hashpath, getuid(), getgid()) is -1) {
				dbg(LOG_ERR, DEBUG_ERR, "Can't chown(%s): %s", magma_environment.hashpath, strerror(errno));
				exit(1);
			}
		} else {
			dbg(LOG_ERR, DEBUG_ERR, "Error checking filesystem: %s", strerror(errno));
			exit(1);
		}
	}

	stat(magma_environment.hashpath, &buf);

	if ( ! S_ISDIR(buf.st_mode) ) {
		dbg(LOG_INFO, DEBUG_BOOT, "Path %s is not a directory!", magma_environment.hashpath);
		exit(1);
	}

	return(0);
}

/**
 * Bootstrap a new lava ring network. First checks the filesystem
 * using check_filesystem(). Than init myself node by assigning
 * the whole key space to this node. Finally creates "/" flare
 * directory and "/.dht" flare directory.
 */
int magma_bootstrap_network()
{
	dbg(LOG_INFO, DEBUG_BOOT, "Bootstrapping network now...");

	magma_check_filesystem();

	/*
	 * this node is keeping all the key space
	 */
	strcpy(myself.start_key, "0000000000000000000000000000000000000000");
	strcpy(myself.stop_key,  "ffffffffffffffffffffffffffffffffffffffff");

	myself.total_keys = 2;

	magma_sql_save_volcano(&myself);

	lava = magma_lava_new();

	magma_flare_t *slash_flare = magma_new_dir_flare("/");
	if (slash_flare is NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Error creating root directory");
		exit(1);
	}

	magma_add_to_cache(slash_flare);

	/* setting permission for root directory */
	slash_flare->st.st_mode |= S_IRWXU|S_IRWXG|S_IRWXO;

	dbg(LOG_INFO, DEBUG_BOOT, "Saving root directory...");
	if (magma_save_flare(slash_flare, TRUE)) {
		dbg(LOG_INFO, DEBUG_BOOT, "done!");
	} else {
		dbg(LOG_INFO, DEBUG_BOOT, "ERROR: %s! Aborting execution now!", strerror(errno));
		exit(1);
	}

	/* create distributed hash table profile directory */
	int dht_res = magma_mkdir(geteuid(), getegid(), MAGMA_DEFAULT_TTL, magma_dht_path, S_ISGID|S_IRWXU|S_IRWXG);
	if ((dht_res is -1) && (errno isNot EEXIST)) {
		dbg(LOG_ERR, DEBUG_ERR, "Creating dht profile directory (%s)", magma_dht_path);
	}

	dbg(LOG_INFO, DEBUG_BOOT, "Bootstrap completed!");
	magma_environment.state = magma_network_ready;

	return (0);
}

/**
 * simplify given path by removing /./ and /../ and ////+
 *
 * @param path the path to be simplified
 * @return simplified path
 */
char *magma_simplify_path(const char *path)
{
	if (strlen(path) is 0) return g_strdup("/");

	// split the path in tokens
	gchar **tokens = g_strsplit(path, "/", 256);
	guint ntokens = g_strv_length(tokens);

	// zero all the undesirable tokens
	guint i;
	for (i = 0; i < ntokens; i++) {
		if (strcmp(tokens[i], ".") is 0) {
			tokens[i][0] = '\0';
		} else

		if (strcmp(tokens[i], "..") is 0) {
			tokens[i][0] = '\0';
			if (i > 0) tokens[i - 1][0] = '\0';
		}
	}

	// cast a new GString
	GString *simplified = g_string_sized_new(4096);

	// append all the non-zero-length tokens
	for (i = 0; i < ntokens; i++) {
		if (tokens[i][0] isNot '\0') {
			g_string_append(simplified, "/");
			g_string_append(simplified, tokens[i]);
		}
	}

	g_strfreev(tokens);

	// if resulting path is zero length, duplicate a "/"
	if (simplified->len is 0) g_string_append(simplified, "/");

	// original path wasn't absolute? remove leading slash
	if (*path isNot '/') g_string_erase(simplified, 0, 1);

	// dbg(LOG_ERR, DEBUG_ERR, "simplify(%s) is %s", path, simplified->str);

	return g_string_free(simplified, FALSE);
}

/** compare two magma flare binary keys */
gint magma_compare_keys(const unsigned char *a, const unsigned char *b, gpointer data)
{
	(void) data;
	int i;

	for (i = 0; i < 20; i++) {
		if (a[i] < b[i]) return (-1);
		if (a[i] > b[i]) return (1);
	}

	return (0);
}

void magma_cache_key_destroyer(gpointer key)
{
	(void) key;
	g_free(key);
}

void magma_g_free_callback(gpointer key, gpointer unused)
{
	(void) unused;
	g_free(key);
}

void magma_free_flare_lock(GRWLock *lock, gpointer unused)
{
	(void) unused;
	g_rw_lock_clear(lock);
}

#ifdef MAGMA_DEAD_CODE
void magma_cache_value_destroyer(gpointer value)
{
	magma_flare_t *flare = (magma_flare_t *) value;
	magma_dispose_flare(flare);
}
#endif

#if MAGMA_DEAD_CODE
GAsyncQueue *magma_add_flare_queue;
GAsyncQueue *magma_remove_flare_queue;

void magma_add_flare_to_parent_thread(GAsyncQueue *q)
{
	g_async_queue_ref(q);

	magma_flare_t *flare;
	while (1) {
		flare = g_async_queue_pop(q);
		dbg(LOG_INFO, DEBUG_FLARE, "Adding %s to parent", flare->path);
		magma_add_flare_to_parent(flare);
	}
}

void magma_remove_flare_from_parent_thread(GAsyncQueue *q)
{
	g_async_queue_ref(q);

	magma_flare_t *flare;
	while (1) {
		flare = g_async_queue_pop(q);
		// magma_remove_flare_from_parent(flare, MAGMA_DEFAULT_TTL);

		dbg(LOG_INFO, DEBUG_DIR, "Removing %s", flare->path);
		magma_erase_flare(flare);
	}
}
#endif

/**
 * This call should be used on top of any program using the
 * flare system to reserve resources internally used.
 *
 * Before calling flare_system_init(), call magma_config_myself().
 * (see vulcano.h)
 *
 * Actually it is declared inside magma_flare_internals.c but
 * is exported by magma_flare.h because is needed to initialize
 * the flare system.
 */
void magma_flare_system_init()
{
	/* init the internal cache */
	magma_cache_gtree = g_tree_new_full(
		(GCompareDataFunc) magma_compare_keys, NULL,
		NULL, // (GDestroyNotify) magma_cache_key_destroyer   // this key is flare->binhash so should not be freed
		NULL  // (GDestroyNotify) magma_cache_value_destroyer // a flare must not be freed when is removed from the cache
	);

	/* init the lock table */
	magma_lock_table = g_tree_new_full(
		(GCompareDataFunc) magma_compare_keys, NULL,
		(GDestroyNotify) magma_g_free_callback,
		(GDestroyNotify) magma_free_flare_lock
	);

	/* set initial state to off */
	magma_environment.state = magma_network_loading;

	/* register signal handlers */
	/* signal(9, magma_cleanup); */  /* kill */
	signal(2,  magma_cleanup); /* SIGINT */
	signal(11, magma_cleanup); /* SIGSEGV */

	/* perform clean shutdown */
	signal(15, magma_clean_shutdown); /* SIGTERM */
	
	/* get pagesize */
	magma_system_pagesize = getpagesize();
	dbg(LOG_INFO, DEBUG_BOOT, "Using pagesize of %d bytes", magma_system_pagesize);

	/* getting magma user ID and group ID out of system files */
	struct passwd *pwline = NULL;
	while ((pwline = getpwent()) isNot NULL) {
		if (strcmp("magma", pwline->pw_name) is 0) {
			magma_flare_system_uid = pwline->pw_uid;
			break;
		}
	}
	endpwent();

	struct group *grline = NULL;
	while ((grline = getgrent()) isNot NULL) {
		if (strcmp("magma", grline->gr_name) is 0) {
			magma_flare_system_gid = grline->gr_gid;
			break;
		}
	}
	endgrent();

	/* setting magma UID and GID */
	if (seteuid(magma_flare_system_uid) is -1) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't change EUID to %d: %s", magma_flare_system_uid, strerror(errno));
	}
	if (setegid(magma_flare_system_gid) is -1) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't change EGID to %d: %s", magma_flare_system_gid, strerror(errno));
	}

	dbg(LOG_INFO, DEBUG_BOOT, "UID: Real: %d, Effective: %d, magma: %d", getuid(), geteuid(), magma_flare_system_uid);
	dbg(LOG_INFO, DEBUG_BOOT, "GID: Real: %d, Effective: %d, magma: %d", getgid(), getegid(), magma_flare_system_gid);

	/* setup directory in /var/run */
	if ((mkdir(magma_environment.hashpath, S_IRWXU|S_IRWXG|S_ISGID) is -1) && errno isNot EEXIST) {
		dbg(LOG_ERR, DEBUG_ERR, "Unable to mkdir(%s): %s", magma_run_path, strerror(errno));
		exit(64);
	}

#ifndef MAGMA_DEBUGGING_BUILD
	if (chown(magma_run_path, magma_flare_system_uid, magma_flare_system_gid) is -1) {
		dbg(LOG_ERR, DEBUG_ERR, "Unable to chown(%s): %s", magma_run_path, strerror(errno));
		exit(64);
	}
#endif

	/* load ACLs from ACL file */
	magma_parse_acl_file();

	/* init the network layer */
	magma_init_net_layer();

	/* init the operation result cache */
	magma_init_server_flare();

	/* init the SQL backend */
	magma_init_sql();

	/* load console commands */
	magma_init_console();

	/* initialize the callback lookup tables */
	magma_init_flare_callbacks();
	magma_init_node_callbacks();

#if 0
	/*
	 * init add_flare_to_parent and remove_flare_from_parent queues
	 */
	magma_add_flare_queue = g_async_queue_new();
	magma_remove_flare_queue = g_async_queue_new();
	
	/*
	 * start add_flare_to_parent thread
	 */
	GError *error = NULL;
	GThread *add_flare_to_parent = g_thread_try_new(
		"Add flare to parent",
		(GThreadFunc) magma_add_flare_to_parent_thread,
		magma_add_flare_queue,
		&error);

	if (!add_flare_to_parent) {
		dbg(LOG_ERR, DEBUG_ERR, "No add_flare_to_parent thread started: %s", error->message);
		dbg(LOG_ERR, DEBUG_ERR, "Exiting now!");
		g_error_free(error);
		exit(1);
	}

	/*
	 * start remove_flare_from_parent thread
	 */
	error = NULL;
	GThread *remove_flare_from_parent = g_thread_try_new(
		"Remove flare from parent",
		(GThreadFunc) magma_remove_flare_from_parent_thread,
		magma_remove_flare_queue,
		&error);

	if (!remove_flare_from_parent) {
		dbg(LOG_ERR, DEBUG_ERR, "No remove_flare_from_parent thread started: %s", error->message);
		dbg(LOG_ERR, DEBUG_ERR, "Exiting now!");
		g_error_free(error);
		exit(1);
	}
#endif
}

/**
 * Build the DHT network and starts the balancer thread, if requested.
 *
 * If bootstrap is true, bootserver and bootport are ignored,
 * so flare_system_init(TRUE, NULL, 0, 0) is a valid call.
 *
 * @param bootstrap if true, new lava will be created, if false this node will remote bootstrap
 * @param bootserver remote boot server used if bootstrap is false
 * @param bootport remote boot port used if bootstrap is false
 * @param start_balancer if true, balancer thread will be started
 */
void magma_build_network(int bootstrap, char *bootserver, int bootport, int start_balancer)
{
	/*
	 * if this node is supposed to create a new lava network, do it.
	 * otherwise, join the existing one on specified bootserver
	 */
	if (bootstrap) {
		magma_bootstrap_network();
	} else if (bootserver) {
		magma_join_network(bootserver, bootport);
	} else {
		magma_load_status();
	}

	if (!lava) {
		dbg(LOG_ERR, DEBUG_BOOT, "No lava topology installed after boot. Exiting now.");
		exit (1);
	}

	if (magma_this_node_is_coordinator() && magma_environment.state isNot magma_network_ready) {
		dbg(LOG_ERR, DEBUG_BOOT, "Lava network is not ready. Exiting now.");
		exit (1);
	}

	/*
	 * if start_balancer was requested, start a thread to balance nodes
	 */
	if (start_balancer) {
		/*
	 	* starting balancer
	 	*/
		GError *error = NULL;
		GThread *balancer_thread = g_thread_try_new("Balancer thread", magma_balance, &myself, &error);

		if (!balancer_thread) {
			dbg(LOG_ERR, DEBUG_ERR, "No balancer started: %s", error->message);
			dbg(LOG_ERR, DEBUG_ERR, "Normal operations will work but node load will not be recalculated");
			g_error_free(error);
		} else {
			dbg(LOG_INFO, DEBUG_BALANCER, "Load balancer started.");
		}
	}
}

/**
 * Create new uncasted flares.
 *
 * @param path the path of new uncasted flare
 * @return a pointer to new flare
 */
magma_flare_t *magma_new_flare_(const char *path, char *file, int line)
{
	/* allocate new flare */
	magma_flare_t *flare = g_new0(magma_flare_t, 1);
	if (!flare) {
		dbg(LOG_ERR, DEBUG_ERR, "New flare allocation failed");
		return (NULL);
	}

	/*
	 * At this stage, the flare has no type. There is however
	 * one particular case: the root flare "/", which must be
	 * forced to be 'd' for consistency.
	 */
	flare->type = (strcmp(path, "/") is 0) ? MAGMA_FLARE_TYPE_DIR : MAGMA_FLARE_TYPE_UNKNOWN;

	/* flare fields related to path */
	flare->binhash = sha1_data(path,strlen(path));
	assert(flare->binhash isNot NULL);

	flare->hash = armour_hash(flare->binhash);
	assert(flare->hash isNot NULL);

	flare->parent_path = g_strdup(path);
	assert(flare->parent_path isNot NULL);

	char *last_slash = rindex(flare->parent_path, '/');
	if (last_slash equals flare->parent_path) {
		flare->parent_path = g_strdup("/");
	} else if (last_slash isNot NULL) {
		*last_slash = '\0';
	}
	
	/* flare fields related to parent path */
	flare->parent_binhash = sha1_data(flare->parent_path, strlen(flare->parent_path));
	assert(flare->parent_binhash isNot NULL);

	flare->parent_hash = armour_hash(flare->parent_binhash);
	assert(flare->parent_hash isNot NULL);

	flare->path = g_strdup(path);
	assert(flare->path isNot NULL);

	/* flare saving contents */
	flare->contents = magma_xlate_path(path);
	assert(flare->contents isNot NULL);

	flare->parent_contents = magma_xlate_path(flare->parent_path);
	assert(flare->parent_contents isNot NULL);

	/* flare scores */
	flare->is_upcasted = 0;

	/* flare metadata (struct stat) */
	memset(&(flare->st), 0, sizeof(struct stat));
	flare->st.st_nlink = 1;

#if MAGMA_DECLARE_FLARE_ITEM_FIELD
	flare->item.dir = NULL;
	flare->item.file = NULL;
	flare->item.blockdev = NULL;
	flare->item.chardev = NULL;
	flare->item.symlink = NULL;
	flare->item.fifo = NULL;
#endif

	dbg(LOG_INFO, DEBUG_FLARE, "new flare created for %s at %s:%d", flare->path, file, line);
	return flare;
}

/**
 * Update the struct stat of a flare
 *
 * @param flare the flare to be updated
 */
void magma_flare_update_stat(magma_flare_t *flare)
{
	struct stat st_tmp;
	if (lstat(flare->contents, &st_tmp) isNot -1) {
		flare->st.st_size = st_tmp.st_size;
		flare->st.st_blocks = st_tmp.st_blocks;
		flare->st.st_blksize = st_tmp.st_blksize;
	}
}

/**
 * cast a flare to a directory flare
 *
 * @param flare the flare to be casted
 * @return 1 on success, 0 otherwise
 */
int magma_cast_to_dir(magma_flare_t *flare) {
	if (flare is NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Null flare passed to magma_cast_to_dir()");
		return (0);
	}

	flare->st.st_mode |= __S_IFDIR;
	flare->type = MAGMA_FLARE_TYPE_DIR;
	flare->is_upcasted = 1;

	dbg(LOG_INFO, DEBUG_FLARE, "Flare %s casted to dir", flare->path);

	return (1);
}

/**
 * create a new, upcasted directory flare
 *
 * @param path the path of new directory flare
 * @return a pointer to new directory flare
 */
magma_flare_t *magma_new_dir_flare(const char *path)
{
	magma_flare_t *flare = magma_new_flare(path);
	magma_cast_to_dir(flare);
	return flare;
}

/**
 * cast a flare to a regular file flare
 *
 * @param flare the flare to be casted
 * @return 1 on success, 0 otherwise
 */
int magma_cast_to_file(magma_flare_t *flare) {
	if (flare is NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "null flare passed to magma_cast_to_file()");
		return (0);
	}

	flare->st.st_mode |= __S_IFREG;
	flare->type = MAGMA_FLARE_TYPE_REGULAR;
	flare->is_upcasted = 1;

	dbg(LOG_INFO, DEBUG_FLARE, "Flare %s casted to file", flare->path);
	return (1);
}

/**
 * create a new, upcasted regular file flare
 *
 * @param path the path of the new flare
 * @return a pointer to the new flare
 */
magma_flare_t *magma_new_file_flare(const char *path)
{
	magma_flare_t *flare = magma_new_flare(path);
	magma_cast_to_file(flare);
	return flare;
}

/**
 * cast a flare to a block device flare
 *
 * @param flare the flare to be casted
 * @return 1 on success, 0 otherwise
 */
int magma_cast_to_blockdev(magma_flare_t *flare) {
	if (flare is NULL ) {
		dbg(LOG_ERR, DEBUG_ERR, "null flare passed to magma_cast_to_blockdev()");
		return (0);
	}

#if MAGMA_DECLARE_FLARE_ITEM_FIELD
	if ((flare->item.blockdev = malloc(sizeof(magma_flare_blockdev_t))) is NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't allocate magma_flare_blockdev_t for flare %s", flare->path);
		return (0);
	}
#endif

	flare->st.st_mode |= __S_IFBLK;
	flare->type = MAGMA_FLARE_TYPE_BLOCK;
	flare->is_upcasted = 1;

	dbg(LOG_INFO, DEBUG_FLARE, "Flare %s casted to blockdev", flare->path);
	return (1);
}

/**
 * create a new, upcasted block device flare
 *
 * @param path the path of the new flare
 * @return a pointer to the new flare
 */
magma_flare_t *magma_new_blockdev_flare(const char *path)
{
	magma_flare_t *flare = magma_new_flare(path);
	magma_cast_to_blockdev(flare);
	return flare;
}

/**
 * cast a flare to a character device flare
 *
 * @param flare the flare to be casted
 * @return 1 on success, 0 otherwise
 */
int magma_cast_to_chardev(magma_flare_t *flare) {
	if (flare is NULL ) {
		dbg(LOG_ERR, DEBUG_ERR, "null flare passed to magma_cast_to_chardev()");
		return (0);
	}

	flare->st.st_mode |= __S_IFCHR;
	flare->type = MAGMA_FLARE_TYPE_CHAR;
	flare->is_upcasted = 1;

	dbg(LOG_INFO, DEBUG_FLARE, "Flare %s casted to chardev", flare->path);
	return (1);
}

/**
 * create a new, upcasted character device flare
 *
 * @param path the path of the new flare
 * @return a pointer to the new flare
 */
magma_flare_t *magma_new_chardev_flare(const char *path)
{
	magma_flare_t *flare = magma_new_flare(path);
	magma_cast_to_chardev(flare);
	return flare;
}

/**
 * cast a flare to a symbolic link flare
 *
 * @param flare the flare to be casted
 * @return 1 on success, 0 otherwise
 */
int magma_cast_to_symlink(magma_flare_t *flare) {
	if (flare is NULL ) {
		dbg(LOG_ERR, DEBUG_ERR, "null flare passed to magma_cast_to_symlink()");
		return (0);
	}

	flare->st.st_mode |= __S_IFLNK;
	flare->type = MAGMA_FLARE_TYPE_SYMLINK;
	flare->is_upcasted = 1;

	dbg(LOG_INFO, DEBUG_FLARE, "Flare %s casted to symlink", flare->path);
	return (1);
}

/**
 * create a new, upcasted symlink flare
 *
 * @param path the path of the new flare
 * @return a pointer to the new flare
 */
magma_flare_t *magma_new_symlink_flare(const char *path)
{
	magma_flare_t *flare = magma_new_flare(path);
	magma_cast_to_symlink(flare);
	return flare;
}

/**
 * cast a flare to a FIFO flare
 *
 * @param flare the flare to be casted
 * @return 1 on success, 0 otherwise
 */
int magma_cast_to_fifo(magma_flare_t *flare) {
	if (flare is NULL ) {
		dbg(LOG_ERR, DEBUG_ERR, "null flare passed to magma_cast_to_fifo()");
		return (0);
	}

	flare->st.st_mode |= __S_IFIFO;
	flare->type = MAGMA_FLARE_TYPE_FIFO;
	flare->is_upcasted = 1;

	dbg(LOG_INFO, DEBUG_FLARE, "Flare %s casted to FIFO", flare->path);
	return (1);
}

/**
 * create a new, upcasted FIFO flare
 *
 * @param path the path of the new flare
 * @return a pointer to the new flare
 */
magma_flare_t *magma_new_fifo_flare(const char *path)
{
	magma_flare_t *flare = magma_new_flare(path);
	magma_cast_to_fifo(flare);
	return flare;
}

int magma_flare_upcast(magma_flare_t *flare) {
	     if (magma_isdir(flare))  return magma_cast_to_dir(flare);
	else if (magma_isreg(flare))  return magma_cast_to_file(flare);
	else if (magma_isblk(flare))  return magma_cast_to_blockdev(flare);
	else if (magma_ischr(flare))  return magma_cast_to_chardev(flare);
	else if (magma_isfifo(flare)) return magma_cast_to_fifo(flare);
	else if (magma_islnk(flare))  return magma_cast_to_symlink(flare);

	dbg(LOG_ERR, DEBUG_ERR, "flare %s can't be upcasted!", flare->path);
	return (0);
}

/**
 * free resources used by a flare
 *
 * @param flare the flare to be destroyed
 * @return TRUE on success, FALSE otherwise
 */
void magma_dispose_flare(magma_flare_t *flare)
{
	return;
	if (flare is NULL) return;

	dbg(LOG_INFO, DEBUG_FLARE, "Destroying flare %s", flare->path);

    g_free((flare->contents));
    g_free((flare->parent_path));
    g_free((flare->parent_hash));
    g_free((flare->parent_binhash));
    g_free((flare->parent_contents));
    g_free((flare->hash));
    g_free((flare->binhash));
    g_free((flare->commit_path));
    g_free((flare->commit_time));
    g_free((flare->commit_url));
    g_free((flare->path));


	if (flare->is_upcasted) {
#if MAGMA_DECLARE_FLARE_ITEM_FIELD
		if (magma_islnk(flare)) {
			if (flare->item.symlink isNot NULL) {
				if (flare->item.symlink->target isNot NULL) {
					g_free(flare->item.symlink->target);
				}
				g_free(flare->item.symlink);
			}
		} else if (magma_isdir(flare)) {
			if (flare->item.dir isNot NULL) {
				g_free(flare->item.dir);
			}
		} else if (magma_isblk(flare)) {
			if (flare->item.blockdev isNot NULL) {
				g_free(flare->item.blockdev);
			}
		} else if (magma_ischr(flare)) {
			if (flare->item.chardev isNot NULL) {
				g_free(flare->item.chardev);
			}
		} else if (magma_isfifo(flare)) {
			if (flare->item.fifo isNot NULL) {
				g_free(flare->item.fifo);
			}
		} else if (magma_isreg(flare)) {
			if (flare->item.file isNot NULL) {
				g_free(flare->item.file);
			}
		} else {
			dbg(LOG_ERR, DEBUG_ERR, "Unknown flare type for %s", flare->path);
		}
#endif
	}

	g_free(flare);
}

/**
 * duplicate a magma_flare_t object
 *
 * @param flare the flare to be duplicated
 * @return a copy of the flare object
 */
magma_flare_t *magma_duplicate_flare(magma_flare_t *flare)
{
	if (flare is NULL) return (NULL);
	return (flare);

	dbg(LOG_INFO, DEBUG_FLARE, "Duplicating flare %s", flare->path);

	magma_flare_t *flare_dup = g_new0(magma_flare_t, 1);
	if (!flare_dup) {
		dbg(LOG_ERR, DEBUG_ERR, "New flare allocation failed");
		return (NULL);
	}

	/*
	 * copy strings
	 */
	flare_dup->contents        = g_strdup(flare->contents);
	flare_dup->parent_path     = g_strdup(flare->parent_path);
	flare_dup->parent_hash     = g_strdup(flare->parent_hash);
	flare_dup->parent_contents = g_strdup(flare->parent_contents);
	flare_dup->hash            = g_strdup(flare->hash);
	flare_dup->commit_path     = g_strdup(flare->commit_path);
	flare_dup->commit_time     = g_strdup(flare->commit_time);
	flare_dup->commit_url      = g_strdup(flare->commit_url);
	flare_dup->path            = g_strdup(flare->path);

	/*
	 * copy binary buffers
	 */
	flare_dup->parent_binhash = g_new(unsigned char, 20);
	memcpy(flare_dup->parent_binhash, flare->parent_binhash, 20);

	flare_dup->binhash = g_new(unsigned char, 20);
	memcpy(flare_dup->binhash, flare->binhash, 20);

	/*
	 * copy struct stat
	 */
	memcpy(&flare_dup->st, &flare->st, sizeof(struct stat));

	/*
	 * copy specialized fields
	 */
	if (flare->is_upcasted) {
#if MAGMA_DECLARE_FLARE_ITEM_FIELD
		if (magma_islnk(flare)) {
			flare_dup->item.symlink = g_new0(magma_flare_symlink_t, 1);
			if (flare->item.symlink && flare->item.symlink->target) {
				flare_dup->item.symlink->target = g_strdup(flare->item.symlink->target);
			}
		} else if (magma_isdir(flare)) {
			flare_dup->item.dir = g_new0(magma_flare_dir_t, 1);
			if (flare_dup->item.dir) {
				flare_dup->item.dir->entries = flare->item.dir->entries;
				flare_dup->item.dir->needs_flush = FALSE;
				flare_dup->item.dir->entry_list = g_list_copy_deep(
						flare->item.dir->entry_list, (GCopyFunc) g_strdup, NULL);
			}
		} else if (magma_isblk(flare)) {
			flare_dup->item.blockdev = g_new0(magma_flare_blockdev_t, 1);
		} else if (magma_ischr(flare)) {
			flare_dup->item.chardev = g_new0(magma_flare_chardev_t, 1);
		} else if (magma_isfifo(flare)) {
			flare_dup->item.fifo = g_new0(magma_flare_fifo_t, 1);
		} else if (magma_isreg(flare)) {
			flare_dup->item.file = g_new0(magma_flare_file_t, 1);
		} else {
			dbg(LOG_ERR, DEBUG_ERR, "Unknown flare type for %s", flare->path);
		}
#endif
	}

	dbg(LOG_INFO, DEBUG_FLARE, "%s duplicated", flare_dup->path);
	return (flare_dup);
}


/**
 * outputs to debugging facility all informations avalable about given flare.
 *
 * @param flare the flare to be printed
 */
void magma_print_flare(magma_flare_t *flare)
{
	gchar *last_access = g_time_val_to_iso8601(&flare->last_access);
	dbg(LOG_INFO, DEBUG_FLARE, "path: %s", flare->path);
	dbg(LOG_INFO, DEBUG_FLARE, "hash: %s", flare->hash);
	dbg(LOG_INFO, DEBUG_FLARE, "contents file: %s", flare->contents);
	dbg(LOG_INFO, DEBUG_FLARE, "in_use: %s", last_access);
	g_free(last_access);

	if (magma_isdir(flare)) {
		dbg(LOG_INFO, DEBUG_FLARE, "flare subtype: directory");
	} else if (magma_isreg(flare)) {
		dbg(LOG_INFO, DEBUG_FLARE, "flare subtype: regular file");
	} else if (magma_isblk(flare)) {
		dbg(LOG_INFO, DEBUG_FLARE, "flare subtype: block device");
	} else if (magma_ischr(flare)) {
		dbg(LOG_INFO, DEBUG_FLARE, "flare subtype: character device");
	} else if (magma_isfifo(flare)) {
		dbg(LOG_INFO, DEBUG_FLARE, "flare subtype: FIFO");
	} else if (magma_islnk(flare)) {
		dbg(LOG_INFO, DEBUG_FLARE, "flare subtype: symbolic link");
	}
}

/**
 * searches flares from internal cache. if an entry
 * is not available, a null pointer is returned.
 * cache is searched by binary hash
 *
 * if entry is missing, it will *NOT* be created. this
 * function just query cache contents, don't update or
 * add nothing. a more generic function, called
 * magma_search_or_create() exists and should be
 * preferred as main interface to flare subsystem.
 *
 * @param hash the binary hash of the flare to be searched
 * @return a pointer to the flare if found, NULL otherwise
 *
 */
magma_flare_t *magma_search_by_hash(const unsigned char *hash)
{
	/*
	 * lock the global mutex
	 */
	g_mutex_lock(&magma_lookup_mutex);

	/*
	 * find the flare and update its last_access field
	 */
	magma_flare_t *flare = g_tree_lookup(magma_cache_gtree, hash);
	if (flare) g_get_current_time(&flare->last_access);

	/*
	 * unlock and return
	 */
	g_mutex_unlock(&magma_lookup_mutex);
	return (flare);
}

/**
 * searches flares from internal cache. if an entry
 * is not available, a null pointer is returned.
 *
 * if entry is missing, it will *NOT* be created. this
 * function just query cache contents, don't update or
 * add nothing. a more generic function, called
 * magma_search_or_create() exists and should be
 * preferred as main interface to flare subsystem.
 *
 * @param path the path to be searched
 * @return a pointer to the flare if found, NULL otherwise
 */
magma_flare_t *magma_search(const char *path)
{
	/* getting path's hash */
	unsigned char *hash = sha1_data(path, strlen(path));
	if (!hash) {
		dbg(LOG_ERR, DEBUG_FLARE, "Error converting hash for %s to printable form", path);
		return (NULL);
	}

	magma_flare_t *result = magma_search_by_hash(hash);

	g_free(hash);
	return result;
}

/**
 * searches flares from internal cache. if flare is not found,
 * a new one is created using new_flare() macro.
 *
 * after a flare has ben got (from cache or from scratch) is
 * checked on disk and, if found, loaded from disk.
 *
 * in both cases, if flare->is_upcasted is false, flare basically
 * does not exists on disk, or is not loadable.
 *
 * @param path the path to be searched
 * @param lock if 'r' the flare is read-locked, if 'w' is write-locked
 * @return a pointer to the flare or NULL if something went wrong
 */
magma_flare_t *magma_search_or_create(const char *path)
{
	if (!path) {
		dbg_stack(LOG_ERR, DEBUG_ERR, "magma_search_or_create called with NULL *path");
		return (NULL);
	}

	if (strlen(path) is 0) {
		dbg_stack(LOG_ERR, DEBUG_ERR, "magma_search_or_create called with zero length *path");
		return (NULL);
	}

	/*
	 * simplify flare path, removing ../
	 */
	char *simplepath = magma_simplify_path(path);
	dbg(LOG_INFO, DEBUG_CACHE, "searching flare %s", simplepath);

	/*
	 * first search flare in cache
	 */
	magma_flare_t *flare = magma_search(simplepath);

	/*
	 * if it's not cached, create un-casted flare and
	 * then load it from disk
	 */
	if (!flare) {
		flare = magma_new_flare(simplepath);
		if (flare && magma_check_flare(flare)) {
			magma_load_flare(flare);
		}
	} else {
		if (!flare->is_upcasted || !flare->commit_time) {
			magma_load_flare(flare);
		}
	}

	g_free(simplepath);

	if (flare) return (magma_duplicate_flare(flare));
	return (NULL);
}

/**
 * Create a flare lock in the lock table
 *
 * @param flare the flare the lock has to be created for
 * @return the GRWLock flare lock on success, NULL otherwise
 */
GRWLock *magma_create_flare_lock(magma_flare_t *flare)
{
	/*
	 * copy the binhash key
	 */
	gchar *binhash = g_new0(gchar, 20);
	if (!binhash) return (NULL);
	memcpy(binhash, flare->binhash, 20);

	/*
	 * declare the flare lock
	 */
	GRWLock *flare_lock = g_new0(GRWLock, 1);
	if (!flare_lock) {
		g_free(binhash);
		return (NULL);
	}
	g_rw_lock_init(flare_lock);

	/*
	 * add the flare lock to the lock table
	 */
	g_tree_insert(magma_lock_table, binhash, flare_lock);
	return (flare_lock);
}

/**
 * Adds an entry to magma to caching system.
 *
 * @param flare is the flare to be added to cache
 * @return the node added if successful, NULL otherwise.
 */
magma_flare_t *magma_add_to_cache(magma_flare_t *flare)
{
	if (!flare) {
		dbg(LOG_ERR, DEBUG_ERR, "NULL flare passed to magma_add_to_cache()");
		return (NULL);
	}

	dbg(LOG_INFO, DEBUG_CACHE, "Request to cache %s", flare->path);

	/*
	 * lock the global lookup mutex and lookup the flare lock
	 */
	g_mutex_lock(&magma_lookup_mutex);

	/*
	 * add the flare to the hash table
	 */
#if 0
	gchar *binhash = g_new0(gchar, 20);
	if (binhash) {
		memcpy(binhash, flare->binhash, 20);
		g_tree_insert(magma_cache_gtree, binhash, flare);
	}
#endif
	g_tree_insert(magma_cache_gtree, flare->binhash, flare);

	/*
	 * unlock and return
	 */
	g_mutex_unlock(&magma_lookup_mutex);
	return (flare);
}

/**
 * Remove an entry from the cache.
 *
 * @param flare the flare to be removed
 * @return TRUE on success, FALSE on failure
 */
gboolean magma_remove_from_cache(magma_flare_t *flare)
{
	if (flare is NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "NULL flare passed to magma_remove_from_cache()");
		return (FALSE);
	}

	dbg(LOG_INFO, DEBUG_CACHE, "Request to uncache %s", flare->path);

	/*
	 * lock the global mutex, remove the flare
	 * and release the lock
	 */
	g_mutex_lock(&magma_lookup_mutex);
	gboolean result = g_tree_remove(magma_cache_gtree, flare->binhash);
	g_mutex_unlock(&magma_lookup_mutex);

	return (result);
}

/**
 * create a flare on disk. that is: making directory,
 * creating metadata and contents files and issuing
 * chmod on everything, using flare->st.st_mode ORed
 * with user read and write permissions.
 *
 * @param flare magma_flare_t structure to use as model
 * @return 0 on success, -1 on error (errno is set accordingly)
 * \todo check errno in this function, it's not always set.
 */
int magma_init_flare_on_disk(magma_flare_t *flare)
{
	if (flare is NULL) {
		dbg(LOG_ERR, DEBUG_FLARE, "NULL flare passed to magma_create_flare");
		return (-1);
	}

	magma_touch_flare(flare, MAGMA_TOUCH_ATIME|MAGMA_TOUCH_CTIME, 0, 0);

	/* create contents file */
	if (magma_isdir(flare)) {
		char buf[5] = ".\0..\0";
		GError *error = NULL;
		g_file_set_contents(flare->contents, buf, 5, &error);
		if (error) {
			g_error_free(error);
		}

		chmod(flare->path, S_IRUSR|S_IWUSR|S_IXUSR);
	} else {
		if (magma_isblk(flare) || magma_ischr(flare) || magma_isfifo(flare)) {
			/* BLOCK or CHAR DEVICE or FIFO */
			int res = mknod(flare->contents, flare->st.st_mode, flare->st.st_rdev);
			if ( res is -1 ) {
				dbg(LOG_ERR, DEBUG_ERR, "Can't create file contents for %s: %s", flare->path, strerror(errno));
				return (-1);
			}
		} else {
			GError *error = NULL;
			g_file_set_contents(flare->contents, "", 0, &error);
			if (error) {
				dbg(LOG_ERR, DEBUG_ERR, "Can't create file contents for %s: %s", flare->path, error->message);
				g_error_free(error);
				return (-1);
			}
		}

		chmod(flare->contents, S_IRUSR|S_IWUSR|S_IXUSR);
	}

	return (0);
}

/**
 * check if flare is still on disk (has not been removed by
 * external agent able to access repository space)
 *
 * @param flare flare to be checked
 * @return 1 on success, 0 on failure (errno is set accordingly)
 */
int magma_check_flare(magma_flare_t *flare)
{
	return (access(flare->contents, F_OK) is 0) ? 1 : 0;
}

/**
 * save a flare to disk. if flare does not exists,
 * magma_create_flare() is called. then, flare st,
 * path, hash, xlated, parent_path, parent_hash and
 * parent_xlated members are dumped inside metadata.
 *
 * if flare is a directory, pages are msync()ed to
 * disk.
 *
 * @param flare the flare to be saved
 * @param init_flare if TRUE, the flare is initialized on disk and saved into SQL
 * @return 1 on success, 0 on failure
 */
gboolean magma_save_flare(magma_flare_t *flare, gboolean init_flare)
{
	if (!flare) {
		dbg(LOG_ERR, DEBUG_ERR, "Null flare passed to magma_save_flare()");
		return (FALSE);
	}

	ssize_t res = 0;
	dbg(LOG_INFO, DEBUG_FLARE, "Saving flare %s", flare->path);

	/*
	 * check if flare already exists on disk,
	 * otherwise prepare its structure
	 */
	if (!magma_check_flare(flare)) {
		res = magma_init_flare_on_disk(flare);
		if (res isNot 0) {
			return (TRUE);
		}
	}

	/*
	 * save the flare in the SQL repository
	 */
	if (init_flare) {
		magma_flare_sql_save(flare);
		magma_load_flare(flare);
	}

	/*
	 * update flare->contents mode
	 */
	chmod(flare->contents, flare->st.st_mode);

	/*
	 * do specific actions
	 */
#if 0
	if (magma_isdir(flare)) {
	} else if (magma_islnk(flare)) {
	} else if (magma_isreg(flare)) {
	} else if (magma_isblk(flare)) {
	} else if (magma_ischr(flare)) {
	} else if (magma_isfifo(flare)) {
	} else {
		dbg(LOG_ERR, DEBUG_ERR, "Flare %s is not casted!", flare->path);
	}
#endif

	return (TRUE);
}

/**
 * Erase a flare from disk
 *
 * @param flare the flare to be saved
 * @return 0 on success, -1 on failure
 */
int magma_erase_flare_from_disk(magma_flare_t *flare)
{
	dbg(LOG_INFO, DEBUG_FLARE, "Erasing flare %s", flare->path);

	if (!magma_check_flare(flare)) return (-1);

	int res = unlink(flare->contents);

	if (res is -1) {
		dbg(LOG_ERR, DEBUG_ERR, "Error while erasing %s contents: %s", flare->path, strerror(errno));
		return (-1);
	}

	return (0);
}

/**
 * Erase a flare from parent and from disk, basically
 * calling remove_flare_from_parent() and
 * erase_flare_from_disk() in sequence. Flare is also
 * removed from cache.
 *
 * @param flare the flare to be saved
 * @return 0 on success, -1 on failure
 */
int magma_erase_flare(magma_flare_t *flare)
{
	if (!flare) {
		dbg(LOG_ERR, DEBUG_ERR, "magma_erase_flare(NULL) called!");
		return (-1);
	}

	dbg(LOG_INFO, DEBUG_FLARE, "Erasing flare %s", flare->path);

	int res = magma_remove_from_cache(flare);
	if (res) {
		res = magma_erase_flare_from_disk(flare);
		if (res is 0) {
			magma_flare_sql_delete(flare);
			magma_dispose_flare(flare);
		}
	}

	return res;
}

/**
 * Load a flare from disk.
 *
 * @param flare the flare to be loaded
 * @return 1 on success, 0 otherwise (errno is set accordingly)
 */
void magma_load_flare(magma_flare_t *flare)
{
	/*
	 * update struct stat from flare content
	 */
	magma_flare_update_stat(flare);

	/*
	 * load flare metadata from flare_<hostname> table
	 * if at least one of the metadata involved is missing
	 */
	dbg(LOG_INFO, DEBUG_FLARE,
		"Loading flare metadata from SQL "
		"[type: %c] [com.path: %s] [com.time: %s] [uid: %d] [gid: %d]",
		flare->type, flare->commit_path, flare->commit_time, flare->st.st_uid, flare->st.st_gid
	);

	gchar *query = g_strdup_printf(
		"select type, commit_path, commit_time, uid, gid from flare_%s where path = '%s'",
		myself.node_name, flare->path);

	dbi_result result = magma_sql_query(query);
	g_free(query);

	if (result && dbi_result_next_row(result)) {
		gchar *type = magma_sql_fetch_string(result, 1);
		flare->type = type[0];
		g_free(type);

		flare->commit_path = magma_sql_fetch_string(result, 2);
		flare->commit_time = magma_sql_fetch_string(result, 3);

		/*
		 * set the flare type in the struct stat st_mode field
		 */
		flare->st.st_mode &= ~S_IFMT;
		switch (flare->type) {
			case 'r': flare->st.st_mode |= S_IFREG;  break;
			case 'd': flare->st.st_mode |= S_IFDIR;  break;
			case 'c': flare->st.st_mode |= S_IFCHR;  break;
			case 'b': flare->st.st_mode |= S_IFBLK;  break;
			case 'l': flare->st.st_mode |= S_IFLNK;  break;
			case 'p': flare->st.st_mode |= S_IFIFO;  break;
			case 's': flare->st.st_mode |= S_IFSOCK; break;
		}

		/*
		 * build flare commit URL
		 */
		flare->commit_url = (flare->type && flare->commit_path && flare->commit_time) ?
			g_strdup_printf("%c://%s@%s", flare->type, flare->commit_path, flare->commit_time) : NULL;
		flare->st.st_uid = magma_sql_fetch_integer(result, 4);
		flare->st.st_gid = magma_sql_fetch_integer(result, 5);

		dbi_result_free(result);

		/*
		 * upcast this flare adding informations specific to object type
		 */
		if (!magma_flare_upcast(flare)) {
			dbg(LOG_ERR, DEBUG_ERR, "Error while upcasting flare %s", flare->path);
		}
	}

	magma_add_to_cache(flare);
}

/**
 * update flare atime, ctime and/or mtime. mode is
 * a ORed combination of MAGMA_TOUCH_ATIME, MAGMA_TOUCH_CTIME and
 * MAGMA_TOUCH_MTIME.
 *
 * @param flare the flare to be updated
 * @param mode a bitmask of fields to be modified
 * @return 0 on success, -1 on failure (errno is set accordingly)
 */
int magma_touch_flare(magma_flare_t *flare, magma_touch_mode_t mode, time_t atime, time_t mtime)
{
	struct utimbuf times;

	if (!flare) {
		errno = EFAULT;
		return (-1);
	}
	
	time_t t;
	if (time(&t) isNot (time_t) -1) {

		if (!atime) atime = t;
		if (!mtime) mtime = t;

		if (mode & MAGMA_TOUCH_ATIME) {
			flare->st.st_atime = atime;
			times.actime = atime;
		}
		if (mode & MAGMA_TOUCH_CTIME) {
			flare->st.st_ctime = t;
		}
		if (mode & MAGMA_TOUCH_MTIME) {
			flare->st.st_mtime = mtime;
			times.modtime = mtime;
		}

		utime(flare->contents, &times);

		errno = 0;
		return (0);
	}

	errno = EFAULT;
	return (-1);
}

/**
 * Return a flare inode. that function should be expanded to
 * consider vulcano on which flare is located. inodes should
 * be 64bit integers and should be ORed with vulcano number
 * in MSB to allow a consistent numbering scheme for all nodes.
 * that should result in 16bit for vulcano network (which is
 * 65535 nodes! quite a big SAN) and 48 bits for local inodes.
 *
 * @param flare the flare
 * @return unsigned value rappresenting flare inode
 */
unsigned int magma_get_flare_inode(magma_flare_t *flare)
{
	/*
	 * TODO include hosting node in the inode, to avoid
	 * inode duplications across the lava ring
	 */
	return ((unsigned int) flare->st.st_ino);
}

GMutex magma_add_or_remove_flare_on_parent_mutex;

/**
 * Check if an entry is already part of a directory
 *
 * @param flare the directory flare
 * @param entry the entry to check
 * @returns TRUE if found, FALSE otherwise
 */
gboolean magma_dir_contains_entry(magma_flare_t *flare, const gchar *entry)
{
	if (!entry || !flare) return (FALSE);
	magma_flare_read_lock(flare);

	magma_flare_update_stat(flare);

	GError *error = NULL;
	GMappedFile *map = g_mapped_file_new(flare->contents, FALSE, &error);
	if (error) {
		magma_flare_read_unlock(flare);
		dbg(LOG_ERR, DEBUG_DIR, "Can't load dir %s contents: %s", flare->path, error->message);
		g_error_free(error);
		return (FALSE);
	}

	gchar *content = g_mapped_file_get_contents(map);
	gchar *ptr = content;
	gboolean found = FALSE;
	while (ptr < content + flare->st.st_size) {
		if (strcmp(ptr, entry) is 0) {
			found = TRUE;
			break;
		}
		ptr += strlen(ptr) + 1;
	}

	g_mapped_file_unref(map);
	magma_flare_read_unlock(flare);

	return (found);
}

/**
 * Points the filename inside its path. The filename
 * is still part of the path, so must NOT be freed.
 */
gchar *magma_point_filename_in_path(gchar *path)
{
	/*
	 * get last part of flare path, excluding /.
	 * if path is "/test/element", match_dirent
	 * will point to "element" (rindex() + 1)
	 */
	gchar *match_dirent = rindex(path, '/');
	if (match_dirent) {
		match_dirent++;
	} else {
		match_dirent = path;
	}

	return (match_dirent);
}

/**
 * Adds a flare to its parent directory.
 *
 * @param flare the flare to be added
 * @return 0 on success, -1 on failure (errno is set accordingly)
 */
int magma_add_flare_to_parent(magma_flare_t *flare)
{
	int res = 0, server_errno = 0;

	/*
	 * Check input
	 */
	if (!flare || !flare->parent_path || !flare->path) {
		dbg(LOG_ERR, DEBUG_FLARE, "magma_add_flare_to_parent() called with wrong params");
		return (0);
	}

	/*
	 * get owner and redundant owner of the path
	 */
	magma_volcano *parent_owner = magma_route_path(flare->parent_path);
	magma_volcano *red_parent_owner = parent_owner->next ? parent_owner->next : lava->first_node;

	/*
	 * guess if this node is owner or redundant owner
	 */
	gboolean im_owner = magma_compare_nodes(parent_owner, &myself);
	gboolean im_red_owner = im_owner ? FALSE : magma_compare_nodes(red_parent_owner, &myself);

	/*
	 * do the local operation, if applies
	 */
	if (im_owner || im_red_owner) {
		const gchar *match_dirent = magma_point_filename_in_path(flare->path);

		/*
		 * get the parent flare
		 */
		magma_flare_t *parent = magma_search_or_create(flare->parent_path);

		/*
		 * Do some integrity checks: the parent is a directory?
		 * The entry_list field is filled with disk contents?
		 */
		if (parent->type isNot MAGMA_FLARE_TYPE_DIR) magma_load_flare(parent);
		if (parent->type isNot MAGMA_FLARE_TYPE_DIR) res = -1;

		/*
		 * then do the real add
		 */
		else if (!magma_dir_contains_entry(parent, match_dirent)) {
			magma_flare_write_lock(flare);
			int fd = open(parent->contents, O_WRONLY|O_APPEND);
			if (fd) {
				magma_full_write(fd, match_dirent, strlen(match_dirent) + 1);
				close (fd);
				magma_flare_update_stat(flare);
			} else {
				res = -1;
			}
			magma_flare_write_unlock(flare);
		}

		magma_dispose_flare(parent);

	} else {

		/* remote parent */
		magma_node_response response;
		GSocketAddress *peer = NULL;
		GSocket *socket = magma_open_client_connection(parent_owner->ip_addr, MAGMA_NODE_PORT, &peer);
		magma_pktqs_add_flare_to_parent(socket, peer, flare->path, &response);
		magma_close_client_connection(socket, peer);

		res = response.header.res;
		server_errno = response.header.err_no;

		dbg(LOG_INFO, DEBUG_DIR, "Forwarding add_flare_to_parent(%s) returned %d: %s",
			flare->path, res, strerror(server_errno));

	}

	if (res is -1) {
		dbg(LOG_ERR, DEBUG_ERR, "Error adding %s to parent", flare->path);
	} else {
		dbg(LOG_INFO, DEBUG_DIR, "Flare %s added to parent", flare->path);
	}

	return (res);
}

/**
 * Removes a flare from parent directory.
 *
 * @param flare the flare to be saved
 * @return 0 on success, -1 on failure (errno is set accordingly)
 */
int magma_remove_flare_from_parent(magma_flare_t *flare)
{
	int res = -1;

	gchar zeros[256];
	memset(zeros, '\0', 256);

	dbg(LOG_INFO, DEBUG_FLARE, "Removing %s from parent %s", flare->path, flare->parent_path);

	/*
	 * get owner and redundant owner of the path
	 */
	magma_volcano *parent_owner = magma_route_path(flare->parent_path);
	magma_volcano *red_parent_owner = parent_owner->next ? parent_owner->next : lava->first_node;

	/*
	 * guess if this node is owner or redundant owner
	 */
	gboolean im_owner = magma_compare_nodes(parent_owner, &myself);
	gboolean im_red_owner = im_owner ? FALSE : magma_compare_nodes(red_parent_owner, &myself);

	/*
	 * do the local operation, if applies
	 */
	if (im_owner || im_red_owner) {
		const gchar *entry = magma_point_filename_in_path(flare->path);

		magma_flare_t *parent = magma_search_or_create(flare->parent_path);
		if (parent) {
			magma_flare_write_lock(parent);

			GError *error = NULL;
			GMappedFile *map = g_mapped_file_new(flare->contents, TRUE, &error);
			if (error) {
				dbg(LOG_ERR, DEBUG_DIR, "Error mmapping %s: %s", flare->path, error->message);
			} else {
				/*
				 * remove the entry
				 */
				gchar *content = g_mapped_file_get_contents(map);
				gchar *ptr = content;
				gboolean removed = FALSE;

				while (ptr < content + flare->st.st_size) {
					if (strcmp(ptr, entry) is 0) {
						memset(ptr, 0, strlen(entry));
						removed = TRUE;
						break;
					}
					ptr += strlen(ptr) + 1;
				}

				/*
				 * write new contents
				 */
				error = NULL;
				if (removed) g_file_set_contents(flare->contents, content, flare->st.st_size, &error);
				if (error) {
					dbg(LOG_ERR, DEBUG_DIR, "Error saving dir %s: %s", flare->path, error->message);
					g_error_free(error);
				}

				g_mapped_file_unref(map);
			}
			magma_flare_write_lock(parent);
		}
		magma_dispose_flare(parent);

	} else {

		/* remote parent */
		magma_node_response response;
		GSocketAddress *peer;
		GSocket *socket = magma_open_client_connection(parent_owner->ip_addr, MAGMA_NODE_PORT, &peer);
		magma_pktqs_remove_flare_from_parent(socket, peer, flare->path, &response);
		magma_close_client_connection(socket, peer);

		res = response.header.res;

	}

	if (res is -1) {
		dbg(LOG_ERR, DEBUG_ERR, "Error removing %s from parent", flare->path);
	} else {
		dbg(LOG_INFO, DEBUG_DIR, "Flare %s removed from parent", flare->path);
	}

	return (res);
}

/**
 * check if a directory is empty (excluding "." and ".." entries)
 *
 * @param dir the magma_flare_t pointing to the directory
 * @return TRUE if empty, FALSE otherwise
 */
gboolean magma_dir_is_empty(magma_flare_t *dir)
{
	if (!dir) return (1);

	gsize size;
	GError *error = NULL;
	gchar *buffer;

	g_file_get_contents(dir->contents, &buffer, &size, &error);
	if (error) {
		dbg(LOG_ERR, DEBUG_DIR, "Can't check if dir %s is empty: %s", dir->path, error->message);
		g_error_free(error);
		return (FALSE);
	}

	/*
	 * look for chars different from '\0'
	 */
	gboolean res = TRUE;
	gchar *ptr = buffer + 5; // skip "." and ".."
	for (; ptr < buffer + size; ptr++) {
		if (*ptr isNot '\0') {
			res = FALSE;
			break;
		}
	}

	g_free(buffer);
	return (res);
}

// vim:ts=4:nocindent:autoindent:nowrap
