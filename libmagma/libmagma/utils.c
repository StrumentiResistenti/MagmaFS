/*
   MAGMA -- utils.c
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

#include "magma.h"

char *magma_debug_prefix[255];

/**
 * init log environment
 */
void magma_init_log()
{
    magma_debug_prefix['B'] = g_strdup("          BOOT:");
    magma_debug_prefix['b'] = g_strdup("      BALANCER:");
    magma_debug_prefix['c'] = g_strdup("         CACHE:");
    magma_debug_prefix['d'] = g_strdup("     DIRECTORY:");
    magma_debug_prefix['g'] = g_strdup("       GARBAGE:");
    magma_debug_prefix['F'] = g_strdup("   FLARE PROTO:");
    magma_debug_prefix['f'] = g_strdup("  FLARE SYSTEM:");
    magma_debug_prefix['m'] = g_strdup("        MALLOC:");
    magma_debug_prefix['N'] = g_strdup("     NFS PROTO:");
    magma_debug_prefix['n'] = g_strdup("       NETWORK:");
    magma_debug_prefix['r'] = g_strdup("       ROUTING:");
    magma_debug_prefix['u'] = g_strdup("         UTILS:");
    magma_debug_prefix['V'] = g_strdup(" VULCANO PROTO:");
    magma_debug_prefix['v'] = g_strdup("VULCANO SYSTEM:");
    magma_debug_prefix['x'] = g_strdup("         MUTEX:");
    magma_debug_prefix['z'] = g_strdup(" ------> ERROR:");
}

/**
 * returns a dynamic string with debug mask translated
 * into human readable strings
 */
gchar *magma_print_log_mask(gchar levels[])
{
	GString *s = g_string_sized_new(10240);

	if (levels['B']) g_string_append(s, "BOOT, ");
    if (levels['b']) g_string_append(s, "BALANCER, ");
    if (levels['c']) g_string_append(s, "CACHE, ");
    if (levels['d']) g_string_append(s, "DIRECTORY, ");
    if (levels['g']) g_string_append(s, "GARBAGE, ");
    if (levels['F']) g_string_append(s, "FLARE PROTO, ");
    if (levels['f']) g_string_append(s, "FLARE SYSTEM, ");
    if (levels['m']) g_string_append(s, "MALLOC, ");
    if (levels['N']) g_string_append(s, "NFS PROTO, ");
    if (levels['n']) g_string_append(s, "NETWORK, ");
    if (levels['r']) g_string_append(s, "ROUTING, ");
    if (levels['u']) g_string_append(s, "UTILS, ");
    if (levels['V']) g_string_append(s, "VULCANO PROTO, ");
    if (levels['v']) g_string_append(s, "VULCANO SYSTEM, ");
    if (levels['x']) g_string_append(s, "MUTEX, ");

    gchar *result = s->str;
    gchar *last_comma = rindex(result, ',');
    if (last_comma) *last_comma = '\0';

    g_string_free(s, FALSE);

    return (result);
}

/**
 * just print out which debug levels are enabled
 */
void magma_print_log_levels()
{
	fprintf(stderr, \
		"  B: BOOT           b: BALANCER        c: CACHE           d: DIR,\n"\
		"  g: GARBAGE        F: FLARE PROTO     f: FLARE SYS       m: MALLOC\n"\
		"  N: NFS PROTO      n: NETWORK         r: ROUTING         u: UTILS\n"\
		"  V: VULCANO PROTO  v: VULCANO         s: SQL             z: MISC");
}

/**
 * Translate the ->logstring field of the magma_enviroment_t struct
 * in the expanded for recorded in the ->log field of the same struct
 *
 * @param magma_environment_t a pointer to the magma_environment_t struct to be transformed
 */
void magma_expand_log_channels(magma_environment_t *magma_environment)
{
	if (!magma_environment->logstring) return;

	gchar *ptr = magma_environment->logstring;
	while (*ptr) {
		int index = (int) *ptr;
		magma_environment->log[index] = 1;
		ptr++;
	}
}

