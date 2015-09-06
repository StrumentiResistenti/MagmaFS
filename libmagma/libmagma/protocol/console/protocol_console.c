/*
   MAGMA -- protocol_console.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Implements console protocol for management sessions

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

#include "../../magma.h"

// just to fool eclipse, I don't know why is needed...
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION ""
#endif

/**
 * The prompt of the magma console
 */
#define MAGMA_CONSOLE_PROMPT "\nMAGMA [%s]:%s> "

/**
 * Send a line
 */
#define magma_console_xsendline(env, pattern,...) {\
	gchar *line = g_strdup_printf(pattern, ## __VA_ARGS__);\
	if (line) {\
		magma_console_sendline(env, line);\
		g_free(line);\
	}\
}

/** mutex protecting usage of getpwent() */
GRecMutex passwd_mutex;

/** mutex protecting usage of getgrent() */
GRecMutex group_mutex;

/**
 * Send one line to remote clients
 */
void magma_console_sendline(magma_session_environment *env, gchar *line)
{
	g_socket_send(env->socket, line, strlen(line), NULL, NULL);
}

/**
 * Read one line from remote clients
 */
gchar *magma_console_readline(magma_session_environment *env)
{
	/*
	 * If the connection has been previously closed by a call to "quit",
	 * we should return FALSE here to detach the watch
	 */
	if (!env->connection) return (NULL);

	/*
	 * Read a GString from the socket
	 */
	GString *s = g_string_new(NULL);
	GError *error = NULL;
	GIOStatus ret = g_io_channel_read_line_string(env->channel, s, NULL, &error);

	if (ret is G_IO_STATUS_ERROR) {
		dbg(LOG_ERR, DEBUG_NET, "Error reading from console client: %s", error->message);
		g_error_free(error);
		g_object_unref(env->connection);
		g_string_free(s, TRUE);
		return (NULL);
	}

	/*
	 * Save the buffer for return and free the GString
	 */
	gchar *line = g_string_free(s, FALSE);

	/*
	 * Chomp \r and \n at the end of the line
	 */
	gchar *line_ptr = line;
	while (*line_ptr) {
		if (*line_ptr is '\r' || *line_ptr is '\n') {
			*line_ptr = '\0';
			break;
		}
		line_ptr++;
	}
	return (line);
}

/**
 * linked list of all registered commands 
 */
magma_console_command *command_list;

/**
 * add new command to available console commands. pattern is compiled and saved
 * with funtion pointer and function help message in the linked list.
 *
 * @param pattern regular expression pattern of the command
 * @param hook pointer to function code
 * @param help explaining message of this command
 * @return a pointer to added command, NULL if something failed
 */
magma_console_command *magma_console_add_hook(char *pattern, void (*hook)(magma_session_environment *, char *, regmatch_t *), char *help)
{
	magma_console_command *result = malloc(sizeof(magma_console_command));
	if (result is NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Command %s not registered due to malloc()ation failure", pattern);
		return NULL;
	}

	result->compiled = malloc(sizeof(regex_t));
	if (result->compiled is NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Command %s not registered due to malloc()ation failure", pattern);
		g_free(result);
		return NULL;
	}

	int cmpres = regcomp(result->compiled, pattern, REG_EXTENDED|REG_ICASE|REG_NEWLINE);
	if (cmpres != 0) {
		char *regerrmsg = get_regerror(cmpres, result->compiled);
		dbg(LOG_ERR, DEBUG_ERR, "Error compiling regexp %s: %s", pattern, regerrmsg);
		g_free(regerrmsg);
		g_free(result->compiled);
		g_free(result);
		return NULL;
	}

	result->pattern = g_strdup(pattern);
	result->hook = hook;
	result->next = NULL;	
	result->help = g_strdup(help);

	if (command_list is NULL) {
		command_list = result;
	} else {
		magma_console_command *cursor = command_list;
		while (cursor->next != NULL)
			cursor = cursor->next;
		cursor->next = result;
	}

	return result;
}

