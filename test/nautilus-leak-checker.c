/* nautilus-leak-checker.c - simple leak checking library
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

#include "nautilus-leak-checker.h"
/* included first, defines following switch*/
#if LEAK_CHECKER

#include <malloc.h>
#include <dlfcn.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nautilus-leak-checker-stubs.h"
#include "nautilus-leak-hash-table.h"
#include "nautilus-leak-symbol-lookup.h"

/* this is the maximum number of stack crawl levels we can capture */
enum {
	TRACE_ARRAY_MAX = 512
};

static volatile gboolean nautilus_leak_hooks_initialized;
static volatile gboolean nautilus_leak_initializing_hooks;
static volatile gboolean nautilus_leak_check_leaks;

void *(* real_malloc) (size_t size);
void *(* real_memalign) (size_t boundary, size_t size);
void *(* real_realloc) (void *ptr, size_t size);
void *(* real_calloc) (void *ptr, size_t size);
void (* real_free) (void *ptr);
int  (* real_start_main) (int (*main) (int, char **, char **), int argc, 
			  char **argv, void (*init) (void), void (*fini) (void), 
			  void (*rtld_fini) (void), void *stack_end);



static const char *app_path;

void
nautilus_leak_allocation_record_init (NautilusLeakAllocationRecord *record, 
	void *block, size_t initial_size, void **stack_crawl, int max_depth)
{
	int stack_depth, index;
	
	record->block = block;
	record->size = initial_size;

	for (index = 0; index < max_depth; index++) {
		if (stack_crawl[index] == NULL)
			break;
	}
	
	stack_depth = index;

	/* call real_malloc to avoid recursion and messing up results */
	record->stack_crawl = real_malloc ((stack_depth + 1) * sizeof(void *));
	memcpy (record->stack_crawl, stack_crawl, stack_depth * sizeof(void *));
	record->stack_crawl[stack_depth] = NULL;
}

NautilusLeakAllocationRecord *
nautilus_leak_allocation_record_copy (const NautilusLeakAllocationRecord *record)
{
	int stack_depth, index;
	NautilusLeakAllocationRecord *result;

	result = real_malloc (sizeof(*result));

	result->block = record->block;
	result->size = record->size;
	
	for (index = 0; ; index++) {
		if (record->stack_crawl[index] == NULL)
			break;
	}
	stack_depth = index;
	result->stack_crawl = real_malloc ((stack_depth + 1) * sizeof(void *));
	memcpy (result->stack_crawl, record->stack_crawl, (stack_depth + 1) * sizeof(void *));

	return result;
}

void 
nautilus_leak_allocation_record_finalize (NautilusLeakAllocationRecord *record)
{
	/* call real_free to avoid recursion and messing up results */
	real_free (record->stack_crawl);
}

void 
nautilus_leak_allocation_record_free (NautilusLeakAllocationRecord *record)
{
	/* call real_free to avoid recursion and messing up results */
	real_free (record->stack_crawl);
	real_free (record);
}

/* return a strcmp-like result to be used in sort funcitons */
int 
nautilus_leak_stack_crawl_compare (void **stack_crawl1, void **stack_crawl2, int levels)
{
	int index;
	for (index = 0; index < levels; index++) {
		if (stack_crawl1 [index] == NULL && stack_crawl2 [index] == NULL) {
			return 0;
		}
		
		if (stack_crawl1 [index] < stack_crawl2 [index]) {
			return -1;
		} else if (stack_crawl1 [index] > stack_crawl2 [index]) {
			return 1;
		}
	}

	return 0;
}

static void 
nautilus_leak_initialize (void) 
{
	/* Locate the original malloc calls. The dlsym calls
	 * will allocate memory when doing lookups. We use a special
	 * trick to deal with the fact that real_malloc is not set up
	 * yet while we are doing the first dlsym -- see malloc.
	 */
	real_malloc = dlsym (RTLD_NEXT, "__libc_malloc");
	real_realloc = dlsym (RTLD_NEXT, "__libc_realloc");
	real_free = dlsym (RTLD_NEXT, "__libc_free");
	real_memalign = dlsym (RTLD_NEXT, "__libc_memalign");
	real_calloc = dlsym (RTLD_NEXT, "__libc_calloc");
	real_start_main = dlsym (RTLD_NEXT, "__libc_start_main");

	nautilus_leak_hooks_initialized = TRUE;
	nautilus_leak_check_leaks = TRUE;
}

typedef struct StackFrame StackFrame;

