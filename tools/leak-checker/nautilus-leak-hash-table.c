/* nautilus-leak-hash-table.c - hash table for a leak checking library
   Virtual File System Library

   Copyright (C) 2000 Eazel

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Pavel Cisler <pavel@eazel.com>
*/

#include "nautilus-leak-checker.h"
/* included first, defines following switch*/
#if LEAK_CHECKER

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "nautilus-leak-checker-stubs.h"
#include "nautilus-leak-hash-table.h"

/* We have our own hash table here mainly to allow avoiding calls to 
 * malloc and realloc that would cause reentry
 */

static void
nautilus_leak_hash_element_finalize (NautilusHashEntry *element)
{
	nautilus_leak_allocation_record_finalize (&element->data);
	memset (element, 0, sizeof(NautilusHashEntry));
}

static unsigned long
nautilus_leak_hash_element_hash (NautilusHashEntry *element)
{
	return (unsigned long)element->data.block;
}

static gboolean
nautilus_leak_hash_element_match (NautilusHashEntry *element, unsigned long key)
{
	return (unsigned long)element->data.block == key;
}

/* NautilusHashEntries are allocated inside a NautilusHashEntryVector.
 * NautilusHashEntryVector keeps a linked list of deleted entries.
 */
typedef struct {
	NautilusHashEntry *data;
	size_t size;
	int next_free;
	int next_deleted;
} NautilusHashEntryVector;

static void
nautilus_leak_hash_element_vector_inititalize (NautilusHashEntryVector *vector, size_t initial_size)
{
	int index;
	
	vector->data = (NautilusHashEntry *)real_malloc(initial_size * sizeof(NautilusHashEntry));
	if (vector->data == NULL) {
		g_warning ("leak checker out of memory");
		abort();
	}
	memset (vector->data, 0, initial_size * sizeof(NautilusHashEntry));
	for (index = initial_size - 1; index >= 0; index --) {
		vector->data[index].next = -1;
	}
	
	vector->size = initial_size;
	vector->next_free = 0;
	vector->next_deleted = -1;
}

static void
nautilus_leak_hash_element_vector_finalize (NautilusHashEntryVector *vector)
{
	int index;
	for (index = 0; index < vector->size; index++) {
		nautilus_leak_hash_element_finalize (&vector->data[index]);
	}
	real_free (vector->data);
}

static NautilusHashEntry *
nautilus_leak_hash_element_vector_at (NautilusHashEntryVector *vector, int index)
{
	return &vector->data[index];
}

enum {
	HASH_ELEMENT_VECTOR_GROW_CHUNK = 1024
};

static int
nautilus_leak_hash_element_vector_add (NautilusHashEntryVector *vector)
{
	int index;
	NautilusHashEntry *new_element;
	size_t new_size;
	NautilusHashEntry *new_data;

	if (vector->next_deleted >= 0) {
		/* Reuse a previously deleted item. */
		index = vector->next_deleted;
		vector->next_deleted = nautilus_leak_hash_element_vector_at (vector, index)->next;
	} else if (vector->next_free >= vector->size - 1) {
		/* We need grow the vector because it cannot fit more entries. */
		new_size = vector->size + HASH_ELEMENT_VECTOR_GROW_CHUNK;
		new_data = (NautilusHashEntry *)real_malloc(new_size * sizeof(NautilusHashEntry));
		if (new_data == NULL) {
			g_warning ("leak checker out of memory");
			abort();
		}
		/* FIXME: only clean the unused part */
		memset (new_data, 0, new_size * sizeof(NautilusHashEntry));

		/* copy all the existing items over*/
		memcpy (new_data, vector->data, vector->size * sizeof(NautilusHashEntry));
		/* delete the old array */
		real_free (vector->data);
		vector->data = new_data;
		vector->size = new_size;
		index = vector->next_free;
		++vector->next_free;
	} else {
		/* Just take the next free item. */
		index = vector->next_free;
		++vector->next_free;
	}

	/* Initialize the new element to an empty state. */
	new_element = nautilus_leak_hash_element_vector_at (vector, index);
	memset (new_element, 0, sizeof(NautilusHashEntry));
	new_element->next = -1;

	return index;
}

static void
nautilus_leak_hash_element_vector_remove (NautilusHashEntryVector *vector, int index)
{
	/* free the data */
	nautilus_leak_hash_element_finalize (&vector->data[index]);

	/* insert item as first into deleted item list */
	nautilus_leak_hash_element_vector_at (vector, index)->next = vector->next_deleted;
	vector->next_deleted = index;
}

struct NautilusLeakHashTable {
	size_t array_size;
	int *hash_array;
	NautilusHashEntryVector element_vector;
};

/* These primes are close to 2^n numbers for optimal hashing performance
 * and near-2^n size.
 */
long nautilus_leak_hash_table_primes [] = {
	509, 1021, 2039, 4093, 8191, 16381, 32749, 65521, 131071, 262139,
	524287, 1048573, 2097143, 4194301, 8388593, 16777213, 33554393, 67108859,
	134217689, 268435399, 536870909, 1073741789, 2147483647, 0
};


