/*
   MAGMA -- balance.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Provide internal logic to calculate node load and activate key
   redistribution while MAGMA performs regular operations.

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

// #define INCLUDE_FLARE_INTERNALS
#include "../magma.h"

void magma_ping_nodes()
{
//	gboolean all_nodes_responding = TRUE;

	magma_volcano *v = lava->first_node;
	do {
		if (magma_compare_nodes(v, &myself)) {
			v->alive = TRUE;
		} else {
			GSocketAddress *peer;
			GSocket *socket = magma_open_client_connection(v->ip_addr, MAGMA_NODE_PORT, &peer);

			magma_node_response response;
			magma_pktqs_heartbeat(socket, peer, &response);

			if (G_IO_STATUS_NORMAL == response.header.status) {
				v->alive 		= TRUE;
	//			v->load			= response.body.heartbeat.volcano.load;
				v->storage		= response.body.heartbeat.volcano.storage;
				v->free_storage	= response.body.heartbeat.volcano.free_storage;
				v->bandwidth	= response.body.heartbeat.volcano.bandwidth;
				v->total_keys	= response.body.heartbeat.volcano.total_keys;

				magma_sql_save_volcano(v);
			} else {
				v->alive = FALSE;
	//			all_nodes_responding = FALSE;
			}
		}

		v = v->next;
	} while (v && (v != lava->first_node));

//	magma_environment.state = all_nodes_responding ? magma_network_ready : magma_network_loading;
}

void magma_balance_update_total_keys()
{
    gchar *query = g_strdup_printf(
    	"select cast(count(*) as integer) as total_keys from flare_%s where hash >= '%s' and hash <= '%s' limit 1",
    	myself.node_name, myself.start_key, myself.stop_key);

    dbi_result result = magma_sql_query(query);
	g_free(query);

	if (result && dbi_result_next_row(result)) {
		myself.total_keys = magma_sql_fetch_integer(result, 1);
		dbi_result_free(result);
	}

	magma_volcano *v = magma_get_myself_from_lava(lava);
	if (v) {
		v->total_keys = myself.total_keys;
	}

	dbg(LOG_INFO, DEBUG_BALANCER, "This node hosts %u keys", myself.total_keys);
}

/**
 * Update this node free space
 */
void magma_balance_update_free_space()
{
	struct statfs s;
	if (0 == statfs(myself.hashpath, &s)) {
		myself.storage      = s.f_bsize * s.f_blocks;
		myself.free_storage = s.f_bsize * s.f_bavail;
	} else {
		dbg(LOG_ERR, DEBUG_ERR, "Error getting filesystem available space: %s", strerror(errno));
	}

	magma_volcano *v = magma_get_myself_from_lava(lava);
	if (v) {
		v->storage = myself.storage;
		v->free_storage = myself.free_storage;
	}

	dbg(LOG_INFO, DEBUG_BALANCER, "This node has %u / %u bytes available", myself.free_storage, myself.storage);
}

/**
 * This is the balancer thread function which periodically
 * checks for lava network overall load and perform balancing
 * on sibling vulcanos.
 *
 * @param data magma_volcano structure rappresenting this node,
 *    declared (void *) according to GLib thread specification
 * @return NULL (result can be safely ignored)
 */
gpointer magma_balance(gpointer data)
{
	(void) data;

#if 0
	magma_volcano *thisnode = (magma_volcano *) data;
	magma_volcano *nodep = lava->first_node;
	long double load, max_load = 0.0, min_load = powl(2,64);
#endif

	/* endless loop */
	while (1) {
		/*
		 * Run the balancer only if the network has been formed
		 */
		if (magma_network_ready == magma_environment.state) {
			/*
			 * guess the number of hosted keys
			 */
			magma_balance_update_total_keys();

			/*
			 * guess free space
			 */
			magma_balance_update_free_space();

			/*
			 * if there's at least another node, make sure it's alive
			 */
			if (lava->participants > 1) magma_ping_nodes();

			/*
			 * compute network load to apply the balancer strategy
			 */
			if (lava->participants > 2) {
#if 0
				nodep = lava->first_node;
				for (; nodep != NULL;) {
					if (!nodep) break;

					load = magma_load_on_node(nodep);
					if (load > max_load) max_load = load;
					if (load < min_load) min_load = load;
	
					nodep = nodep->next;
					if (nodep == lava->first_node) break;
				}
				load = (max_load + min_load) / 2.0;

				if ( thisnode->load < load - 2 ) {
					dbg(LOG_INFO, DEBUG_BALANCER, "%s says: I'm underloaded", thisnode->node_name);
					magma_manage_underload();
				} else if ( thisnode->load > load + 2 ) {
					dbg(LOG_INFO, DEBUG_BALANCER, "%s says: I'm overloaded", thisnode->node_name);
					magma_manage_overload();
				}
#endif
			} else {
				dbg(LOG_INFO, DEBUG_BALANCER, "Skipping balancer for single or double node networks");
			}

			if (lava) magma_save_node_profile();
		}

		g_thread_yield();
		g_usleep(MAGMA_CHECK_LOAD_TIMEOUT);
	}

	return (NULL);
}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

