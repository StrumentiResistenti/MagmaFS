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

#include "../../vulcano.h"
#include "../protocol_pkt.h"

extern const uint8_t MAGMA_OK;
extern const uint8_t MAGMA_ERROR;
extern const uint8_t MAGMA_INTERNAL_ERROR;		/* an internal error happened on this node */
extern const uint8_t MAGMA_NODE_NAME_COLLISION;	/* another node has same node_name */
extern const uint8_t MAGMA_FQDN_NAME_COLLISION;	/* another node has same fqdn_name */
extern const uint8_t MAGMA_IP_PORT_COLLISION; 	/* another node has same ip address + port pair */
extern const uint8_t MAGMA_NONEXISTING_SHARE; 	/* requested share doesn't exists */
extern const uint8_t MAGMA_CONNECTION_REFUSED; 	/* well... connection refused */

/**
 * Join network
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	char ip_addr[MAX_IP_LENGTH];			/* node IP address as printable string */
	guint16 port;							/* node port number */
	char node_name[MAX_HOSTNAME_LENGTH];	/* node nick name */
	char fqdn_name[MAX_HOSTNAME_LENGTH];	/* node fully qualified host name */
	guint16 bandwidth;						/* node bandwidth in Kb/s */
	guint32 storage;						/* node available storage in MB */
} magma_node_request_join_network;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar joining_start_key[SHA_READABLE_DIGEST_LENGTH];
	gchar joining_stop_key[SHA_READABLE_DIGEST_LENGTH];
} magma_node_response_join_network;

/**
 * Finish join network
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	guint32 number_of_nodes;

	gchar start_key[SHA_READABLE_DIGEST_LENGTH];
	gchar stop_key[SHA_READABLE_DIGEST_LENGTH];

	gchar joining_start_key[SHA_READABLE_DIGEST_LENGTH];
	gchar joining_stop_key[SHA_READABLE_DIGEST_LENGTH];

	char ip_addr[MAX_IP_LENGTH];			/* node IP address as printable string */
	guint16 port;							/* node port number */
	char node_name[MAX_HOSTNAME_LENGTH];	/* node nick name */
	char fqdn_name[MAX_HOSTNAME_LENGTH];	/* node fully qualified host name */
	guint16 bandwidth;						/* node bandwidth in Kb/s */
	guint32 storage;						/* node available storage in MB */
} magma_node_request_finish_join_network;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	guint32 number_of_nodes;
} magma_node_response_finish_join_network;

/**
 * Send node or whole topology. Both send node and send topology
 * requests return magma_node_response_send_node structs. But
 * the first just returns one of them, the second sends a copy
 * for each node and when the ring has been transmitted, the
 * last struct has more_nodes_waiting set to 0.
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gint8 direction;
	gchar start_key[SHA_READABLE_DIGEST_LENGTH];
} magma_node_request_send_node;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	guint8  more_nodes_waiting;
	guint16 port;
	guint16 bandwidth;
	guint16 storage;

	gchar ip_addr[MAX_IP_LENGTH];

	gchar fqdn_name[MAX_HOSTNAME_LENGTH];
	gchar node_name[MAX_HOSTNAME_LENGTH];

	gchar start_key[SHA_READABLE_DIGEST_LENGTH];
	gchar stop_key[SHA_READABLE_DIGEST_LENGTH];

	gchar redundant_start_key[SHA_READABLE_DIGEST_LENGTH];
	gchar redundant_stop_key[SHA_READABLE_DIGEST_LENGTH];
} magma_node_response_send_node;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	guint16 offset;
} magma_node_request_send_topology;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	guint16 port;
	gchar ip_addr[MAX_IP_LENGTH];
	gchar fqdn_name[MAX_HOSTNAME_LENGTH];
	gchar node_name[MAX_HOSTNAME_LENGTH];
	gchar start_key[SHA_READABLE_DIGEST_LENGTH];
	gchar stop_key[SHA_READABLE_DIGEST_LENGTH];
} magma_node_inside_topology;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	guint8 transmitted_nodes;
	guint8 more_nodes_waiting;
	magma_node_inside_topology nodes[255];
} magma_node_response_sent_topology;

/**
 * Exchange a key in the DHT key-space, one chunk at time
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	guint64 offset;
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
	guint16 size;
	gchar *chunk;
	magma_uid uid;
	magma_gid gid;
	magma_mode mode;
} magma_node_request_send_key;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	guint64 offset;
} magma_node_response_send_key;

/**
 * Node heartbeat
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
} magma_node_request_heartbeat;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_volcano volcano;
} magma_node_response_heartbeat;

/**
 * Network built
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	guint8 status;
} magma_node_request_network_built;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
} magma_node_response_network_built;

/**
 * Add flare to parent
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_node_request_add_flare_to_parent;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
} magma_node_response_add_flare_to_parent;

/**
 * Remove flare from parent
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gchar path[MAGMA_TERMINATED_PATH_LENGTH];
} magma_node_request_remove_flare_from_parent;

typedef struct MAGMA_PROTOCOL_ALIGNMENT {
} magma_node_response_remove_flare_from_parent;

/**
 * Node protocol request header
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_optype type;
	magma_transaction_id transaction_id;
	uid_t uid;
	gid_t gid;
} magma_node_request_header;

/**
 * Node protocol packet structure
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_node_request_header header;
	union {
		magma_node_request_join_network join_network;
		magma_node_request_finish_join_network finish_join_network;
		magma_node_request_send_node send_node;
		magma_node_request_send_topology send_topology;
		magma_node_request_send_key send_key;
		magma_node_request_network_built network_built;
		magma_node_request_add_flare_to_parent add_flare_to_parent;
		magma_node_request_add_flare_to_parent remove_flare_from_parent;
	} body;
} magma_node_request;

#if 0
/**
 * Node response header
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	gint16 res;
	gint16 err_no;
} magma_node_response_header;
#endif

/**
 * Node protocol response
 */
