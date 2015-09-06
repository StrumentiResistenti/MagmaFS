/*
   MAGMA -- protocol_node.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Implements MAGMA node protocol used between nodes.

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

#define MAGMA_BUILD_NODE_SUPPORT 0

/**
 * Update the singleton myself struct with information
 * extracted from a magma_lava struct
 *
 * @param lava the magma_lava struct used as source
 */
void magma_update_myself_from_lava(magma_lava *lava) {
	magma_volcano *me = lava->first_node;
	while (1) {
		if (g_strcmp0(me->fqdn_name, myself.fqdn_name) == 0) {

			g_strlcpy(myself.start_key, me->start_key, SHA_READABLE_DIGEST_LENGTH);
			g_strlcpy(myself.stop_key, me->stop_key, SHA_READABLE_DIGEST_LENGTH);

			break;
		}
		me = me->next;
		if (me == lava->first_node) break;
	}
}

/**
 * Send a whole key (all its content) to a remote node described by peer.
 *
 * @param socket the socket to transmit the data
 * @param peer the remote node
 * @param key_path the path of the flare
 */
void magma_node_transmit_key(GSocket *socket, GSocketAddress *peer, const gchar *flare_key)
{
	/*
	 * Retrieve flare data from SQL
	 */
	gchar *query = g_strdup_printf("select path from flare_%s where hash = '%s'",
		myself.node_name, flare_key);

	dbi_result result = magma_sql_query(query);
	g_free(query);

	gchar *path = NULL;
	if (result && dbi_result_next_row(result)) {
		path = magma_sql_fetch_string(result, 1);
	}

	if (!path) {
		dbg(LOG_ERR, DEBUG_PNODE, "Unable to fetch path while transmitting key %s", flare_key);
		return;
	}

	dbg(LOG_INFO, DEBUG_PNODE, "Sending key %s (%s)", flare_key, path);

	/*
	 * build the flare object
	 */
	magma_flare_t *flare = magma_search_or_create(path);
	if (!flare) {
		dbg(LOG_ERR, DEBUG_PNODE, "Unable to fetch flare while transmitting key %s (%s)", flare_key, path);
		free(path);
		return;
	}

	free(path);

	int fd = open(flare->contents, O_RDONLY);
	if (!fd) {
		dbg(LOG_ERR, DEBUG_PNODE, "Unable to open %s: %s", flare->contents, strerror(errno));
		magma_dispose_flare(flare);
		return;
	}

	/*
	 * Loop on the key
	 */
	magma_flare_read_lock(flare);
	magma_flare_update_stat(flare);

	off_t offset;
	for (offset = 0; offset < flare->st.st_size; offset += 32 * 1024) {
		gchar buffer[32 * 1024];
		gsize size = magma_perfect_read(fd, buffer, 32 * 1024);

		if (size > 0) {
			magma_node_response response;
			response.header.res = -1;

			while (response.header.res is -1) {
				dbg(LOG_INFO, DEBUG_PNODE, "Sending key %s @%lu:%ld", flare->path, offset, size);
				magma_pktqs_transmit_key(socket, peer, offset, size, buffer, flare, &response);
				dbg(LOG_INFO, DEBUG_PNODE, "Key %s offset moved to %lu",
					flare->path, response.body.send_key.offset);
			}
		}
	}
	magma_flare_read_unlock(flare);

	close(fd);
	magma_dispose_flare(flare);
}

/**
 * Transmit this node entire key-space to a remote node.
 * This is usually done after a new node joins the DHT and
 * needs all the keys of its peer to become its redundant
 * node.
 *
 * @param socket an network socket
 * @param peer the remote peer
 */
void magma_node_transmit_keyspace_to_node(magma_volcano *node) {
	/*
	 * duplicate all the keys to the joining node to let
	 * it act as redundant node of this node
	 */
	GError *error = NULL;
	GDir *dir = g_dir_open(magma_environment.hashpath, 0, &error);

	if (!dir) {
		dbg(LOG_ERR, DEBUG_ERR, "Error opening %s dir", magma_environment.hashpath);
		return;
	}

	GSocketAddress *peer = NULL;
	GSocket *socket = magma_open_client_connection(node->ip_addr, MAGMA_NODE_PORT, &peer);

	while (1) {
		/*
		 * read another key entry from local storage directory
		 */
		const gchar *key = g_dir_read_name(dir);
		if (!key) break;
		if (strcmp(key, "store.sql") is 0) continue;
		magma_node_transmit_key(socket, peer, key);
	}

	g_dir_close(dir);
}

