/* nautilus-leak-hash-table.h - hash table for a leak checking library
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

#ifndef LEAK_HASH_TABLE__
#define LEAK_HASH_TABLE__

typedef struct NautilusLeakHashTable NautilusLeakHashTable;
typedef struct NautilusHashEntry NautilusHashEntry;

NautilusLeakHashTable *nautilus_leak_hash_table_new 	(size_t 		initial_size);
void 		  nautilus_leak_hash_table_free 	(NautilusLeakHashTable 	*hash_table);
gboolean 	  nautilus_leak_hash_table_remove 	(NautilusLeakHashTable 	*table, 
					 		 unsigned long 		key);
NautilusHashEntry *nautilus_leak_hash_table_add 	(NautilusLeakHashTable 	*table, 
							 unsigned long 		key);
NautilusHashEntry *nautilus_leak_hash_table_find 	(NautilusLeakHashTable 	*table, 
							 unsigned long 		key);


typedef struct {
	int 				count;
	size_t 				total_size;
	NautilusLeakAllocationRecord 	*sample_allocation;
} NautilusLeakTableEntry;

typedef struct NautilusLeakTable NautilusLeakTable;
typedef gboolean (* NautilusEachLeakTableFunction) (NautilusLeakTableEntry *entry, void *context);

NautilusLeakTable *nautilus_leak_table_new		(NautilusLeakHashTable 	*hash_table, 
							 int 			stack_grouping_depth);
void		  nautilus_leak_table_free		(NautilusLeakTable 	*leak_table);
void		  nautilus_leak_table_sort_by_count	(NautilusLeakTable 	*leak_table);
void		  nautilus_leak_table_sort_by_size	(NautilusLeakTable 	*leak_table);
void		  nautilus_leak_table_each_item		(NautilusLeakTable 	*leak_table,
							 NautilusEachLeakTableFunction function, 
							 void 			*context);
#endif