typedef struct MAGMA_PROTOCOL_ALIGNMENT {
	magma_response_header header;
	union {
		magma_node_response_join_network join_network;
		magma_node_response_finish_join_network finish_join_network;
		magma_node_response_send_node send_node;
		magma_node_response_sent_topology send_topology;
		magma_node_response_send_key send_key;
		magma_node_response_heartbeat heartbeat;
		magma_node_response_network_built network_built;
		magma_node_response_add_flare_to_parent add_flare_to_parent;
		magma_node_response_add_flare_to_parent remove_flare_from_parent;
	} body;
} magma_node_response;

extern gchar *magma_node_format_request_header(gchar *buffer, magma_optype type);
extern gchar *magma_node_format_response_header(gchar *buffer, gint32 res, gint16 err_no);

extern void magma_manage_udp_node_protocol(GSocket *socket, GSocketAddress *peer, gchar buffer[MAGMA_MESSAGE_MAX_SIZE]);
extern void magma_manage_udp_node_protocol_pool(magma_incoming_request *incoming, gpointer user_data);

extern gboolean magma_manage_node_protocol(GSocketService *service, GSocketConnection *connection, GObject *source, gpointer data);
extern char *magma_explain_network_error(uint8_t error);

/** Network join */
extern magma_transaction_id magma_pktqs_join_network(GSocket *socket, GSocketAddress *peer, magma_volcano *node, magma_node_response *response);
extern void magma_pktqr_join_network(gchar *buffer, magma_node_request *request);
extern void magma_pktas_join_network(GSocket *socket, GSocketAddress *peer, int res, magma_volcano *node, magma_transaction_id tid);
extern void magma_pktar_join_network(GSocket *socket, GSocketAddress *peer, magma_node_response *response);

/** Finish network join */
extern magma_transaction_id magma_pktqs_finish_join_network(GSocket *socket, GSocketAddress *peer, magma_volcano *myself, magma_volcano *other, guint32 number_of_nodes, magma_node_response *response);
extern void magma_pktqr_finish_join_network(gchar *buffer, magma_node_request *request);
extern void magma_pktas_finish_join_network(GSocket *socket, GSocketAddress *peer, int res, guint32 number_of_nodes, magma_transaction_id tid);
extern void magma_pktar_finish_join_network(GSocket *socket, GSocketAddress *peer, magma_node_response *response);

