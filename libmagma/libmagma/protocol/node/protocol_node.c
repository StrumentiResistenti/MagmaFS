/*
   MAGMA -- protocol_node.c
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

#include "../../magma.h"

gchar *network_ops_explained[256];

char *magma_explain_network_error(uint8_t error)
{
	static int inited = 0;

	if (!inited) {
		memset(network_ops_explained, sizeof(char*), 256);
		network_ops_explained[MAGMA_NODE_NAME_COLLISION] = g_strdup("Another node has same name");
		network_ops_explained[MAGMA_FQDN_NAME_COLLISION] = g_strdup("Another node has same FQDN");
		network_ops_explained[MAGMA_IP_PORT_COLLISION]   = g_strdup("Port is already in use");
		network_ops_explained[MAGMA_NONEXISTING_SHARE]   = g_strdup("Share doesn't exists");
		network_ops_explained[MAGMA_CONNECTION_REFUSED]  = g_strdup("Connection refused");
		inited = 1;
	}

	return network_ops_explained[error];
}

#if 0
/**
 * Format a node request header
 *
 * @param type the magma_optype
 */
gchar *magma_node_format_request_header(gchar *buffer, magma_optype type) {
	gchar *ptr = buffer;
	ptr = magma_serialize_8(ptr, type);
	return (ptr);
}
#endif

gchar *magma_node_format_response_header(gchar *buffer, gint32 res, gint16 err_no) {
	gchar *ptr = buffer;
	ptr = magma_serialize_32(ptr, res);
	ptr = magma_serialize_16(ptr, err_no);
	return (ptr);
}

/**
 * Network join
 */
magma_transaction_id
magma_pktqs_join_network(
	GSocket *socket,
	GSocketAddress *peer,
	magma_volcano *node,
	magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_JOIN, 0, 0, &tid, MAGMA_TERMINAL_TTL);

	ptr = magma_serialize_16(ptr, node->port);
	ptr = magma_serialize_16(ptr, node->bandwidth);
	ptr = magma_serialize_32(ptr, node->storage);
	ptr = magma_serialize_string(ptr, node->ip_addr);
	ptr = magma_serialize_string(ptr, node->node_name);
	ptr = magma_serialize_string(ptr, node->fqdn_name);

	magma_log_transaction(MAGMA_OP_TYPE_JOIN, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_join_network, response);

	return (tid);
}

void magma_pktqr_join_network(gchar *buffer, magma_node_request *request) {
	gchar *ptr = buffer;

	ptr = magma_deserialize_16(ptr, &(request->body.join_network.port));
	ptr = magma_deserialize_16(ptr, &(request->body.join_network.bandwidth));
	ptr = magma_deserialize_32(ptr, &(request->body.join_network.storage));
	ptr = magma_deserialize_string(ptr, request->body.join_network.ip_addr);
	ptr = magma_deserialize_string(ptr, request->body.join_network.node_name);
	ptr = magma_deserialize_string(ptr, request->body.join_network.fqdn_name);
}

void magma_pktas_join_network(
	GSocket *socket,
	GSocketAddress *peer,
	int res,
	magma_volcano *node,
	magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, 0, tid);

	if (res is MAGMA_OK) {
		g_assert(node);

		ptr = magma_serialize_string(ptr, node->start_key);
		ptr = magma_serialize_string(ptr, node->stop_key);
	}

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

void magma_pktar_join_network(GSocket *socket, GSocketAddress *peer, magma_node_response *response) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (response->header.status isNot G_IO_STATUS_NORMAL || response->header.res isNot MAGMA_OK) return;

	ptr = magma_deserialize_string(ptr, response->body.join_network.joining_start_key);
	ptr = magma_deserialize_string(ptr, response->body.join_network.joining_stop_key);
}

/**
 * Finish network join
 */
magma_transaction_id
magma_pktqs_finish_join_network(
	GSocket *socket,
	GSocketAddress *peer,
	magma_volcano *myself,
	magma_volcano *other,
	guint32 number_of_nodes,
	magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_FINISH_JOIN, 0, 0, &tid, MAGMA_TERMINAL_TTL);

	ptr = magma_serialize_32(ptr, number_of_nodes);
	ptr = magma_serialize_16(ptr, myself->port);
	ptr = magma_serialize_16(ptr, myself->bandwidth);
	ptr = magma_serialize_32(ptr, myself->storage);

	ptr = magma_serialize_string(ptr, other->start_key);
	ptr = magma_serialize_string(ptr, other->stop_key);

	ptr = magma_serialize_string(ptr, myself->start_key);
	ptr = magma_serialize_string(ptr, myself->stop_key);

	ptr = magma_serialize_string(ptr, myself->ip_addr);

	ptr = magma_serialize_string(ptr, myself->fqdn_name);
	ptr = magma_serialize_string(ptr, myself->node_name);


	magma_log_transaction(MAGMA_OP_TYPE_FINISH_JOIN, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_finish_join_network, response);

	return (tid);
}

