/*
 * Copyright (C)2001 InfoStreet, Inc.    www.infostreet.com
 *
 * Author:   Laurentiu C. Badea (L.C.) sourceforge.net/users/wotevah
 * Created:  Feb 5, 2001
 * $LastChangedDate$
 * $LastChangedBy$
 * $Revision$
 *
 * Description:
 * Helper module to store data keyed on strings.
 *
 * It uses a fixed array with linked lists as elements.
 * The simple hash function is the sum of all characters modulo hash size.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * Version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc. 
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#define DEFAULT_HASH_SIZE 256

typedef struct {
  unsigned int size;
  unsigned int count; /* number of valid keys in hash                  */
  void *data;         /* pointer to hash structure (defined in hash.c) */
} hash_t;

/* 
 * Create new hash structure. Size will be HASH_SIZE if 0.
 * Returns NULL on error
 */
hash_t *hash_new( int size );
/* 
 * Retrieve a value based on string key.
 * Returns NULL if the key was not found.
 */
void *hash_get( hash_t *hash, const char *key );
/*
 * Insert a new value
 * Will update value if key already exists.
 * Returns 0 on error, non-zero on success.
 * NOTE: value has to be persistent (i.e. not on stack)
 */
int hash_insert( hash_t *hash, const char *key, void *value );
/*
 * Delete a key.
 * Returns 0 if key could not be found.
 * Deallocates memory for value as well.
 */
int hash_delete( hash_t *hash, const char *key );
/*
 * deallocate hash
 */
void hash_destroy( hash_t *hash );
/*
 * (for debugging) print dump of hash structure
 */
void hash_dump( hash_t *hash );
