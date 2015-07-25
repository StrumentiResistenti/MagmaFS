/*
   MAGMA -- magma.c
   Copyright (C) 2006-2015 Tx0 <tx0@strumentiresistenti.org>

   MAGMA is a userspace filesystem for Linux kernel based on FUSE
   API. Its goal is to build a transparent distributed filesystem
   based on distributed hashing which completely hides internals
   to host kernel.

   This is MAGMA node server main file.

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

#include "magmad.h"

#define MAGMA_START_BALANCER TRUE

gulong magma_flare_handler_id = 0;
gulong magma_console_handler_id = 0;

/**
 * Register the flare service port
 */
void create_service(gchar *address, uint16_t port, GCallback *callback, gulong *handler_id)
{
    GSocketService *service = g_socket_service_new();
    GInetAddress *inet_address = g_inet_address_new_from_string(address);
    GSocketAddress *socket_address = g_inet_socket_address_new(inet_address, port);

    g_socket_listener_add_address(
        G_SOCKET_LISTENER(service),
        socket_address,
        G_SOCKET_TYPE_STREAM,
        G_SOCKET_PROTOCOL_TCP,
        NULL, NULL, NULL);

    g_object_unref(socket_address);
    g_object_unref(inet_address);

	*handler_id = g_signal_connect(service, "incoming", G_CALLBACK(callback), NULL);

	g_socket_service_start(service);
	g_assert(g_socket_service_is_active(service));
}

/**
 * This is a debug symbol. Magmad should always use a thread
 * pool to answer to flare requests.
 */
#define MAGMAD_USE_FLARE_POOL TRUE
#define MAGMAD_USE_NODE_POOL  TRUE

/**
 * Open the FLARE protocol port and bind the
 * magma_manage_udp_flare_protocol callback to
 * manage incoming connections
 */
static void magma_open_flare_socket()
{
	uint16_t port = myself.port ? myself.port : MAGMA_PORT;

	magma_start_udp_service(
		"magma flare protocol",
		myself.ip_addr,
		port,
		magma_manage_udp_flare_protocol,
		MAGMAD_USE_FLARE_POOL ? (GFunc) magma_manage_udp_flare_protocol_pool : NULL);
}

/**
 * Open the NODE protocol port and bind the
 * magma_manage_udp_mpde_protocol callback to
 * manage incoming connections
 */
static void magma_open_node_socket()
{
	magma_start_udp_service(
		"magma node protocol",
		myself.ip_addr,
		MAGMA_NODE_PORT,
		magma_manage_udp_node_protocol,
		MAGMAD_USE_NODE_POOL ? (GFunc) magma_manage_udp_node_protocol_pool : NULL);
}

/**
 * Open the CONSOLE protocol port and bind the
 * magma_manage_console callback to manage
 * incoming connections
 */
static void magma_open_console_socket()
{
	create_service(
		myself.ip_addr,
		MAGMA_CONSOLE_PORT,
		(GCallback *) magma_manage_console,
		&magma_console_handler_id);
}

/*
 * Called just once at boot to save this node profile in "/.dht"
 */
gboolean magma_save_this_node(gpointer data) {
	(void) data;

	/*
	 * Write this node profile in the "/.dht" directory
	 */
	magma_save_node_profile();
	return (FALSE);
}

/**
 * magmad main function.
 *
 * @param argc number of arguments on command line
 * @param argv vector of arguments on command line
 * @return 0 on success, positive number on error
 */