void magma_pktqr_finish_join_network(gchar *buffer, magma_node_request *request)
{
	gchar *ptr = buffer;

	ptr = magma_deserialize_32(ptr, &request->body.finish_join_network.number_of_nodes);
	ptr = magma_deserialize_16(ptr, &request->body.finish_join_network.port);
	ptr = magma_deserialize_16(ptr, &request->body.finish_join_network.bandwidth);
	ptr = magma_deserialize_32(ptr, &request->body.finish_join_network.storage);

	ptr = magma_deserialize_string(ptr, request->body.finish_join_network.start_key);
	ptr = magma_deserialize_string(ptr, request->body.finish_join_network.stop_key);

	ptr = magma_deserialize_string(ptr, request->body.finish_join_network.joining_start_key);
	ptr = magma_deserialize_string(ptr, request->body.finish_join_network.joining_stop_key);

	ptr = magma_deserialize_string(ptr, request->body.finish_join_network.ip_addr);

	ptr = magma_deserialize_string(ptr, request->body.finish_join_network.fqdn_name);
	ptr = magma_deserialize_string(ptr, request->body.finish_join_network.node_name);
}

void magma_pktas_finish_join_network(
	GSocket *socket,
	GSocketAddress *peer,
	int res,
	guint32 number_of_nodes,
	magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, 0, tid);

	ptr = magma_serialize_32(ptr, number_of_nodes);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

void magma_pktar_finish_join_network(GSocket *socket, GSocketAddress *peer, magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (response->header.status isNot G_IO_STATUS_NORMAL || response->header.res isNot 0) return;

	ptr = magma_deserialize_32(ptr, &response->body.finish_join_network.number_of_nodes);
}

/**
 * DHT exchange
 */
magma_transaction_id
magma_pktqs_transmit_node(GSocket *socket, GSocketAddress *peer, gchar *start_key, int direction) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_TRANSMIT_NODE, 0, 0, &tid, MAGMA_TERMINAL_TTL);
	ptr = magma_serialize_8(ptr, direction);
	ptr = magma_serialize_string(ptr, start_key);

	// dbg(LOG_INFO, DEBUG_PNODE, "Asking for node sibling of %s, direction %d", start_key, direction);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);

	return (tid);
}

void magma_pktqr_transmit_node(gchar *buffer, magma_node_request *request) {
	gchar *ptr = buffer;
	ptr = magma_deserialize_8(ptr, (guint8 *) &request->body.send_node.direction);
	ptr = magma_deserialize_string(ptr, request->body.send_node.start_key);

	/*
	dbg(LOG_INFO, DEBUG_PNODE, "Asked for node sibling of %s, direction %d",
		request->body.send_node.start_key,
		request->body.send_node.direction);
	*/
}

void magma_pktas_transmit_node(
	GSocket *socket,
	GSocketAddress *peer,
	magma_volcano *node,
	int more_nodes_waiting,
	magma_transaction_id tid)
{
	if (!node) return;

	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, 0, 0, tid);

	dbg(LOG_INFO, DEBUG_PNODE, "Sending new node %s (%s:%d) @%lu@...",
		node->node_name, node->ip_addr, node->port, strlen(node->ip_addr));

	ptr = magma_serialize_8 (ptr, more_nodes_waiting);
	ptr = magma_serialize_16(ptr, node->port);
	ptr = magma_serialize_16(ptr, node->bandwidth);
	ptr = magma_serialize_16(ptr, node->storage);

	ptr = magma_serialize_string(ptr, node->ip_addr);
	ptr = magma_serialize_string(ptr, node->node_name);
	ptr = magma_serialize_string(ptr, node->fqdn_name);

	ptr = magma_serialize_string(ptr, node->start_key);
	ptr = magma_serialize_string(ptr, node->stop_key);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