/** Transmit a key */
extern magma_transaction_id magma_pktqs_transmit_key(GSocket *socket, GSocketAddress *peer, off_t offset, size_t size, gchar *chunk, magma_flare_t *flare, magma_node_response *response);
extern void magma_pktqr_transmit_key(gchar *buffer, magma_node_request *request);
extern void magma_pktas_transmit_key(GSocket *socket, GSocketAddress *peer, int res, off_t offset, magma_transaction_id tid);
extern void magma_pktar_transmit_key(GSocket *socket, GSocketAddress *peer, magma_node_response *response);

/** Add a flare to its parent directory */
extern magma_transaction_id magma_pktqs_add_flare_to_parent(GSocket *socket, GSocketAddress *peer, const gchar *path, magma_node_response *response);
extern void magma_pktqr_add_flare_to_parent(gchar *buffer, magma_node_request *request);
extern void magma_pktas_add_flare_to_parent(GSocket *socket, GSocketAddress *peer, int res, magma_transaction_id tid);
extern void magma_pktar_add_flare_to_parent(GSocket *socket, GSocketAddress *peer, magma_node_response *response);

/** Remove a flare from its parent directory */
extern magma_transaction_id magma_pktqs_remove_flare_from_parent(GSocket *socket, GSocketAddress *peer, const gchar *path, magma_node_response *response);
extern void magma_pktqr_remove_flare_from_parent(gchar *buffer, magma_node_request *request);
extern void magma_pktas_remove_flare_from_parent(GSocket *socket, GSocketAddress *peer, int res, magma_transaction_id tid);
extern void magma_pktar_remove_flare_from_parent(GSocket *socket, GSocketAddress *peer, magma_node_response *response);

/**
 * Node profile exchange
 * Both transmit_node and transmit_topology request are answered
 * by a transmit_node response. The first with one single response,
 * the second with a sequence of responses controlled by the
 * more_nodes_waiting field of the magma_node_response_send_node
 * struct.
 * 
 */
extern magma_transaction_id magma_pktqs_transmit_node(GSocket *socket, GSocketAddress *peer, gchar *start_key, int direction);
extern void magma_pktqr_transmit_node(gchar *buffer, magma_node_request *request);

extern magma_transaction_id magma_pktqs_transmit_topology(GSocket *socket, GSocketAddress *peer, guint16 offset, magma_node_response *response);
extern void magma_pktqr_transmit_topology(gchar *buffer, magma_node_request *request);
extern void magma_pktas_transmit_topology(GSocket *socket, GSocketAddress *peer, magma_lava *lava, int first_node, magma_transaction_id tid);
extern void magma_pktar_transmit_topology(GSocket *socket, GSocketAddress *peer, magma_node_response *response);

extern void magma_pktas_transmit_node(GSocket *socket, GSocketAddress *peer, magma_volcano *node, int more_nodes_waiting, magma_transaction_id tid);
extern void magma_pktar_transmit_node(GSocket *socket, GSocketAddress *peer, magma_node_response *response);

/**
 * Heartbeat
 */
extern void magma_pktqs_heartbeat(GSocket *socket, GSocketAddress *peer, magma_node_response *response);
extern void magma_pktqr_heartbeat(gchar *buffer, magma_node_request *request);
extern void magma_pktas_heartbeat(GSocket *socket, GSocketAddress *peer, magma_volcano *v, magma_transaction_id tid);
extern void magma_pktar_heartbeat(GSocket *socket, GSocketAddress *peer, magma_node_response *response);

/**
 * Network built
 */
extern void magma_pktqs_network_built(GSocket *socket, GSocketAddress *peer, magma_network_status status, magma_node_response *response);
extern void magma_pktqr_network_built(gchar *buffer, magma_node_request *request);
extern void magma_pktas_network_built(GSocket *socket, GSocketAddress *peer, magma_transaction_id tid);
extern void magma_pktar_network_built(GSocket *socket, GSocketAddress *peer, magma_node_response *response);

/**
 * Shutdown
 */
extern void magma_pktqs_shutdown(GSocket *socket, GSocketAddress *peer, magma_node_response *response);
extern void magma_pktqr_shutdown(gchar *buffer, magma_node_request *request);
extern void magma_pktas_shutdown(GSocket *socket, GSocketAddress *peer, magma_transaction_id tid);
extern void magma_pktar_shutdown(GSocket *socket, GSocketAddress *peer, magma_node_response *response);