struct StackFrame {
	StackFrame *next_frame;
	void *return_address;
	/* first argument is here */
};

static void
get_stack_trace (void **trace_array, int trace_array_max)
{
	int index;
	const StackFrame *stack_frame;

	/* point to the stack frame pointer, two words before the
	 * address of the first argument
	 */
	stack_frame = (const StackFrame *)((void **)&trace_array - 2);


	/* Record stack frames; skip first two in the malloc calls. */
	for (index = -2; index < trace_array_max; index++) {
		if (index >= 0) {
			/* Return address is the next pointer after
			 * stack frame. 
			 */
			trace_array [index] = stack_frame->return_address;
		}
		stack_frame = stack_frame->next_frame;
		if (stack_frame == NULL) {
			break;
		}
	}
	if (index < trace_array_max) {
		trace_array [index] = NULL;
	}
}

/* Figure out if one of the malloc calls is reentering itself.
 * There seems no cleaner way to handle this -- we want to override
 * malloc calls at the __libc_* level. 
 * The malloc hooks recurse when initializing themselves.
 * When this happens we need a reliable way to tell that a recursion is
 * underway so as to not record the corresponding malloc call twice
 */
static gboolean
detect_reentry (void *parent_caller)
{
	int count;
	const StackFrame *stack_frame;

	stack_frame = (const StackFrame *)((void **)&parent_caller - 2);
	stack_frame = stack_frame->next_frame;

	for (count = 0; count < 7; count++) {
		/* See if we return address on the stack
		 * that is near the parent_caller -- the start address
		 * of the calling function.
		 * We are using two arbitrary numbers here - 5 should be a "deep enough"
		 * check to detect the recursion, 0x40 bytes should be a large enough distance
		 * from the start of the malloc call to the point where the old malloc call is
		 * being called.
		 * FIXME: this the value 0x40 works well only with certain function sizes, optimization levels,
		 * etc. need a more robust way of doing this. One way might be adding stuffing into the 
		 * __libc_* calls that is never executed but makes the funcitons longer. We could
		 * then up the value without the danger of hitting the next function
		 */
		if (stack_frame->return_address >= parent_caller
			&& stack_frame->return_address < (void *)((char *)parent_caller + 0x40)) {
			/* printf("detected reentry at level %d\n", count); */
			return TRUE;
		}
		stack_frame = stack_frame->next_frame;
		if (stack_frame == NULL) {
			break;
		}
	}
	return FALSE;
}

static pthread_mutex_t nautilus_leak_hash_table_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static NautilusLeakHashTable *hash_table;

static int nautilus_leak_malloc_count;
static size_t nautilus_leak_malloc_outstanding_size;

static void
nautilus_leak_record_malloc (void *ptr, size_t size)
{
	void *trace_array [TRACE_ARRAY_MAX];
	NautilusHashEntry *element;

	/* printf("new block %p, %d\n", ptr, size); */

	if (!nautilus_leak_check_leaks)
		return;

	++nautilus_leak_malloc_count;
	nautilus_leak_malloc_outstanding_size += size;

	get_stack_trace (trace_array, TRACE_ARRAY_MAX);

	pthread_mutex_lock (&nautilus_leak_hash_table_mutex);

	if (hash_table == NULL) {
		hash_table = nautilus_leak_hash_table_new (10 * 1024);
	}
	if (nautilus_leak_hash_table_find (hash_table, (gulong)ptr) != NULL) {
		printf("*** block %p appears to already be allocated "
			"- someone must have sneaked a free past us\n", ptr);
		nautilus_leak_hash_table_remove (hash_table, (gulong)ptr);
	}
	/* insert a new item into the hash table, using the block address as the key */
	element = nautilus_leak_hash_table_add (hash_table, (gulong)ptr);
	
	/* fill out the new allocated element */
	nautilus_leak_allocation_record_init (&element->data, ptr, size, trace_array, TRACE_ARRAY_MAX);

	pthread_mutex_unlock (&nautilus_leak_hash_table_mutex);
}

