/*
   MAGMA -- sha1.h
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   import OpenSSL SHA1 functions for message hashing and provides
   simplyfied interface to hashing functions

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

#ifndef _MAGMA_SHA1_H
#define _MAGMA_SHA1_H

#define READCHUNK 1024 /**< read buffer dimension */
#define KEY_MOVEMENT_SLEEP_TIME 1 /**< sleep time between two key movements */

#ifndef SHA_DIGEST_LENGTH
#define SHA_DIGEST_LENGTH 20
#endif

#ifndef SHA_READABLE_DIGEST_LENGTH
#define SHA_READABLE_DIGEST_LENGTH SHA_DIGEST_LENGTH * 2 + 1
#endif

extern unsigned char *sha1_data(const void *data, unsigned int length);
extern char *armour_hash(const unsigned char *hash);
extern unsigned char *dearmour_hash(const char* armoured);

/** decrement a key by 1 unit */
#define magma_decrement_key(key) magma_decrement_key_p(key, key + 2*SHA_DIGEST_LENGTH - 1)
/** increment a key by 1 unit */
#define magma_increment_key(key) magma_increment_key_p(key, key + 2*SHA_DIGEST_LENGTH - 1)

/*
 * never use magma_increment_key_p and magma_decrement_key_p directly.
 * use magma_decrement_key and magma_increment_key macro defined before
 */
extern int magma_increment_key_p(char *key, char *p);
extern int magma_decrement_key_p(char *key, char *p);

extern long int key_difference(char *k1, char *k2);

/**
 * data structure used in math key computations. since SHA1 keys are
 * 20 bytes long and no native type can hold such a key, this structure
 * fragment an SHA1 key into five unsigned 32bit integers.
 */
typedef struct magma_sha1 {
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
	uint32_t e;
} magma_sha1_t;

extern char *key_difference_by_char(char *max, char *min);
extern magma_sha1_t *sha1_aton(char *key);
extern char *subtract_key(char *k1, char *k2);

extern int get_char_val(char c);
extern char get_int_val(int i);
extern char *key_difference_by_char(char *max, char *min);
extern long int key_difference(char *k1, char *k2);

#endif /* _MAGMA_SHA1_H */

// vim:ts=4:nocindent:autoindent
