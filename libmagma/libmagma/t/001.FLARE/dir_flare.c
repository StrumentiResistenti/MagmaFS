/*
   Magma test suite -- dir_flare.c
   Copyright (C) 2006-2007 Tx0 <tx0@strumentiresistenti.org>
  
   Create, save and reload a directory flare.
  
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

int main(int argc, char **argv)
{
	test_init(DEBUG_FLARE);

	/* directory */
#	define FLAREPATH "/test_dir"
	magma_flare_t *flare = magma_new_dir_flare(FLAREPATH);
	if (flare == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Error creating directory flare!");
		exit(1);
	} else {
		dbg(LOG_INFO, DEBUG_FLARE, "Flare %s created!", flare->hash);
	}
	magma_print_flare_cast(flare);
	magma_save_flare(flare);
	free(flare);

	flare = magma_new_flare(FLAREPATH);
	if (magma_load_flare(flare)) {
		magma_print_flare_cast(flare);
		free(flare);
	} else {
		dbg(LOG_ERR, DEBUG_ERR, "error reloading flare: %s", strerror(errno));
		exit(2);
	}

	dbg(LOG_INFO, DEBUG_FLARE, "OK!");
	return 0;
}

// vim:nocindent:ts=4
