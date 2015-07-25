/*
   MAGMA -- vulcano.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Manage a host inside distributed hash table

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

#include "magma.h"

#define MAGMA_LOAD_SAVE_BUFFER_SIZE 1024

/**
 * structure describing this node
 */
magma_volcano myself;

/**
 * structure hosting the lava ring
 */
magma_lava *lava;

/**
 * Clone a magma_volcano struct.
 *
 * @param src the source magma_volcano struct
 * @return a magma_volcano duplicate
 */
magma_volcano *magma_volcano_clone(magma_volcano *src) {
	magma_volcano *clone = g_new0(magma_volcano, 1);
	if (!clone) return (NULL);

	memcpy(clone->ip_addr, src->ip_addr, MAX_IP_LENGTH);
	clone->port = src->port;
	clone->inaddr.s_addr = src->inaddr.s_addr;

	memcpy(clone->node_name, src->node_name, 1024);
	memcpy(clone->fqdn_name, src->fqdn_name, 1024);
	memcpy(clone->bootserver, src->bootserver, 1024);
	memcpy(clone->secretkey, src->secretkey, 1024);
	memcpy(clone->hashpath, src->hashpath, 1024);

	clone->bandwidth = src->bandwidth;
	clone->storage = src->storage;

	clone->load = src->load;

	memcpy(clone->start_key, src->start_key, SHA_READABLE_DIGEST_LENGTH);
	memcpy(clone->stop_key, src->stop_key, SHA_READABLE_DIGEST_LENGTH);

	clone->prev = src->prev;
	clone->next = src->next;

	return (clone);
}

/**
 * create a new lava ring structure
 *
 * @return new lava ring structure pointer
 */
magma_lava *magma_lava_new()
{
	lava = g_new0(magma_lava, 1);
	if (!lava) {
		dbg(LOG_ERR, DEBUG_ERR, "magma_create_network_ring: Can't allocate space for lava");
		dbg(LOG_ERR, DEBUG_ERR, "magma_create_network_ring: Aborting program execution now");
		exit(1);
	}

	/*
	 * I'm the only volcano of the lava network
	 */
	magma_volcano *myself_clone = magma_volcano_clone(&myself);
	lava->first_node = /* lava->middle_node = */ lava->last_node = myself_clone;

	lava->participants = 1;

	return lava;
}

/**
 * Add a node to a lavanet near a sibling node
 *
 * @param newnode the new node to add
 * @param sibling the sibling node the new node will be added
 */
int magma_volcano_add_near(magma_volcano *newnode, magma_volcano *sibling)
{
	if (!sibling) {
		/* top of the ring */
		lava->first_node->prev = newnode;
		newnode->next = lava->first_node;
		lava->first_node = newnode;
		return (1);
	}

	int cmp = strcmp(newnode->start_key, sibling->start_key);
	if (cmp is 0) {
		dbg(LOG_ERR, DEBUG_ERR, "Request to add existing node %s to the lava ring twice", newnode->fqdn_name);
		return (0);
	}

	if ( cmp < 0) {
		/* node is less than sibling, moving it up */
		return magma_volcano_add_near(newnode, sibling->prev);
	}

	/*
	 * inserting the new node after the sibling node
	 */
	newnode->prev = sibling;

	if (sibling->next) {
		newnode->next = sibling->next;
		sibling->next->prev = newnode;
	}

	sibling->next = newnode;

	if (!sibling->prev) {
		sibling->prev = newnode;
		newnode->next = sibling;
	}

	/*
	 * Adjusting key slices for sibling nodes:
	 * new node previous sibling stop key is this node start key less one
	 */
	memcpy(sibling->stop_key, newnode->start_key, SHA_READABLE_DIGEST_LENGTH);
	magma_decrement_key(sibling->stop_key);

	return (1);
}

/**
 * Add a volcano to a lava ring
 *
 * @param volcano the node to add
 * @param lava the lava ring
 */