gchar *magma_console_transform_path(const gchar *path, magma_session_environment *env)
{
	char *realpath;

	if (path is NULL || strlen(path) is 0 || strcmp(path, ".") is 0) {
		realpath = g_strdup(env->working_path);
	} else if (strcmp(path, "..") is 0) {
		gchar **tokens = g_strsplit(env->working_path, "/", 256);
		guint size = g_strv_length(tokens);
		GString *result = g_string_sized_new(4192);

		guint i = 0;
		for (i = 0; i < size - 1; i++) {
			g_string_append(result, tokens[i]);
		}

		g_strfreev(tokens);
		realpath = g_string_free(result, FALSE);
	} else if (path[0] is '/') {
		realpath = g_strdup(path);
	} else {
		realpath = g_strconcat(env->working_path, "/", path, NULL);
	}

	gchar *simplified_path = magma_simplify_path(realpath);
	g_free(realpath);

	return simplified_path;
}

/**
 * fill a text buffer with the cast kind of given flare
 * 
 * @param flare given flare
 * @return pointer to cast string. result is dynamically
 * allocated and should be freed by outside code.
 */
gchar *magma_strcast(magma_flare_t *flare)
{
	gchar *buf = NULL;

	     if (magma_isreg(flare))  buf = g_strdup("regular file");
	else if (magma_isdir(flare))  buf = g_strdup("directory");
	else if (magma_isfifo(flare)) buf = g_strdup("FIFO");
	else if (magma_ischr(flare))  buf = g_strdup("character device");
	else if (magma_isblk(flare))  buf = g_strdup("block device");
	else if (magma_islnk(flare))  buf = g_strdup("symbolic link");
	else if (magma_issock(flare)) buf = g_strdup("socket");

	return buf;
}

gboolean magma_cache_traverser(gchar *key, magma_flare_t *flare, magma_session_environment *env)
{
	(void) key;

	gchar flarecast = '?';
	if (magma_isreg(flare))  flarecast = 'R'; else
	if (magma_isdir(flare))  flarecast = 'D'; else
	if (magma_isfifo(flare)) flarecast = 'F'; else
	if (magma_ischr(flare))  flarecast = 'C'; else
	if (magma_isblk(flare))  flarecast = 'B'; else
	if (magma_islnk(flare))  flarecast = 'L'; else
	if (magma_issock(flare)) flarecast = 'S';

	gchar *armoured = magma_armour_hash(flare->binhash);
	gchar *last_access = g_time_val_to_iso8601(&flare->last_access);
	gchar *line = g_strdup_printf("[%s] [in use: %s] [%c] %s\n", armoured, last_access, flarecast, flare->path);
	g_free(last_access);

	magma_console_sendline(env, line);

	g_free(armoured);
	g_free(line);

	return FALSE; // keep traversing...
}

/** console command that prints flare system cache contents ad a formatted tree */
void magma_console_print_cache(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) buffer;
	(void) matchptr;
	(void) env;

	g_tree_foreach(magma_cache_gtree, (GTraverseFunc) magma_cache_traverser, env);
}

