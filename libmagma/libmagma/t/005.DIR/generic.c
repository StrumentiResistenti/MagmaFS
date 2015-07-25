/*
   Magma test suite -- directory.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>
  
   Check directory grow.
  
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
#define TESTDIR "/test_dir"

int main(int argc, char **argv)
{
	test_init(0);

	char *dirent, *direntp;
	magma_DIR_t *dirp;
	int c = 0;

	(void) argc;
	(void) argv;

	dirp = magma_opendir(0,0,TESTDIR);
	if (dirp == NULL) {
		magma_flare_t *f = magma_new_dir_flare(TESTDIR);
		if (f == NULL) {
			fprintf(stderr, "Error creating new dir!\n");
			exit(1);
		}
		if (!f->is_upcasted) {
			fprintf(stderr, "Error: directory not upcasted\n");
			exit(2);
		}
		magma_save_flare(f);
		dirp = magma_opendir(0,0,TESTDIR);
		if (dirp == NULL) {
			fprintf(stderr, "Error: can't open directory after flare creation!\n");
			exit(3);
		}
	}

	for (c=0; c<5; c++) {
		dirent = malloc(NAME_MAX);
		if (dirent != NULL) {
			sprintf(dirent, "file%d", c);
			magma_push_dirent(dirp, dirent);
		}
	}

	c = 0;

	/* printf("Directory contains %ld objects:\n", dirp->dir->entries); */

	dirp = magma_opendir(0,0,TESTDIR);
	if (dirp->dir->in_use) {
		printf("directory is in use (%d)\n", dirp->dir->in_use);
	}

	while ((direntp = magma_readdir(dirp)) ) {
		printf(" = %.3d. %s\n", c, direntp);
		c++;
	}
	printf("-----------------------------\n");

	if (dirp->dir->in_use) {
		printf("directory is in use (%d)\n", dirp->dir->in_use);
	}
	magma_closedir(dirp);
	if (!dirp->dir->in_use) {
		printf("directory is not in use (%d)\n", dirp->dir->in_use);
	}

	return 0;
}

// vim:nocindent:ts=4