static void
nautilus_leak_record_realloc (void *old_ptr, void *new_ptr, size_t size)
{
	void *trace_array [TRACE_ARRAY_MAX];
	NautilusHashEntry *element;

	/* printf("reallocing block %p, %d was %p\n", new_ptr, size, old_ptr); */

	if (!nautilus_leak_check_leaks)
		return;

	get_stack_trace (trace_array, TRACE_ARRAY_MAX);

	pthread_mutex_lock (&nautilus_leak_hash_table_mutex);

	/* must have hash table by now */
	g_assert (hash_table != NULL);
	/* must have seen the block already */
	element = nautilus_leak_hash_table_find (hash_table, (gulong)old_ptr);
	if (element == NULL) {
		printf("*** we haven't seen block %p yet "
			"- someone must have sneaked a malloc past us\n", old_ptr);
	} else {
		nautilus_leak_malloc_outstanding_size -= element->data.size;
		nautilus_leak_hash_table_remove (hash_table, (gulong)old_ptr);
	}

	/* shouldn't have this block yet */
	if (nautilus_leak_hash_table_find (hash_table, (gulong)new_ptr) != NULL) {
		printf("*** block %p appears to already be allocated "
			"- someone must have sneaked a free past us\n", new_ptr);
		nautilus_leak_hash_table_remove (hash_table, (gulong)new_ptr);
	}

	/* insert a new item into the hash table, using the block address as the key */
	element = nautilus_leak_hash_table_add (hash_table, (gulong)new_ptr);
	nautilus_leak_malloc_outstanding_size += size;

	/* Fill out the new allocated element.
	 * This way the last call to relloc will be the stack crawl that shows up in the
	 * final balance.
	 */
	nautilus_leak_allocation_record_init (&element->data, new_ptr, size, trace_array, 
		TRACE_ARRAY_MAX);

	pthread_mutex_unlock (&nautilus_leak_hash_table_mutex);
}

static void
nautilus_leak_record_free (void *ptr)
{
	NautilusHashEntry *element;
	/* printf("freeing block %p\n", ptr); */
	if (!nautilus_leak_check_leaks)
		return;

	--nautilus_leak_malloc_count;

	pthread_mutex_lock (&nautilus_leak_hash_table_mutex);

	/* must have hash table by now */
	g_assert (hash_table != NULL);
	/* must have seen the block already */
	element = nautilus_leak_hash_table_find (hash_table, (gulong)ptr);
	if (element == NULL) {
		printf("*** we haven't seen block %p yet "
			"- someone must have sneaked a malloc past us\n", ptr);
	} else {
		nautilus_leak_malloc_outstanding_size -= element->data.size;
		nautilus_leak_hash_table_remove (hash_table, (gulong)ptr);
	}
	pthread_mutex_unlock (&nautilus_leak_hash_table_mutex);

}

static void
nautilus_leak_initialize_if_needed (void)
{
	if (nautilus_leak_hooks_initialized)
		return;

	if (nautilus_leak_initializing_hooks)
		/* guard against reentrancy */
		return;
		
	nautilus_leak_initializing_hooks = TRUE;
	nautilus_leak_initialize ();
	nautilus_leak_initializing_hooks = FALSE;
}

/* we are overlaying the original __libc_malloc */
enum {
	STARTUP_FALLBACK_MEMORY_SIZE = 1024
};

static int startup_fallback_memory_index = 0;
static char startup_fallback_memory [STARTUP_FALLBACK_MEMORY_SIZE];

/* If our malloc hook is not installed yet - for instance when we are being
 * called from the initialize_if_needed routine. We have to fall back on 
 * returning a chunk of static memory to carry us through the initialization.
 */
static void *
allocate_temporary_fallback_memory (ssize_t size)
{
	void *result;

	/* align to natural word boundary */
	size = (size + sizeof(void *) - 1 ) & ~(sizeof(void *) - 1);
	if (size + startup_fallback_memory_index 
		> STARTUP_FALLBACK_MEMORY_SIZE) {
		g_warning ("trying to allocate to much space during startup");
		return NULL;
	}
	result = &startup_fallback_memory [startup_fallback_memory_index];
	startup_fallback_memory_index += size;

	return result;
}

void *
__libc_malloc (size_t size)
{
	void *result;

	nautilus_leak_initialize_if_needed ();

	if (real_malloc == NULL) {
		return allocate_temporary_fallback_memory (size);
	}

	result = (*real_malloc) (size);

	if (result != NULL) {
		if (detect_reentry(&__libc_malloc) 
			|| detect_reentry(&__libc_realloc)
			|| detect_reentry(&__libc_memalign)) {
			/* printf("avoiding reentry in __libc_malloc, block %p\n", result); */
		} else {
			nautilus_leak_record_malloc (result, size);
		}
	}
	
	return result;
}

