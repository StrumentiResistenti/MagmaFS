/*
   MAGMA -- acl.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   ACL support. Defines a linked list of ACL to be used on connection
   accept, defines a parsing function to load ACL from disk, defines
   a checking function to match a struct in_addr object against ACLs.

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

// #define INCLUDE_FLARE_INTERNALS
#include "../magma.h"

/**
 * RWLock for ACL
 */
GRWLock magma_acl_rwlock;

/**
 * The Keyfile holding the ACLS
 */
GKeyFile *magma_acl = NULL;

/**
 * Parse ACL file
 *
 * @param filename path to ACL file
 */
void magma_parse_acl_file()
{
	dbg(LOG_INFO, DEBUG_PFUSE, "Loading ACL from file: %s", MAGMA_ACL_FILE);

	/* lock the rwlock */
	g_rw_lock_writer_lock(&magma_acl_rwlock);

	if (NULL != magma_acl) g_key_file_free(magma_acl);
	magma_acl = g_key_file_new();

	/* parse the key file */
	GError *error = NULL;
	if (!g_key_file_load_from_file(magma_acl, MAGMA_ACL_FILE, G_KEY_FILE_KEEP_COMMENTS, &error)) {
		dbg(LOG_ERR, DEBUG_ERR, "Error loading ACL %s: %s", MAGMA_ACL_FILE, error->message);
		exit(64);
	}

	/* unlock the rwlock */
	g_rw_lock_writer_unlock(&magma_acl_rwlock);
}

/**
 * save current ACL from memory to disk
 */
void magma_save_acl_file()
{
	GError *error = NULL;
	gchar *key_file_content = g_key_file_to_data(magma_acl, NULL, NULL);

	g_rw_lock_reader_lock(&magma_acl_rwlock);

	if (g_file_set_contents(MAGMA_ACL_FILE_TEMP, key_file_content, strlen(key_file_content), &error)) {
		if (0 == rename(MAGMA_ACL_FILE_TEMP, MAGMA_ACL_FILE)) {
			dbg(LOG_INFO, DEBUG_UTILS, "ACL file %s saved", MAGMA_ACL_FILE);
		} else {
			dbg(LOG_ERR, DEBUG_UTILS, "Unable to save %s ACL file: %s", MAGMA_ACL_FILE, strerror(errno));
		}
	} else {
		dbg(LOG_ERR, DEBUG_UTILS, "Unable to save %s ACL file: %s", MAGMA_ACL_FILE, strerror(errno));
		g_error_free(error);
	}

	g_rw_lock_reader_unlock(&magma_acl_rwlock);
	g_free(key_file_content);
}