#if 0
/**
 * Returns the actual load of node *node using logarithmic scale.
 *
 * @param node the node to be measured
 * @return node load as unsigned 64bit integer
 */
uint64_t magma_current_load(magma_volcano *node)
{
	double keyload = 0.0;
	magma_sha1_t *keystart, *keystop;
	magma_volcano *proto_max, *proto_min, *proto_current;

	if (node == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "actual_load received NULL node pointer!");
		return 0;
	}

	proto_max = magma_create_node_empty();
	proto_min = magma_create_node_empty();

	keystart = sha1_aton(node->start_key);
	keystop = sha1_aton(node->stop_key);

	keyload  = (log(1.0 * keystop->a - 1.0) - log(1.0 * keystart->a + 1.0)) / log(2.0) * 5.0;
	keyload += (log(1.0 * keystop->b - 1.0) - log(1.0 * keystart->b + 1.0)) / log(2.0) * 4.0;
	keyload += (log(1.0 * keystop->c - 1.0) - log(1.0 * keystart->c + 1.0)) / log(2.0) * 3.0;
	keyload += (log(1.0 * keystop->d - 1.0) - log(1.0 * keystart->d + 1.0)) / log(2.0) * 2.0;
	keyload += (log(1.0 * keystop->e - 1.0) - log(1.0 * keystart->e + 1.0)) / log(2.0) * 1.0;

	for (proto_current = lava->first_node; proto_current != NULL; proto_current = proto_current->next) {
		if (proto_min->bandwidth < proto_current->bandwidth) proto_current->bandwidth = proto_min->bandwidth;
		if (proto_max->bandwidth > proto_current->bandwidth) proto_current->bandwidth = proto_max->bandwidth;

		if (proto_min->storage < proto_current->storage) proto_current->storage = proto_min->storage;
		if (proto_max->storage > proto_current->storage) proto_current->storage = proto_max->storage;
	}

	magma_lock_node(node);

	proto_current = magma_create_node_empty();
	proto_current->storage = proto_max->storage - proto_min->storage || 1;
	proto_current->bandwidth = proto_max->bandwidth - proto_min->bandwidth || 1;

	/*
	 * Load is calculated on bandwidth and storage space available
	 */
	node->load  = keyload / 4.80;
	node->load *= node->storage * 100.0 / proto_current->storage;
	node->load *= node->bandwidth * 100.0 / proto_current->bandwidth;

	magma_unlock_node(node);

	dbg(LOG_INFO, DEBUG_BALANCER, " BALANCER for %s (storage*keyload/bandwidth)", node->node_name);
	dbg(LOG_INFO, DEBUG_BALANCER, "     Keyload: %f", keyload);
	dbg(LOG_INFO, DEBUG_BALANCER, "     Storage: %d", node->storage);
	dbg(LOG_INFO, DEBUG_BALANCER, "   Bandwidth: %d", node->bandwidth);
	dbg(LOG_INFO, DEBUG_BALANCER, "        Load: %Lf", node->load);

	g_free(proto_max);
	g_free(proto_min);
	g_free(proto_current);
	g_free(keystart);
	g_free(keystop);
	return node->load;
}
#endif

/**
 * Calculate current load of given node using logarithmic scale.
 *
 * @param node node to be calculated
 * @return load as unsigned 64bit integer
 */