void magma_pktar_transmit_node(GSocket *socket, GSocketAddress *peer, magma_node_response *response) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (response->header.status isNot G_IO_STATUS_NORMAL || response->header.res is -1) return;

	ptr = magma_deserialize_8 (ptr, &response->body.send_node.more_nodes_waiting);
	ptr = magma_deserialize_16(ptr, &response->body.send_node.port);
	ptr = magma_deserialize_16(ptr, &response->body.send_node.bandwidth);
	ptr = magma_deserialize_16(ptr, &response->body.send_node.storage);

	ptr = magma_deserialize_string(ptr, response->body.send_node.ip_addr);
	ptr = magma_deserialize_string(ptr, response->body.send_node.node_name);
	ptr = magma_deserialize_string(ptr, response->body.send_node.fqdn_name);

	ptr = magma_deserialize_string(ptr, response->body.send_node.start_key);
	ptr = magma_deserialize_string(ptr, response->body.send_node.stop_key);

	ptr = magma_deserialize_string(ptr, response->body.send_node.redundant_start_key);
	ptr = magma_deserialize_string(ptr, response->body.send_node.redundant_stop_key);
}

/**
 * Topology transmission
 */
magma_transaction_id
magma_pktqs_transmit_topology(
	GSocket *socket,
	GSocketAddress *peer,
	guint16 offset,
	magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_TRANSMIT_TOPOLOGY, 0, 0, &tid, MAGMA_TERMINAL_TTL);

	ptr = magma_serialize_16(ptr, offset);

	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_transmit_topology, response);

	return (tid);
}

void magma_pktqr_transmit_topology(gchar *buffer, magma_node_request *request) {
	gchar *ptr = buffer;

	ptr = magma_deserialize_16(ptr, &(request->body.send_topology.offset));
}

void magma_pktas_transmit_topology(
	GSocket *socket,
	GSocketAddress *peer,
	magma_lava *lava,
	int first_node,
	magma_transaction_id tid)
{
	if (!lava) return;

	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	/*
	 * encode the whole topology
	 */
	magma_volcano *v = lava->first_node;

	/*
	 * seek to requested node
	 */
	int i;
	for (i = 0; i < first_node; i++) {
		if (!v->next) {
			gchar *ptr = magma_format_response_header(buffer, -1, ENOENT, tid);
			magma_send_buffer(socket, peer, buffer, ptr - buffer);
			return;
		}
		v = v->next;
	}

	gchar *ptr = magma_format_response_header(buffer, 0, 0, tid);

	/*
	 * the first byte of the payload is the number of nodes
	 * transmitted within this packet. The max allowed number
	 * of nodes is 255, but due to the variable length nature
	 * of host encoding, 40 nodes could be enough to fill up
	 * a packet
	 */
	gchar *transmitted_nodes = ptr;

	/*
	 * the second byte of the payload is 1 if this packet can't
	 * host the whole topology and more will come
	 */
	gchar *more_nodes_waiting = ptr + 1;

	/*
	 * the topology is located after the more_nodes_waiting field
	 */
	ptr += 2;
	gchar *start = ptr;

	while (1) {
		/*
		 * if v is not null, count it and encode it
		 */
		if (!v) break;

		/*
		 * do not transmit more than 60K of topology per packet
		 */
		if (ptr > start + 60 * 1024) {
			/*
			 * if this was not the last node to transmit
			 * raise the flag of more_nodes_waiting
			 */
			if (v->next && v->next != lava->first_node) *more_nodes_waiting = 1;
			break;
		}

		*transmitted_nodes = *transmitted_nodes + 1;

		ptr = magma_serialize_16(ptr, v->port);
		ptr = magma_serialize_string(ptr, v->ip_addr);

		ptr = magma_serialize_string(ptr, v->start_key);
		ptr = magma_serialize_string(ptr, v->stop_key);

		ptr = magma_serialize_string(ptr, v->node_name);
		ptr = magma_serialize_string(ptr, v->fqdn_name);

		/*
		 * break if the last node was reached
		 */
		if (!v->next || v->next is lava->first_node) break;
		v = v->next;
	}

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

void magma_pktar_transmit_topology(GSocket *socket, GSocketAddress *peer, magma_node_response *response) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (response->header.status isNot G_IO_STATUS_NORMAL || response->header.res is -1) return;

	ptr = magma_deserialize_8(ptr, &(response->body.send_topology.transmitted_nodes));
	ptr = magma_deserialize_8(ptr, &(response->body.send_topology.more_nodes_waiting));

	guint8 i = 0;
	for (i = 0; i < response->body.send_topology.transmitted_nodes; i++) {
		magma_node_inside_topology *n = &(response->body.send_topology.nodes[i]);

		ptr = magma_deserialize_16(ptr, &(n->port));
		ptr = magma_deserialize_string(ptr, n->ip_addr);
		ptr = magma_deserialize_string(ptr, n->start_key);
		ptr = magma_deserialize_string(ptr, n->stop_key);
		ptr = magma_deserialize_string(ptr, n->node_name);
		ptr = magma_deserialize_string(ptr, n->fqdn_name);
	}
}