void magma_console_print_debug(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) buffer;
	(void) matchptr;
	(void) env;

	magma_console_sendline(env, "\n");

	if (magma_environment.log['B']) magma_console_sendline(env, "  B [*] BOOT\n");
	else magma_console_sendline(env, "  B [ ] BOOT\n");

    if (magma_environment.log['b']) magma_console_sendline(env, "  b [*] BALANCER\n");
    else magma_console_sendline(env, "  b [ ] BALANCER\n");

    if (magma_environment.log['c']) magma_console_sendline(env, "  c [*] CACHE\n");
    else magma_console_sendline(env, "  c [ ] CACHE\n");

    if (magma_environment.log['d']) magma_console_sendline(env, "  d [*] DIRECTORY\n");
    else magma_console_sendline(env, "  d [ ] DIRECTORY\n");

    if (magma_environment.log['g']) magma_console_sendline(env, "  g [*] GARBAGE\n");
    else magma_console_sendline(env, "  g [ ] GARBAGE\n");

    if (magma_environment.log['F']) magma_console_sendline(env, "  F [*] FLARE PROTO\n");
    else magma_console_sendline(env, "  F [ ] FLARE PROTO\n");

    if (magma_environment.log['f']) magma_console_sendline(env, "  f [*] FLARE SYSTEM\n");
    else magma_console_sendline(env, "  f [ ] FLARE SYSTEM\n");

    if (magma_environment.log['m']) magma_console_sendline(env, "  m [*] MALLOC\n");
    else magma_console_sendline(env, "  m [ ] MALLOC\n");

    if (magma_environment.log['N']) magma_console_sendline(env, "  N [*] NFS PROTO\n");
    else magma_console_sendline(env, "  N [ ] NFS PROTO\n");

    if (magma_environment.log['n']) magma_console_sendline(env, "  n [*] NETWORK\n");
    else magma_console_sendline(env, "  n [ ] NETWORK\n");

    if (magma_environment.log['r']) magma_console_sendline(env, "  r [*] ROUTING\n");
    else magma_console_sendline(env, "  r [ ] ROUTING\n");

    if (magma_environment.log['u']) magma_console_sendline(env, "  u [*] UTILS\n");
    else magma_console_sendline(env, "  u [ ] UTILS\n");

    if (magma_environment.log['V']) magma_console_sendline(env, "  V [*] VULCANO PROTO\n");
    else magma_console_sendline(env, "  V [ ] VULCANO PROTO\n");

    if (magma_environment.log['v']) magma_console_sendline(env, "  v [*] VULCANO SYSTEM\n");
    else magma_console_sendline(env, "  v [ ] VULCANO SYSTEM\n");

    if (magma_environment.log['x']) magma_console_sendline(env, "  x [*] MUTEX\n");
    else magma_console_sendline(env, "  x [ ] MUTEX\n");
}

#define magma_printable_policy(policy) ('n' is policy) ? "do nothing" : ('r' is policy) ? "read" : "write"

/** print configured ACL */
void magma_console_print_acl(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) buffer;
	(void) matchptr;
	(void) env;

	gchar **paths = magma_acl_get_paths();
	gchar **path = paths;

	while (*path) {
		gchar *line = g_strdup_printf("  path: %s\n", *path);
		magma_console_sendline(env, line);
		g_free(line);

		gchar **netblocks = magma_acl_get_netblocks(*path);
		gchar **netblock = netblocks;

		while (*netblock) {
			gchar policy = magma_acl_get_policy(*path, *netblock);

			line = g_strdup_printf("    %s can %s\n", *netblock, magma_printable_policy(policy));
			magma_console_sendline(env, line);
			g_free(line);

			netblock++;
		}

		g_strfreev(netblocks);
		path++;
	}

	g_strfreev(paths);

	magma_console_sendline(env, "\n");
}

/** console command that shows all available commands */
void magma_console_print_help(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) buffer;
	(void) matchptr;
	(void) env;

	char nl = '\n';
	g_socket_send(env->socket, &nl, 1, NULL, NULL);

	magma_console_command *cursor = command_list;
	while (cursor != NULL) {
		magma_console_xsendline(env, "  %s\n", cursor->help);
		cursor = cursor->next;
	}
}