/**
 * Receive a key chunk from its owner.
 *
 * @param socket the receiving socket
 * @param peer the transmitting node
 * @param buffer a buffer for the packet coming from the wire
 * @param request the incoming request described
 * @return 0 on success, 1 on failure
 */
int magma_node_receive_key(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_node_request *request)
{
	/*
	 * receive the key request
	 */
	magma_pktqr_transmit_key(buffer, request);
	dbg(LOG_INFO, DEBUG_PNODE, "Receiving %d bytes of key %s @%lu",
		request->body.send_key.size, request->body.send_key.path,
		request->body.send_key.offset);

	/*
	 * lookup the flare
	 */
	magma_flare_t *flare = magma_search_or_create(request->body.send_key.path);
	if (!flare) {
		magma_flare_write_unlock(flare);
		return (1);
	}

	magma_flare_write_lock(flare);

	flare->st.st_mode = request->body.send_key.mode;
	flare->st.st_uid  = request->body.send_key.uid;
	flare->st.st_gid  = request->body.send_key.gid;
	// chmod(flare->contents, request->body.send_key.mode);

	/*
	 * upcast the flare if needed
	 */
	if (!flare->is_upcasted) {
		if (flare->st.st_mode & S_IFREG)  flare->type = MAGMA_FLARE_TYPE_REGULAR; else
		if (flare->st.st_mode & S_IFDIR)  flare->type = MAGMA_FLARE_TYPE_DIR;     else
		if (flare->st.st_mode & S_IFLNK)  flare->type = MAGMA_FLARE_TYPE_SYMLINK; else
		if (flare->st.st_mode & S_IFBLK)  flare->type = MAGMA_FLARE_TYPE_BLOCK;   else
		if (flare->st.st_mode & S_IFCHR)  flare->type = MAGMA_FLARE_TYPE_CHAR;    else
		if (flare->st.st_mode & S_IFIFO)  flare->type = MAGMA_FLARE_TYPE_FIFO;    else
		if (flare->st.st_mode & S_IFSOCK) flare->type = MAGMA_FLARE_TYPE_SOCKET;

#if MAGMA_DECLARE_FLARE_ITEM_FIELD
		magma_flare_upcast(flare);

		if (!flare->is_upcasted) {
			/* can't upcast the flare: return an error */
			dbg(LOG_ERR, DEBUG_PNODE, "Error upcasting %s", flare->path);
			magma_pktas_transmit_key(socket, peer, -1, request->body.send_key.offset, request->header.transaction_id);
			magma_flare_write_unlock(flare);
			magma_dispose_flare(flare);
			return (1);
		}
#else
		flare->is_upcasted = 1;
#endif
	}

	magma_save_flare(flare, request->body.send_key.offset ? FALSE : TRUE);
	magma_load_flare(flare);

	/*
	 * Saving flare contents
	 */
	int fd = open(flare->contents, O_WRONLY|O_CREAT, request->body.send_key.mode);
	if (fd isNot -1) {
#if 0
		int res = pwrite(fd,
			request->body.send_key.chunk,
			request->body.send_key.size,
			request->body.send_key.offset);
#else
		lseek(fd, request->body.send_key.offset, SEEK_SET);
		int res = magma_full_write(fd, request->body.send_key.chunk, request->body.send_key.size);
#endif
		int _errno = errno;
		close(fd);

		if (res is -1) {
			dbg(LOG_ERR, DEBUG_PNODE, "Failed to receive key %s: %s", request->body.send_key.path, strerror(_errno));
			magma_pktas_transmit_key(socket, peer, -1, request->body.send_key.offset, request->header.transaction_id, 0);
			magma_flare_write_unlock(flare);
			magma_dispose_flare(flare);
			return (1);
		}
	} else {
		/* can't open file, return an error */
		dbg(LOG_ERR, DEBUG_PNODE, "Can't open %s: %s", flare->contents, strerror(errno));
		magma_pktas_transmit_key(socket, peer, -1, request->body.send_key.offset, request->header.transaction_id, 0);
		magma_flare_write_unlock(flare);
		magma_dispose_flare(flare);
		return (1);
	}

	magma_flare_write_unlock(flare);

	/*
	 * key chunk received and recorded
	 */
	off_t new_offset = request->body.send_key.offset + request->body.send_key.size;
	magma_pktas_transmit_key(socket, peer, 0, new_offset, request->header.transaction_id, 0);

	magma_dispose_flare(flare);
	return (0);
}

