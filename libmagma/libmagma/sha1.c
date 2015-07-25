/*
   MAGMA -- sha1.c
   Copyright (C) 2006-2013 Tx0 <tx0@strumentiresistenti.org>

   import GLib SHA1 functions for message hashing and provides
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

#include "magma.h"

/**
 * return hash bucket of buffered data
 *
 * @param data memory location of given data
 * @param length size of memory buffer *data
 * @return binary hashing of given data
 */
unsigned char *sha1_data(const void *data, unsigned int length)
{
	// return g_compute_checksum_for_data(G_CHECKSUM_SHA1, data, length);

	// get the hash and free the object
	gsize hash_length = 20;
	guint8 *hash = calloc(sizeof(guint8), hash_length);
	if (!hash) {
		dbg(LOG_ERR, DEBUG_ERR, "Error allocating hash buffer");
		return NULL;
	}

	// create the checksum object
	GChecksum *gc = g_checksum_new(G_CHECKSUM_SHA1);

	// feed the checksum object
	g_checksum_update(gc, data, length);

	// get the hash
	g_checksum_get_digest(gc, hash, &hash_length);

	// free the object
	g_checksum_free(gc);

	// return the hash
	return (unsigned char *) hash;
}

/**
 * provides printable (armoured) hash bucked
 *
 * @param hash the binary hash value to be armoured
 * @return pointer to armoured hash
 */
char *armour_hash(const unsigned char *hash)
{
	/* sha1 produces 160 bit (20 bytes) hashes which can be rappresented
	 * by a 20*2 string of chars + 1 for \0 */
	char *armour = malloc(SHA_DIGEST_LENGTH*2+1);
	char *ap = armour;

#define ARMOUR_HASH 1
#if ARMOUR_HASH == 1

	/* one call variant */
	sprintf(
		armour,
		"%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x",
		hash[0],  hash[1],  hash[2],  hash[3],  hash[4],
		hash[5],  hash[6],  hash[7],  hash[8],  hash[9],
		hash[10], hash[11], hash[12], hash[13], hash[14],
		hash[15], hash[16], hash[17], hash[18], hash[19]
	);
	ap += SHA_DIGEST_LENGTH*2;
	*ap = '\0';

#else

	/* iterative variant */
	int i;
	for (i=0; i<20; i++) {
		sprintf(ap, "%.2x", *hash++);
		ap += 2;
	}
	*ap = '\0';

#endif

	return armour;
}

unsigned char *dearmour_hash(const char* armoured)
{
	unsigned char *result = g_new0(unsigned char, 20);
	int i, ten, unit;
	char buffer[3];
	buffer[2] = '\0';

	for (i = 0; i < 20; i += 2) {
		buffer[0] = armoured[i*2];
		buffer[1] = armoured[i*2+1];

		sscanf(buffer, "%x%x", &ten, &unit);

		result[i] = ten * 10 + unit;
	}

	return result;
}

/**
 * decrement key value by one. returns 1 if key has been decremented,
 * 0 otherwise (tipically if key is already all zero). Should never
 * be called directly: always use macro decrement_key(key)!
 * 
 * @param key buffer holding armoured key
 * @param p used internally for recursive operations
 * @return 1 on success, 0 otherwise
 */
int magma_decrement_key_p(char *key, char *p)
{
	/* if key is of wrong size, return 0 */
	if (strlen(key) != 2 * SHA_DIGEST_LENGTH) {
		dbg(LOG_ERR, DEBUG_ERR, "Key of wrong length passwd to decrement_key");
		return 0;
	}

	/* if key is all zero can't be further decremented */
	if (strcmp(key, "0000000000000000000000000000000000000000") == 0) {
		return 0;
	}

	if (((*p > 'a') && (*p <= 'f')) || ((*p > '0') && (*p <= '9'))) {
		/*
		 * if char is in range )0-9) or )a-f) simply decrement it by
		 * 1 using ASCII table arithmetics
		 */
		*p = *p - 1;
	} else if ((*p == '0') && (p != key)) {
		/*
		 * if char is 0 and excluding limit case of the first char
		 * (that's why we check that p does not point to string first
		 * char), set char to 'f' and decrement preceding one calling
		 * magma_decrement_key_p recursively.
		 */
		*p = 'f';
		magma_decrement_key_p(key, p - 1);
	} else if ( *p == 'a' ) {
		/*
		 * if char is 'a' we have to handle it specially since there is
		 * a gap in ASCII table between '9' and 'a'
		 */
		*p = '9';
	}

	return 1;
}

