/*
   MAGMA -- magmad.h
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Parses server command line.

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

#ifndef _MAGMAD_H
#define _MAGMAD_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "../config.h"
#include <libmagma/magma.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

/* defines standard MAGMA network port */
#define MAGMA_VERSION PACKAGE_VERSION

#define DEFAULT_BUFFER 1024
#define DEFAULT_HASHPATH "/tmp/magma/"
#define DEFAULT_BANDWIDTH 56
#define DEFAULT_STORAGE 1
#define DEFAULT_NICKNAME "magmanode"
#define DEFAULT_SERVERNAME "localhost"
#define DEFAULT_IPADDR "127.0.0.1"

extern void magma_usage(char *msg);
extern void magma_parse_args(int argc, char **argv, magma_environment_t *magma_environment);

#endif // _MAGMAD_H

// vim:ts=4:nocindent