/** internally used by magma_console_ls() */
void magma_console_send_ls_entry(magma_session_environment *env, char *realpath, char *de)
{
	char *depath;
	depath = g_strconcat(realpath, "/", de, NULL);

	struct stat st;
	int getattr_res;
	if (g_strcmp0(de, ".") is 0) {
		getattr_res = magma_getattr(0, 0, realpath, &st);
	} else if (strcmp(de, "..") is 0) {
		char *tmp_path = g_strdup(realpath);
		char *r = rindex(tmp_path, '/');
		if (r != NULL) *r = '\0';
		if (strlen(tmp_path) is 0)
			strcpy(tmp_path, "/");
		getattr_res = magma_getattr(0, 0, tmp_path, &st);
		g_free(tmp_path);
	} else {
		getattr_res = magma_getattr(0, 0, depath, &st);
	}

	if (getattr_res is 0) {
		char meta[50];
		meta[0] = '\0';

		     if (S_ISREG(st.st_mode))  { strcat(meta, "-"); }
		else if (S_ISBLK(st.st_mode))  { strcat(meta, "b"); }
		else if (S_ISCHR(st.st_mode))  { strcat(meta, "c"); }
		else if (S_ISLNK(st.st_mode))  { strcat(meta, "l"); }
		else if (S_ISFIFO(st.st_mode)) { strcat(meta, "p"); }
		else                           { strcat(meta, "d"); }

		if (st.st_mode & S_IRUSR) { strcat(meta, "r"); } else { strcat(meta, "-"); }
		if (st.st_mode & S_IWUSR) { strcat(meta, "w"); } else { strcat(meta, "-"); }
		if (st.st_mode & S_ISUID) {
			if (st.st_mode & S_IXUSR) { strcat(meta, "s"); } else strcat(meta, "S");
		} else {
			if (st.st_mode & S_IXUSR) { strcat(meta, "x"); } else { strcat(meta, "-"); }
		}
		if (st.st_mode & S_IRGRP) { strcat(meta, "r"); } else { strcat(meta, "-"); }
		if (st.st_mode & S_IWGRP) { strcat(meta, "w"); } else { strcat(meta, "-"); }
		if (st.st_mode & S_ISGID) {
			if (st.st_mode & S_IXGRP) { strcat(meta, "s"); } else strcat(meta, "S");
		} else {
			if (st.st_mode & S_IXGRP) { strcat(meta, "x"); } else { strcat(meta, "-"); }
		}
		if (st.st_mode & S_IROTH) { strcat(meta, "r"); } else { strcat(meta, "-"); }
		if (st.st_mode & S_IWOTH) { strcat(meta, "w"); } else { strcat(meta, "-"); }
		if (st.st_mode & S_ISVTX) {
			if (st.st_mode & S_IXOTH) { strcat(meta, "t"); } else strcat(meta, "T");
		} else {
			if (st.st_mode & S_IXOTH) { strcat(meta, "x"); } else { strcat(meta, "-"); }
		}

		strcat(meta, " ");

		magma_mutex_lock(&passwd_mutex);
		struct passwd *pwent = NULL;
		while ((pwent = getpwent()) != NULL) {
			if (pwent->pw_uid is st.st_uid) {
				unsigned int c = 0;
				for (; c < 8 - strlen(pwent->pw_name); c++)
					strcat(meta, " ");
				strcat(meta, pwent->pw_name);
				break;
			}
			pwent = NULL;
		}
		endpwent();
		magma_mutex_unlock(&passwd_mutex);

		if (pwent is NULL) {
			char auid[6] = "xxxxxx";
			sprintf(auid, "%5u", st.st_uid);
			strcat(meta, auid);
		}

		strcat(meta, " ");

		magma_mutex_lock(&group_mutex);
		struct group *grent = NULL;
		while ((grent = getgrent()) != NULL) {
			if (grent->gr_gid is st.st_gid) {
				unsigned int c = 0;
				for (; c < 8 - strlen(grent->gr_name); c ++)
					strcat(meta, " ");
				strcat(meta, grent->gr_name);
				break;
			}
			grent = NULL;
		}
		endgrent();
		magma_mutex_unlock(&group_mutex);

		if (grent is NULL) {
			char agid[6] = "xxxxxx";
			sprintf(agid, "%5u", st.st_gid);
			strcat(meta, agid);
		}

		strcat(meta, " ");

		char asize[12] = "xxxxxxxxxxx";
#if SIZEOF_SIZE_T is 4
			sprintf(asize, "%11lu", (long unsigned int) st.st_size);
#else
			sprintf(asize, "%11llu", (long long unsigned int) st.st_size);
#endif
		strcat(meta, asize);

		magma_console_xsendline(env, "%s %s", meta, de);

		if (S_ISLNK(st.st_mode)) {
			char target[PATH_MAX];
			memset(target, 0, PATH_MAX);
			if (magma_readlink(0, 0, depath, target, PATH_MAX) is 0)
				magma_console_xsendline(env, " -> %s", target);
		}

		magma_console_sendline(env, "\n");
	} else {
		magma_console_xsendline(env, "%s\n", de);
	}

	g_free(depath);
}