void magma_lava_add_volcano(magma_lava *lava, magma_volcano *volcano) {
	if (magma_volcano_add_near(volcano, lava->first_node))
		lava->participants++;
}

/**
 * Clone the lava network to perform parallel network transformations
 *
 * @return a copy of the lava network. Must be destroyed with magma_lava_destroy()
 */
magma_lava *magma_lava_clone()
{
	assert(lava);

	magma_lava *lava_clone = g_new0(magma_lava, 1);
	if (!lava_clone) {
		dbg(LOG_ERR, DEBUG_ERR, "magma_lava_clone: Can't allocate space for lava");
		return (NULL);
	}

	magma_volcano *node_ptr = lava->first_node;
	for (;;) {
		/*
		 * clone each node and add it to the lava ring network
		 */
		magma_volcano *node_clone = magma_volcano_clone(node_ptr);
		if (!node_clone) {
			dbg(LOG_ERR, DEBUG_ERR, "magma_lava_clone: Can't allocate space for lava");
			magma_lava_destroy(lava_clone);
			return (NULL);
		}

		if (lava_clone->participants) {
			magma_volcano_add_near(node_clone, lava_clone->last_node);
		} else {
			lava_clone->first_node = lava_clone->last_node = node_clone;
		}
		lava_clone->participants++;

		/*
		 * proceed to next node, break cycle if its the first node
		 */
		node_ptr = node_ptr->next;
		if (!node_ptr || (node_ptr == lava->first_node)) break;
	}

	g_assert(lava_clone->participants == lava->participants);

	return (lava_clone);
}

/**
 * Destroy a magma_lava structure
 *
 * @param lava the magma_lava structure
 */
void magma_lava_destroy(magma_lava *lava) {
	if (!lava) {
		dbg(LOG_ERR, DEBUG_ERR, "Null lava network passed to magma_lava_destroy()");
		return;
	}

	/*
	 * Clear all the nodes first
	 */
	magma_volcano *node_ptr = lava->first_node;
	magma_volcano *first_ptr = lava->first_node;
	while (1) {
		magma_volcano *next = node_ptr->next;
		g_free(node_ptr); // was magma_vulcano_destroy(node_ptr);
		if (next == first_ptr) break;
		if (!next) break;
		node_ptr = next;
	}

	g_free(lava);
}

/**
 * Save a node profile into a magma_volcano structure
 *
 * @param node the destination magma_volcano structure
 * @param node_name the node nick name
 * @param fqdn_name the node fully qualified domain name
 * @param ipaddr the IP address as a string
 * @param ipport the IP port as a number
 * @param bandwidth the node available bandwidth in KB/s
 * @param storage the node available storage in MBytes
 * @return the magma_volcano structure filled with provided configuration
 */
magma_volcano *magma_volcano_fill(magma_volcano *node, char *node_name,
	char *fqdn_name, char *ipaddr, int ipport, int bandwidth, int storage,
	char *start_key, char *stop_key, const gchar *dht_path)
{
#if MAGMA_ENABLE_NODE_LOCKING
	/* init node mutex */
	g_rec_mutex_init(&(node->mutex));
#endif

	/* copy node IP address (as string) and perform binary translation (inet_aton) */
	if (ipaddr) {
		if ( ! inet_aton(ipaddr, &node->inaddr) ) {
			dbg(LOG_ERR, DEBUG_ERR, "magma_create_node: IP address not valid!");
#if MAGMA_ENABLE_NODE_LOCKING
			g_rec_mutex_clear(&(node->mutex));
#endif
			g_free(node);
			return NULL;
		}
		g_strlcpy(node->ip_addr, ipaddr, MAX_IP_LENGTH);
	}

	node->port = ipport;
	node->bandwidth = bandwidth;
	node->storage = storage;
	node->load = 0;

	/* copy node nick name */
	if (node_name) g_strlcpy(node->node_name, node_name, MAX_HOSTNAME_LENGTH);

	/* copy fully qualified name */
	if (fqdn_name) g_strlcpy(node->fqdn_name, fqdn_name, MAX_HOSTNAME_LENGTH);

	/* set node default keyspace */
	g_strlcpy(
		node->start_key,
		start_key ? start_key : "0000000000000000000000000000000000000000",
		SHA_READABLE_DIGEST_LENGTH);

	g_strlcpy(
		node->stop_key,
		stop_key ? stop_key : "ffffffffffffffffffffffffffffffffffffffff",
		SHA_READABLE_DIGEST_LENGTH);

	node->prev = NULL;
	node->next = NULL;

	if (dht_path) g_strlcpy(node->hashpath, dht_path, MAX_PATH_LENGTH);

	return (node);
}