void *
__libc_memalign (size_t boundary, size_t size)
{
	void *result;

	nautilus_leak_initialize_if_needed ();
	result = (*real_memalign) (boundary, size);	

	if (result != NULL) {
		if (detect_reentry(&__libc_memalign)) {
			/* printf("avoiding reentry in __libc_memalign, block %p\n", result); */
		} else {
			nautilus_leak_record_malloc (result, size);
		}
	}

	return result;
}

/* We are implementing __libc_calloc by calling __libc_malloc and memset
 * instead of calling real_calloc for a reason. dlsym calls
 * calloc and this way we prevent recursion during initialization.
 * If we didn't do this, we would have to teach __libc_calloc to use
 * fallback startup memory like __libc_malloc does.
 */
void *
__libc_calloc (size_t count, size_t size)
{
	size_t total;
	void *result;

	total = count * size;
	nautilus_leak_initialize_if_needed ();
	result = __libc_malloc (total);
	
	if (result != NULL) {
		memset (result, 0, total);
	}

	return result;
}

void *
__libc_realloc (void *ptr, size_t size)
{
	void *result;
	nautilus_leak_initialize_if_needed ();
	result = (*real_realloc) (ptr, size);	

	if (detect_reentry(&__libc_realloc)) {
		/* printf("avoiding reentry in __libc_realloc, block %p, old block %p\n", 
			result, ptr); */
	} else {
		if (result != NULL && ptr == NULL) {
			/* we are allocating a new block */
			nautilus_leak_record_malloc (result, size);
		} else {
			nautilus_leak_record_realloc (ptr, result, size);
		}
	}
	return result;
}

void
__libc_free (void *ptr)
{
	nautilus_leak_initialize_if_needed ();

	if (ptr > (void *)&startup_fallback_memory[0] 
 		&& ptr < (void *)&startup_fallback_memory[STARTUP_FALLBACK_MEMORY_SIZE]) {
		/* this is a temporary startup fallback block, don't do anything
		 * with it
		 */
		return;
 	}
 	if (ptr != NULL) {
		if (detect_reentry(&__libc_realloc)) {
			/* printf("avoiding reentry in __libc_free, block %p\n", ptr); */
		} else {
			nautilus_leak_record_free (ptr);
		}
	}

	(real_free) (ptr);
}

static void
print_leaks_at_exit (void)
{
	/* If leak checking, dump all the outstanding allocations just before exiting. */
	nautilus_leak_print_leaks (8, 15, 100, TRUE);
}

int
__libc_start_main (int (*main) (int, char **, char **), int argc, 
		   char **argv, void (*init) (void), void (*fini) (void), 
		   void (*rtld_fini) (void), void *stack_end)
{
	nautilus_leak_initialize_if_needed ();

	nautilus_leak_checker_init (argv[0]);

	g_atexit (print_leaks_at_exit);

	return real_start_main (main, argc, argv, init, fini,  rtld_fini, stack_end);
}

/* We try to keep a lot of code in between __libc_free and malloc to make
 * the reentry detection that depends on call address proximity work.
 */
typedef struct {
	int max_count;
	int counter;
	int stack_print_depth;
	int stack_match_depth;
} PrintOneLeakParams;

/* we don't care if printf, etc. allocates (as long as it doesn't leak)
 * because by now we have a snapshot of the leaks at the time of 
 * calling nautilus_leak_print_leaks
 */
static gboolean
print_one_leak (NautilusLeakTableEntry *entry, void *context)
{
	int index;
	PrintOneLeakParams *params = (PrintOneLeakParams *)context;

	printf("----------------- total_size %ld count %d -------------------\n",
		(long)entry->total_size, entry->count);

	for (index = 0; index < params->stack_print_depth; index++) {
		/* only print stack_grouping worth of stack crawl -
		 * beyond that different blocks may have different addresses
		 * and we would be printing a lie
		 */
		if (entry->sample_allocation->stack_crawl[index] == NULL)
			break;
		printf("  %c ", index >= params->stack_match_depth ? '?' : ' ');

		nautilus_leak_print_symbol_address (app_path, 
			entry->sample_allocation->stack_crawl[index]);
	}

	/* only print max_counter groups */
	return params->counter++ < params->max_count;
}