/** console command that lists given directory flare contents */
void magma_console_ls(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	char *path = strndup(buffer + matchptr[1].rm_so, matchptr[1].rm_eo - matchptr[1].rm_so);

	gchar *realpath = magma_console_transform_path(path, env);	

	struct stat st;
	if (magma_getattr(0, 0, realpath, &st)) {
		if (errno is ENOENT) {
			magma_console_xsendline(env, "error opening %s: %s", path, strerror(errno));
			g_free(path);
			g_free(realpath);
			return;
		}
	}

	if (!S_ISDIR(st.st_mode)) {
		char *entry = rindex(realpath, '/');
		*entry = '\0';
		entry++;

		magma_console_send_ls_entry(env, realpath, entry);

		g_free(path);
		g_free(realpath);
		return;
	}

	magma_DIR_t *dir = magma_opendir(0, 0, realpath);
	if (!dir) {
		magma_console_xsendline(env, "error opening directory: %s", strerror(errno));
		g_free(path);
		g_free(realpath);
		return;
	}

	magma_console_sendline(env, "\n");

	unsigned int entries = 0;
	char *de;
	while ((de = magma_readdir(dir)) != NULL) {
		magma_console_send_ls_entry(env, realpath, de);
		entries++;
	}
	magma_closedir(dir);
	magma_console_xsendline(env, "\n -- total entries: %u\n", entries);

	g_free(path);
	g_free(realpath);
}

/** console command that prints current working path */
void magma_console_pwd(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) buffer;
	(void) matchptr;
	magma_console_xsendline(env, "%s\n", env->working_path);
}

/** console command that changes current working path */
void magma_console_console_cd(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	char *path = strndup(buffer + matchptr[1].rm_so, matchptr[1].rm_eo - matchptr[1].rm_so);

	gchar *realpath = magma_console_transform_path(path, env);	

	struct stat st;
	int res = magma_getattr(0, 0, realpath, &st);
	if (res is -1) {
		magma_console_xsendline(env, "Error doing lstat on path: %s!\n", strerror(errno));
		g_free(path);
		g_free(realpath);
		return;
	} else if (!S_ISDIR(st.st_mode)) {
		magma_console_xsendline(env, "Error: %s is not a directory!\n", path);
		g_free(path);
		g_free(realpath);
		return;
	}

	/* finally changing the path */
	g_free(env->working_path);
	env->working_path = g_strdup(realpath);

	g_free(path);
	g_free(realpath);
}

/** console command that prints given flare contents */
void magma_console_console_cat(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	char *path = strndup(buffer + matchptr[1].rm_so, matchptr[1].rm_eo - matchptr[1].rm_so);

	gchar *realpath = magma_console_transform_path(path, env);	

	gchar *buf = g_new0(gchar, 4096);

	struct stat st;
	int res = magma_getattr(0, 0, realpath, &st);
	if (res != 0) {
		sprintf(buf, "error getting directory informations: %s\n", strerror(errno));
		magma_console_sendline(env, buf);
		g_free(buf);
		g_free(path);
		g_free(realpath);
		return;
	}

	if (!S_ISREG(st.st_mode)) {
		sprintf(buf, "error: %s is not a regular file!\n", path);
		magma_console_sendline(env, buf);
		g_free(buf);
		g_free(path);
		g_free(realpath);
		return;
	}

	int read_bytes;
	off_t offset = 0;
	do {
		read_bytes = magma_read(0, 0, realpath, 4096, offset, buf);
		g_socket_send(env->socket, buf, read_bytes, NULL, NULL);
		offset += read_bytes;
	} while (read_bytes != 0);

	g_free(realpath);
	g_free(buf);
	g_free(path);
}

/**
 *  console command that print lava topology
 */
void magma_console_print_lava(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) buffer;
	(void) matchptr;
	(void) env;

	char buf[1024];
	sprintf(buf, "\nLava topology:\n\n");
	magma_console_sendline(env, buf);

	magma_volcano *n = lava->first_node;
	while (1) {
		if (!n) break; // safe condition

		sprintf(buf,
			"   node: %s (%s)\n"
		    "  alive: %s\n"
			"  start: %s\n"
			"   stop: %s\n"
			"   keys: %u\n"
			" bwidth: %u Kbit/s\n"
			"storage: %u / %u MByte\n\n",

			n->node_name, n->fqdn_name,
			n->alive ? "yes" : "no",
			n->start_key,
			n->stop_key,
			n->total_keys,
			n->bandwidth,
			n->free_storage, n->storage
		);
		magma_console_sendline(env, buf);
		n = n->next;

		if (n is lava->first_node) break; // exit condition
	}
}