#if 0
void magma_mutex_lock(GRecMutex *mutex) {
	dbg(LOG_DEBUG, DEBUG_MUTEX, "locking mutex %s at %s:%d", __STRING(mutex), __FILE__, __LINE__);
	g_rec_mutex_lock(mutex);
	dbg(LOG_DEBUG, DEBUG_MUTEX, "mutex locked at %s:%d", __FILE__, __LINE__);
}

void magma_mutex_unlock(GRecMutex *mutex) {
	dbg(LOG_DEBUG, DEBUG_MUTEX, "unlocking mutex %s at %s:%d", __STRING(mutex), __FILE__, __LINE__);
	g_rec_mutex_unlock(mutex);
	dbg(LOG_DEBUG, DEBUG_MUTEX, "mutex unlocked at %s:%d", __FILE__, __LINE__);
}
#endif

/**
 * translate path prepending -d option
 *
 * @param @path the path to be translated
 */
char *magma_xlate_path(const char *path)
{
	gchar *xlated, *armoured;
	unsigned char *hash;

	assert(magma_environment.hashpath);
	assert(path);

#ifdef MAGMA_SIMPLIFY_PATHS
	dbg(LOG_INFO, DEBUG_UTILS, "simplifing path %s", path);
	char *normpath = magma_simplify_path(path);
	dbg(LOG_INFO, DEBUG_UTILS, "path simplified is %s", normpath);
	g_free(normpath);
#endif /* MAGMA_SIMPLIFY_PATHS */

	hash = sha1_data(path, strlen(path));
	armoured = armour_hash(hash);
	g_free(hash);
	
	xlated = g_strconcat(magma_environment.hashpath, "/", armoured, NULL);
	if (xlated == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't allocate memory for xlate_path");
	} else {
		dbg(LOG_INFO, DEBUG_UTILS, "xlated %s to %s", path, xlated);
	}

	g_free(armoured);
	return xlated;
}

#ifdef MAGMA_ENABLE_DUMP_TO_FILE

/* dump file location can be changed by other modules or programs */
int dump_fd = -1, dump_in = -1, dump_out = -1;
FILE *fdump_fd = NULL, *fdump_in = NULL, *fdump_out = NULL;
int enable_dump_to_file = 0;

void open_dump_file(int *fd, FILE **fds, const char *path)
{
	if ((*fd = open(path, O_WRONLY|O_APPEND|O_CREAT) ) == -1 ) {
		int open_errno = errno;
		dbg(LOG_ERR, DEBUG_ERR, "Can't open logfile %s: %s", path, strerror(open_errno));
	} else {
		dbg(LOG_INFO, DEBUG_UTILS, "Enabling dump to logfile %s", path);
		fchmod(*fd, S_IREAD|S_IWRITE);
		*fds = fdopen(*fd, "w");
	}
}

#define _DUMP_LOG_IN_EXTENSION "-in"
#define _DUMP_LOG_OUT_EXTENSION "-out"

void init_dump_to_file(const char *path)
{
	char *path_in = g_strdup(path);
	char *path_out = g_strdup(path);

	path_in = realloc(path_in, strlen(path) + strlen(_DUMP_LOG_IN_EXTENSION) + 1);
	path_out = realloc(path_out, strlen(path) + strlen(_DUMP_LOG_OUT_EXTENSION) + 1);

	strcat(path_in, _DUMP_LOG_IN_EXTENSION);
	strcat(path_out, _DUMP_LOG_OUT_EXTENSION);

	open_dump_file(&dump_fd,  &fdump_fd,  path);
	open_dump_file(&dump_in,  &fdump_in,  path_in);
	open_dump_file(&dump_out, &fdump_out, path_out);

	g_free(path_in);
	g_free(path_out);
}

#endif /* MAGMA_ENABLE_DUMP_TO_FILE */