void
nautilus_leak_print_leaks (int stack_grouping_depth, int stack_print_depth, 
	int max_count, gboolean sort_by_count)
{
	NautilusLeakTable *temp_leak_table;
	PrintOneLeakParams each_context;

	pthread_mutex_lock (&nautilus_leak_hash_table_mutex);
	/* must have hash table by now */
	g_assert (hash_table != NULL);

	/* Build a leak table by grouping blocks with the same
	 * stackcrawls (stack_grouping_depth levels considered)
	 * from the allocated block hash table.
	 */
	temp_leak_table = nautilus_leak_table_new (hash_table, stack_grouping_depth);	
	pthread_mutex_unlock (&nautilus_leak_hash_table_mutex);

	printf("%d outstanding allocations %d bytes total ============ \n", 
		nautilus_leak_malloc_count, nautilus_leak_malloc_outstanding_size);
	printf("stack trace match depth %d\n", stack_grouping_depth);

	/* sort the leak table */
	if (sort_by_count) {
		nautilus_leak_table_sort_by_count (temp_leak_table);
	} else {
		nautilus_leak_table_sort_by_size (temp_leak_table);
	}

	/* we have a sorted table of all the leakers, we can print it out. */
	each_context.counter = 0;
	each_context.max_count = max_count;
	each_context.stack_print_depth = stack_print_depth;
	each_context.stack_match_depth = stack_grouping_depth;
	nautilus_leak_table_each_item (temp_leak_table, print_one_leak, &each_context);
	
	/* we are done with it, free the leak table */
	nautilus_leak_table_free (temp_leak_table);
	
	/* we are done with it, clean up cached up data used by the symbol lookup */
	nautilus_leak_print_symbol_cleanup ();
}

void 
nautilus_leak_checker_init (const char *path)
{
	/* we should get rid of this and find another way to find our
	 * binary's name
	 */
	printf("setting up the leakchecker for %s\n", path);
	app_path = path;

	g_atexit (print_leaks_at_exit);
}

void *
malloc (size_t size)
{
	return __libc_malloc (size);
}

void *
realloc (void *ptr, size_t size)
{
	return __libc_realloc (ptr, size);
}

void *
memalign (size_t boundary, size_t size)
{
	return __libc_memalign (boundary, size);
}

void *
calloc (size_t nmemb, size_t size)
{
	return __libc_calloc (nmemb, size);
}

void
free (void *ptr)
{
	__libc_free (ptr);
}

#endif

#ifdef LEAK_CHECK_TESTING
/* normally disabled */

static void
allocate_lots (int count)
{
	GList *list;
	GList *p;

	list = NULL;
	for (; count > 0; count--) {
		list = g_list_prepend (list, g_malloc (rand() % 256));
		list = g_list_prepend (list, NULL);
	}
	for (p = list; p != NULL; p = p->next) {
		g_free (p->data);
		p->data = NULL;
	}
	g_list_free (list);
}


static void
leak_mem2 (void)
{
	int i;
	for (i = 0; i < 40; i++) {
		g_strdup("bla");
	}
	allocate_lots (1280);
}

static void
leak_mem (void)
{
	int i;
	for (i = 0; i < 1010; i++) {
		malloc(13);
	}
	leak_mem2();
	allocate_lots (200);
}


int
main (int argc, char **argv)
{
	void *non_leak;
	void *leak;
	int i;

	nautilus_leak_checker_init (*argv);

	non_leak = g_malloc(100);
	leak = g_malloc(200);
	g_assert(non_leak != NULL);
	non_leak = g_realloc(non_leak, 1000);
	g_assert(non_leak != NULL);
	non_leak = g_realloc(non_leak, 10000);
	leak = g_malloc(200);
	non_leak = g_realloc(non_leak, 100000);
	leak = g_malloc(200);
	g_assert(non_leak != NULL);
	g_free(non_leak);

	non_leak = calloc(1, 100);
	g_assert(non_leak != NULL);
	g_free(non_leak);
	leak = g_malloc(200);

	non_leak = memalign(16, 100);
	g_assert(non_leak != NULL);
	g_free(non_leak);
	leak = g_malloc(200);
	leak = memalign(16, 100);
	leak = memalign(16, 100);
	leak = memalign(16, 100);
	leak = memalign(16, 100);
	leak = memalign(16, 100);
	leak = memalign(16, 100);

	for (i = 0; i < 13; i++) {
		leak = malloc(13);
	}

	leak_mem();
	leak_mem2();

	allocate_lots (1);
	for (i = 0; i < 100; i++) {
		allocate_lots(rand() % 40);
	}
	printf("done\n");
	nautilus_leak_print_leaks (6, 12, 100, TRUE);

	return 0;
}


#endif