/**
 * allocate a new node structure
 *
 * @param node_name short name of this node
 * @param fqdn_name fully qualified domain name of this node
 * @param ipaddr string rappresenting node IP address
 * @param ipport node IP port
 * @param bandwidth declared bandwidth available
 * @param storage declare storage available
 * @return new node structure pointer
 */
magma_volcano *magma_volcano_new(
	char *node_name,
	char *fqdn_name,
	char *ipaddr,
	int ipport,
	int bandwidth,
	int storage,
	char *start_key,
	char *stop_key,
	const gchar *dht_path)
{
	magma_volcano *node = g_new0(magma_volcano, 1);
	if (node == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Error allocating node");
		return NULL;
	}

	return magma_volcano_fill(
		node, node_name, fqdn_name, ipaddr, ipport, bandwidth, storage,
		start_key, stop_key, dht_path);
}

/**
 * Configure "this" node of the lavanet and start
 * the incoming connection handler
 *
 * @param node_name the nick name of the node
 * @param fqdn_name the fully qualified domain name of the node
 * @ipaddr the IP address as a string
 * @ipport the IP port as a number
 * @bandwidth the bandwidth available on this node in KB/s
 * @storage the storage available on this node in MBytes
 */
magma_volcano *magma_config_myself(
	char *node_name,
	char *fqdn_name,
	char *ipaddr,
	int ipport,
	int bandwidth,
	int storage,
	const gchar *dht_path)
{
	/*
	 * save config profile into magma_volcano myself16
	 */
	magma_volcano_fill(
		&myself, node_name, fqdn_name, ipaddr, ipport, bandwidth, storage,
		"0000000000000000000000000000000000000000",
		"ffffffffffffffffffffffffffffffffffffffff",
		dht_path);

	return &myself;
}

/**
 * Destroy a magma_volcano structure
 *
 * @param node the magma_volcano structure to destroy
 */
int magma_vulcano_destroy(magma_volcano *node)
{
	if (node == NULL) return 1;

	/* re-link the net */
	// magma_lock_node(node);
	if (node->prev) node->prev->next = node->next;
	if (node->next) node->next->prev = node->prev;
	// magma_unlock_node(node);

	if (lava->first_node == node) {
		lava->first_node = node->next;
	} else if (lava->last_node == node) {
		lava->last_node = node->prev;
	}

	/* free resources */
#if MAGMA_ENABLE_NODE_LOCKING
	g_rec_mutex_clear(&(node->mutex));
#endif

	g_free(node);

	/* return */
	return (1);
}

int magma_check_keyspace_continuity()
{
	magma_volcano *n = lava->first_node;
	char key[SHA_READABLE_DIGEST_LENGTH] = "0000000000000000000000000000000000000000";

	for (;;) {
		int cmp = strcmp(n->start_key, key);
		if (cmp < 0) {
			dbg(LOG_ERR, DEBUG_ERR, "Hole in lava ring after vulcano %s", n->node_name);
			dbg(LOG_ERR, DEBUG_ERR, "  start_key is %s", n->start_key);
			dbg(LOG_ERR, DEBUG_ERR, "     should be %s", key);
			return 0;
		} else if (cmp > 0) {
			dbg(LOG_ERR, DEBUG_ERR, "Overlapping in lava ring after vulcano %s", n->node_name);
			dbg(LOG_ERR, DEBUG_ERR, "  start_key is %s", n->start_key);
			dbg(LOG_ERR, DEBUG_ERR, "     should be %s", key);
			return 0;
		}
	}
	return 1;
}