/**
 * increment key value by one. returns 1 if key has been incremented,
 * 0 otherwise (tipically if key is already all 0xf). Should never
 * be called directly: always use macro increment_key(key)!
 *
 * @param key buffer holding armoured key
 * @param p used internally for recursive operations
 * @return 1 on success, 0 otherwise
 */
int magma_increment_key_p(char *key, char *p)
{
	/* if key is of wrong size, return 0 */
	if (strlen(key) != 2*SHA_DIGEST_LENGTH) {
		dbg(LOG_ERR, DEBUG_ERR, "Key of wrong length passed to increment_key");
		return 0;
	}

	/* if key is all 0xFF can't be further incremented */
	if (strcmp(key, "ffffffffffffffffffffffffffffffffffffffff") == 0)
		return 0;

	if (((*p >= 'a') && (*p < 'f')) || ((*p >= '0') && (*p < '9'))) {
		/*
		 * if char is in range )0-9) or )a-f) simply increment it by
		 * 1 using ASCII table arithmetics
		 */
		*p = *p + 1;
	} else if ((*p == '9')) {
		/*
		 * if char is 0 and excluding limit case of the first char
		 * (that's why we check that p does not point to string first
		 * char), set char to 'f' and decrement preceding one calling
		 * magma_decrement_key_p recursively.
		 */
		*p = 'a';
	} else if ( *p == 'f' && (p != key)) {
		/*
		 * if char is 'a' we have to handle it specially since there is
		 * a gap in ASCII table between '9' and 'a'
		 */
		*p = '0';
		magma_increment_key_p(key, p - 1);
	}

	return 1;
}

#if 0
/**
 * change a key to move near destination key
 */
int move_key(char *actual, const char *destination)
{
	int direction;
	char *tmp_key = strndup(actual, SHA_DIGEST_LENGTH*2);

	/*
	 * we use tmp_key, instead of directly modifying actual,
	 * because key movement is not yet done when calling
	 * decrement_key or increment_key. so modifying key in
	 * actual (which can be a pointer to node->start_key
	 * or node->stop_key) can lead to unconsistency (even
	 * if just for one single key) in key routing
	 */

	while ((direction = strcmp(tmp_key, destination)) != 0) {
		if (direction < 0) {
			if ( magma_decrement_key(tmp_key) ) {
				/* get this key */
				strcpy(actual, tmp_key);
			}
		} else {
			if ( magma_increment_key(tmp_key) ) {
				/* get this key */
				strcpy(actual, tmp_key);
			}
		}
		sleep(KEY_MOVEMENT_SLEEP_TIME);
	}
	return 1;
}
#endif


/**
 * transform hexadecimal character into its integer value
 *
 * @param c given hexadecimal character
 * @return numeric integer value of c
 */
int get_char_val(char c)
{
	switch (c) {
		case '0': return 0;
		case '1': return 1;
		case '2': return 2;
		case '3': return 3;
		case '4': return 4;
		case '5': return 5;
		case '6': return 6;
		case '7': return 7;
		case '8': return 8;
		case '9': return 9;
		case 'a':
		case 'A': return 10;
		case 'b':
		case 'B': return 11;
		case 'c':
		case 'C': return 12;
		case 'd':
		case 'D': return 13;
		case 'e':
		case 'E': return 14;
		case 'f':
		case 'F': return 15;
	}
	return 0;
}

