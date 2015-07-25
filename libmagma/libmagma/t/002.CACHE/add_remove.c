/*
   Magma test suite -- add_remove.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>

	 Add and remove from cache a list of paths, converting it to
	 flare before adding.

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

#include "../magma_test.h"

char paths[][255] = {
	"/usr/bin/arch",
	"/usr/bin/bash",
	"/usr/bin/cat",
	"/usr/bin/chgrp",
	"/usr/bin/chmod",
	"/usr/bin/chown",
	"/usr/bin/cp",
	"/usr/bin/cpio",
	"/usr/bin/csh",
	"/usr/bin/date",
	"/usr/bin/dd",
	"/usr/bin/df",
	"/usr/bin/dir",
	"/usr/bin/dmesg",
	"/usr/bin/dnsdomainname",
	"/usr/bin/echo",
	"/usr/bin/ed",
	"/usr/bin/egrep",
	"/usr/bin/false",
	"/usr/bin/fgconsole",
	"/usr/bin/fgrep",
	"/usr/bin/fuser",
	"/usr/bin/grep",
	"/usr/bin/gunzip",
	"/usr/bin/gzexe",
	"/usr/bin/gzip",
	"/usr/bin/hostname",
	"/usr/bin/ip",
	"/usr/bin/kernelversion",
	"/usr/bin/kill",
	"/usr/bin/ksh",
	"/usr/bin/ln",
	"/usr/bin/loadkeys",
	"/usr/bin/login",
	"/usr/bin/losetup",
	"/usr/bin/ls",
	"/usr/bin/lsmod",
	"/usr/bin/lsmod.modutils",
	"/usr/bin/lspci",
	"/usr/bin/mkdir",
	"/usr/bin/mknod",
	"/usr/bin/mktemp",
	"/usr/bin/more",
	"/usr/bin/mount",
	"/usr/bin/mount.old",
	"/usr/bin/mountpoint",
	"/usr/bin/mt",
	"/usr/bin/mt-gnu",
	"/usr/bin/mv",
	"/usr/bin/nano",
	"/usr/bin/netstat",
	"/usr/bin/pidof",
	"/usr/bin/ping",
	"/usr/bin/ps",
	"/usr/bin/pwd",
	"/usr/bin/rbash",
	"/usr/bin/readlink",
	"/usr/bin/rm",
	"/usr/bin/rmdir",
	"/usr/bin/run-parts",
	"/usr/bin/rzsh",
	"/usr/bin/sed",
	"/usr/bin/setpci",
	"/usr/bin/setserial",
	"/usr/bin/sh",
	"/usr/bin/sleep",
	"/usr/bin/stty",
	"/usr/bin/su",
	"/usr/bin/sync",
	"/usr/bin/tar",
	"/usr/bin/tcsh",
	"/usr/bin/tempfile",
	"/usr/bin/touch",
	"/usr/bin/true",
	"/usr/bin/umount",
	"/usr/bin/umount.old",
	"/usr/bin/uname",
	"/usr/bin/uncompress",
	"/usr/bin/vdir",
	"/usr/bin/zcat",
	"/usr/bin/zcmp",
	"/usr/bin/zdiff",
	"/usr/bin/zegrep",
	"/usr/bin/zfgrep",
	"/usr/bin/zforce",
	"/usr/bin/zgrep",
	"/usr/bin/zless",
	"/usr/bin/zmore",
	"/usr/bin/znew",
	"/usr/bin/zsh",
	"/usr/bin/zsh4",
	""
};

int main(int argc, char **argv)
{
	test_init(0);

	debug |= DEBUG_CACHE|DEBUG_MUTEX;

	fprintf(stderr, "Adding elements to cache...\n");

	magma_flare_t *f;
	int c = 0;
	while (strlen(paths[c]) > 0) {
		/* fprintf(stderr, "Creating flare %s\n", paths[c]); */
		f = magma_new_file_flare(paths[c]);
		/* fprintf(stderr, "Adding flare to cache\n"); */
		magma_add_to_cache(f);
		f->in_use++;
		c++;
	}

	// TODO change magma_cache->load with proper gtree call
	// fprintf(stderr, "\nCache contains %d elements of %d\n", magma_cache->load, c);
	fprintf(stderr, "Searching all elements back...\n\n");

	c = 0;
	while (strlen(paths[c]) > 0) {
		f = NULL;
		f = magma_search(paths[c]);
		if (f != NULL) {
			unlock_flare(f);
		} else {
			fprintf(stderr, "%s already removed.\n", paths[c]);
		}
		c++;
	}

	// TODO change magma_print_cache_node(magma_cache) with proper gtree call
	// magma_print_cache_node(magma_cache, "");

	fprintf(stderr, "\n\nDeleting all the cache, one by one...\n\n");
	c = 0;
	while (strlen(paths[c]) > 0) {
		f = NULL;
		f = magma_search(paths[c]);
		if (f != NULL) {
			unlock_flare(f);
			if (magma_remove_from_cache(f)) {
				fprintf(stderr, "\n\nRemoving %s\n\n", f->path);
				// TODO change magma_print_cache_node(magma_cache) with proper gtree call
				// magma_print_cache_node(magma_cache, "");
			} else {
				fprintf(stderr, "ERROR: flare %s not removed!\n", paths[c]);
				exit(2);
			}
		} else {
			fprintf(stderr, "%s already removed.\n", paths[c]);
		}
		c++;
	}

	// TODO change magma_print_cache_node(magma_cache) with proper gtree call
	// fprintf(stderr, "Cache claims to contain %d elements of %d while containing %lld\n", magma_cache->load, c, magma_count_cache_elements());
	// magma_print_cache_node(magma_cache, "");

	return 0;
}

// vim:ts=2:nowrap:nocindent:bg=dark
