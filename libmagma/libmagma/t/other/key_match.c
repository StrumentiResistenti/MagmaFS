/*
   MAGMA -- key_match.c
   Copyright (C) 2006 Tx0 <tx0@strumentiresistenti.org>

   test routing function of MAGMA

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

#include "magma_test.h"

int main(int argc, char **argv)
{
	test_init(0);
	magma_node_t node;
	int included;

	if ( argc < 3 ) {
		fprintf(stderr, "\n\n  Usage: %s <start_key> <stop_key> <key_to_match>\n\n", argv[0]);
		return 1;
	}

	strcpy(node.start_key, argv[1]);
	strcpy(node.stop_key, argv[2]);

	included = magma_key_is_included(argv[3], &node);

	if ( included > 0 ) {
		printf("Key is located on a node following this one!\n\n");
	} else if ( included == 0 ) {
		printf("Key is located on this node!\n\n");
	} else {
		printf("Key is located on a node preceding this one!\n\n");
	}

	return 0;
}
