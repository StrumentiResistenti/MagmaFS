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

#define DEFAULT_BUFFER 1024

#include "../protocol_pkt.h"

/**
 * holds informations about current session
 */
typedef struct {
	GSocketConnection *connection;	/** the incoming connection */
	GSocket *socket;				/** the socket associated to the connection */
    GSocketAddress *sockaddr;
    GInetAddress *addr;
    guint16 port;
    gchar *printable_addr;
    gint fd;
    GIOChannel *channel;
	gchar *working_path;			/** current working path */
	GInputStream *in;
	GOutputStream *out;
} magma_session_environment;

/**
 * console commands are implemented as a linked list of struct console_command
 * entries. each entry stores the regex pattern to be matched and a hook function
 * to be called.
 */
typedef struct console_command {
	/** hook function */
	void (*hook)(magma_session_environment *, char *, regmatch_t *);

	/** the regex pattern */
	char *pattern;

	/** compiled version of regex */
	regex_t *compiled;

	/** help message */
	char *help;

	/** next command pointer */
	struct console_command *next;
} magma_console_command;

extern gboolean magma_manage_console(GSocketService *service, GSocketConnection *connection, GObject *source, gpointer data);
