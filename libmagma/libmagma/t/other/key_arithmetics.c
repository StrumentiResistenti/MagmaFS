/*
   Magma test suite -- key_arithmetics.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>
  
   Perform tests on key arithmetics.
  
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
	char *dec, *inc;

	if (argc == 1) {
		fprintf(stderr, "\n\n  Usage: %s <key> [key_to_subtract]\n\n\n", argv[0]);
		exit(1);
	}

	dec = (char *) strdup(argv[1]);
	inc = (char *) strdup(argv[1]);

	if (decrement_key(dec) == 0) fprintf(stderr, "Can't decrement key: %s\n", dec);
	if (increment_key(inc) == 0) fprintf(stderr, "Can't increment key: %s\n", inc);

	printf("\n");
	printf(" increment:\n\n");
	printf("     %s +\n", argv[1]);
	printf("                                            1 =\n");
	printf("     ----------------------------------------  \n");
	printf("     %s\n", inc);
	printf("\n");
	printf(" decrement:\n\n");
	printf("     %s -\n", argv[1]);
	printf("                                            1 =\n");
	printf("     ----------------------------------------  \n");
	printf("     %s\n", dec);
	printf("\n");

	/*
	printf("    Decremented Key: %s\n", dec);
	printf("       Received Key: %s\n", argv[1]);
	printf("    Incremented Key: %s\n", inc);
	*/

	if (argc == 3) {
		char *diff = subtract_key(argv[1], argv[2]);
		printf(" subtraction:\n\n");
		printf("     %s -\n", argv[1]);
		printf("     %s =\n", argv[2]);
		printf("     ----------------------------------------  \n");
		printf("     %s\n", diff);
		printf("\n");

		/*
		printf("     Difference Key: %s\n", argv[2]);
		printf("         Difference: %s\n", diff);
		*/
	}

	return 0;
}

// vim:ts=4:nocindent:autoindent