/**
 * join the net syncing from specified node ipaddr and port.
 *
 * @param ipaddr string rappresentation of remote server IP address
 * @param port remote server port
 * @return result can be safely ignored since exit(N) is called in case of failure.
 */
int magma_join_network(char *ipaddr, int port)
{
	dbg(LOG_INFO, DEBUG_PNODE, "Joining network on server IP %s:%u", ipaddr, port);

	magma_check_filesystem();

	GSocketAddress *peer;
	GSocket *socket = magma_open_client_connection(ipaddr, port, &peer);
	if (!socket) {
		dbg(LOG_ERR, DEBUG_ERR, "Error joining network.");
		dbg(LOG_ERR, DEBUG_ERR, "Can't open NODE socket to server %s.", ipaddr);
		dbg(LOG_ERR, DEBUG_ERR, "Aborting now!");
		exit(1);
	}

	/*
	 * read the lava topology
	 */
	lava = g_new0(magma_lava, 1);
	guint16 offset = 0;
	int i = 0;

	while (1) {
		magma_node_response topology_response;
		magma_pktqs_transmit_topology(socket, peer, offset, &topology_response);

		if (topology_response.header.status isNot G_IO_STATUS_NORMAL) exit(1);
		if (topology_response.header.res isNot 0) exit(1);

		for (; i < topology_response.body.send_topology.transmitted_nodes; i++) {
			magma_node_inside_topology *n = &(topology_response.body.send_topology.nodes[i]);

			/*
			 * failsafe check: port can't be zero, that means the
			 * struct has not been filled
			 */
			if (!n->port) {
				dbg(LOG_ERR, DEBUG_PNODE, "Null node received @%d/%d",
					i, topology_response.body.send_topology.transmitted_nodes);

				exit (1);
			}

			/*
			 * create the new node
			 */
			magma_volcano *new_node = magma_volcano_new(
				n->node_name,
				n->fqdn_name,
				n->ip_addr,
				n->port,
				0, 0,
				n->start_key,
				n->stop_key,
				NULL);

			if (!new_node) {
				dbg(LOG_ERR, DEBUG_ERR, "Null new node: aborting lava network join");
				exit (1);
			}

			magma_sql_save_volcano(new_node);

			/*
			 * add the node to the lava ring
			 */
			lava->participants++;

			if (!lava->first_node) {
				lava->first_node = lava->last_node = new_node;
			} else {
				lava->last_node->next = new_node;
				new_node->prev = lava->last_node;
				lava->last_node = new_node;
			}

			/*
			 * if no more nodes are left to be received
			 * then close the ring and end the cycle
			 */
			if (
				(i is topology_response.body.send_topology.transmitted_nodes - 1) &&
				(!topology_response.body.send_topology.more_nodes_waiting)) {
				// lava->first_node->prev = new_node;
				// new_node->next = lava->first_node;
				break;
			}
		}

		if (!topology_response.body.send_topology.more_nodes_waiting) break;
	}

	/*
	 * positioning of this node inside the network
	 */
	dbg(LOG_INFO, DEBUG_PNODE, "Signaling myself: %s (%s)", myself.node_name, myself.fqdn_name);
	dbg(LOG_INFO, DEBUG_PNODE, " Connecting from %s:%d", myself.ip_addr, myself.port);
	dbg(LOG_INFO, DEBUG_PNODE, "  Bandwidth: %d", myself.bandwidth);
	dbg(LOG_INFO, DEBUG_PNODE, "  Start Key: %s", myself.start_key);
	dbg(LOG_INFO, DEBUG_PNODE, "   Stop Key: %s", myself.stop_key);

	/*
	 * Send a join network request an wait for a response
	 */
	magma_node_response response;
	magma_pktqs_join_network(socket, peer, &myself, &response);

	/*
	 * check if another node with the same name is already part of the network
	 */
	if (response.header.res is MAGMA_NODE_NAME_COLLISION) {
		dbg(LOG_ERR, DEBUG_ERR, "A node called %s already exists in network", myself.node_name);
		exit(1);
	}

	/*
	 * copy new sector boundaries to myself and to
	 * the lava ring
	 */
	strncpy(myself.start_key, response.body.join_network.joining_start_key, strlen(response.body.join_network.joining_start_key));
	strncpy(myself.stop_key, response.body.join_network.joining_stop_key, strlen(response.body.join_network.joining_stop_key));

	magma_volcano *me = magma_volcano_new(
		myself.node_name,
		myself.fqdn_name,
		myself.ip_addr,
		myself.port,
		myself.bandwidth,
		myself.storage,
		myself.start_key,
		myself.stop_key,
		myself.hashpath);

	if (!me) {
		dbg(LOG_ERR, DEBUG_PNODE, "Error allocating this node inside lava network");
		exit(1);
	}

	magma_lava_add_volcano(lava, me);

	magma_volcano *other = me->prev;

	/*
	 * some logging
	 */
	dbg(LOG_INFO, DEBUG_PNODE, "I've been assigned to sector [%s, %s]",	myself.start_key, myself.stop_key);
	dbg(LOG_INFO, DEBUG_PNODE, "Joined node now has [%s, %s]", other->start_key, other->stop_key);

	/*
	 * signaling the other peer that this node is ready
	 * for duty
	 */
	magma_node_response finish_response;
	magma_pktqs_finish_join_network(socket, peer, &myself, other, lava->participants, &finish_response);

	if (finish_response.header.status isNot G_IO_STATUS_NORMAL) {
		dbg(LOG_ERR, DEBUG_PNODE, "Error receiving network joint message. Aborting now.");
		exit (1);
	}

	if (lava->participants isNot finish_response.body.finish_join_network.number_of_nodes) {
		dbg(LOG_INFO, DEBUG_ERR, "Error joining network. Aborting now.");
		exit (1);
	}

	dbg(LOG_INFO, DEBUG_PNODE, "Network joint!");
	magma_environment.state = magma_network_ready;

	return 0;
}

