/*
   MAGMA -- magma_flare_internals.h
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   You should never use this file. This file is for internal
   implementation only. If you want to play with magma contents
   use magma_flare.h! You've bee warned...

   * Defines flare types used for files, directories and so on...
   
   * Defines magma_DIR_t for directory operations

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

#ifndef _MAGMA_FLARE_TYPES_H
#define _MAGMA_FLARE_TYPES_H

typedef guint64 magma_offset;
typedef guint32 magma_uid;
typedef guint32 magma_gid;
typedef guint64	magma_dev;
typedef guint64 magma_ino;
typedef guint64 magma_size;
typedef guint32 magma_size32;
typedef guint64 magma_blocks;
typedef guint64 magma_atime;
typedef guint64 magma_ctime;
typedef guint64 magma_mtime;
typedef guint32 magma_mode;
typedef guint64 magma_nlink;
typedef guint64 magma_rdev;
typedef guint32 magma_blksize;
typedef guint16 magma_datagram_size;
typedef guint16 magma_readdir_entry;
typedef guint32 magma_flags;
typedef gint32  magma_result;
typedef guint16 magma_errno;
typedef guint16 magma_transaction_id;
typedef guint8  magma_ttl;

/** cast structure for directory flares */
typedef struct {
	/**
	 * number of entries present
	 */
	unsigned int entries;
} magma_flare_dir_t;

/** cast structure for regular flares */
typedef struct {
} magma_flare_file_t;

/** cast structure for FIFO flares */
typedef struct {
} magma_flare_fifo_t;

/** cast structure for character device flares */
typedef struct {
} magma_flare_chardev_t;

/** cast structure for block device flares */
typedef struct {
} magma_flare_blockdev_t;

/** cast structure for symlink flares */
typedef struct {
} magma_flare_symlink_t;

#define MAGMA_DECLARE_FLARE_ITEM_FIELD FALSE

/**
 * magma_flare_t is the base type of all Flare Objects.
 * Each flare is of this kind, and is later specialized
 * (casted) to one of supported types to be fully usefull.
 *
 * each flare share the same basic structure (magma_flare_t).
 * that structure has an internal pointer, called "item", which
 * is the way flare system uses to "cast" a generic flare into
 * a specific one rappresenting a file or a directory or a symlink
 * or a (char/block) device or a FIFO.
 *
 * "item" field is implemented as a union of N different pointers
 * (where N is the number of different flare types known by the
 * flare system), to grant correctness of data type. so, if a flare
 * is casted as a directory flare, it's flare->item.dir field
 * will be allocated to hold a magma_flare_dir_t* structure.
 *
 * after that casting operation succeeded, field "is_upcasted"
 * will be placed to 1.
 */
typedef struct magma_flare {
	/** last time this flare was looked up in the cache */
	GTimeVal last_access;

	/** flare path */
	char *path;

	/** flare type (r, d, l, c, b, p, s) */
	char type;

	/** flare commit path */
	char *commit_path;

	/** flare commit time */
	char *commit_time;

	/** flare commit url */
	char *commit_url;

	/** flare path hashed to binary representation */
	unsigned char *binhash;

	/** flare path hashed */
	char *hash;

	/** contents file path */
	char *contents;

	/** flare parent directory path */
	char *parent_path;

	/** flare parent directory path hashed to binary representation */
	unsigned char *parent_binhash;

	/** flare parent directory path hashed */
	char *parent_hash;

	/** parent directory translated (relocated) path */
	char *parent_contents;

	/** stat buffer */
	struct stat st;

	/** has been upcasted? */
	int is_upcasted;

#if MAGMA_DECLARE_FLARE_ITEM_FIELD
	/** additional informations for item hosted by this flare */
	union {
		magma_flare_dir_t		*dir;		/**< holds a magma_flare_dir_t object if this flare is a directory */
		magma_flare_file_t		*file;		/**< holds a magma_flare_file_t object if this flare is a regular file */
		magma_flare_blockdev_t	*blockdev;	/**< holds a magma_flare_blockdev_t object if this flare is a block device */
		magma_flare_chardev_t	*chardev;	/**< holds a magma_flare_chardev_t object if this flare is a character device */
		magma_flare_symlink_t	*symlink;	/**< holds a magma_flare_symlink_t object if this flare is a symbolic link */
		magma_flare_fifo_t		*fifo;		/**< holds a magma_flare_fifo_t object if this flare is a FIFO pipe */
	} item;
#endif

} magma_flare_t;

/**
 * Glib-based magma_DIR_t struct.
 *
 * This new version preloads the directory contents and
 * saves them into *content. The magma_opendir() operation
 * really reads the whole directory. Every magma_readdir()
 * just get another name from the actual offset.
 */
typedef struct {
	magma_offset offset;
	gchar *content;
	magma_flare_t *dir;
	gsize length;
} magma_DIR_t;

/**
 * Each ACL is defined as follow:
 *
 * <pre>
 * SHARE:
 *   ADDRESS PATH OPERATIONS</pre>
 *
 * where:
 * 	 SHARE is the path of the share to which this ACL applies
 *   ADDRESS is an IP address or a netaddress with a netmask
 *   PATH is the path this rule applies to
 *   OPERATIONS is one of "n", "r" or "w" ("w" implies "r")
 *      n: nothing
 *      r: read
 *      w: write
 *
 * Each SHARE can have as many rules as needed. For example:
 * <pre>
 * /users:
 *   192.168.1.0/24 / w
 *   192.168.1.0/24 /root n</pre>
 *
 * refers to share "/users" which is exported read/write to all
 * subnet 192.168.1.0/24, with the exception of path "/root" which
 * is not accessible to any node of the network.
 */
typedef struct magma_acl {
	struct in_addr ip;
	struct in_addr mask;
	char *path;
	char operations_allowed;
	uint16_t line;

	struct magma_acl *prev;
	struct magma_acl *next;
} magma_acl_t;

/**
 * magma can export more than one share. each share is defined by
 * this type.
 */
typedef struct magma_share {
	char *share;				/*<< the name of the share */
	magma_acl_t *acl;			/*<< a linked list of ACL */

	struct magma_share *prev;	/*<< previous share in share linked list */
	struct magma_share *next;	/*<< next share in share linked list */
} magma_share_t;

#endif /* _MAGMA_FLARE_TYPES_H */

// vim:ts=4:nocindent:autoindent
