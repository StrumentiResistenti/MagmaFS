/*
   Magma test suite -- sha1.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>
  
   Perform tests on SHA1 keys
  
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include "../src/sha1.h"

int main(int argc, char **argv)
{
	unsigned char *hash;
	struct stat buf;
	int c, fd;

	for ( c = 1; c < argc; c++ ) {
		if ( ! stat(argv[c], &buf) ) {
			if ((fd = open(argv[c], 0)) > 1) {
				hash = sha1_fd(fd);
				close(fd);
			} else {
				printf("Can't open file %s\n", argv[c]);
				continue;
			}
		} else {
			hash = sha1_data(argv[c], strlen(argv[c]));
		}
		printf("%s  %s\n", armour_hash(hash), argv[c]);
	}
	return 0;
}
