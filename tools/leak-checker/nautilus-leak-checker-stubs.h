/* nautilus-leak-checker-stubs.h - simple leak checking library
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
   based on MemProf by Owen Taylor, <otaylor@redhat.com>
*/

#ifndef LEAK_CHECKER_STUBS_H
#define LEAK_CHECKER_STUBS_H


extern void *(* real_malloc) (size_t size);
extern void *(* real_memalign) (size_t boundary, size_t size);
extern void *(* real_realloc) (void *ptr, size_t size);
extern void *(* real_calloc) (void *ptr, size_t size);
extern void (*real_free) (void *ptr);

     
void *__libc_malloc (size_t size);
void *__libc_memalign (size_t boundary, size_t size);
void *__libc_calloc (size_t count, size_t size);
void *__libc_realloc (void *ptr, size_t size);
void __libc_free (void *ptr);

/* Records the context of an allocation.
 * We could add pid, allocation time, etc. if needed. 
 */
typedef struct {
	/* pointer returned by malloc/realloc */
	void 	*block;

	/* allocated size */
	size_t 	size;

	/* NULL-terminated array of return addresses */
	void 	**stack_crawl;
} NautilusLeakAllocationRecord;

void 			nautilus_leak_allocation_record_finalize
					 	(NautilusLeakAllocationRecord *record);
void 			nautilus_leak_allocation_record_free 	(NautilusLeakAllocationRecord 	*record);
void 			nautilus_leak_allocation_record_init 	(NautilusLeakAllocationRecord 	*record, 
								 void 				*block, 
								 size_t 			initial_size, 
								 void 				**stack_crawl, 
								 int 				max_depth);
NautilusLeakAllocationRecord 	*nautilus_leak_allocation_record_copy 
						(const NautilusLeakAllocationRecord *record);

int 			nautilus_leak_stack_crawl_compare (void	**stack_crawl1, 
							   void **stack_crawl2, 
							   int 	levels);

/* Hash table entry. */
struct NautilusHashEntry {
	int 				next;
	NautilusLeakAllocationRecord 	data;
};


#endif