int magma_remove_node_from_ring(magma_volcano *node)
{
	if (node->prev != NULL) node->prev->next = node->next;
	if (node->next != NULL) node->next->prev = node->prev;

	magma_vulcano_destroy(node);
	return 1;
}

gchar *magma_cat_profile()
{
	GString *profile = g_string_sized_new(4192);

	g_string_append_printf(profile,"hashpath = %s\n", magma_environment.hashpath);
	g_string_append_printf(profile,"servername = %s\n", magma_environment.servername);
	g_string_append_printf(profile,"nickname = %s\n", magma_environment.nickname);
	g_string_append_printf(profile,"ipaddr = %s\n", magma_environment.ipaddr);
	g_string_append_printf(profile,"port = %d\n", magma_environment.port);
	g_string_append_printf(profile,"bandwidth = %u\n", magma_environment.bandwidth);
	g_string_append_printf(profile,"bootserver = %s\n", magma_environment.bootserver);
	g_string_append_printf(profile,"secretkey = %s\n", magma_environment.secretkey);
	g_string_append_printf(profile,"load = %F\n", myself.load);

	if ((magma_environment.bootserver != NULL) && (strcmp(magma_environment.bootserver, "127.0.0.1") != 0)) {
		g_string_append_printf(profile,"bootserver = %s\n", magma_environment.bootserver);
	}

	g_string_append_printf(profile,"start_key = %s\n", myself.start_key);
	g_string_append_printf(profile,"stop_key = %s\n", myself.stop_key);

// return the profile->str field which is not freed because the 2nd arg. is FALSE
	return g_string_free(profile, FALSE);
}

/**
 * Save this node profile under "/.dht" in the filesystem
 */
int magma_save_node_profile()
{
	assert(myself.node_name);

	dbg(LOG_INFO, DEBUG_FLARE, "Saving node profile");
	gchar *mypath = g_strconcat(magma_dht_path, "/", myself.node_name, NULL);
	if (NULL == mypath) {
		dbg(LOG_ERR, DEBUG_ERR, "Error allocating path in save_node_profile()");
		return 0;
	}

	/*
	 * write the node profile in a buffer to be saved on disk
	 */
	gchar *buf = magma_cat_profile();

	/*
	 * creating the profile file
	 */
	struct stat st;
	if (magma_getattr(0, 0, mypath, &st) == -1) {
		if (magma_mknod(0, 0, MAGMA_DEFAULT_TTL, mypath, S_IFREG|S_IRUSR|S_IWUSR|S_IRGRP, 0) == -1) {
			if (errno != EEXIST) {
				dbg(LOG_ERR, DEBUG_ERR, "magma_mknod(0,0,\"%s\",%o,0) == -1", mypath, S_IRUSR|S_IWUSR|S_IRGRP);
				g_free(buf);
				g_free(mypath);
				return 0;
			}
		}
	}

	/* zeroing the file */
	magma_truncate(0, 0, MAGMA_DEFAULT_TTL, mypath, 0);

	/* writing the buffer */
	if (magma_write(0, 0, MAGMA_DEFAULT_TTL, mypath, strlen(buf), 0, buf) == -1) {
		int x = errno;
		dbg(LOG_ERR, DEBUG_ERR, "writing profile to %s: %s", mypath, strerror(x));
		g_free(mypath);
		g_free(buf);
		return 0;
	}

	g_free(mypath);
	g_free(buf);
	return 1;
}