/**
 * act as a server for a join operation
 *
 * @param socket a GSocket object
 * @param peer the GSocketAddress object with peer profile
 * @param req the magma_node_request struct
 * @return 0 on success, 1 on failure
 */
int magma_node_join_network(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_node_request *request)
{
	/*
	 * receive the join request and adapt data to local endianness
	 */
	magma_pktqr_join_network(buffer, request);

	/*
	 * Create a new node with incoming data
	 */
	magma_volcano *new_node = magma_volcano_new(
		request->body.join_network.node_name,
		request->body.join_network.fqdn_name,
		request->body.join_network.ip_addr,
		request->body.join_network.port,
		request->body.join_network.bandwidth,
		request->body.join_network.storage,
		NULL, NULL,	NULL); // keyspace and dhtpath are not required to join a network

	if (!new_node) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't allocate space for new node on manage_node");
		magma_pktas_join_network(socket, peer, MAGMA_INTERNAL_ERROR, NULL, request->header.transaction_id, 0);
		return (1);
	}

	/*
	 * check the node does not already exist
	 */
	uint8_t res = magma_node_exists(new_node, lava);
	if (res isNot MAGMA_OK) {
		magma_pktas_join_network(socket, peer, res, new_node, request->header.transaction_id, 0);
		dbg(LOG_ERR, DEBUG_ERR, "joining new node %s", new_node->node_name);
		magma_explain_join_error(res);
		magma_vulcano_destroy(new_node);
		return (1);
	}
	
	dbg(LOG_INFO, DEBUG_PNODE, "New node %s (%s) joining", new_node->node_name, new_node->fqdn_name);
	dbg(LOG_INFO, DEBUG_PNODE, "New nodes connects from %s:%d", new_node->ip_addr, new_node->port);

	/*
	 * New node own key slice will start with next key after the
	 * highest used one...
	 */
	gchar *highest_key = magma_get_highest_used_key();
	magma_increment_key(highest_key);
	memcpy(new_node->start_key, highest_key, SHA_READABLE_DIGEST_LENGTH);
	g_free(highest_key);

	/*
	 * ...and will end with this node highest key
	 */
	memcpy(new_node->stop_key, myself.stop_key, SHA_READABLE_DIGEST_LENGTH);

	dbg(LOG_INFO, DEBUG_PNODE, "New node will be assigned to [%s, %s]", new_node->start_key, new_node->stop_key);

	/*
	 * Inform the joining node of its place in the topology
	 */
	magma_pktas_join_network(socket, peer, MAGMA_OK, new_node, request->header.transaction_id, 0);

	return (1);
}

