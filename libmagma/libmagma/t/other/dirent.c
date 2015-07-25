/*
   Magma test suite -- dirent.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>
  
   Old dirent test, no longer useful, kept here just in case can be
   ported to Magma.
  
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

#include <dirent.h>
#include <stdio.h>

int main()
{
	struct dirent d;
	printf("   d_ino: %d\n", sizeof(d.d_ino));
	printf("   d_off: %d\n", sizeof(d.d_off));
	printf("d_reclen: %d\n", sizeof(d.d_reclen));
	printf("  d_type: %d\n", sizeof(d.d_type));
	printf("  d_name: %d\n", sizeof(d.d_name));
	printf("\n");
	printf("  struct: %d\n", sizeof(d));

	return 0;
}