/**
 * transform an integer value to hexadecimal rappresentation
 *
 * @param i integer given value
 * @return ASCII rappresentation
 */
char get_int_val(int i)
{
	if (i < 10) return i + 48;
	else return i + 87;
}

/** internally used by key_difference_by_char() */
void riporto(char *result, int c)
{
	if (*(result + c) != '0') {
		*(result + c) = get_int_val(get_char_val(*(result + c)) - 1);
	} else {
		*(result + c) = 'f';
		if (c!=0) riporto(result, c-1);
	}
}

/**
 * calculate difference between two keys
 *
 * @param max first key, supposed to be the greater
 * @param min second key, supposed to be the smallest
 * @return pointer to a key which is the difference. memory
 * is dynamically allocated and should be freed by outside code.
 * \todo unused code to be removed?
 */
char *key_difference_by_char(char *max, char *min)
{
	int c;
	char *result = g_strdup(max);
	if (result == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't allocate result in key_difference_by_char");
		return NULL;
	}

	for (c = SHA_DIGEST_LENGTH * 2 - 1; c >= 0; c--) {
		int max_val = get_char_val(*(result + c));
		int min_val = get_char_val(*(min + c));
		
		if (max_val >= min_val) {
			*(result + c) = get_int_val(max_val - min_val);
			fprintf(stderr, "%s\n",result);
		} else {
			*(result + c) = get_int_val(16 - min_val + max_val);
			riporto(result, c - 1);
			fprintf(stderr, "%s (riporto) \n",result);
		}
	}

	return result;
}

/**
 * return key difference between two keys. max is
 * supposed to be greater than min. never use this
 * function directly. use key_difference().
 *
 * @param max pointer to first key, supposed to be the greater
 * @param min pointer to second key, supposed to be the smaller
 * @return integer difference of the keys
 */
long int real_key_difference(char *max, char *min)
{
	long int difference = 0;
	unsigned int c;
	int max_val, min_val;

	for (c = 0; c < sizeof(long int); c++) {
		max_val = get_char_val(*(max+c));
		min_val = get_char_val(*(min+c));
		difference = difference * 4 + max_val - min_val;
	}

	return difference;
}

/**
 * return difference between two keys.
 *
 * @param k1 pointer to first key
 * @param k2 pointer to second key
 * @return integer difference between the keys
 */
long int key_difference(char *k1, char *k2)
{
	int cmp = strcmp(k1,k2);

	if ( cmp == 0 )
		return 0;
	else if ( cmp > 0 )
		return real_key_difference(k2, k1);
	else
		return real_key_difference(k1, k2);
}

/**
 * transform ASCII key rappresentation to numeric rappresentation
 *
 * @param key pointer to given key
 * @return numeric rappresentation of key, NULL if something went wrong
 */
magma_sha1_t *sha1_aton(char *key)
{
	int c = 0;
	magma_sha1_t *nkey = malloc(sizeof(magma_sha1_t));

	if (nkey == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "Can't allocate space for key in sha1_aton");
		return NULL;
	}

	memset(nkey,0,sizeof(magma_sha1_t));

	if (key == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "NULL pointer passed to sha1_aton");
		g_free(nkey);
		return NULL;
	}

	for (; c < 8; c++) {
		int char_val = get_char_val(*(key+c));
		nkey->a = nkey->a*16 + char_val;
	}
	for (; c < 16; c++) {
		int char_val = get_char_val(*(key+c));
		nkey->b = nkey->b*16 + char_val;
	}
	for (; c < 24; c++) {
		int char_val = get_char_val(*(key+c));
		nkey->c = nkey->c*16 + char_val;
	}
	for (; c < 32; c++) {
		int char_val = get_char_val(*(key+c));
		nkey->d = nkey->d*16 + char_val;
	}
	for (; c < 40; c++) {
		int char_val = get_char_val(*(key+c));
		nkey->e = nkey->e*16 + char_val;
	}

	return nkey;
}

