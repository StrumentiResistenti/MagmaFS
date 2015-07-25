/*
   MAGMA -- utils.h
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Repository for useful facilities like path translation, dump to file
   and other debugging features.

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

#ifndef _MAGMA_UTILS_H
#define _MAGMA_UTILS_H

#include "magma_setup.h"

#include <endian.h>

#define MAGMA_MAX_WRITE_RETRY 16

/**
 * g_free() a symbol only if it's not NULL
 *
 * @param symbol the symbol to free
 * @return
 */
#define g_free_null(symbol) { if (symbol) {	g_free(symbol);	symbol = NULL; } }

extern char *magma_xlate_path(const char *path);

#ifdef MAGMA_ENABLE_DUMP_TO_FILE
extern int dump_fd, dump_in, dump_out;
extern FILE *fdump_in, *fdump_out;
extern void init_dump_to_file(const char *path);
#define dump_to_file(buf,size) if (dump_fd != -1) write(dump_fd,buf,size);

#define dump_in(buf,size) if (fdump_in != NULL) {\
	size_t c = 0;\
	char *bufp = buf;\
	fprintf(fdump_in, "New packet:\n");\
	for (c=0; c<size; c++) fprintf(fdump_in, "%.2x", bufp[c]);\
	fprintf(fdump_in, "\n");\
	fflush(fdump_in);\
}
#define dump_in_string(buf) if (fdump_in != NULL) fprintf(fdump_in, "%s", buf);

#define dump_out(buf,size) if (fdump_out != NULL) {\
	size_t c = 0;\
	char *bufp = buf;\
	fprintf(fdump_out, "New packet:\n");\
	for (c=0; c<size; c++) fprintf(fdump_out, "%.2x", bufp[c]);\
	fprintf(fdump_out, "\n");\
	fflush(fdump_out);\
}
#define dump_out_string(buf) if (fdump_out != NULL) fprintf(fdump_out, "%s", buf);
#endif /* MAGMA_ENABLE_DUMP_TO_FILE */

extern int magma_full_write(int fd, const void *buf, size_t size);
extern int magma_perfect_read(int fd, void *buf, size_t size);

extern char *get_regerror(int errcode, regex_t *compiled);

#define magma_full_write_string(fd,string) magma_full_write(fd,string,strlen(string)+1);

#if __BYTE_ORDER == __LITTLE_ENDIAN

extern uint64_t magma_htonll(uint64_t from_host);
extern uint64_t magma_ntohll(uint64_t from_net);

#else

#define magma_htonll(from_host) from_host
#define magma_ntohll(from_net) from_net

#endif


extern gid_t *magma_get_user_groups(uid_t uid);
extern void magma_clean_group_cache();

extern gchar *magma_get_peer_addr(GSocketAddress *peer);

extern void magma_log_stacktrace(int levels);

#endif /* _MAGMA_UTILS_H */

// vim:nocindent:autoindent:ts=4