/**
 * guarantee that all provided buffer will be written on filedescriptor
 *
 * @param @fd the filedescriptor
 * @param @buf the buffer to be written
 * @param @size buffer size to be written
 */
int magma_full_write(int fd, const void *buf, size_t size)
{
	size_t total = 0;
	int written = 0, count = 0;
	int xwrite_errno = -1;

	while (total < size) {
		written = write(fd, buf + total, size - total);
		if ( written is -1 ) {
			xwrite_errno = errno;
			dbg(LOG_ERR, DEBUG_ERR, "magma_full_write(%d, ...) error: %s", fd, strerror(errno));
			break;
		}
		total += written;
		if (count++ is MAGMA_MAX_WRITE_RETRY) {
			xwrite_errno = 70; /* Communication error on send! */
			dbg(LOG_ERR, DEBUG_ERR, "magma_full_write(%d, ...) max retry: %s", fd, strerror(xwrite_errno));
			break;
		}
	}

	if (written is -1) {
		errno = xwrite_errno;
		return -1;
	}
	return total;
}

/**
 * guarantee that all provided buffer will be read from filedescriptor
 *
 * @param @fd the filedescriptor
 * @param @buf the buffer to be read
 * @param @size buffer size to be read
 */
int magma_perfect_read(int fd, void *buf, size_t size)
{
	size_t total = 0;
	int bytes_read = 0, count = 0;
	int xread_errno = -1;

	while (total < size) {
		bytes_read = read(fd, buf + total, size - total);

		/* error reading */
		if (bytes_read is -1) {
			xread_errno = errno;
			dbg(LOG_ERR, DEBUG_ERR, "magma_perfect_read() error: %s", strerror(xread_errno));
			break;
		}

		/* end of file reached */
		if (bytes_read is 0) break;

		/* account read bytes */
		total += bytes_read;
		if (count++ is MAGMA_MAX_WRITE_RETRY) {
			xread_errno = 70; /* Communication error on send! */
			dbg(LOG_ERR, DEBUG_ERR, "magma_perfect_read() max retry: %s", strerror(xread_errno));
			break;
		}
	}

	if ( bytes_read is -1 ) {
		errno = xread_errno;
		return (-1);
	}
	return (total);
}

/*
 * stolen from glibc documentation
 */
char *get_regerror (int errcode, regex_t *compiled)
{
	size_t length = regerror (errcode, compiled, NULL, 0);
	char *buffer = malloc (length);
	if (buffer == NULL) {
		return NULL;
	}
	(void) regerror (errcode, compiled, buffer, length);
	return buffer;
}

/**
 * ntoh and hton functions for unsigned 64bit integers
 */
#if __BYTE_ORDER == __LITTLE_ENDIAN

typedef union net64int {
	uint64_t i64;
	uint32_t i32[2];
} net64int_t;

uint64_t magma_htonll(uint64_t from_host)
{
	net64int_t buf, bufres;

	buf.i64 = from_host;

	bufres.i32[0] = htonl(buf.i32[1]);
	bufres.i32[1] = htonl(buf.i32[0]);

	return bufres.i64;
}

uint64_t magma_ntohll(uint64_t from_net)
{
	net64int_t buf, bufres;	

	buf.i64 = from_net;

	bufres.i32[0] = ntohl(buf.i32[1]);
	bufres.i32[1] = ntohl(buf.i32[0]);

	return bufres.i64;
}

#else

#define magma_htonll(from_host) from_host
#define magma_ntohll(from_net) from_net

#endif

typedef struct group_cache {
	struct group_cache *next;
	gid_t *groups;
	uid_t uid;
} group_cache_t;

group_cache_t *group_cache = NULL;

GRecMutex group_cache_mutex;

/**
 * return a list of group id for userid uid
 *
 * @param @uid the UID of the user
 */