void magma_parse_node_line(char *buf, magma_volcano *node) {
	char *value = index(buf,' ');
	if (value != NULL) {
		*value = '\0';
		value += 3;
		if (strcmp(buf,"servername") == 0) {
			g_stpcpy(node->fqdn_name, value);
		} else if (strcmp(buf,"nickname") == 0) {
			g_stpcpy(node->node_name, value);
		} else if (strcmp(buf,"ipaddr") == 0) {
			g_stpcpy(node->ip_addr, value);
		} else if (strcmp(buf,"port") == 0) {
			node->port = atol(value);
		} else if (strcmp(buf,"bandwidth") == 0) {
			node->bandwidth = atol(value);
		} else if (strcmp(buf,"load") == 0) {
			sscanf(value, "%f", &(node->load));
		}
	}
}

#if 0
int magma_load_node_from_dht(magma_volcano *node)
{
	if (node == NULL) return 0;
	if (node->node_name == NULL) return 0;

	char *path = NULL;
	path = g_strconcat(magma_dht_path, "/", node->node_name, NULL);

	magma_flare_t *flare = magma_search_or_create(path);
	if (flare == NULL) return 0;
	if (!flare->is_upcasted) return 0;
	g_free(path);

	char buf[MAGMA_LOAD_SAVE_BUFFER_SIZE];
	FILE *f = fopen(flare->contents, "r");
	if (f == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't load state from %s: %s", flare->contents, strerror(errno));
		return 0;
	}

	do {
		if (0 == fgets(buf,MAGMA_LOAD_SAVE_BUFFER_SIZE,f)) {
			continue;
		}
		char *newline = rindex(buf,'\n');
		if (newline != NULL) {
			*newline = '\0';
		}
		magma_parse_node_line(buf,node);
	} while (strlen(buf) != 0);

	fclose(f);
	return (1);
}
#endif

/**
 * Checks is a node with at least one identical
 * characteristic exists
 *
 * @param node the node to check against the DHT topology
 * @returns MAGMA_OK if no match is found,
 *   MAGMA_NODE_NAME_COLLISION if the name exists,
 *   MAGMA_FQDN_NAME_COLLISION if the fully qualified name exists,
 *   MAGMA_IP_PORT_COLLISION if the same (ip addr, port) pair exists
 */
uint8_t magma_node_exists(magma_volcano *node, magma_lava *lava)
{
	if (!lava->first_node) {
		dbg(LOG_ERR, DEBUG_ERR, "Lava first node is NULL!");
		return (MAGMA_ERROR);
	}

	magma_volcano *p = lava->first_node;

	do {
		if (strcmp(p->node_name, node->node_name) == 0)
			return (MAGMA_NODE_NAME_COLLISION);

		if (strcmp(p->fqdn_name, node->fqdn_name) == 0)
			return (MAGMA_FQDN_NAME_COLLISION);

		if ((strcmp(p->ip_addr, node->ip_addr) == 0) && (p->port == node->port))
			return (MAGMA_IP_PORT_COLLISION);

		p = p->next;

	} while (p && p != lava->first_node);

	return (MAGMA_OK);
}

void magma_write_profile(int fd)
{
	char buf[MAGMA_LOAD_SAVE_BUFFER_SIZE];

	sprintf(buf,"hashpath = %s\n", magma_environment.hashpath);
	magma_full_write(fd, buf, strlen(buf));

	sprintf(buf,"servername = %s\n", magma_environment.servername);
	magma_full_write(fd, buf, strlen(buf));

	sprintf(buf,"nickname = %s\n", magma_environment.nickname);
	magma_full_write(fd, buf, strlen(buf));

	sprintf(buf,"ipaddr = %s\n", magma_environment.ipaddr);
	magma_full_write(fd, buf, strlen(buf));

	sprintf(buf,"port = %d\n", magma_environment.port);
	magma_full_write(fd, buf, strlen(buf));

	sprintf(buf,"bandwidth = %u\n", magma_environment.bandwidth);
	magma_full_write(fd, buf, strlen(buf));

	sprintf(buf,"bootserver = %s\n", magma_environment.bootserver);
	magma_full_write(fd, buf, strlen(buf));

	sprintf(buf,"secretkey = %s\n", magma_environment.secretkey);
	magma_full_write(fd, buf, strlen(buf));

	sprintf(buf,"load = %F\n", myself.load);
	magma_full_write(fd, buf, strlen(buf));

	if ((magma_environment.bootserver != NULL) && (strcmp(magma_environment.bootserver, "127.0.0.1") != 0)) {
		sprintf(buf,"bootserver = %s\n", magma_environment.bootserver);
		magma_full_write(fd, buf, strlen(buf));
	}

	sprintf(buf,"start_key = %s\n", myself.start_key);
	magma_full_write(fd, buf, strlen(buf));

	sprintf(buf,"stop_key = %s\n", myself.stop_key);
	magma_full_write(fd, buf, strlen(buf));
}