GMutex magma_finish_join_network_mutex;

void magma_node_finish_join_network(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_node_request *request)
{
	/*
	 * receive the join request and adapt data to local endianness
	 */
	magma_pktqr_finish_join_network(buffer, request);

	/*
	 * Clone the lava ring
	 */
	g_mutex_lock(&magma_finish_join_network_mutex);
	magma_lava *lava_clone = magma_lava_clone();
	g_assert(lava_clone->participants equals lava->participants);

	if (!lava_clone) {
		g_mutex_unlock(&magma_finish_join_network_mutex);
		dbg(LOG_ERR, DEBUG_ERR, "Can't allocate space for new node on manage_node");
		magma_pktas_join_network(socket, peer, MAGMA_INTERNAL_ERROR, NULL, request->header.transaction_id, 0);
		return;
	}

	/*
	 * Create a new node with incoming data
	 */
	magma_volcano *new_node = magma_volcano_new(
		request->body.finish_join_network.node_name,
		request->body.finish_join_network.fqdn_name,
		request->body.finish_join_network.ip_addr,
		request->body.finish_join_network.port,
		request->body.finish_join_network.bandwidth,
		request->body.finish_join_network.storage,
		request->body.finish_join_network.joining_start_key,
		request->body.finish_join_network.joining_stop_key,
		NULL); // dhtpath is not required to join a network

	if (!new_node) {
		g_mutex_unlock(&magma_finish_join_network_mutex);
		dbg(LOG_ERR, DEBUG_ERR, "Can't allocate space for new node on manage_node");
		magma_pktas_finish_join_network(socket, peer, MAGMA_INTERNAL_ERROR, 0, request->header.transaction_id, 0);
		magma_lava_destroy(lava_clone);
		return;
	}

	/*
	 * And actually add it to this lava ring
	 */
	magma_lava_add_volcano(lava_clone, new_node);

	/*
	 * Compare this node slice with the one transmitted by
	 * joining node
	 */
	magma_volcano *me = lava_clone->first_node;
	while (1) {
		if (strcmp(me->fqdn_name, myself.fqdn_name) is 0) break;
		if (!me->next) {
			me = NULL;
			break;
		}
		me = me->next;
	}

	/*
	 * Accept or reject the join procedure. If rejected, the new
	 * lava is discarded and a negative ACK is sent. Otherwise the
	 * new lava is put in production and the old one discarded.
	 * The joining node starts to act as redundant node of this node.
	 */
	if ((strcmp(me->start_key, request->body.finish_join_network.start_key) isNot 0) ||
			(strcmp(me->stop_key, request->body.finish_join_network.stop_key) isNot 0))
	{
		g_mutex_unlock(&magma_finish_join_network_mutex);
		dbg(LOG_ERR, DEBUG_PNODE, "Error joining the network: remote node does not agree on this node slice");
		magma_lava_destroy(lava_clone);
		magma_pktas_finish_join_network(socket, peer, -1, 0, request->header.transaction_id, 0);
		return;
	}

	/*
	 * keep the new node up to date on all new writings. This will
	 * perform redundant Flare operations on this node even if
	 * according to the topology another node is currently acting
	 * as redundant node for this one.
	 */
	myself.joining_node = new_node;

	/*
	 * confirm network join
	 */
	magma_pktas_finish_join_network(socket, peer, 0, lava_clone->participants, request->header.transaction_id, 0);

	/*
	 * transmit the whole key space
	 */
	magma_node_transmit_keyspace_to_node(new_node);

	/*
	 * put new lava topology in production
	 */
	magma_lava *old_lava = lava;
	lava = lava_clone;
	g_mutex_unlock(&magma_finish_join_network_mutex);

	/*
	 * update myself
	 */
	magma_update_myself_from_lava(lava);

	myself.joining_node = NULL;

	/*
	 * save the updated nodes
	 */
	magma_sql_save_volcano(&myself);
	magma_sql_save_volcano(new_node);

	/*
	 * destroy the old lava
	 */
	magma_lava_destroy(old_lava);
}

