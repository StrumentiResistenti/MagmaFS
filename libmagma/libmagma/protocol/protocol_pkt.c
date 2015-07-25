/*
   MAGMA -- protocol_pkt.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Defines packets exchanged between clients and nodes and between nodes.

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

const int8_t MAGMA_DIR_IS_CLOSE = 0;
const int8_t MAGMA_DIR_IS_OPEN  = 1;

/* generic operation OK! */
const uint8_t MAGMA_OK = 0;

/* generic operation ERROR (internal server error */
const uint8_t MAGMA_ERROR = 1;

/* internal server error */
const uint8_t MAGMA_INTERNAL_ERROR = 2;

/* network operations exit code */
const uint8_t MAGMA_NODE_NAME_COLLISION = 10;
const uint8_t MAGMA_FQDN_NAME_COLLISION = 11;
const uint8_t MAGMA_IP_PORT_COLLISION = 12;
const uint8_t MAGMA_NONEXISTING_SHARE = 254;
const uint8_t MAGMA_CONNECTION_REFUSED = 255;

/* string explanation of network operations exit code */
char *network_ops_explained[256];
int network_ops_explained_init = 0;

const int16_t NO_MORE_NODES_TO_BE_SENT = 0;
const int16_t MORE_NODES_TO_BE_SENT = 1;

const int16_t PREVIOUS_NODE = 0;
const int16_t NEXT_NODE = 1;

/*
 * if an error occurred a byte with 1 is sent as header of the packet,
 * otherwise a zero is sent.
 */
const uint8_t magma_ok = 0;
const uint8_t magma_error = 1;

// vim:ts=4:nocindent:autoindent
