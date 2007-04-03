/*
 * Copyright (C)2001,2002 InfoStreet, Inc.    www.infostreet.com
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
 * It uses a fixed array with doubly linked lists as elements.
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

static const char *VERSION = "$Id$";

#include <malloc.h>
#include <string.h>

#include "debug.h"
#include "hash.h"

typedef struct _list_element {
  struct _list_element *next;
  struct _list_element *prev;
  char *key;
  void *value;
} list_element;

typedef struct {
  list_element *first;
  int count;
} hash_element;

/* 
 * Compute an index with sufficient dispersion over the entire array
 * Returns a number in the range [0..size-1]
 */
unsigned int hash_func( const char * string, unsigned int size ){
  unsigned int sum;
  char c;

  sum = 0;
  
  while( c = *string++ ){
    sum += (unsigned) c;
  }

  sum %= size;

  return sum;
}

/* 
 * Create new hash structure. Size will be HASH_SIZE if 0.
 * Returns NULL on error
 */
hash_t *hash_new( int size ){
  hash_t *hash;
  hash_element *data;

  if (size <= 0){
    size = DEFAULT_HASH_SIZE;
  }

  LOG_PRINTF( DEBUG_MED, ZONE, 
	      "hash.new(): creating hash of size %d", size );

  hash = (hash_t*) malloc( sizeof(hash_t) );
  data = (hash_element*) malloc( size * sizeof(hash_element) );
  if (!hash || !data){
    LOG_PRINTF( DEBUG_ERROR, ZONE, 
		"hash.new(): malloc(hash|data): %s", LAST_ERROR );
    return NULL;
  }

  bzero( data, size * sizeof(hash_element) );
  hash->size = size;
  hash->data = data;
  hash->count = 0;

  return hash;
}

/* 
 * Retrieve the list element containing the key or NULL
 */
static void *_hash_get_list_elem( hash_t *hash, const char *key ){
  hash_element *data;
  list_element *cur;
  
  data = (hash_element*)hash->data + hash_func( key, hash->size );
  cur = data->first;

  while (cur){
    if (!strcmp( key, cur->key )){
      break;
    }
    cur = cur->next;
  }
  return cur;
}

/* 
 * Retrieve a value based on string key.
 * Returns NULL if the key was not found.
 */
void *hash_get( hash_t *hash, const char *key ){
  list_element *cur;

  cur = _hash_get_list_elem( hash, key );

  return (cur) ? cur->value : NULL;
}

/*
 * Insert a new value/Update existing value
 * Will return 0 on error (memory allocation problems)
 */
int hash_insert( hash_t *hash, const char *key, void *value ){
  hash_element *data;
  list_element *cur, *prev, *new;
  int result;
  
  result = hash_func( key, hash->size );
  data = (hash_element*)hash->data + result;

  LOG_PRINTF( DEBUG_MAX, ZONE, 
	      "hash.insert(): \"%s\" in bucket %d", 
	      key, result );

  cur = data->first;
  prev = cur;

  /* 
   * Scan the list until our key becomes less than the list's, or we
   * reach the end (the keys are ordered in each list).
   */

  result = 1; /* just some value that's greater than zero */
  while (cur && result > 0){
    result = strcmp( key, cur->key );
    prev = cur;
    cur = cur->next;
  }
  cur = prev; /* go back one */

  /* result < 0 <=> key < cur->key so insert before cur */
  /* result > 0 <=> key > cur->key so append after cur / create list head */

  if (result != 0){

    /* 
     * We need to create a new element, either insert (if result > 0) or
     * append at end of list (if result < 0, then cur == NULL).
     */
    new = (list_element*) malloc( sizeof(list_element) );
    if (!new){
      LOG_PRINTF( DEBUG_ERROR, ZONE, 
		  "hash.insert(): malloc(list_element): %s", LAST_ERROR );
      return 0;
    }
    new->value = value;
    new->key   = strdup(key);

    if (result < 0){ /* insert before cur. cur is defined. */
      new->next = cur;
      new->prev = cur->prev;
      cur->prev = new;
      if (new->prev){
	new->prev->next = new;
      } else {
	data->first = new; /* cur was the first element, update list head */
      }
    } else {         /* append after cur / create list */
      if (cur){
	new->next = cur->next;
	cur->next = new;
	new->prev = cur;
      } else {       /* create first element in list */
	new->next = NULL;
	new->prev = NULL;
	data->first = new;
      }
    }
    data->count++;
    hash->count++;

  } else {

    /* Key already exists, replace value */
    
    LOG_PRINTF( DEBUG_MAX, ZONE, 
		"hash.insert(): \"%s\": replaced value", key );

    free( cur->value );
    cur->value = value;
    
  }

  return 1;
}

/*
 * Delete a key.
 * Returns 0 if key could not be found.
 */
int hash_delete( hash_t *hash, const char *key ){
  list_element *cur;
  hash_element *data;
  
  data = (hash_element*)hash->data + hash_func( key, hash->size );

  LOG_PRINTF( DEBUG_MAX, ZONE, 
	      "hash.delete(): \"%s\"", key );

  cur = _hash_get_list_elem( hash, key );

  if (!cur){
    return 0;
  }

  /* Remove element from list */

  if (cur->prev){   /* we are not in the first slot */

    cur->prev->next = cur->next;

  } else {

    data->first = cur->next;

  }

  data->count--;
  hash->count--;

  /* free the memory used by element */

  free(cur->value);
  free(cur->key);
  free(cur);

  return 1;
}

/* 
 * Deallocate list recursively
 */
static void _list_destroy( list_element *list ){
  if (list){
    _list_destroy( list->next );
    free(list->key);
    free(list->value);
    free(list);
  } else {
    return;
  }
}

/*
 * deallocate entire hash
 */
void hash_destroy( hash_t *hash ){
  list_element *cur;
  hash_element *data;
  int i;

  LOG_PRINTF( DEBUG_MIN, ZONE, "hash.destroy(): deallocating memory." );

  for( i=0; i<hash->size; i++ ){
    data = (hash_element*)hash->data + i;
    _list_destroy( data->first );
  }
  free(hash->data);
  free(hash);
}

/*
 * (for debugging) print dump of hash structure
 */
void hash_dump( hash_t *hash ){
  list_element *cur;
  hash_element *data;
  unsigned int i;

  printf( "hash_dump(): %d buckets, %d keys total.\n",
	  hash->size, hash->count );

  for( i=0; i<hash->size; i++ ){
    data = (hash_element*)hash->data + i;

    if (data->count){
      printf( "  [%d], %d keys: ", i, data->count );
      cur = data->first;

      while( cur ){
	printf( "\"%s\" ", cur->key, cur->value );
	cur = cur->next;
      }
      printf( "\n" );
    }
  }
}