gchar magma_acl_match(GSocket *socket, const char *path, GKeyFile *key_file)
{
	GError *error = NULL;

	return 'w'; // uncomment this line to exclude ACL subsystem

	GInetAddress *incoming_address = NULL, *acl_address = NULL;
	gchar policy = '?';

	/* if path is zero length, deny everything */
	if (path == NULL) return 'n';
	if (strlen(path) == 0) return 'n';
	// if (strcmp(path, ".") == 0) return 'n';

	/* if the keyfile does not have a group matching the path, shorten the path and call recursively */
	if (!g_key_file_has_group(key_file, path)) {
		gchar *shorter_path = g_path_get_dirname(path);
		if (strcmp(shorter_path, path) == 0) {
			g_free(shorter_path);
			return 'n';
		}

		dbg(LOG_INFO, DEBUG_PFUSE, "Path not found, trying %s", shorter_path);
		policy = magma_acl_match(socket, shorter_path, key_file);
		g_free(shorter_path);
		return policy;
	}

	gchar **keys = g_key_file_get_keys(key_file, path, NULL, NULL);
	gchar **key = keys;

	/* cycle all keys */
	while (NULL != *key) {
		dbg(LOG_INFO, DEBUG_PFUSE, "Evaluating netblock %s", *key);
		gboolean addresses_do_match = FALSE;

		// 1. fetching remote address from GSocketConnection
		incoming_address = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(g_socket_get_remote_address(socket, NULL)));

		// 2. separate IP address from netmask
		gchar **splitted = g_strsplit(*key, "/", 2);

		// 3. create a GInetAddress from netblock
		dbg(LOG_INFO, DEBUG_PFUSE, "Creating ACL GInetAddress from %s", splitted[0]);
		acl_address = g_inet_address_new_from_string(splitted[0]);

		gchar *remote_address;

		// 4. applying netmask to incoming address, if present, and comparing
		if (splitted[1]) {
			const guint8 *original_bytes = g_inet_address_to_bytes(incoming_address);

			int netmask;
			if (strcmp(splitted[0], "0") == 0) {
				netmask = 0xffffffff << (32 - atoi(splitted[1]));
				netmask = htonl(netmask);
			} else {
				netmask = 0;
			}

			guint8 *netmask_bytes = (guint8 *) &netmask;
			dbg(LOG_INFO, DEBUG_PFUSE, "Applying netmask %u.%u.%u.%u (/%s)",
				netmask_bytes[0], netmask_bytes[1], netmask_bytes[2], netmask_bytes[3],
				splitted[1]);

			guint8 *bytes = g_new(guint8, 4);
			bytes[0] = original_bytes[0] & netmask_bytes[0];
			bytes[1] = original_bytes[1] & netmask_bytes[1];
			bytes[2] = original_bytes[2] & netmask_bytes[2];
			bytes[3] = original_bytes[3] & netmask_bytes[3];
			
			dbg(LOG_INFO, DEBUG_PFUSE, "Creating incoming GInetAddress from %u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);
			GInetAddress *netmasked_address = g_inet_address_new_from_bytes(bytes, G_SOCKET_FAMILY_IPV4);

			g_free(bytes);

			if (NULL == netmasked_address) {
				dbg(LOG_ERR, DEBUG_PFUSE, "Error applying netmask to remote address: %s", error->message);
				g_error_free(error);
			}

			addresses_do_match = g_inet_address_equal(netmasked_address, acl_address);
			remote_address = g_inet_address_to_string(netmasked_address);
			g_object_unref(netmasked_address);
		} else {
			addresses_do_match = g_inet_address_equal(incoming_address, acl_address);
			remote_address = g_inet_address_to_string(incoming_address);
		}
		
		// 5. return policy if a match was found
		if (addresses_do_match) {
			gchar *value = g_key_file_get_value(key_file, path, *key, NULL);
			policy = value[0];

			g_free(value);
			g_free(remote_address);
			g_strfreev(splitted);

			break;
		} else {
			dbg(LOG_INFO, DEBUG_PFUSE, "%s != %s", splitted[0], remote_address);
		}

		g_free(remote_address);
		g_strfreev(splitted);

		key++;
	}

	g_strfreev(keys);

	return policy;
}

/**
 * validate incoming connection using ACLs
 *
 * @param s requesting IP address
 * @param path requested path
 * @param optype operation requested, can be 'r' or 'w'
 *
 * @return true if valid, false otherwise
 */
int magma_validate_connection(GSocket *socket, GSocketAddress *peer, const char *path, char optype)
{
	/* TODO remove after debugging */
	return (1);

	/* read-lock the ACL and search the closest */

	g_rw_lock_reader_lock(&magma_acl_rwlock);
	gchar policy = magma_acl_match(socket, path, magma_acl);
	g_rw_lock_reader_unlock(&magma_acl_rwlock);

	GInetAddress *inet_address = g_inet_socket_address_get_address((GInetSocketAddress *) peer);
	gchar *printable = g_inet_address_to_string((GInetAddress *) inet_address);

	if ('n' == policy) {
		dbg(LOG_INFO, DEBUG_PFUSE, "operation %c denied on %s from %s", optype, path, printable);
		g_free(printable);
		return 0;
	}

	if ((optype == policy) || (('r' == optype) && ('w' == policy))) {
		dbg(LOG_INFO, DEBUG_PFUSE, "operation %c allowed on %s from %s", optype, path, printable);
		g_free(printable);
		return 1;
	}

	dbg(LOG_INFO, DEBUG_PFUSE, "operation %c denied by default on %s from %s", optype, path, printable);
	g_free(printable);
	return 0;
}

/**
 * List all paths listed in ACL
 * Free it with g_strfreev()
 */
gchar **magma_acl_get_paths()
{
	return g_key_file_get_groups(magma_acl, NULL);
}

/**
 * List all netblocks listed in ACL for a path
 * Free it with g_strfreev()
 *
 * @param path the path of the ACL the netblocks refer to
 */
gchar **magma_acl_get_netblocks(const gchar *path)
{
	return g_key_file_get_keys(magma_acl, path, NULL, NULL);
}

/**
 * Return the policy of an ACL rule
 *
 * @param path the path of the rule
 * @param netblock the netblock of the rule
 */
gchar magma_acl_get_policy(const gchar *path, const gchar *netblock)
{
	GError *error = NULL;
	gchar *policy = g_key_file_get_value(magma_acl, path, netblock, &error);
	if (NULL == policy) {
		// if the policy can't be found, return 'n'
		dbg(LOG_ERR, DEBUG_PFUSE, "Unable to fetch %s policy for %s: %s", path, netblock, error->message);
		return 'n';
	}
	gchar result = policy[0];
	g_free(policy);
	return result;
}

// vim:ts=4:nocindent:autoindent
