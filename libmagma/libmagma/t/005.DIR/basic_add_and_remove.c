/*
   Magma test suite -- basic_add_and_remove.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>
  
   Perform basic add and remove into a directory.
  
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

#define TESTDIR "/"

int main(int argc, char **argv)
{
	test_init(DEBUG_CACHE|DEBUG_ERR);
	srand(time(0));

	/* getting random number of entries to create */
	int entries_number = (int) ((double) rand() / (double) RAND_MAX * 100.0);
	if (argc > 1) {
		entries_number = atoi(argv[1]);
	}
	dbg(LOG_INFO, DEBUG_DIR, "We'll create %d entries in test directory", entries_number);

	magma_flare_t *flare_dir = magma_search_or_create(TESTDIR));
	if (flare_dir is NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "test directory is NULL!");
		exit(1);
	}
	if (!flare_dir->is_upcasted) {
		magma_cast_to_dir(flare_dir);
	}
	magma_save_flare(flare_dir);
	magma_load_flare(flare_dir);

	/* open DHT directory */
	magma_DIR_t *dirp = magma_opendir(0,0,TESTDIR);
	if (dirp == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "opening %s", TESTDIR);
		exit(2);
	}
	char name[NAME_MAX+1];
	for (; entries_number > 0; entries_number--) {
		sprintf(name, "test_entry_n._%d", entries_number);

		char *fullpath = g_strdup_printf("%s/%s", TESTDIR, name);

		magma_flare_t *f = magma_new_file_flare(fullpath);
		magma_save_flare(f);

		/* insert new dirent into directory */
		magma_push_dirent(dirp, name);
	}
	magma_closedir(dirp);
	// dbg(LOG_INFO, DEBUG_CACHE, "cache contains %llu / %u elements", magma_count_cache_elements(), entries_number);

	/* perform a full directory scan */
	dirp = magma_opendir(0,0,TESTDIR);
	char *de;
	while ((de = magma_readdir(dirp)) != NULL) {
		/* dbg(LOG_INFO, DEBUG_DIR, "Letto Il File: %s", de); */
	}
	magma_closedir(dirp);
	// dbg(LOG_INFO, DEBUG_CACHE, "cache contains %llu / %u elements", magma_count_cache_elements(), entries_number);

	/* then delete one entry every two */
	dirp = magma_opendir(0,0,TESTDIR);
	(void) magma_readdir(dirp); /* skipping . */
	(void) magma_readdir(dirp); /* skipping .. */
	while (magma_readdir(dirp) != NULL) {
		if ((de = magma_readdir(dirp)) != NULL) {
			magma_pop_dirent(dirp, de);
		}
	}
	magma_closedir(dirp);
	// dbg(LOG_INFO, DEBUG_CACHE, "cache contains %llu / %u elements", magma_count_cache_elements(), (int) (entries_number/2));

	/* perform a full directory scan */
	dirp = magma_opendir(0,0,TESTDIR);
	while ((de = magma_readdir(dirp)) != NULL) {
		/* dbg(LOG_INFO, DEBUG_DIR, "Letto Il File: %s", de); */
	}
	magma_closedir(dirp);

	/* refill directory with clone entries to test entries interleaving */
	dirp = magma_opendir(0,0,TESTDIR);
	de = malloc(NAME_MAX);
	int add_elements = 10;
	int c = 0;
	for ( c = 0; c < add_elements; c++ ) {
		sprintf(de, "Riempitivo_%.2d.txt",c);
		char *fullpath = g_strdup_printf("%s/%s", TESTDIR, de);
		magma_flare_t *f = magma_new_file_flare(fullpath);
		magma_save_flare(f);
		magma_push_dirent(dirp, de);
	}
	magma_closedir(dirp);
	dirp = magma_opendir(0,0,TESTDIR);
	for ( c = 0; c < add_elements; c++ ) {
		sprintf(de, "Short_%.2d.txt", c);
		char *fullpath = g_strdup_printf("%s/%s", TESTDIR, de);
		magma_flare_t *f = magma_new_file_flare(fullpath);
		magma_save_flare(f);
		magma_push_dirent(dirp, de);
	}
	magma_closedir(dirp);
	// dbg(LOG_INFO, DEBUG_CACHE, "cache contains %llu / %u elements", magma_count_cache_elements(), (int) (entries_number/2) + add_elements);

	/* perform a full directory scan */
	dirp = magma_opendir(0,0,TESTDIR);
	while ((de = magma_readdir(dirp)) != NULL) {
		dbg(LOG_INFO, DEBUG_DIR, "Letto Il File: %s", de);
	}
	magma_closedir(dirp);
	// dbg(LOG_INFO, DEBUG_CACHE, "cache contains %llu / %u elements", magma_count_cache_elements(), (int) (entries_number/2) + add_elements);

	return 0;
}

// vim:ts=4:nocindent:autoindent
