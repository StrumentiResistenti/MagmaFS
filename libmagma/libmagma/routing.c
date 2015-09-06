/*
   MAGMA -- routing.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Define code for key routing.

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
#include "magma.h"

/**
 * check if key is hosted on node specified by *node
 *
 * if redundant_space is true, node->redundant_start_key
 * and node->redundant_stop_key are used instead of
 * main key space node->start_key and node->stop_key.
 *
 * @param key the key to be routed
 * @param node starting node
 * @param redundant_space if true, redundant key space is used
 * @return 0 if key is owned by this node, -1 if owned by previous one and +1 if owned by next one
 *
 * TODO ROUTING: this function should cope with split edges,
 *   that's when a node have the stop_key lesser than the
 *   start key, because it cross the ring limit
 */
int magma_key_is_included(const char *key, magma_volcano *node)
{
	g_assert(node isNot NULL);
	g_assert(node->node_name isNot NULL);
	g_assert(key isNot NULL);

	dbg(LOG_INFO, DEBUG_ROUTING, "Checking key %s on node %s", key, node->node_name);

	if (strcmp(key, node->start_key) < 0) return -1; // key falls before this node slice
	if (strcmp(key, node->stop_key)  > 0) return  1; // key falls after  this node slice
	return (0);                                      // key falls inside this node slice
}

/**
 * route a key in key space.
 *
 * @param key pointer to key to be routed
 * @param node pointer to node used for operation in this round
 * @param redundant_space if true, use redundant key space for routing
 * @return pointer to owner node, NULL in case of failure.
 */
magma_volcano *magma_route_key(const char *key, magma_volcano *node)
{
	if (node is NULL) return (NULL);

	int included = magma_key_is_included(key, node);

	if (included < 0) {
		dbg(LOG_INFO, DEBUG_ROUTING, "trying routing on previous node");
		return magma_route_key(key, magma_get_previous_node(node));
	} else if (included > 0) {
		dbg(LOG_INFO, DEBUG_ROUTING, "trying routing on next node");
		return magma_route_key(key, magma_get_next_node(node));
	}

	assert(node->node_name != NULL);
	assert(strlen(node->start_key) != 0);
	assert(strlen(node->stop_key) != 0);

	dbg(LOG_INFO, DEBUG_ROUTING, "Key is on %s", node->node_name);
	dbg(LOG_INFO, DEBUG_ROUTING, "  [%s", node->start_key);
	dbg(LOG_INFO, DEBUG_ROUTING, "   %s]", node->stop_key);

	return (node);
}

/**
 * route a file path in key space. never use this function directly.
 * use macros route_path() and redundant_route_path()
 *
 * @param path pointer to path to be routed
 * @return pointer to owner node, NULL in case of failure.
 */
magma_volcano *magma_route_path(const char *path)
{
	if (!path) {
		dbg(LOG_ERR, DEBUG_ERR, "magma_route_path called with NULL path");
		return (NULL);
	}

	unsigned char *hash = magma_sha1_data(path, strlen(path));
	if (!hash) {
		dbg(LOG_ERR, DEBUG_ERR, "magma_route_path: no hash calculated");
		return (NULL);
	}

	char *armoured = magma_armour_hash(hash);
	g_free(hash);

	assert(armoured isNot NULL);
	assert(lava isNot NULL);
	assert(lava->first_node isNot NULL);
	assert(lava->first_node->node_name isNot NULL);

	dbg(LOG_INFO, DEBUG_ROUTING, "starting routing on node %s", lava->first_node->node_name);

	magma_volcano *node = magma_route_key(armoured, lava->first_node);
	g_free(armoured);

	return (node);
}

/**
 * Compares two nodes to check if are the same.
 *
 * @param n1 first given node
 * @param n2 second given node
 * @return 1 if nodes are the same, 0 otherwise
 */
int magma_compare_nodes(const magma_volcano *n1, const magma_volcano *n2)
{
	if (!n1 || !n2) return 0;
	if (strcmp(n1->start_key, n2->start_key) != 0) return 0;
	if (strcmp(n1->stop_key,  n2->stop_key)  != 0) return 0;
	return 1;
}

// vim:ts=4:nocindent
