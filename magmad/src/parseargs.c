/*
   MAGMA -- parseargs.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libmagma/magma.h>

/**
 * This is the structure that will hold the entire
 * environment of magmad
 */
magma_environment_t magma_environment;

/**
 * print an usage message, describing magmad command line options
 *
 * @param msg pointer to a string to be added at the end of usage() output
 */
void magma_usage(char *msg)
{
	fprintf(stderr, "\n  MAGMA v.0.1 (build date: %s)\n", __DATE__);
	fprintf(stderr, "  Distributed Hash Table based file system for Linux kernels\n");
	fprintf(stderr, "  (c) 2006-2013 Tx0 <tx0@strumentiresistenti.org>\n");
	fprintf(stderr, "  This is free software released under GPL\n");
	fprintf(stderr, "  THIS SOFTWARE IS PROVIDED AS IS!\n");
	fprintf(stderr, "\n  Usage: %s [OPTS] -d /path/\n\n", magma_environment.progname);
	fprintf(stderr, "    -h            Print this help message\n");
	fprintf(stderr, "    -b            Bootstrap a new lava network\n");
	fprintf(stderr, "    -D <MASK>     Set debug mask, see later\n");
	fprintf(stderr, "    -A            Set debug mask to maximum level (as -D32768)\n");
	fprintf(stderr, "    -p <PORTNUM>  Set server port (unlikely you'll change that)\n");
	fprintf(stderr, "    -i <IPADDR>   Local IP address (default INADDR_ANY)\n");
	fprintf(stderr, "    -n <STRING>   Local server nickname\n");
	fprintf(stderr, "    -s <DNSname>  Local server Fully Qualified DNS name\n");
	fprintf(stderr, "  * -d <PATH>     Path used as hash repository (defaults to %s)\n", MAGMA_DEFAULT_HASHPATH);
	fprintf(stderr, "    -w <NUM>      Available bandwidth in Kbit/s (defaults to %dKbit/s)\n", MAGMA_DEFAULT_BANDWIDTH);
	fprintf(stderr, "    -S <NUM>      Available storage in GB (defaults to %dGB)\n", MAGMA_DEFAULT_STORAGE);
	fprintf(stderr, "    -r <IP>       Remote boot server (if missing new Magmanet is created)\n");
	fprintf(stderr, "                  bootserver syntax is bootserver[:port]\n");
	fprintf(stderr, "  * -k <STRING>   Secret keyphrase used to join the net\n");
	fprintf(stderr, "    -l            Load last active status from disk (require -n)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  Debug mask can contain:\n\n");

	magma_print_log_levels();

	if (msg != NULL) fprintf(stderr, "\n\n  ERROR: %s\n", msg);
	fprintf(stderr, "\n\n");
	exit(0);
}

/**
 * magmad command line parsing code 
 *
 * @param argc number of command line arguments
 * @param argv vector of command line arguments
 */