void magma_console_enable_debug(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) env;
	gchar *level = buffer + matchptr[1].rm_so;
	magma_environment.log[(int) *level] = 1;
}

void magma_console_disable_debug(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) env;
	gchar *level = buffer + matchptr[1].rm_so;
	magma_environment.log[(int) *level] = 0;
}

/** console command that shows information about given flare */
void magma_console_inspect(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) env;

	char *path = strndup(buffer + matchptr[1].rm_so, matchptr[1].rm_eo - matchptr[1].rm_so);

	gchar *realpath = magma_console_transform_path(path, env);	

	magma_volcano *owner = magma_route_path(realpath);
	magma_volcano *red_owner = owner->next ? owner->next : lava->first_node;
	if (!magma_compare_nodes(owner, &myself) && !magma_compare_nodes(red_owner, &myself)) {
		magma_console_xsendline(env, "flare %s is located on another node\n", realpath);
		g_free(path);
		g_free(realpath);
		return;
	}

	magma_flare_t *flare = magma_search_or_create(realpath);
	if (!flare || !flare->is_upcasted) {
		magma_console_xsendline(env, "flare %s not found!\n", realpath);
		g_free(path);
		g_free(realpath);
		return;
	}

	magma_console_sendline(env, "\n");
	char *cast = magma_strcast(flare);
	owner = magma_route_key(flare->hash, lava->first_node);
	char *node = (owner != NULL) ? g_strdup(owner->node_name) : strdup("N/A");
	owner = owner->next ? owner->next : lava->first_node;
	char *rnode = (owner != NULL) ? g_strdup(owner->node_name) : strdup("N/A");

	magma_console_xsendline(env,
		"   flare: %s (%s, uid=%u, gid=%u, size=%lu)\n"
		"          %s on %s (%s)\n\n",
		flare->path, cast, flare->st.st_uid, flare->st.st_gid,
		flare->st.st_size,
		flare->hash, node, rnode);

	g_free(cast);
	g_free(node);
	g_free(rnode);

	owner = magma_route_key(flare->parent_hash, lava->first_node);
	node = (owner != NULL) ? g_strdup(owner->node_name) : strdup("N/A");
	owner = owner->next ? owner->next : lava->first_node;
	rnode = (owner != NULL) ? g_strdup(owner->node_name) : strdup("N/A");

	magma_flare_t *parent_flare = magma_search_or_create(flare->parent_path);
	if (parent_flare != NULL) {
		magma_console_xsendline(env,
			"  parent: %s (directory, uid=%u, gid=%u, size=%lu)\n"
			"          %s on %s (%s)\n",
			flare->parent_path, parent_flare->st.st_uid, parent_flare->st.st_gid,
			parent_flare->st.st_size,
			flare->parent_hash, node, rnode);
	} else {
		magma_console_xsendline(env,
			"  parent: %s (directory)\n"
			"          %s on %s (%s)\n",
			flare->parent_path, flare->parent_hash, node, rnode);
	}

	magma_dispose_flare(flare);
	magma_dispose_flare(parent_flare);

	g_free(node);
	g_free(rnode);
}

/** console command that erases a flare */
void magma_console_erase_flare(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) env;

	char *path = strndup(buffer + matchptr[1].rm_so, matchptr[1].rm_eo - matchptr[1].rm_so);

	gchar *realpath = magma_console_transform_path(path, env);	

	struct stat st;
	int res = magma_getattr(0, 0, realpath, &st);
	if (res is -1) {
		magma_console_xsendline(env, "Error doing lstat on %s: %s\n", path, strerror(errno));
		g_free(path);
		g_free(realpath);
		return;
	}

	if (S_ISDIR(st.st_mode)) {
		magma_console_xsendline(env, "Error: %s is a directory!\n", path);
		g_free(path);
		g_free(realpath);
		return;
	}

	res = magma_unlink(0, 0, MAGMA_DEFAULT_TTL, realpath);
	if (res != -1) {
		magma_console_xsendline(env, "Error: %s\n", strerror(errno));
	} else {
		magma_console_xsendline(env, "Ok: %s erased\n", path);
	}
	g_free(path);
	g_free(realpath);
	return;

	return;
}

