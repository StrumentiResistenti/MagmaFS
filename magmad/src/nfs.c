/*
   MAGMA -- nfs.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   Magma NFS interface

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

#ifdef MAGMA_ENABLE_NFS_INTERFACE
#define unset_rpcservices() {\
	pmap_unset (MOUNTPROG, MOUNTVERS);\
	pmap_unset (NFS_PROGRAM, NFS_VERSION);\
}

SVCXPRT *tcp_mount;	/**< Mount protocol TCP oriented RPC handle */
SVCXPRT *udp_mount;	/**< Mount protocol UDP oriented RPC handle */
SVCXPRT *tcp_nfs;	/**< NFS protocol TCP oriented RPC handle */
SVCXPRT *udp_nfs;	/**< NFS protocol UDP oriented handle */

/**
 * install RPC hooks for mount protocol
 *
 * @param arg ignored. Declared only for compliance with GLib thread prototype
 * \return NULL. Can be safely ignored.
 */
gpointer magma_start_mount(gpointer arg)
{
	(void) arg;

	struct sockaddr_in s_addr;
	int optval;

	s_addr.sin_family = AF_INET;
	s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	s_addr.sin_port = htons(746);

	pmap_unset (MOUNTPROG, MOUNTVERS);

	int udp_socket = socket(PF_INET, SOCK_DGRAM, 17); /* UDP is 17 */
	if (!udp_socket) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't create udp_socket for NFS mounter.");
		unset_rpcservices();
		exit(1);
	}

	optval = 1;
	setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
	if (bind(udp_socket, (struct sockaddr *) &s_addr, sizeof s_addr) != -1) {
		udp_mount = svcudp_create(udp_socket);
		if (udp_mount == NULL) {
			dbg(LOG_ERR, DEBUG_ERR, "cannot create MOUNT udp service.");
			unset_rpcservices();
			exit(1);
		}
		if (!svc_register(udp_mount, MOUNTPROG, MOUNTVERS, mountprog_1, IPPROTO_UDP)) {
			dbg(LOG_ERR, DEBUG_ERR, "unable to register MOUNT udp, may be portmap is not running");
			unset_rpcservices();
			exit(1);
		}
	} else {
		int port = ntohs(s_addr.sin_port);
		dbg(LOG_ERR, DEBUG_ERR, "Can't bind MOUNT udp socket to port %d: %s", port, strerror(errno));
		unset_rpcservices();
		exit(1);
	}

	s_addr.sin_port = htons(749);
	int tcp_socket = socket(PF_INET, SOCK_STREAM, 6); /* TCP is 6 */
	if (!tcp_socket) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't bind MOUNT udp socket to port %d: %s", ntohs(s_addr.sin_port), strerror(errno));
		unset_rpcservices();
		exit(1);
	}

	optval = 1;
	setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
	if (bind(tcp_socket, (struct sockaddr *) &s_addr, sizeof s_addr) != -1) {
		tcp_mount = svctcp_create(tcp_socket, 0, 0);
		if (tcp_mount == NULL) {
			dbg(LOG_ERR, DEBUG_ERR, "cannot create MOUNT tcp service.");
			exit(1);
		}
		if (!svc_register(tcp_mount, MOUNTPROG, MOUNTVERS, mountprog_1, IPPROTO_TCP)) {
			dbg(LOG_ERR, DEBUG_ERR, "unable to register MOUNT tcp, may be portmap is not running");
			exit(1);
		}
	} else {
		dbg(LOG_ERR, DEBUG_ERR, "Can't bind MOUNT tcp socket to port %d: %s", ntohs(s_addr.sin_port), strerror(errno));
		unset_rpcservices();
		exit(1);
	}

	svc_run ();

	sleep(5);

	dbg(LOG_ERR, DEBUG_ERR, "magma_start_mount() ended unexpectedly!");

	return 0;
}

/**
 * install RPC hooks for nfs protocol
 *
 * @param arg ignored. Declared only for compliance with GLib thread prototype
 * \return NULL. Can be safely ignored.
 */
gpointer magma_start_nfs(gpointer arg)
{
	(void) arg;
	struct sockaddr_in s_addr;
	int optval;

	s_addr.sin_family = AF_INET;
	s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	s_addr.sin_port = htons(2049);

	pmap_unset (NFS_PROGRAM, NFS_VERSION);

	int udp_socket = socket(PF_INET, SOCK_DGRAM, 17); /* UDP is 17 */
	if (!udp_socket) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't create udp_socket for NFS protocol.");
		unset_rpcservices();
		exit(1);
	}

	optval = 1;
	setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
	if (bind(udp_socket, (struct sockaddr *) &s_addr, sizeof s_addr) != -1) {
		udp_nfs = svcudp_create(udp_socket);
		if (udp_nfs == NULL) {
			dbg(LOG_ERR, DEBUG_ERR, "Cannot create NFS udp service");
			unset_rpcservices();
			exit(1);
		}
		if (!svc_register(udp_nfs, NFS_PROGRAM, NFS_VERSION, nfs_program_2, IPPROTO_UDP)) {
			dbg(LOG_ERR, DEBUG_ERR, "unable to register NFS udp, may be portmap is not running");
			unset_rpcservices();
			exit(1);
		}
	} else {
		dbg(LOG_ERR, DEBUG_ERR, "Can't bind MOUNT tcp socket to port %d: %s", ntohs(s_addr.sin_port), strerror(errno));
		unset_rpcservices();
		exit(1);
	}

	int tcp_socket = socket(PF_INET, SOCK_STREAM, 6); /* TCP is 6 */
	if (!tcp_socket) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't create tcp_socket for NFS protocol.");
		unset_rpcservices();
		exit(1);
	}

	optval = 1;
	setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
	if (bind(tcp_socket, (struct sockaddr *) &s_addr, sizeof s_addr) != -1) {
		tcp_nfs = svctcp_create(tcp_socket, 0, 0);
		if (tcp_nfs == NULL) {
			dbg(LOG_ERR, DEBUG_ERR, "cannot create NFS tcp service.");
			unset_rpcservices();
			exit(1);
		}
		if (!svc_register(tcp_nfs, NFS_PROGRAM, NFS_VERSION, nfs_program_2, IPPROTO_TCP)) {
			dbg(LOG_ERR, DEBUG_ERR, "unable to register NFS tcp, may be portmap is not running");
			unset_rpcservices();
			exit(1);
		}
	} else {
		dbg(LOG_ERR, DEBUG_ERR, "Can't bind MOUNT tcp socket to port %d: %s", ntohs(s_addr.sin_port), strerror(errno));
		unset_rpcservices();
		exit(1);
	}

	svc_run ();

	dbg(LOG_ERR, DEBUG_ERR, "magma_start_nfs() ended unexpectedly!");

	return 0;
}

void magma_start_nfs_interface()
{
	/* start NFS and MOUNT thread for nfs emulation */
	GError *error = NULL;

	GThread *nfs_th = g_thread_try_new("NFS thread", magma_start_nfs, NULL, &error);
	GThread *mount_th = g_thread_try_new("MOUNT thread", magma_start_mount, NULL, &error);

	if (NULL != nfs_th && NULL !=  mount_th) {
		dbg(LOG_INFO, DEBUG_PNFS, "NFS mount server... started!");
		dbg(LOG_INFO, DEBUG_PNFS, "NFS protocol server... started!");
	} else {
		dbg(LOG_INFO, DEBUG_PNFS, "NFS server can't start: %s", gerror->message);
		exit(1);
	}
}

#endif /* MAGMA_ENABLE_NFS_INTERFACE */

