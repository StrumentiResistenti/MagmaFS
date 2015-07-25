/*
   Magma test suite -- key_val.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>
  
   Test key convertion to integer values
  
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

#include <string.h>
#include "magma_test.h"

int main(int argc, char **argv)
{
	test_init(0);

	if (argv[1] == NULL) return 1;
	fprintf(stderr, "Il carattere %c vale %d\n", argv[1][0], get_char_val(argv[1][0]));
	return 0;	
}

// vim:ts=4:nocindent
