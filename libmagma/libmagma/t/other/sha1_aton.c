/*
   Magma test suite -- sha1_aton.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>
  
   Test key convertion from strings to integer values
  
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
	magma_sha1_t *nkey1, *nkey2;
	char *result;

	if (argv[1] != NULL) {
		nkey1 = sha1_aton(argv[1]);
		fprintf(stderr, "Key %s is %.8lx%.8lx%.8lx%.8lx%.8lx\n", argv[1], nkey1->a, nkey1->b, nkey1->c, nkey1->d, nkey1->e);
	}
	if (argv[2] != NULL) {
		nkey2 = sha1_aton(argv[2]);
		fprintf(stderr, "Key %s is %.8lx%.8lx%.8lx%.8lx%.8lx\n", argv[2], nkey2->a, nkey2->b, nkey2->c, nkey2->d, nkey2->e);

		/*
		result = subtract_key(argv[1], argv[2]);
		fprintf(stderr, " ++> Difference is %s\n", result);
		*/

		result = key_difference_by_char(argv[1], argv[2]);
		fprintf(stderr, "                              ++> Difference is %s\n", result);
	}
	return 0;
}

// vim:nocindent:ts=4
