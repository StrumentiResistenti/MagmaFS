/*
   MAGMA -- debug.h
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Some debug code.

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

#ifndef _MAGMA_DEBUG_H
#define _MAGMA_DEBUG_H

#define DEBUG_BOOT			'B'
#define DEBUG_BALANCER		'b'
#define DEBUG_CACHE			'c'
#define DEBUG_DIR			'd'
#define DEBUG_GARBAGE		'g'
#define DEBUG_PFUSE			'F'
#define DEBUG_FLARE_PROTO	'F'
#define DEBUG_FLARE			'f'
#define DEBUG_MALLOC		'm'
#define DEBUG_PNFS			'N'
#define DEBUG_NFS_PROTO		'N'
#define DEBUG_NET			'n'
#define DEBUG_NETWORK		'n'
#define DEBUG_ROUTING		'r'
#define DEBUG_SQL			's'
#define DEBUG_UTILS			'u'
#define DEBUG_PNODE			'V'
#define DEBUG_VULCANO_PROTO	'V'
#define DEBUG_VULCANO		'v'
#define DEBUG_MUTEX			'x'
#define DEBUG_ERR			'z'


extern void magma_print_log_levels();
extern gchar *magma_print_debug_mask();

/*
 * choosing debug reporting system.
 * mount.magma should always debug on syslog
 */
#if 0
#ifdef MAGMA_MOUNT_PROGRAM
#	ifdef _DEBUG_STDERR
#		undef _DEBUG_STDERR
#	endif
#	ifndef MAGMA_DEBUG_SYSLOG
#		define MAGMA_DEBUG_SYSLOG 1
#	endif
#else
#	ifndef MAGMA_DEBUG_SYSLOG
#		ifndef _DEBUG_STDERR
#			define MAGMA_DEBUG_SYSLOG 1
#		endif
#	endif
#endif
#endif

/** by default debug on syslog */
#ifndef MAGMA_DEBUG_SYSLOG
#	ifndef _DEBUG_STDERR
#		define MAGMA_DEBUG_SYSLOG 1
#	endif
#endif

extern char  magma_debug_levels[255];
extern char *magma_debug_prefix[255];

#if MAGMA_DEBUG_SYSLOG
#	include <syslog.h>
#	define dbg(facility, type, string,...) if (magma_environment.log[type] || DEBUG_ERR == type) \
		syslog(facility, "[%s] " string " [@%s:%d]", magma_environment.nickname ? magma_environment.nickname : "mount.magma", ##__VA_ARGS__, __FILE__, __LINE__)
#else
#	include <stdio.h>
#	define dbg(facility, type, string, ...) { \
		if (magma_environment.log[type]) { \
			fprintf(stderr, "[%s] %s ", magma_environment.nickname ? magma_environment.nickname : "mount.magma", magma_debug_prefix[type]); \
			fprintf(stderr, string, ## __VA_ARGS__); \
			fprintf(stderr," in %s() @%s:%d\n", __func__, __FILE__, __LINE__); }}

#endif

#define dbg_stack(facility, type, string,...) dbg(facility, type, string, ## __VA_ARGS__); magma_log_stacktrace(20)

extern void magma_init_log();
extern gchar *magma_print_log_mask(gchar levels[]);
extern void magma_expand_log_channels(magma_environment_t *magma_environment);

#ifndef MAGMA_DEBUG_MASK
#	define MAGMA_DEBUG_MASK
	extern uint16_t debug;
#endif

#endif /* _MAGMA_DEBUG_H */

#if 0
extern void magma_mutex_lock(GRecMutex *mutex);
extern void magma_mutex_unlock(GRecMutex *mutex);
#else
#define magma_mutex_lock(mutex) {\
	dbg(LOG_DEBUG, DEBUG_MUTEX, "locking mutex \"%s\" at %s:%d", __STRING(mutex), __FILE__, __LINE__);\
	g_rec_mutex_lock(mutex);\
	dbg(LOG_DEBUG, DEBUG_MUTEX, "mutex locked at %s:%d", __FILE__, __LINE__);\
}

#define magma_mutex_unlock(mutex) {\
	dbg(LOG_DEBUG, DEBUG_MUTEX, "unlocking mutex \"%s\" at %s:%d", __STRING(mutex), __FILE__, __LINE__);\
	g_rec_mutex_unlock(mutex);\
	dbg(LOG_DEBUG, DEBUG_MUTEX, "mutex unlocked at %s:%d", __FILE__, __LINE__);\
}
#endif

// vim:ts=4:nocindent