uint64_t magma_load_on_node(magma_volcano *node)
{
	double keyload = 0.0;

	if (!node) {
		dbg(LOG_ERR, DEBUG_ERR, "load_on_node received NULL node pointer!");
		return 0;
	}

	magma_sha1_t *keystart = sha1_aton(node->start_key);
	magma_sha1_t *keystop  = sha1_aton(node->stop_key);

	keyload  = (log(1.0 * keystop->a - 1.0) - log(1.0 * keystart->a + 1.0)) / log(2.0) * 5.0;
	keyload += (log(1.0 * keystop->b - 1.0) - log(1.0 * keystart->b + 1.0)) / log(2.0) * 4.0;
	keyload += (log(1.0 * keystop->c - 1.0) - log(1.0 * keystart->c + 1.0)) / log(2.0) * 3.0;
	keyload += (log(1.0 * keystop->d - 1.0) - log(1.0 * keystart->d + 1.0)) / log(2.0) * 2.0;
	keyload += (log(1.0 * keystop->e - 1.0) - log(1.0 * keystart->e + 1.0)) / log(2.0) * 1.0;

	magma_lock_node(node);

	/*
	 * Load is calculated on bandwidth and storage space available
	 */
	node->load  = keyload / 4.80 * node->storage / 100 / node->bandwidth * 1024;

	magma_unlock_node(node);

	dbg(LOG_INFO, DEBUG_BALANCER, "%s: BALANCER on %s state %f", myself.node_name, node->node_name, node->load);

	g_free(keystart);
	g_free(keystop);

	return node->load;
}

/**
 * manages an underload situation. If a node has lighter load than sibling
 * nodes can explicitly request some keys to leverage the load profile.
 */
void magma_manage_underload()
{
#if 0
	char *path = NULL;
	char *key = g_strdup(myself.start_key);

	/* getting forward socket for PFUSE operations */
	GSocket *s = magma_get_flare_client_connection(myself.prev->ip_addr);

	int c;
	for (c = 0; c < MAGMA_BALANCER_KEY_DELTA; c++) {
		decrement_key(key);
		dbg(LOG_INFO, DEBUG_BALANCER, "Requesting key %s from %s", key, myself.prev->node_name);

		magma_lock_me();
		magma_pktqs_get_key(myself.prev->connection, key);
		magma_pktar_get_key(myself.prev->connection, path);
		magma_unlock_me();

		if (strlen(path) != 0) {
			magma_flare_t *flare = magma_new_flare(path);
			if (flare != NULL) {
				int res, server_errno;

				/* do a getattr/lstat on flare */
				magma_response response;
				magma_pktqs_getattr(s, 0, 0, path);
				magma_pktar(s, (gchar *) &response);
				magma_pktar_getattr(&response);
				memcpy(&(flare->st), &response.body.getattr.stbuf, sizeof(struct stat));

				res = magma_flare_upcast(flare);

				/* get content if needed */
				if (magma_isdir(flare) || magma_isreg(flare) || magma_islnk(flare)) {
					char *buffer = NULL;
					size_t size = 0;
					magma_pktqs_get_key_content(s, key);
					magma_pktar_get_key_content(s, size, buffer);
					int fd = -1;
					if ((fd = magma_open_flare_contents(flare)) != -1) {
						magma_full_write(fd,buffer,size);
						close(fd);
					}
				}

				/* tell remote peer to drop key and reduce key space */
				magma_pktqs_drop_key(s,key);
				magma_pktar_drop_key(s,res);

				if (res != 1) {
					dbg(LOG_ERR, DEBUG_ERR, "Error: server not dropped key %s", key);
				} else {
					magma_lock_me();
					strcpy(myself.start_key, key);
					magma_unlock_me();

					magma_lock_node(myself.prev);
					strcpy(myself.prev->stop_key, key);
					decrement_key(myself.prev->stop_key);
					magma_unlock_node(myself.prev);
				}

				magma_save_node_profile();
				/* dbg(LOG_INFO, DEBUG_BALANCER, "Saving status from manage_underload"); */
				magma_save_status();
			} else {
				/******** BIG ERROR!!!! flare can be lost! *********/
				/*  tell peer to readmit lost key in its keyspace  */
				/***************************************************/
				dbg(LOG_ERR, DEBUG_ERR, "Key content is damaged! Peer should be asked to readmit the key!");
				dbg(LOG_ERR, DEBUG_ERR, "Key %s is probably LOST!", key);
			}
		}
	}

	g_socket_shutdown(s, 1, 1, NULL);
	dbg(LOG_INFO, DEBUG_BALANCER, "%s says: My first key is %s", myself.node_name, myself.start_key);
#endif
}

/**
 * manages overload. If a node has heavier load than sibling nodes can
 * request to give some keys to leverage load profile.
 */
void magma_manage_overload()
{
	magma_lock_me();
	/*
	 * TODO to be implemented
	 */
	dbg(LOG_INFO, DEBUG_BALANCER, "magma_manage_overload() still to be written!");
	magma_unlock_me();
}

// vim:ts=4:nocindent:autoindent