/**
 * transmit network topology
 */
void magma_node_transmit_topology(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_node_request *request)
{
	/*
	 * receive the join request and adapt data to local endianness
	 */
	magma_pktqr_transmit_topology(buffer, request);
	magma_pktas_transmit_topology(socket, peer, lava, request->body.send_topology.offset, request->header.transaction_id, 0);
}

/**
 * manage a add flare to parent request
 */
void magma_node_manage_add_flare_to_parent(
	GSocket *socket,
	GSocketAddress *peer,
	gchar *buffer,
	magma_node_request *request)
{
	magma_pktqr_add_flare_to_parent(buffer, request);

	magma_flare_t *flare = magma_search_or_create(request->body.add_flare_to_parent.path);
	int res = magma_add_flare_to_parent(flare);
	magma_dispose_flare(flare);
	magma_pktas_add_flare_to_parent(socket, peer, res, request->header.transaction_id, 0);
}

/**
 * manage a remove flare from parent request
 */
void magma_node_manage_remove_flare_from_parent(
	GSocket *socket,
	GSocketAddress *peer,
	gchar *buffer,
	magma_node_request *request)
{
	magma_pktqr_remove_flare_from_parent(buffer, request);

	magma_flare_t *flare = magma_search_or_create(request->body.remove_flare_from_parent.path);
	int res = magma_remove_flare_from_parent(flare);
	magma_dispose_flare(flare);
	magma_pktas_remove_flare_from_parent(socket, peer, res, request->header.transaction_id, 0);
}

/**
 * manage a heartbeat request
 */
void magma_node_manage_heartbeat(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_node_request *request)
{
	magma_pktqr_heartbeat(buffer, request);
	magma_pktas_heartbeat(socket, peer, &myself, request->header.transaction_id, 0);
	return;
}

/**
 * manage a shutdown request
 */
void magma_node_manage_shutdown(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_node_request *request)
{
	magma_pktqr_shutdown(buffer, request);
	magma_pktas_shutdown(socket, peer, request->header.transaction_id, 0);

	gchar *addr = magma_get_peer_addr(peer);
	dbg(LOG_INFO, DEBUG_PNODE, "Shutdown request received by %s. Exiting now.", addr);
	g_free(addr);
	magma_cleanup(0);
}

/**
 * manage a "network built" request
 */
void magma_node_manage_network_built(GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_node_request *request)
{
	magma_pktqr_network_built(buffer, request);

	gchar *addr_string = magma_get_peer_addr(peer);

	magma_volcano *v = magma_route_path(MAGMA_DHT_PATH);

	if (strcmp(v->ip_addr, addr_string) == 0) {
		magma_pktas_network_built(socket, peer, request->header.transaction_id, 0);

		if (magma_network_ready != request->body.network_built.status) {
			dbg(LOG_ERR, DEBUG_BOOT, "Error building network: coordinator says network build failed. Exiting now.");
			exit (1);
		} else {
			magma_environment.state = request->body.network_built.status;
		}
	} else {
		magma_pktas_network_built(socket, peer, request->header.transaction_id, 0);

		dbg(LOG_ERR, DEBUG_BOOT,
			"Error building network: coordinator transmits from %s while we expect %s. Exiting now.",
			addr_string, v->ip_addr);
		exit (1);
	}

	g_free(addr_string);
	return;
}

/*
 * The prototype of the node callback used to answer
 * to node requests
 */