#ifdef MAGMA_DEAD_CODE
void magma_save_status()
{
	return;

	/* dbg(LOG_ERR, DEBUG_ERR, "run_path = %s", run_path); */

	struct stat st;
	if (lstat(magma_run_path, &st) == -1 && errno == ENOENT) {
		if (-1 == mkdir(magma_run_path, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP)) {
			dbg(LOG_ERR, DEBUG_ERR, "Can't mkdir %s: %s", magma_run_path, strerror(errno));
			return;
		}
#ifdef MAGMA_DEBUGGING_BUILD
		if (-1 == chown(magma_run_path, magma_flare_system_uid, magma_flare_system_gid)) {
			dbg(LOG_ERR, DEBUG_ERR, "Can't chown %s: %s", magma_run_path, strerror(errno));
			return;
		}
#endif
	}

	char *local_path = g_strconcat(magma_run_path, "/", myself.node_name, NULL);
	if (lstat(local_path, &st) == -1 && errno == ENOENT) {
		if (-1 == mkdir(local_path, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP)) {
			dbg(LOG_ERR, DEBUG_ERR, "Can't mkdir %s: %s", local_path, strerror(errno));
			g_free(local_path);
			return;
		}
#ifdef MAGMA_DEBUGGING_BUILD
		if (-1 == chown(local_path, magma_flare_system_uid, magma_flare_system_gid)) {
			dbg(LOG_ERR, DEBUG_ERR, "Can't chown %s: %s", local_path, strerror(errno));
			g_free(local_path);
			return;
		}
#endif
	}

	char *status_path = g_strconcat(local_path, "/status", NULL);
	int fd = open(status_path, O_WRONLY|O_CREAT, S_IRWXU);
	if (fd == -1) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't save state in %s: %s", status_path, strerror(errno));
		g_free(local_path);
		g_free(status_path);
		return;
	}

	if (-1 == fchmod(fd, S_IRUSR|S_IWUSR)) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't fchmod %s: %s", status_path, strerror(errno));
		g_free(local_path);
		g_free(status_path);
		return;
	}
#ifdef MAGMA_DEBUGGING_BUILD
	if (-1 == fchown(fd, magma_flare_system_uid, magma_flare_system_gid)) {
		// dbg(LOG_ERR, DEBUG_ERR, "Can't fchown %s: %s", status_path, strerror(errno));
		g_free(local_path);
		g_free(status_path);
		return;
	}
#endif

	magma_write_profile(fd);
	close(fd);

	/* save magmad pid */
	char buf[8];
	char *pid_path = g_strconcat(local_path, "/pid", NULL);
	fd = open(pid_path, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
	if (fd != -1) {
		if (-1 == fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) {
			dbg(LOG_ERR, DEBUG_ERR, "Can't fchmod %s: %s", pid_path, strerror(errno));
			g_free(pid_path);
			return;
		}
#ifdef MAGMA_DEBUGGING_BUILD
		if (-1 == fchown(fd, magma_flare_system_uid, magma_flare_system_gid)) {
			dbg(LOG_ERR, DEBUG_ERR, "Can't fchown %s: %s", pid_path, strerror(errno));
			g_free(pid_path);
			return;
		}
#endif
		sprintf(buf,"%u",getpid());
		magma_full_write(fd,buf,strlen(buf));
		close(fd);
	}

	g_free(status_path);
	g_free(pid_path);
	g_free(local_path);
}
#endif