/**
 * Transmit a key
 *
 * the protocol sends a chunk of data at a specified offset
 * and responds with the expected offset of next iteration,
 * which is the incoming offset + the incoming chunk size.
 *
 * Since the key exchange is fragmented into chunks and each
 * chunk exchange is atomic, every exchange can be performed
 * as many time as requested.
 */
magma_transaction_id
magma_pktqs_transmit_key(
	GSocket *socket,
	GSocketAddress *peer,
	off_t offset,
	size_t size,
	gchar *chunk,
	magma_flare_t *flare,
	magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_TRANSMIT_KEY, 0, 0, &tid, MAGMA_TERMINAL_TTL);

	ptr = magma_serialize_string(ptr, flare->path);			/* the flare path */
	ptr = magma_serialize_64(ptr, offset);					/* the offset of the chunk */
	ptr = magma_serialize_32(ptr, flare->st.st_mode);		/* the flare type and permissions */
	ptr = magma_serialize_32(ptr, flare->st.st_uid);		/* the flare UID */
	ptr = magma_serialize_32(ptr, flare->st.st_gid);		/* the flare GID */
	ptr = magma_serialize_16(ptr, size);					/* the chunk length */
	ptr = magma_append_to_buffer(ptr, chunk, size);			/* the chunk itself */

	magma_log_transaction(MAGMA_OP_TYPE_TRANSMIT_KEY, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_transmit_key, response);

	return (tid);
}

void magma_pktqr_transmit_key(gchar *buffer, magma_node_request *request) {
	gchar *ptr = buffer;

	ptr = magma_deserialize_string(ptr, request->body.send_key.path);	/* the flare path */
	ptr = magma_deserialize_64(ptr, &request->body.send_key.offset);	/* the offset of the chunk */
	ptr = magma_deserialize_32(ptr, &request->body.send_key.mode);		/* the flare type and permissions */
	ptr = magma_deserialize_32(ptr, &request->body.send_key.uid);		/* the flare UID */
	ptr = magma_deserialize_32(ptr, &request->body.send_key.gid);		/* the flare GID */
	ptr = magma_deserialize_16(ptr, &request->body.send_key.size);		/* the chunk length */
	request->body.send_key.chunk = ptr;									/* the chunk itself */
}

void magma_pktas_transmit_key(GSocket *socket, GSocketAddress *peer, int res, off_t offset, magma_transaction_id tid) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, 0, tid);

	ptr = magma_serialize_64(ptr, offset);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

void magma_pktar_transmit_key(GSocket *socket, GSocketAddress *peer, magma_node_response *response) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (response->header.status isNot G_IO_STATUS_NORMAL || response->header.res is -1) return;

	ptr = magma_deserialize_64(ptr, &response->body.send_key.offset);
}

/**
 * Add a flare to its parent
 */
magma_transaction_id
magma_pktqs_add_flare_to_parent(
	GSocket *socket,
	GSocketAddress *peer,
	const gchar *path,
	magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_ADD_FLARE_TO_PARENT, 0, 0, &tid, MAGMA_TERMINAL_TTL);

	ptr = magma_serialize_string(ptr, path);

	magma_log_transaction(MAGMA_OP_TYPE_ADD_FLARE_TO_PARENT, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_add_flare_to_parent, response);

	return (tid);
}

void magma_pktqr_add_flare_to_parent(gchar *buffer, magma_node_request *request) {
	gchar *ptr = buffer;

	ptr = magma_deserialize_string(ptr, request->body.add_flare_to_parent.path);	/* the flare path */
}

void magma_pktas_add_flare_to_parent(GSocket *socket, GSocketAddress *peer, int res, magma_transaction_id tid) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, 0, tid);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

void magma_pktar_add_flare_to_parent(GSocket *socket, GSocketAddress *peer, magma_node_response *response) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	(void) ptr;
	// if (response->header.status isNot G_IO_STATUS_NORMAL || response->header.res is -1) return;
}

/**
 * Remove a flare from its parent
 */
magma_transaction_id
magma_pktqs_remove_flare_from_parent(
	GSocket *socket,
	GSocketAddress *peer,
	const gchar *path,
	magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_REMOVE_FLARE_FROM_PARENT, 0, 0, &tid, MAGMA_TERMINAL_TTL);

	ptr = magma_serialize_string(ptr, path);
	(void) ptr;

	magma_log_transaction(MAGMA_OP_TYPE_REMOVE_FLARE_FROM_PARENT, tid, peer);
	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_remove_flare_from_parent, response);

	return (tid);
}