int main(int argc, char **argv)
{
#if GLIB_MAJOR_VERSION < 2 || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 34)
	/* init GLib thread and type system */
	g_type_init();
#endif

	/* blank umask */
	umask(0);

#ifdef MAGMA_DEBUG_SYSLOG
	/* open syslog connection */
	openlog(magma_environment.progname, LOG_PID, LOG_DAEMON);
#endif

	/* init the log system */
	magma_init_log();

	/* allowing core dumps */
	struct rlimit rlim;
	rlim.rlim_cur = RLIM_INFINITY;
	rlim.rlim_max = RLIM_INFINITY;

	/* read command line */
	magma_parse_args(argc, argv, &magma_environment);

	/* setup process resources */
	int resout = setrlimit(RLIMIT_CORE, &rlim);
	if (-1 == resout) {
		dbg(LOG_ERR, DEBUG_ERR, "While setting core dump size: %s", strerror(errno));
	}

#ifdef MAGMA_ENABLE_DUMP_TO_FILE
	init_dump_to_file("/tmp/magmad.dump");
#endif /* MAGMA_ENABLE_DUMP_TO_FILE */

	/* print debug mask */
	char *dmask = magma_print_log_mask(magma_environment.log);
	dbg(LOG_INFO, DEBUG_BOOT, "Enabled log channels: %s", dmask);
	g_free_null(dmask);

	dbg(LOG_INFO, DEBUG_BOOT, "PID: %d", getpid());

	dbg(LOG_INFO, DEBUG_BOOT, " sizeof(ino_t) == %lu [expecting 8] [config.h says %d]", sizeof(ino_t), SIZEOF_INO_T);
	dbg(LOG_INFO, DEBUG_BOOT, "sizeof(size_t) == %lu [expecting 4] [config.h says %d]", sizeof(size_t), SIZEOF_SIZE_T);
	dbg(LOG_INFO, DEBUG_BOOT, " sizeof(dev_t) == %lu [expecting 8] [config.h says %d]", sizeof(dev_t), SIZEOF_DEV_T);
	dbg(LOG_INFO, DEBUG_BOOT, "sizeof(mode_t) == %lu [expecting 4] [config.h says %d]", sizeof(mode_t), SIZEOF_MODE_T);

	/*
	 * if debug mask includes DEBUG_MALLOC,
	 * memory debugging is enabled. ENV variable MALLOC_TRACE should
	 * contain the name of stack trace file to be later analyzed with
	 * mtrace command line utility. filename is set to failsafe value
	 * here but can be overwritten on program start
	 */
	if (magma_environment.log[DEBUG_MALLOC]) {
		gchar *tracefile = g_strdup_printf("/tmp/magmad.memorytrace.%s", magma_environment.nickname);
		if (getenv("MALLOC_TRACE") == NULL)
			setenv("MALLOC_TRACE", tracefile, 1);
		dbg(LOG_INFO, DEBUG_BOOT, "Enabling memory trace in file %s", getenv("MALLOC_TRACE"));
		mtrace();
		g_free_null(tracefile);
	}

	/* creates the first node representing this node */
	magma_config_myself(
		magma_environment.nickname,
		magma_environment.servername,
		magma_environment.ipaddr,
		magma_environment.port,
		magma_environment.bandwidth,
		magma_environment.storage,
		magma_environment.hashpath);

	dbg(LOG_INFO, DEBUG_BOOT, "Myself created as follow:");
	dbg(LOG_INFO, DEBUG_BOOT, "...ip address: %s", magma_environment.ipaddr);
	dbg(LOG_INFO, DEBUG_BOOT, "....node port: %d", magma_environment.port);
	dbg(LOG_INFO, DEBUG_BOOT, "....node name: %s", magma_environment.nickname);
	dbg(LOG_INFO, DEBUG_BOOT, "....fqdn name: %s", magma_environment.servername);
	dbg(LOG_INFO, DEBUG_BOOT, "....bandwidth: %d", magma_environment.bandwidth);
	dbg(LOG_INFO, DEBUG_BOOT, "......storage: %d", magma_environment.storage);

	/*
	 * initialize the flare system
	 */
	magma_flare_system_init();

	/*
	 * Enable node protocol
	 */
	magma_open_node_socket();

	/*
	 * Build the network
	 */
	magma_build_network(
		magma_environment.bootstrap,
		magma_environment.bootserver,
		MAGMA_NODE_PORT,
		MAGMA_START_BALANCER);

	dbg(LOG_INFO, DEBUG_BOOT, "lava ring created! first node is %s", lava->first_node->node_name);

	/*
	 * report on syslog about network
	 */
	magma_volcano *nodep = lava->first_node;
	dbg(LOG_INFO, DEBUG_BOOT, "MAGMA network is formed by %d nodes", lava->participants);
	do {
		dbg(LOG_INFO, DEBUG_BOOT, "Node %s has sector: ", nodep->node_name);
		dbg(LOG_INFO, DEBUG_BOOT, "  [%s", nodep->start_key);
		dbg(LOG_INFO, DEBUG_BOOT, "   %s]", nodep->stop_key);
		nodep = nodep->next;
	} while (nodep && nodep != lava->first_node);

#ifdef MAGMA_ENABLE_NFS_INTERFACE
	magma_start_nfs_interface();
#endif

	/*
	 * Enable console protocol
	 */
	magma_open_console_socket();

	/*
	 * Enable flare protocol TCP socket
	 */
	magma_open_flare_socket();

	/*
	 * Create the GLib main loop
	 */
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	g_timeout_add(2000, magma_save_this_node, NULL);

	/* start event cycle */
	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	dbg(LOG_INFO, DEBUG_BOOT, "terminating. Good bye ;-)");

	return(0);
}

// vim:ts=4:nocindent:autoindent
