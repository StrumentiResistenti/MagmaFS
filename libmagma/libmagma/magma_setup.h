/*
   MAGMA -- magma_setup.h
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Defines some macros and many includes

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

#ifndef _NET_LAYER_INCLUDE_GET_SOCKET
#define _NET_LAYER_INCLUDE_GET_SOCKET
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef __USE_UNIX98
#define __USE_UNIX98
#endif

#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif
#define _XOPEN_SOURCE 500

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#ifndef _REENTRANT
#define _REENTRANT
#endif

#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <stddef.h>
#include <utime.h>
#include <sys/vfs.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <sys/mman.h>
#include <netinet/tcp.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <math.h>
#include <signal.h>
#include <mcheck.h>
#include <execinfo.h>
#include <netdb.h>
#include <stdarg.h>
#include <regex.h>
#include <sys/resource.h>
#include <glib.h>
#include <gio/gio.h>
#include <errno.h>

#include <dbi/dbi.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