static size_t
nautilus_leak_hash_table_optimal_size (size_t size)
{
	int index;
	for (index = 0; ; index++) {
		if (!nautilus_leak_hash_table_primes [index] || nautilus_leak_hash_table_primes [index] >= size) {
			return nautilus_leak_hash_table_primes [index];
		}
	}

	return 0;
}

static void
nautilus_leak_hash_table_initialize (NautilusLeakHashTable *table, size_t initial_size)
{
	/* calculate the size of the bucket array */
	table->array_size = nautilus_leak_hash_table_optimal_size (initial_size);

	/* allocate the element array */
	nautilus_leak_hash_element_vector_inititalize (&table->element_vector, table->array_size * 5);

	/* allocate the bucket array */
	table->hash_array = (int *)real_malloc (table->array_size * sizeof(int));

	/* initialize the to empty state */
	memset (table->hash_array, -1, table->array_size * sizeof(int));
}

static void
nautilus_leak_hash_table_finalize (NautilusLeakHashTable *table)
{
	nautilus_leak_hash_element_vector_finalize (&table->element_vector);
	real_free (table->hash_array);
}

NautilusLeakHashTable *
nautilus_leak_hash_table_new (size_t initial_size)
{
	NautilusLeakHashTable *new_table;
	new_table = real_malloc (sizeof(NautilusLeakHashTable));
	nautilus_leak_hash_table_initialize (new_table, initial_size);

	return new_table;
}

void
nautilus_leak_hash_table_free (NautilusLeakHashTable *hash_table)
{
	nautilus_leak_hash_table_finalize (hash_table);
	real_free (hash_table);
}

static unsigned long
nautilus_leak_hash_table_hash (NautilusLeakHashTable *table, unsigned long seed)
{
	return (seed >> 2) % table->array_size;
}

NautilusHashEntry *
nautilus_leak_hash_table_find (NautilusLeakHashTable *table, unsigned long key)
{
	int index;
	NautilusHashEntry *result;

	for (index = table->hash_array [nautilus_leak_hash_table_hash (table, key)]; index >= 0;) {
		result = nautilus_leak_hash_element_vector_at (&table->element_vector, index);
		if (nautilus_leak_hash_element_match (result, key)) {
			return result;
		}
		index = result->next;
	}

	return NULL;
}

NautilusHashEntry *
nautilus_leak_hash_table_add (NautilusLeakHashTable *table, unsigned long key)
{
	int new_index;
	NautilusHashEntry *result;
	unsigned long hash;
	
	/* calculate the index of the bucket */
	hash = nautilus_leak_hash_table_hash (table, key);

	/* allocate space for new item in element vector */
	new_index = nautilus_leak_hash_element_vector_add (&table->element_vector);
	result = nautilus_leak_hash_element_vector_at (&table->element_vector, new_index);

	/* insert new item first in the list for bucket <hash> */
	result->next = table->hash_array[hash];
	table->hash_array[hash] = new_index;

	return result;
}

static void
nautilus_leak_hash_table_remove_element (NautilusLeakHashTable *table, NautilusHashEntry *element)
{
	unsigned long hash;
	int next;
	int index;
	NautilusHashEntry *tmp_element;

	/* find the bucket */
	hash = nautilus_leak_hash_table_hash (table, nautilus_leak_hash_element_hash (element));
	next = table->hash_array[hash];

	g_assert (next >= 0);

	/* try to match bucket list head */
	if (nautilus_leak_hash_element_vector_at (&table->element_vector, next) == element) {
		table->hash_array[hash] = element->next;
		nautilus_leak_hash_element_vector_remove (&table->element_vector, next);
		return;
	}

	for (index = next; index >= 0; ) {
		/* look for an existing match in table */
		next = nautilus_leak_hash_element_vector_at (&table->element_vector, index)->next;
		if (next < 0) {
			g_assert (!"should not be here");
			return;
		}
		
		tmp_element = nautilus_leak_hash_element_vector_at (&table->element_vector, index);
		if (nautilus_leak_hash_element_vector_at (&table->element_vector, next) == element) {
			nautilus_leak_hash_element_vector_at (&table->element_vector, index)->next = element->next;
			nautilus_leak_hash_element_vector_remove (&table->element_vector, next);
			return;
		}
		index = next;
	}
}

gboolean
nautilus_leak_hash_table_remove (NautilusLeakHashTable *table, unsigned long key)
{
	NautilusHashEntry *element;

	element = nautilus_leak_hash_table_find (table, key);
	if (element != NULL) {
		/* FIXME: this could be faster if we just found the element
		 * here and deleted it.
		 */
		nautilus_leak_hash_table_remove_element (table, element);

		return TRUE;
	}
	return FALSE;
}

struct NautilusLeakTable {
	size_t size;
	NautilusLeakTableEntry *data;
};

