/*
   Magma test suite -- destroy_flare.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>
  
   Check for flare destruction.
  
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
#define FLAREPATH "/test_file3"
#define magma_erase_flare(x)

int main(int argc, char **argv)
{
	test_init(DEBUG_FLARE);

	magma_flare_t *flare = magma_new_dir_flare(FLAREPATH);
	if (flare == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Error creating regular file flare!");
		exit(1);
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "Flare %s created!", flare->hash);
	}
	magma_print_flare_cast(flare);

	/* test flare destruction */
	if (!magma_dispose_flare(flare)) {
		dbg(LOG_ERR, DEBUG_ERR, "Error destroying flare!");
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "OK!");
	}
	magma_erase_flare(flare);

	flare = magma_new_file_flare(FLAREPATH);
	if (flare == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Error creating directory flare!");
		exit(1);
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "Flare %s created!", flare->hash);
	}
	magma_print_flare_cast(flare);

	/* test flare destruction */
	if (!magma_dispose_flare(flare)) {
		dbg(LOG_ERR, DEBUG_ERR, "Error destroying flare!");
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "OK!");
	}
	magma_erase_flare(flare);

	flare = magma_new_blockdev_flare(FLAREPATH);
	if (flare == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Error creating block device flare!");
		exit(1);
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "Flare %s created!", flare->hash);
	}
	magma_print_flare_cast(flare);

	/* test flare destruction */
	if (!magma_dispose_flare(flare)) {
		dbg(LOG_ERR, DEBUG_ERR, "Error destroying flare!");
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "OK!");
	}
	magma_erase_flare(flare);

	flare = magma_new_chardev_flare(FLAREPATH);
	if (flare == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Error creating character device flare!");
		exit(1);
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "Flare %s created!", flare->hash);
	}
	magma_print_flare_cast(flare);

	/* test flare destruction */
	if (!magma_dispose_flare(flare)) {
		dbg(LOG_ERR, DEBUG_ERR, "Error destroying flare!");
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "OK!");
	}
	magma_erase_flare(flare);

	flare = magma_new_fifo_flare(FLAREPATH);
	if (flare == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Error creating FIFO flare!");
		exit(1);
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "Flare %s created!", flare->hash);
	}
	magma_print_flare_cast(flare);

	/* test flare destruction */
	if (!magma_dispose_flare(flare)) {
		dbg(LOG_ERR, DEBUG_ERR, "Error destroying flare!");
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "OK!");
	}
	magma_erase_flare(flare);

	flare = magma_new_symlink_flare(FLAREPATH);
	if (flare == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Error creating symlink flare!");
		exit(1);
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "Flare %s created!", flare->hash);
	}
	magma_print_flare_cast(flare);

	/* test flare destruction */
	if (!magma_dispose_flare(flare)) {
		dbg(LOG_ERR, DEBUG_ERR, "Error destroying flare!");
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "OK!");
	}
	magma_erase_flare(flare);

	return 0;
}

// vim:nocindent:ts=4
