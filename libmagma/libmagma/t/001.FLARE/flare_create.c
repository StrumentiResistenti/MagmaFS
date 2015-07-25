/*
   Magma test suite -- flare_create.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>
  
   Test some flare creation, saving and reload operations.
  
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

/*
 * Test if flare creation is properly done
 */

#include "../magma_test.h"

#define magma_reload_flare(flare)\
	fprintf(stderr, "reloading flare from disk...\n");\
	flare = magma_new_flare(FLAREPATH);\
	debug = DEBUG_FLARE|DEBUG_ERR;\
	if (magma_load_flare(flare)) {\
		fprintf(stderr, "flare reloaded correctly\n");\
		magma_print_flare_cast(flare);\
		free(flare);\
	} else {\
		fprintf(stderr, "error reloading flare: %s\n", strerror(errno));\
	}\
	debug = DEBUG_ERR;\
	fprintf(stderr, "\n");

int main(int argc, char **argv)
{
	test_init(0);

	magma_flare_t *flare;

	/* regular file */
#	define FLAREPATH "/test_file"
	fprintf(stderr, "creating new REGULAR FILE flare %s...\n", FLAREPATH);
	flare = magma_new_file_flare(FLAREPATH);
	assert(flare != NULL);
	fprintf(stderr, "new flare %s points to %s\n", FLAREPATH, flare->xlated);
	debug = DEBUG_FLARE|DEBUG_ERR;
	magma_print_flare_cast(flare);
	debug = DEBUG_ERR;
	magma_save_flare(flare);
	free(flare);

	magma_reload_flare(flare);

	/* directory */
#	undef FLAREPATH
#	define FLAREPATH "/test_dir"
	fprintf(stderr, "creating new DIRECTORY flare %s...\n", FLAREPATH);
	flare = magma_new_dir_flare(FLAREPATH);
	assert(flare != NULL);
	fprintf(stderr, "new flare %s points to %s\n", FLAREPATH, flare->xlated);
	debug = DEBUG_FLARE|DEBUG_ERR;
	magma_print_flare_cast(flare);
	debug = DEBUG_ERR;
	magma_save_flare(flare);
	free(flare);

	magma_reload_flare(flare);

	/* block device */
#	undef FLAREPATH
#	define FLAREPATH "/test_block"
	fprintf(stderr, "creating new BLOCK DEV flare %s...\n", FLAREPATH);
	flare = magma_new_blockdev_flare(FLAREPATH);
	assert(flare != NULL);
	fprintf(stderr, "new flare %s points to %s\n", FLAREPATH, flare->xlated);
	debug = DEBUG_FLARE|DEBUG_ERR;
	magma_print_flare_cast(flare);
	debug = DEBUG_ERR;
	magma_save_flare(flare);
	free(flare);

	magma_reload_flare(flare);

	/* char device */
#	undef FLAREPATH
#	define FLAREPATH "/test_char"
	fprintf(stderr, "creating new CHAR DEV flare %s...\n", FLAREPATH);
	flare = magma_new_chardev_flare(FLAREPATH);
	assert(flare != NULL);
	fprintf(stderr, "new flare %s points to %s\n", FLAREPATH, flare->xlated);
	debug = DEBUG_FLARE|DEBUG_ERR;
	magma_print_flare_cast(flare);
	debug = DEBUG_ERR;
	magma_save_flare(flare);
	free(flare);

	magma_reload_flare(flare);

	/* symlink */
#	undef FLAREPATH
#	define FLAREPATH "/test_symlink"
	fprintf(stderr, "creating new SYMLINK flare %s...\n", FLAREPATH);
	flare = magma_new_symlink_flare(FLAREPATH);
	assert(flare != NULL);
	fprintf(stderr, "new flare %s points to %s\n", FLAREPATH, flare->xlated);
	debug = DEBUG_FLARE|DEBUG_ERR;
	magma_print_flare_cast(flare);
	debug = DEBUG_ERR;
	magma_save_flare(flare);
	free(flare);

	magma_reload_flare(flare);

	/* FIFO */
#	undef FLAREPATH
#	define FLAREPATH "/test_fifo"
	fprintf(stderr, "creating new FIFO flare %s...\n", FLAREPATH);
	flare = magma_new_fifo_flare(FLAREPATH);
	assert(flare != NULL);
	fprintf(stderr, "new flare %s points to %s\n", FLAREPATH, flare->xlated);
	debug = DEBUG_FLARE|DEBUG_ERR;
	magma_print_flare_cast(flare);
	debug = DEBUG_ERR;
	magma_save_flare(flare);
	free(flare);

	magma_reload_flare(flare);

	return 0;
}

// vim:nocindent:ts=4