/** print a key to standard err */
#define print_key(key)\
	fprintf(stderr, "E: %.8lx\n", (unsigned long int) key->e);\
	fprintf(stderr, "D: %.8lx\n", (unsigned long int) key->d);\
	fprintf(stderr, "C: %.8lx\n", (unsigned long int) key->c);\
	fprintf(stderr, "B: %.8lx\n", (unsigned long int) key->b);\
	fprintf(stderr, "A: %.8lx\n", (unsigned long int) key->a);

/*
 * if you need more debug output to check how keys are managed
 * just comment out following to lines
 */
#undef  print_key
#define print_key(key)

/**
 * make the difference of two keys
 *
 * @param k1 pointer to first key
 * @param k2 pointer to second key
 * @return pointer to difference key, NULL if something went wrong
 */
char *subtract_key(char *k1, char *k2)
{
	char *result;
	magma_sha1_t *nk1, *nk2, *nresult;

	/* key k1 should be greater that key k2 */
	if (strcmp(k1,k2) < 0) return g_strdup("0000000000000000000000000000000000000000");

	/* binary rappresentation */
	nk1 = sha1_aton(k1);
	if (nk1 == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "No space for malloc in subtract_key");
		return NULL;
	}

	nk2 = sha1_aton(k2);
	if (nk2 == NULL) {
		dbg(LOG_ERR, DEBUG_ERR, "No space for malloc in subtract_key");
		g_free(nk1);
		return NULL;
	}

	/* */
	nresult = malloc(sizeof(magma_sha1_t));
	if ( nresult == NULL ) {
		dbg(LOG_ERR, DEBUG_ERR, "No space for malloc in subtract_key");
		g_free(nk1);
		g_free(nk2);
		return NULL;
	}
	memset(nresult,0,sizeof(magma_sha1_t));

	/* making difference */
	if (nk1->e >= nk2->e) {
		nresult->e = nk1->e - nk2->e;
	} else {
		if (nresult->d > 0) {
			nresult->d--;
		} else if (nresult->c > 0) {
			nresult->d = ~0;
			nresult->c--;
		} else if (nresult->b > 0) {
			nresult->d = nresult->c = ~0;
			nresult->b--;
		} else {
			nresult->d = nresult->c = nresult->b = ~0;
			nresult->a--;
		}
		nresult->e = ~0 - nk2->e + nk1->e;
	}
	print_key(nresult);

	if (nk1->d >= nk2->d) {
		nresult->d = nk1->d - nk2->d;
	} else {
		if (nresult->c > 0) {
			nresult->c--;
		} else if (nresult->b > 0) {
			nresult->c = ~0;
			nresult->b--;
		} else {
			nresult->c = nresult->b = ~0;
			nresult->a--;
		}
		nresult->d = ~0 - nk2->d + nk1->d;
	}
	print_key(nresult);

	if (nk1->c >= nk2->c) {
		nresult->c = nk1->c - nk2->c;
	} else {
		if (nresult->b > 0) {
			nresult->b--;
		} else {
			nresult->b = ~0;
			nresult->a--;
		}
		nresult->c = ~0 - nk2->c + nk1->c;
	}
	print_key(nresult);

	if (nk1->b >= nk2->b) {
		nresult->b = nk1->b - nk2->b;
	} else {
		nresult->a--;
		nresult->b = ~0 - nk2->b + nk1->b;
	}
	print_key(nresult);

	nresult->a = nk1->a - nk2->a;
	print_key(nresult);

	result = malloc(SHA_READABLE_DIGEST_LENGTH);
	if (result == NULL) return NULL;
	memset(result,0,SHA_READABLE_DIGEST_LENGTH);

	sprintf(
		result,
		"%.8lx%.8lx%.8lx%.8lx%.8lx",
		(unsigned long int) nresult->a,
		(unsigned long int) nresult->b,
		(unsigned long int) nresult->c,
		(unsigned long int) nresult->d,
		(unsigned long int) nresult->e
	);

	return result;
}

// vim:ts=4:nocindent:autoindent