#define MAGMA_CONFIRM_SHUTDOWN 0

/** console command that issues node shutdown */
void magma_console_server_shutdown(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) buffer;
	(void) matchptr;
	(void) env;

#if MAGMA_CONFIRM_SHUTDOWN
	magma_console_sendline(env, "Do you really want to shutdown magma? [y/N]\n");
	gchar *answer = magma_console_readline(env);

	if (g_ascii_strncasecmp(answer, "y", 1) is 0) {
#endif
		magma_console_sendline(env, "Shutting down magma... ");

		/*
		 * sends a shutdown request to other nodes
		 */
		magma_volcano *v = lava->first_node;
		while (1) {
			if (!v) break;

			if (magma_compare_nodes(v, &myself)) { // avoid shutting down myself
				v = v->next;
				if (v is lava->first_node) break;
			} else {
				GSocketAddress *peer;
				GSocket *socket = magma_open_client_connection(v->ip_addr, MAGMA_NODE_PORT, &peer);
				magma_node_response response;
				magma_pktqs_shutdown(socket, peer, &response);

				if (G_IO_STATUS_NORMAL is response.header.status) {
					v = v->next;
					if (v is lava->first_node) break;
				}
			}
		}

		magma_cleanup(0);
#if MAGMA_CONFIRM_SHUTDOWN
	}
#endif
}

/** console command that prints actual cache load */
void magma_console_cache_load(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) buffer;
	(void) matchptr;
	(void) env;

	magma_console_xsendline(env, "Cache contains %d flares.\n", g_tree_nnodes(magma_cache_gtree));
}

/** close current connection */
void magma_console_quit(magma_session_environment *env, char *buffer, regmatch_t *matchptr)
{
	(void) buffer;
	(void) matchptr;
	(void) env;

	magma_console_xsendline(env, "Closing console connection... good bye!\n");
	g_io_channel_shutdown(env->channel, TRUE, NULL);
	g_object_unref(env->socket);
	g_object_unref(env->connection);
	g_free(env);
}

/**
 * register console commands. called by flare_system_init().
 */
void magma_init_console()
{
	magma_console_add_hook("cache load",	magma_console_cache_load,		"    cache load: print number of flare actyally cached");
	magma_console_add_hook("cd (.+)",		magma_console_console_cd,		"     cd <path>: change current directory");
	magma_console_add_hook("cat (.+)",		magma_console_console_cat,		"    cat <path>: show content of file <path>");
	magma_console_add_hook("debug on ?(.)", magma_console_enable_debug,     "    debug on X: enable debug on channel X (see print debug)");
	magma_console_add_hook("debug off ?(.)",magma_console_disable_debug,    "   debug off X: enable debug on channel X (see print debug)");
	magma_console_add_hook("erase (.+)",	magma_console_erase_flare,		"  erase <path>: erase flare referred by <path>, only if not a directory");
	magma_console_add_hook("exit",			magma_console_quit,				"          exit: close current session");
	magma_console_add_hook("help",			magma_console_print_help,		"          help: print this message");
	magma_console_add_hook("inspect ?(.*)",	magma_console_inspect,			"inspect <path>: show available data on flare <path");
	magma_console_add_hook("debug on ?(.)", magma_console_enable_debug,     "    debug on X: enable debug on channel X (see print debug)");
	magma_console_add_hook("lava",			magma_console_print_lava,		"          lava: print network topology");
	magma_console_add_hook("ls *([^ ]*)",	magma_console_ls,				"     ls <path>: show <path> contents");
	magma_console_add_hook("print cache",	magma_console_print_cache,		"   print cache: cache state as a tree");
	magma_console_add_hook("print debug",   magma_console_print_debug,      "   print debug: show current debug mask");
	magma_console_add_hook("print acl",		magma_console_print_acl,		"     print acl: print established ACL");
	magma_console_add_hook("pwd",			magma_console_pwd,				"           pwd: print working directory");
	magma_console_add_hook("quit",			magma_console_quit,				"          quit: close current session");
	magma_console_add_hook("shutdown",		magma_console_server_shutdown,	"      shutdown: shutdown magma server");
}