void magma_pktqr_remove_flare_from_parent(gchar *buffer, magma_node_request *request) {
	gchar *ptr = buffer;

	ptr = magma_deserialize_string(ptr, request->body.remove_flare_from_parent.path);	/* the flare path */
}

void magma_pktas_remove_flare_from_parent(GSocket *socket, GSocketAddress *peer, int res, magma_transaction_id tid) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, res, 0, tid);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

void magma_pktar_remove_flare_from_parent(GSocket *socket, GSocketAddress *peer, magma_node_response *response) {
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	(void) ptr;
	// if (response->header.status isNot G_IO_STATUS_NORMAL || response->header.res is -1) return;
}

/**
 * Ping
 */
void magma_pktqs_heartbeat(GSocket *socket, GSocketAddress *peer, magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_HEARTBEAT, 0, 0, &tid, MAGMA_TERMINAL_TTL);

	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_heartbeat, response);
}

void magma_pktqr_heartbeat(gchar *buffer, magma_node_request *request)
{
	(void) buffer;
	(void) request;
}

void magma_pktas_heartbeat(GSocket *socket, GSocketAddress *peer, magma_volcano *v, magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, 0, 0, tid);

#if 0
	guint16 load = (int) 100.0 * v->load;
	ptr = magma_serialize_16(ptr, load);
#endif

	ptr = magma_serialize_32(ptr, v->bandwidth);
	ptr = magma_serialize_32(ptr, v->storage);
	ptr = magma_serialize_32(ptr, v->free_storage);

	ptr = magma_serialize_32(ptr, v->total_keys);
	ptr = magma_serialize_string(ptr, v->start_key);
	ptr = magma_serialize_string(ptr, v->stop_key);

	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

void magma_pktar_heartbeat(GSocket *socket, GSocketAddress *peer, magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);

	if (response->header.status isNot G_IO_STATUS_NORMAL || response->header.res is -1) return;

#if 0
	guint16 load;
	ptr = magma_deserialize_16(ptr, &load);
	response->body.heartbeat.volcano.load = 1.0 * load / 100.0;
#endif

	ptr = magma_deserialize_32(ptr, &(response->body.heartbeat.volcano.bandwidth));
	ptr = magma_deserialize_32(ptr, &(response->body.heartbeat.volcano.storage));
	ptr = magma_deserialize_32(ptr, &(response->body.heartbeat.volcano.free_storage));

	ptr = magma_deserialize_32(ptr, &(response->body.heartbeat.volcano.total_keys));
	ptr = magma_deserialize_string(ptr, response->body.heartbeat.volcano.start_key);
	ptr = magma_deserialize_string(ptr, response->body.heartbeat.volcano.stop_key);
}

/**
 * Network built
 */
void magma_pktqs_network_built(GSocket *socket, GSocketAddress *peer, magma_network_status status, magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_NETWORK_BUILT, 0, 0, &tid, MAGMA_TERMINAL_TTL);

	ptr = magma_serialize_8(ptr, status);

	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_network_built, response);
}

void magma_pktqr_network_built(gchar *buffer, magma_node_request *request)
{
	gchar *ptr = buffer;
	ptr = magma_deserialize_8(ptr, &request->body.network_built.status);
}

void magma_pktas_network_built(GSocket *socket, GSocketAddress *peer, magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, 0, 0, tid);
	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

void magma_pktar_network_built(GSocket *socket, GSocketAddress *peer, magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);
	(void) ptr;
}

/**
 * Shutdown
 */
void magma_pktqs_shutdown(GSocket *socket, GSocketAddress *peer, magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	magma_transaction_id tid = 0;
	gchar *ptr = magma_format_request_header(buffer, MAGMA_OP_TYPE_SHUTDOWN, 0, 0, &tid, MAGMA_TERMINAL_TTL);

	magma_send_and_receive(socket, peer, buffer, ptr - buffer, magma_pktar_shutdown, response);
}

void magma_pktqr_shutdown(gchar *buffer, magma_node_request *request)
{
	(void) buffer;
	(void) request;
}

void magma_pktas_shutdown(GSocket *socket, GSocketAddress *peer, magma_transaction_id tid)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_format_response_header(buffer, 0, 0, tid);
	magma_send_buffer(socket, peer, buffer, ptr - buffer);
}

void magma_pktar_shutdown(GSocket *socket, GSocketAddress *peer, magma_node_response *response)
{
	gchar buffer[MAGMA_MAX_BUFFER_SIZE];
	memset(buffer, 0, MAGMA_MAX_BUFFER_SIZE);

	gchar *ptr = magma_pktar(socket, peer, buffer, (magma_response *) response);
	(void) ptr;
}