/**
 * Coordinate a network boot when loading from disk.
 * This node acts as network coordinator to check that
 * the topology has been restored as it was before
 * saving to disk and shutting down.
 */
void magma_coordinate_network_boot()
{
	/*
	 * start by pinging all the nodes
	 */
	while (1) {
		magma_environment.state = magma_network_ready;

		magma_volcano *v = lava->first_node;
		while (v) {
			/*
			 * avoid myself
			 */
			if (!magma_compare_nodes(&myself, v)) {

				/*
				 * send heartbeat request on this node
				 */
				GSocketAddress *peer;
				GSocket *socket = magma_open_client_connection(v->ip_addr, MAGMA_NODE_PORT, &peer);
				magma_node_response response;
				magma_pktqs_heartbeat(socket, peer, &response);

				/*
				 * on error or no response (G_IO_STATUS_AGAIN) we assume that
				 * at least one node in not ready and we have to start the
				 * process over
				 */
				if ((0 != response.header.res) || (G_IO_STATUS_NORMAL != response.header.status)) {

					magma_environment.state = magma_network_loading;

				} else {
					/*
					 * check that start_key and stop_key claimed by the remote host
					 * are the same we expect from disk, otherwise abort network build.
					 */
					if (strcmp(response.body.heartbeat.volcano.start_key, v->start_key) isNot 0) {
						dbg(LOG_ERR, DEBUG_BOOT,
							"Node %s reports start key %s while we have %s. Aborting network build.",
							v->node_name, response.body.heartbeat.volcano.start_key, v->start_key);

						magma_environment.state = magma_key_mismatch;
						break;
					}

					if (strcmp(response.body.heartbeat.volcano.stop_key, v->stop_key) isNot 0) {
						dbg(LOG_ERR, DEBUG_BOOT,
							"Node %s reports stop key %s while we have %s. Aborting network build.",
							v->node_name, response.body.heartbeat.volcano.stop_key, v->stop_key);

						magma_environment.state = magma_key_mismatch;
						break;
					}
				}
			}

			if (v->next == lava->first_node) break;
			v = v->next;
		}

		if (magma_environment.state isNot magma_network_loading) break;
	}

	/*
	 * send network ready signal
	 */
	while (1) {
		gboolean network_signaled = TRUE;

		magma_volcano *v = lava->first_node;
		while (v) {
			if (!magma_compare_nodes(&myself, v)) {
				GSocketAddress *peer;
				GSocket *socket = magma_open_client_connection(v->ip_addr, MAGMA_NODE_PORT, &peer);
				magma_node_response response;
				magma_pktqs_network_built(socket, peer, magma_environment.state, &response);

				if ((response.header.res isNot 0) || (response.header.status isNot G_IO_STATUS_NORMAL)) {
					network_signaled = FALSE;
				}
			}

			if (v->next == lava->first_node) break;
			v = v->next;
		}

		if (network_signaled) break;
	}
}

/**
 * Returns true if this node is the coordinator of the
 * boot process when loading lava topology from disk,
 * false otherwise
 */
gboolean magma_this_node_is_coordinator()
{
	magma_volcano *owner = magma_route_path(MAGMA_DHT_PATH);
	if (magma_compare_nodes(owner, &myself)) return (TRUE);
	return (FALSE);
}

/*
 * Load the lava topology from network
 */