void magma_parse_args(int argc, char **argv)
{
	memset(magma_environment.log, 0, 255);

	magma_environment.progname = *argv;						/* saving program name */
	magma_environment.port = MAGMA_PORT;					/* default port */
	magma_environment.servername = g_strdup("");			/* default server name */
	magma_environment.nickname = g_strdup("");				/* default server nickname */
	magma_environment.ipaddr = g_strdup("");				/* default server IP address */
	magma_environment.hashpath = g_strdup("");				/* default server path for local storage */
	magma_environment.bootserver = NULL;					/* default remote boot server */
	magma_environment.secretkey = NULL;
	magma_environment.bandwidth = MAGMA_DEFAULT_BANDWIDTH;	/* Declared bandwidth */
	magma_environment.storage = MAGMA_DEFAULT_STORAGE;		/* Declared storage */
	magma_environment.bootstrap = 0;						/* If true, this node should bootstrap a new network, if false this node should join an existing one */

	/*
	 * cycling through options
	 */
	char c;
	while ((c = getopt(argc, argv, "blhHA?D:Tp:i:n:s:d:w:r:k:" )) != -1) {
		switch (c) {
			case 'b':
				if (magma_environment.bootserver) {
					dbg(LOG_ERR, DEBUG_BOOT, "Bootstrap and remote server can't be specified together. Exiting now.");
					exit(1);
				}
				magma_environment.bootstrap = 1;
				break;
			case 'h':
			case 'H':
				magma_usage(NULL);
				break;
			case 'A':
				memset(magma_environment.log, 1, 255);
				dbg(LOG_INFO, DEBUG_BOOT, "Enabling all debug statements");
				break;
			case 'D':
				if (optarg) {
					magma_environment.logstring = g_strdup(optarg);
					dbg(LOG_INFO, DEBUG_BOOT, "Setting debug mask to %s", magma_environment.logstring);
				}
				break;
			case 'p':
				if (optarg) {
					magma_environment.port = atol(optarg);
					dbg(LOG_INFO, DEBUG_BOOT, "port: %d", magma_environment.port);
				}
				break;
			case 'i':
				if (optarg) {
					g_free_null(magma_environment.ipaddr);
					magma_environment.ipaddr = g_strdup(optarg);
					dbg(LOG_INFO, DEBUG_BOOT, "Server local IP addr: %s", magma_environment.ipaddr);
				}
				break;
			case 'n':
				if (optarg) {
					g_free_null(magma_environment.nickname);
					magma_environment.nickname = g_strndup(optarg, 255);
					dbg(LOG_INFO, DEBUG_BOOT, "Server Nickname: %s", magma_environment.nickname);
				}
				break;
			case 's':
				if (optarg) {
					g_free_null(magma_environment.servername);
					magma_environment.servername = g_strndup(optarg, 255);
					dbg(LOG_INFO, DEBUG_BOOT, "Server FQDN name: %s", magma_environment.servername);
				}
				break;
			case 'd':
				if (optarg) {
					g_free_null(magma_environment.hashpath);
					magma_environment.hashpath = g_strdup(optarg);
					if (magma_environment.hashpath[strlen(magma_environment.hashpath) - 1] == '/') {
						magma_environment.hashpath[strlen(magma_environment.hashpath) - 1] = '\0';
					}
					dbg(LOG_INFO, DEBUG_BOOT, "Hash Dir Path: %s", magma_environment.hashpath);
				}
				break;
			case 'w':
				if (optarg) {
					magma_environment.bandwidth = atol(optarg);
					dbg(LOG_INFO, DEBUG_BOOT, "Declared Bandwidth: %dKbit/s", magma_environment.bandwidth);
				}
				break;
			case 'r':
				if (optarg) {
					if (magma_environment.bootstrap) {
						dbg(LOG_ERR, DEBUG_BOOT, "Bootstrap and remote server can't be specified together. Exiting now.");
						exit(1);
					}
					g_free_null(magma_environment.bootserver);
					char *portp = NULL;
					if ((portp = rindex(optarg,':')) != NULL) {
						*portp = '\0';
						portp++;
						magma_environment.bootport = atoi(portp);
					} else {
						magma_environment.bootport = MAGMA_PORT;
					}
					magma_environment.bootserver = g_strdup(optarg);
					magma_environment.bootstrap = 0;
					dbg(LOG_INFO, DEBUG_BOOT, "Remote boot server: %s:%u", magma_environment.bootserver, magma_environment.bootport);
				}
				break;
			case 'k':
				if (optarg) {
					magma_environment.secretkey = g_strdup(optarg);
					dbg(LOG_INFO, DEBUG_BOOT, "Secret Key is [%s]", magma_environment.secretkey);
				}
				break;
			case '?':
				if (isprint(optopt)) {
					dbg(LOG_ERR, DEBUG_ERR, "Unknown option -%c", optopt);
					magma_usage(NULL);
				} else {
					dbg(LOG_INFO, DEBUG_BOOT, "Unknown option character \\x%x", optopt);
				}
				break;
			default:
				break;
		}
	}

	magma_expand_log_channels(&magma_environment);

	GResolver *resolver = g_resolver_get_default();

	/*
	 * try to replace default nickname with a valid one from the DNS
	 */
	if (!strlen(magma_environment.nickname)) {
		char *hostname = malloc(HOST_NAME_MAX);
		if (hostname != NULL) {
			if (gethostname(hostname, HOST_NAME_MAX) == 0) {
				g_free_null(magma_environment.nickname);
				magma_environment.nickname = g_strdup(hostname);
			}
			g_free_null(hostname);
		}
	}
	
	/*
	 * try to replace default server name with a valid one from the DNS
	 */
	if (!strlen(magma_environment.servername)) {
		/*
		struct hostent *h = gethostbyname(magma_environment.nickname);
		if (h != NULL) {
			g_free_null(magma_environment.servername);
			magma_environment.servername = g_strdup(h->h_name);
			if (index(magma_environment.servername, '.') == NULL) {
				// server name should be a fully qualified name!!!
				magma_usage("Server Fully Qualified Name is not Fully Qualified!");
			}
		}
		*/

		GList *addr = g_resolver_lookup_by_name(resolver, magma_environment.nickname, NULL, NULL);
		gchar *addr_string = g_list_nth_data(addr, 0);

		if (!addr_string) {
			gchar *string = g_strdup_printf(
				"No FQDN provided and magmad was unable to resolve one from %s",
				magma_environment.nickname);
			magma_usage(string);
		} else {
			if (!index(magma_environment.servername, '.')) {
				magma_usage("Server Fully Qualified Name is not Fully Qualified!");
			}
		}
	}
	
	/*
	 * Without a valid IP address the server can't start.
	 * Try to resolve magma_environment.servername and exit
	 * if the operation fails
	 */
	if (inet_aton(magma_environment.ipaddr, NULL) == 0) {
		GList *addr = g_resolver_lookup_by_name(resolver, magma_environment.servername, NULL, NULL);
		gchar *addr_string = g_list_nth_data(addr, 0);

		if (!addr_string) {
			gchar *string = g_strdup_printf("No IP address provided and can't resolve %s", magma_environment.servername);
			magma_usage(string);
		} else {
			magma_environment.ipaddr = addr_string;
		}
	}

	/*
	 * if neither boot server or bootstrap has been specified, the
	 * server should load its state from last saved lava topology
	 */
	if (!magma_environment.bootstrap && !magma_environment.bootserver) {
		if (!magma_environment.nickname || !strlen(magma_environment.nickname)) {
			magma_usage("Please specify server nickname to allow status loading from disk!");
		}
	} else if (magma_environment.secretkey == NULL ) {
		magma_usage("No secret key specified! Network can't be created or joined!");
	}

	g_object_unref(resolver);
}

// vim:ts=4:nocindent:autoindent