typedef void (*magma_optype_callback) (GSocket *socket, GSocketAddress *peer, gchar *buffer, magma_node_request *request);

/*
 * A callback lookup table used to answer to node request without
 * walking through a long if/switch cycle
 */
static magma_optype_callback magma_optype_callbacks[MAGMA_OP_TYPE_MAX] = {};
static gchar *magma_optype_callbacks_desc[MAGMA_OP_TYPE_MAX] = {};

/*
 * Initialize node callbacks lookup table
 */
void magma_init_node_callbacks()
{
	/*
	 * Set all the slots to NULL
	 */
	magma_optype i = 0;
	for (; i < MAGMA_OP_TYPE_MAX; i++)
		magma_optype_callbacks[i] = NULL;

	/*
	 * Register node callbacks
	 */
	magma_register_callback(MAGMA_OP_TYPE_JOIN,						magma_node_join_network						);
	magma_register_callback(MAGMA_OP_TYPE_FINISH_JOIN,				magma_node_finish_join_network				);
	magma_register_callback(MAGMA_OP_TYPE_TRANSMIT_KEY,				magma_node_receive_key						);
	magma_register_callback(MAGMA_OP_TYPE_TRANSMIT_TOPOLOGY,		magma_node_transmit_topology   				);
	magma_register_callback(MAGMA_OP_TYPE_NETWORK_BUILT,			magma_node_manage_network_built 			);
	magma_register_callback(MAGMA_OP_TYPE_HEARTBEAT,				magma_node_manage_heartbeat					);
	magma_register_callback(MAGMA_OP_TYPE_ADD_FLARE_TO_PARENT,		magma_node_manage_add_flare_to_parent		);
	magma_register_callback(MAGMA_OP_TYPE_REMOVE_FLARE_FROM_PARENT,	magma_node_manage_remove_flare_from_parent	);
	magma_register_callback(MAGMA_OP_TYPE_SHUTDOWN,					magma_node_manage_shutdown					);
}

/**
 * Answer to a NODE request
 */
void magma_manage_udp_node_protocol(GSocket *socket, GSocketAddress *peer, gchar buffer[MAGMA_MESSAGE_MAX_SIZE])
{
	magma_node_request request;
	memset(&request, 0, sizeof(magma_node_request));

	gchar *ptr = magma_parse_request_header(buffer, (magma_request *) &request);
	if (!ptr) {
		dbg(LOG_WARNING, DEBUG_ERR, "error deserializing request headers");
		magma_send_udp_failure(socket, peer, 0, request.header.transaction_id, 0);
	}

	/*
	 * if requested to close the connection, do it
	 */
	if (MAGMA_OP_TYPE_CLOSECONNECTION == request.header.type) return;

	dbg(LOG_ERR, DEBUG_PNODE, "Incoming request #%05d: %s",
		request.header.transaction_id,
		magma_optype_callbacks_desc[request.header.type]);

	/*
	 * A request can be:
	 *   1. a duplicate of an already processed request, resent because
	 *      no answer were received
	 *
	 *   2. a brand new request to be processed
	 *
	 *   3. a malformed bunch of bytes
	 *
	 * First this function must check if there's already a response in the
	 * cache. If one is found, its sent back and the function returns.
	 * If no cached answer is found and the request is valid, it's answered.
	 */
	if (!magma_optype_callbacks[request.header.type]) {
		dbg(LOG_WARNING, DEBUG_ERR, "operation type unknown: %d", request.header.type);
		magma_send_udp_failure(socket, peer, 254, request.header.transaction_id, 0);
		return;
	}

	(magma_optype_callbacks[request.header.type])(socket, peer, ptr, &request);
}

/**
 * Called by the thread pool to manage a request
 */
void magma_manage_udp_node_protocol_pool(magma_incoming_request *incoming, gpointer user_data) {
	(void) user_data;

	/*
	 * just call the original callback with proper parameters
	 */
	magma_manage_udp_node_protocol(incoming->socket, incoming->peer, incoming->buffer);

	/*
	 * unref GLib objects to allow their disposal
	 */
	g_object_unref(incoming->peer);
	g_free(incoming);
}

// vim:ts=4:nocindent:autoindent
