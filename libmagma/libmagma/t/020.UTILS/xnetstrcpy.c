/*
   Magma test suite -- xnetstrcpy.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>
  
   xnetstrcpy() get a new string from a stream connected to a socket.
	 stream in read until a '\0' char is found. xnetstrcpy do dynamic
	 allocation of target string (a netstrcpy() function without
	 dyn-allocation is also available).
  
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

#include <stdio.h>

#include "../../debug.h"
#include "../../utils.h"
#include "../../net_layer.h"

int main(int argc, char **argv)
{
	char *string, *ipaddr = "192.168.100.2", *getstring = "GET / HTTP/1.0\n\n";
	int res, s, c;
	struct sockaddr_in s_addr;

	(void) argc;

	if ( argv[1] != NULL )
		ipaddr = argv[1];

	fprintf(stderr, "IP address: %s\n", ipaddr);

	s_addr.sin_family = AF_INET;
	if ( inet_aton(ipaddr, &(s_addr.sin_addr)) == 0 ) {
		fprintf(stderr, "Remote IP %s is not valid", ipaddr);
		return 0;
	}
	s_addr.sin_port = htons(80);

	for (c=0; c<10; c++) {
		if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
			fprintf(stderr, "Error opening socket: %s", strerror(errno));
			exit(errno);
		}
	
		if ( connect(s, (struct sockaddr *) &s_addr, sizeof s_addr) == -1 ) {
			fprintf(stderr, "Error connecting: %s", strerror(errno));
			exit(errno);
		}

		send(s, getstring, strlen(getstring), 0);
		res = xnetstrcpy(s, &string, 1000);
		printf("\n\n ::::: Cycle %d. <%x> [%d]\n%s", c, (int) string, res, string);
	}

	return 0;
}
