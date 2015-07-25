/*
   Magma test suite -- page_size_check.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>

   Test directory pages allocation to ensure that page allocation
   does not waste space on disk.

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

#define testpath "/testdir"

int main(int argc, char **argv)
{
	test_init(DEBUG_CACHE);

	/* creating directory flare... */
	magma_flare_t *testdir = magma_new_dir_flare(testpath);
	magma_save_flare(testdir);
	magma_DIR_t *dirp = magma_opendir(0,0,testpath);

	/* ... and filling it */
	char entry[255];
	int c = 0;
	while (c<1024) {
		sprintf(entry, "%.5d", c);
		magma_push_dirent(dirp, entry);

		/*
		 * computing actual page size
		 *
		 * why + 2? because there is a step in dir managing when
		 * directory is grown because of reached size limit and
		 * a new page is added. having 1 extra page is not a waste
		 * of size: soon or later will be anyway allocated. but
		 * having more than 1 extra page is!
		 */
		int psize = (c * (strlen(entry) + 1) / getpagesize() + 2) * getpagesize();
		struct stat st;
		if (stat(dirp->dir->contents, &st) != -1) {
			if (psize < st.st_size) {
				dbg(LOG_ERR, DEBUG_ERR, "dir should be %dbytes but is %ubytes", psize, (unsigned int) st.st_size);
				exit(1);
			}
		} else {
			dbg(LOG_ERR, DEBUG_ERR, "Can't check directory size by stat(): %s", strerror(errno));
			exit(2);
		}

		c++;
	}

	magma_closedir(dirp);
	dbg(LOG_INFO, DEBUG_DIR, "OK!");

	return 0;
}

// vim:ts=4:nocindent:autoindent:nowrap