/**
 * check a console command line on command list using regular expressions.
 * if a match is found, related function is executed.
 *
 * @param buffer pointer to command line string
 * @param s GSocket object connected to remote peer
 * @param env pointer to current session
 */
void magma_match_query(gchar *buffer, magma_session_environment *env)
{
	if (!buffer || !strlen(buffer)) return;

	regmatch_t *matchptr = malloc(10*sizeof(regmatch_t));
	if (matchptr is NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Error malloc() on regmatch_t array, aborting [%s]", buffer);
		return;
	}

	magma_console_command *cursor = command_list;
	while (cursor != NULL) {
		int mres = regexec(cursor->compiled, buffer, 10, matchptr, 0);
		if (mres is 0) {
			/*
			 * call the command hook
			 */
			(cursor->hook)(env, buffer, matchptr);
			g_free(matchptr);
			return;
		}
		cursor = cursor->next;
	}

	g_free(matchptr);

	magma_console_sendline(env, "Command unknown!\n");
	return;
}

gboolean magma_console_watch(GIOChannel *source, GIOCondition cond, gpointer data)
{
	(void) cond;
	(void) source;

	magma_session_environment *env = (magma_session_environment *) data;
	gchar out[DEFAULT_BUFFER];

	/*
	 * Read a line from the client
	 */
	gchar *in = magma_console_readline(env);
	if (!in) return (FALSE);

	/*
	 * Call matching command
	 */
	magma_match_query(in, env);
	if (!env->connection) return (FALSE);

	sprintf(out, MAGMA_CONSOLE_PROMPT, myself.node_name, env->working_path);
	magma_console_sendline(env, out);
	g_free(in);

	/* check if thread should shutdown */
	if (magma_network_shutdown is magma_environment.state) {
		strcpy(out, "Magma is shutting down, you will be force disconnected!\n");
		magma_console_sendline(env, out);
		g_io_channel_shutdown(env->channel, TRUE, NULL);
		return (FALSE);
	}

	return (TRUE);
}

/**
 * manage an incoming console connection.
 *
 * @param peer_socket network socket
 * @param peer remote peer IP address
 * @param peer_len size of *peer structure
 */
gboolean magma_manage_console(GSocketService *service, GSocketConnection *connection, GObject *source, gpointer data)
{
	(void) source;
	(void) service;
	(void) data;

	g_object_ref (connection);

	/*
	 * Setup the environment
	 */
	magma_session_environment *env = malloc(sizeof(magma_session_environment));
	if (!env) return (TRUE);

	env->working_path		= g_strdup("/");
	env->connection			= connection;
	env->socket				= g_socket_connection_get_socket(connection);
    env->sockaddr			= g_socket_connection_get_remote_address(connection, NULL);
    env->addr				= g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(env->sockaddr));
    env->port				= g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(env->sockaddr));
    env->printable_addr		= g_inet_address_to_string(env->addr);
    env->fd					= g_socket_get_fd(env->socket);
    env->channel			= g_io_channel_unix_new(env->fd);

    dbg(LOG_INFO, DEBUG_NET, "New Connection from %s:%d\n", env->printable_addr, env->port);

	// g_socket_set_blocking(env->socket, TRUE);

    /*
     * print out a welcome banner
     */
	gchar out[DEFAULT_BUFFER];
	sprintf(out, "\nWelcome to MAGMA version %s\n", MAGMA_VERSION);
	magma_console_sendline(env, out);
	sprintf(out, "This is server %s (%s)\n", myself.node_name, myself.ip_addr);
	magma_console_sendline(env, out);
	sprintf(out, "Type help [ENTER] for available commands.\n");
	magma_console_sendline(env, out);
	sprintf(out, MAGMA_CONSOLE_PROMPT, myself.node_name, env->working_path);
	magma_console_sendline(env, out);

    /*
     * Add a watch using proper callback
     */
	g_io_add_watch(env->channel, G_IO_IN|G_IO_HUP, (GIOFunc) magma_console_watch, env);

	return (TRUE);
}

// vim:ts=4:nocindent:autoindent