static NautilusLeakTableEntry *
nautilus_leak_table_new_entry_at (NautilusLeakTable *table, int index)
{
	/* Allocate a new slot. Avoid using real_realloc here because
	 * it ends up calling our version of __libc_malloc and messes up
	 * the leak table
	 */
	NautilusLeakTableEntry *new_table = (NautilusLeakTableEntry *) real_malloc 
		((table->size + 1) * sizeof (NautilusLeakTableEntry));

	if (new_table == NULL) {
		g_warning ("Ran out of memory while allocating leak checker structures");
		abort ();
	}

	/* finish what realloc would have done if we could call it */
	memcpy (new_table, table->data, (table->size) * sizeof (NautilusLeakTableEntry));
	real_free (table->data);
	table->data = new_table;

	/* move the items over by one to make room for new item */
	if (index < table->size) {
		memmove (&table->data[index + 1], 
			&table->data[index],
			(table->size - index) * sizeof (NautilusLeakTableEntry));
	}
	
	table->size++;

	return &table->data[index];
}

static void
nautilus_leak_table_add_entry (NautilusLeakTable *table, NautilusHashEntry *entry, int stack_grouping_depth)
{
	int r, l;
	int resulting_index;
	int compare_result;

	/* do a binary lookup of the item */
	r = table->size - 1;
	resulting_index = 0;
	compare_result = 0;
	
	for (l = 0; l <= r; ) {
		resulting_index = (l + r) / 2;

		compare_result = nautilus_leak_stack_crawl_compare (
			table->data[resulting_index].sample_allocation->stack_crawl, 
			entry->data.stack_crawl,
			stack_grouping_depth);
		
		if (compare_result > 0) {
			r = resulting_index - 1;
		} else if (compare_result < 0) {
			l = resulting_index + 1;
		} else {
			break;
		}
	}

	if (compare_result < 0) {
		resulting_index++;
	}

	if (compare_result == 0 && resulting_index < table->size) {
		/* we already have a match, just bump up the count and size */
		 table->data[resulting_index].count++;
		 table->data[resulting_index].total_size += entry->data.size;
		 return;
	}
	
	nautilus_leak_table_new_entry_at (table, resulting_index);
	table->data[resulting_index].count = 1;
	table->data[resulting_index].total_size = entry->data.size;
	table->data[resulting_index].sample_allocation = nautilus_leak_allocation_record_copy (&entry->data);
}

NautilusLeakTable *
nautilus_leak_table_new (NautilusLeakHashTable *hash_table, int stack_grouping_depth)
{
	NautilusLeakTable *result;
	NautilusHashEntry *nautilus_leak_hash_table_entry;
	int index;

	result = real_malloc (sizeof(NautilusLeakTable));
	result->size = 0;
	result->data = NULL;

	for (index = 0; index < hash_table->element_vector.size; index++) {
		/* traverse the hash table element vector */
		nautilus_leak_hash_table_entry = nautilus_leak_hash_element_vector_at (&hash_table->element_vector, index);
		if (nautilus_leak_hash_table_entry->data.stack_crawl != NULL) {
			nautilus_leak_table_add_entry (result, nautilus_leak_hash_table_entry, stack_grouping_depth);
		}
	}

	return result;
}

void
nautilus_leak_table_free (NautilusLeakTable *leak_table)
{
	int index;
	if (leak_table != NULL) {
		for (index = 0; index < leak_table->size; index++) {
			nautilus_leak_allocation_record_free(leak_table->data[index].sample_allocation);
		}
		real_free (leak_table->data);
	}
	
	real_free (leak_table);
}

static int
sort_by_count (const void *entry1, const void *entry2)
{
	int result;
	
	result = ((NautilusLeakTableEntry *)entry2)->count 
		- ((NautilusLeakTableEntry *)entry1)->count;
		
	if (result == 0) {
		/* match, secondary sort order by size */
		return ((NautilusLeakTableEntry *)entry2)->total_size 
			- ((NautilusLeakTableEntry *)entry1)->total_size;
	}
	return result;
}

void
nautilus_leak_table_sort_by_count (NautilusLeakTable *leak_table)
{
	qsort (leak_table->data, leak_table->size, 
		sizeof(NautilusLeakTableEntry), sort_by_count);
}

static int
sort_by_size (const void *entry1, const void *entry2)
{
	int result; 
	
	result = ((NautilusLeakTableEntry *)entry2)->total_size 
		- ((NautilusLeakTableEntry *)entry1)->total_size;

	if (result == 0) {
		/* match, secondary sort order by count */
		return ((NautilusLeakTableEntry *)entry2)->count 
			- ((NautilusLeakTableEntry *)entry1)->count;
	}
	return result;
}

void
nautilus_leak_table_sort_by_size (NautilusLeakTable *leak_table)
{
	qsort (leak_table->data, leak_table->size, 
		sizeof (NautilusLeakTableEntry), sort_by_size);
}

void
nautilus_leak_table_each_item (NautilusLeakTable *leak_table, NautilusEachLeakTableFunction function, 
	void *context)
{
	int index;
	for (index = 0; index < leak_table->size; index++) {
		if (!function (&leak_table->data[index], context)) {
			/* break early */
			return;
		}
	}
}

#endif