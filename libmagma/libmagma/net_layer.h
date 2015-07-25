/*
   MAGMA -- net_layer.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Provide network basic operations like socket creation, data read()
   and send() using facilities which well behave in case of errors.

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

#ifndef _MAGMA_NET_LAYER_H
#define _MAGMA_NET_LAYER_H

#include "magma_setup.h"

/**
 * How many times must the wait cycle be executed?
 */
#define MAGMA_MAX_WAIT_CYCLE_ITERATIONS 16

/**
 * the retransmit timeout multiplied by the current retry gives an
 * incrementing timeout
 */
#define MAGMA_WAIT_CYCLE_UNIT 20 * 1000

/**
 * if set to TRUE, when a connection is closed, a MAGMA_OPTYPE_CLOSE_CONNECTION
 * message is sent.
 */
#define MAGMA_SEND_EXPLICIT_CLOSE_CONNECTION TRUE

/**
 * default magma port
 */
enum uint16_t {
	MAGMA_PORT			= 12000,
	MAGMA_NODE_PORT		= 12001,
	MAGMA_CONSOLE_PORT	= 12002,
} MAGMA_PORTS;

/**
 * A struct holding a connection environment
 */
typedef struct {
	GSocketConnection *connection;	/** the incoming connection */
	GSocketClient *client;
	GSocket *socket;				/** the socket associated to the connection */
    GSocketAddress *sockaddr;
    GInetAddress *addr;
    uint16_t port;
    gchar *printable_addr;
    gint fd;
    GIOChannel *channel;
} magma_connection;

extern void magma_init_net_layer();

extern GSocket *magma_open_client_connection(const gchar *host, const uint16_t port, GSocketAddress **peer);
extern void magma_close_client_connection(GSocket *socket, GSocketAddress *peer);
extern void close_connection(magma_connection *connection);

#if 0
#define MAGMA_BUFFER_SIZE sizeof(uint16_t)
#define FLUSH_CHANNEL TRUE
#define DONT_FLUSH_CHANNEL FALSE
#endif

#define MAGMA_MAX_BUFFER_SIZE 65507
#define MAGMA_READ_WRITE_BUFFER_SIZE 32768

extern GIOStatus magma_perfect_send(magma_connection *connection, const gchar *buffer, guint16 size, gboolean flush, const gchar *caption);
extern GIOStatus magma_send_buffer(GSocket *socket, GSocketAddress *peer, const gchar *buffer, guint16 size);

extern GIOStatus perfect_receive(magma_connection *connection, gchar *buffer, guint16 size, const gchar *caption);
extern GIOStatus magma_receive_buffer(GSocket *socket, GSocketAddress *peer, gchar *buffer, gsize max_size);

#endif /* _MAGMA_NET_LAYER_H */