gid_t *magma_get_user_groups(uid_t uid)
{
	group_cache_t *group_pointer = group_cache;

	magma_mutex_lock(&group_cache_mutex);
	while (group_pointer != NULL) {
		if (group_pointer->uid == uid) {
			magma_mutex_unlock(&group_cache_mutex);
			dbg(LOG_INFO, DEBUG_UTILS, "Returning cache groups of uid %u", uid);
			return group_pointer->groups;
		}
		group_pointer = group_pointer->next;	
	}
	magma_mutex_unlock(&group_cache_mutex);

    #define GRPBUFLEN 4096

	struct group grp, *grpp;
	struct passwd pw, *pwp;
	char buf[GRPBUFLEN], *username = NULL;
	char bufdiag[GRPBUFLEN];

	/* getting username from userid */
	setpwent();
	while (!getpwent_r(&pw, buf, GRPBUFLEN, &pwp)) {
		if (pwp->pw_uid == uid) {
			username = g_strdup(pwp->pw_name);	
			break;
		}
	}
	endpwent();

	if (username == NULL)
		return NULL;

	/* allocating groups list */
	int gidx = 0;
	gid_t *groups = calloc(sizeof(gid_t), NGROUPS_MAX + 1);
	if (groups == NULL) return NULL;

	/* getting gids for this user */
	setgrent();
	memset(bufdiag,0,GRPBUFLEN);
	while (!getgrent_r(&grp, buf, GRPBUFLEN, &grpp)) {
		int i;
		for (i = 0; grpp->gr_mem[i] != NULL; i++) {
			if (strcmp(grpp->gr_mem[i], username) == 0) {
				groups[gidx] = grpp->gr_gid;
				char x[80];
				sprintf(x, "%u, ", grpp->gr_gid);
				strcat(bufdiag, x);
				gidx++;
				break;
			}
		}
		if (gidx >= NGROUPS_MAX)
			break;
	}
	endgrent();

	/* mark groups end */
	groups[gidx] = 0;
	groups[gidx+1] = 0;

	/* cache results */
	group_cache_t *new = calloc(sizeof(group_cache_t), 1);
	new->uid = uid;
	new->groups = groups;
	new->next = NULL;

	magma_mutex_lock(&group_cache_mutex);
	if (group_cache == NULL) {
		group_cache = new;
	} else {
		/* caching on top of cache list */
		new->next = group_cache;
		group_cache = new;
	}
	magma_mutex_unlock(&group_cache_mutex);

	dbg(LOG_INFO, DEBUG_UTILS, "Returning fresh groups of uid %u", uid);
	dbg(LOG_INFO, DEBUG_UTILS, "groups: %s", bufdiag);
	
	return groups;
}

void magma_clean_group_cache()
{
	group_cache_t *group_pointer = group_cache, *gnext = group_cache;

	magma_mutex_lock(&group_cache_mutex);
	group_cache = NULL;
	magma_mutex_unlock(&group_cache_mutex);

	while (gnext != NULL) {
		group_pointer = gnext;
		g_free(group_pointer->groups);
		g_free(group_pointer);
		gnext = group_pointer->next;
	}
}

gchar *magma_get_peer_addr(GSocketAddress *peer)
{
	GInetSocketAddress *_peer = G_INET_SOCKET_ADDRESS(peer);
	GInetAddress *addr = g_inet_socket_address_get_address(_peer);
	gchar *addr_string = g_inet_address_to_string(addr);
	return (addr_string);
}

/**
 * log a stack trace
 */
void magma_log_stacktrace(int levels)
{
	void *array[levels];
	size_t size = backtrace(array, levels);
	char **strings = backtrace_symbols(array, size);

	dbg(LOG_ERR, DEBUG_ERR, "Obtained %zd stack frames", size);

	size_t i;
	for (i = 0; i < size; i++)
		dbg(LOG_ERR, DEBUG_ERR, "[%.2lu] %s", size-i, strings[i]);

	free (strings);
}

// vim:ts=4:nocindent:autoindent
