/*
   Magma tools -- ls.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Accept a path as first command line argument and load corresponding
   flare. If it's a directory, perform a directory listing.

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

#define MAGMA_DEBUG_STDERR 1
#include <magma/magma.h>

int main(int argc, char **argv)
{
	magma_environment.debug = DEBUG_ERR;

	if (argc < 3) {
		dbg(LOG_ERR, DEBUG_ERR, "Usage: %s <repository> <path>", argv[0]);
		exit(1);
	}

	/* copy repository path */
	magma_environment.hashpath = g_strdup(argv[1]);
	struct stat st;
	if (stat(magma_environment.hashpath, &st) == -1) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't open %s: %s", magma_environment.hashpath, strerror(errno));
		exit(1);
	}

	/* copy flare path */
	char *path = g_strdup(argv[2]);

	magma_config_myself(g_strdup("x"),strdup("x"),strdup("127.0.0.1"),12000,1,1);
	magma_flare_system_init(1,NULL,0,0);

	/* create uncasted flare */
	magma_flare_t *flare = magma_new_flare(path);
	if (flare == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Error creating uncasted flare!");
		exit(1);
	}

	/* check flare existance */
	if (!magma_check_flare(flare)) {
		dbg(LOG_ERR, DEBUG_ERR, "Flare %s does not exist!", flare->path);
		exit(1);
	}

	/* load flare from disk */
	if (!magma_load_flare(flare)) {
		dbg(LOG_ERR, DEBUG_ERR, "Error loading flare from disk!");
		exit(4);
	}

	/* is a directory? */
	if (!magma_isdir(flare)) {
		dbg(LOG_ERR, DEBUG_ERR, "Flare %s is not a directory", flare->path);
		exit(5);
	}

	/* open directory */
	magma_DIR_t *dirp = magma_opendir(0,0,flare->path);
	if (dirp == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't open magma directory %s", flare->path);
		exit(6);
	}

	/* read directory contents */
	char *de;
	while ((de = magma_readdir(dirp)) != NULL) {
		printf("%s\n", de);	
	}

	magma_closedir(dirp);
	exit(0);

	if (stat(path, &st) == -1) {
		dbg(LOG_ERR, DEBUG_ERR, "can't access %s: %s", path, strerror(errno));
		exit(2);
	}

	if (!S_ISDIR(st.st_mode)) {
		dbg(LOG_ERR, DEBUG_ERR, "element is not a directory!");
		exit(3);
	}

	char *contents, *metadata;
	contents = g_strconcat(path, "/contents", NULL);
	metadata = g_strconcat(path, "/metadata", NULL);

	if (stat(contents, &st) == -1) {
		dbg(LOG_ERR, DEBUG_ERR, "can't stat %s: %s", contents, strerror(errno));
		exit(5);
	}

	if (stat(metadata, &st) == -1) {
		dbg(LOG_ERR, DEBUG_ERR, "can't stat %s: %s", metadata, strerror(errno));
		exit(5);
	}

	int fd;
	char repname[1024];
	if ((fd = open(metadata, O_RDONLY)) != -1) {
		if (read(fd, &st, sizeof(struct stat)) != sizeof(struct stat)) {
			dbg(LOG_ERR, DEBUG_ERR, "Short read on struct stat");
			exit(6);
		}
		if (read(fd, repname, 1024) != 1024) {
			dbg(LOG_ERR, DEBUG_ERR, "Short read on repname");
			exit(6);
		}
		close(fd);
	} else {
		dbg(LOG_ERR, DEBUG_ERR, "can't recover struct stat and entry path!");
		exit(6);
	}

	if (!S_ISDIR(st.st_mode)) {
		dbg(LOG_ERR, DEBUG_ERR, "requested flare is not a directory!");
		exit(0);
	}

	magma_check_filesystem();

	dirp = magma_opendir(0,0,repname);
	if (dirp == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't open repository");
		exit(7);
	}

	dbg(LOG_INFO, DEBUG_DIR, "scanning directory %s", repname);

	while ((de = magma_readdir(dirp)) != NULL) {
		dbg(LOG_INFO, DEBUG_DIR, "%s", de);		
	}

	dbg(LOG_INFO, DEBUG_DIR, "That's all folks!");

	magma_closedir(dirp);

	return 0;
}

// vim:ts=4:nocindent:autoindent:nowrap