void magma_load_status()
{
	/*
	 * allocate a new lava topology
	 */
	magma_lava *new_lava = g_new0(magma_lava, 1);
	if (!new_lava) {
		dbg(LOG_ERR, DEBUG_BOOT, "Error allocating lava network, exiting now.");
		exit (1);
	}

	/*
	 * send the SQL query to load lava nodes
	 */
	gchar *query = g_strdup_printf(
		"select nickname, fqdn, ipaddr, ipport, start_key, stop_key, load, storage, bandwidth "
		"from lava_%s order by start_key", magma_environment.nickname);

	dbi_result result = magma_sql_query(query);
	g_free(query);

	/*
	 * if the query succeeded...
	 */
	if (result) {
		/*
		 * loop through values to load nodes into a lava topology
		 */
        while (dbi_result_next_row(result)) {
        	gchar *nickname		= magma_sql_fetch_string (result, 1);
        	gchar *fqdn			= magma_sql_fetch_string (result, 2);
        	gchar *ipaddr		= magma_sql_fetch_string (result, 3);
        	guint32 ipport		= magma_sql_fetch_integer(result, 4);
        	gchar *start_key	= magma_sql_fetch_string (result, 5);
        	gchar *stop_key		= magma_sql_fetch_string (result, 6);
        	// double load			= magma_sql_fetch_double (result, 7);
        	guint32 storage		= magma_sql_fetch_integer(result, 8);
        	guint32 bandwidth	= magma_sql_fetch_integer(result, 9);

        	magma_volcano *v = magma_volcano_new(
        		nickname, fqdn,	ipaddr, ipport, bandwidth,
        		storage, start_key, stop_key, "");

        	if (!v) {
        		dbg(LOG_ERR, DEBUG_BOOT, "Error allocating volcano %s", nickname);
        		exit(1);
        	}

        	if (!new_lava->first_node) { new_lava->first_node = v; }
        	if ( new_lava->last_node ) { new_lava->last_node->next = v; v->prev = new_lava->last_node; }
        	new_lava->last_node = v;

        	new_lava->participants++;
        }

        if (new_lava->participants < 1) {
        	dbg(LOG_ERR, DEBUG_BOOT, "Error loading lava topology from disk: zero nodes found");
        	exit (1);
        }

        new_lava->last_node->next = new_lava->first_node;
        new_lava->first_node->prev = new_lava->last_node;

        /*
         * free DBI result
         */
		dbi_result_free(result);

		/*
		 * update this node from the lava topology
		 */
		magma_update_myself_from_lava(new_lava);

		/*
		 * install the new topology
		 */
		lava = new_lava;

		/*
		 * check if this node should act as coordinator
		 * or should be coordinated
		 */
		if (!magma_this_node_is_coordinator()) return;

		/*
		 * coordinate network boot
		 */
		magma_coordinate_network_boot();
	}
}

/**
 * Return the magma_volcano object corresponding to
 * this node in the lava ring
 */
magma_volcano *magma_get_myself_from_lava(magma_lava *lava)
{
	magma_volcano *v = lava->first_node;
	while (1) {
		if (magma_compare_nodes(v, &myself)) return (v);
		if (v->next == lava->first_node) break;
		v = v->next;
	}
	return (NULL);
}

/**
 * Return the highest used key in this node key slice
 */
gchar *magma_get_highest_used_key() {
	// TODO: LAVA: Save highest used key in magma_lava struct to speed up this function
	DIR *dir = opendir(myself.hashpath);
	if (!dir) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't open %s: %s", myself.hashpath, strerror(errno));
		return (NULL);
	}

	gchar highest_entry[SHA_READABLE_DIGEST_LENGTH];
	memset(highest_entry, 0, SHA_READABLE_DIGEST_LENGTH);

	struct dirent *filename;
	do {
		filename = readdir(dir);
		if (!filename) break;

		/* consider only files with proper length */
		if (strlen(filename->d_name) != SHA_READABLE_DIGEST_LENGTH - 1) continue;

		/* on first iteration or if filename is higher that the last saved, save it */
		if ((0 == highest_entry[0]) || (g_strcmp0(filename->d_name, highest_entry) > 0)) {
			memcpy(highest_entry, filename->d_name, SHA_READABLE_DIGEST_LENGTH);
		}
	} while (filename);

	closedir(dir);

	return (g_strdup(highest_entry));
}

// vim:ts=4:nocindent:autoindent
