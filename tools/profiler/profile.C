/*
 * Cprof profiler tool
 * (C) Copyright 1999-2000 Corel Corporation   
 * Copyright (C) 2000 Eazel
 * 
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  
 *  adapted for shared library support:
 *  Pavel Cisler <pavel@eazel.com>
 */

#define _XOPEN_SOURCE 500	/* for pwrite */

/* Define to whatever is needed to declare a stdcall function. */
#define STDCALL __attribute__((__stdcall__))

/* Define to whatever prefix is used on pthread calls. */
#define PTHREAD_PREFIX pthread_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "profileP.h"
#include "machine-profileP.h"
#include "profiledata.h"
#include "totaltime.h"
#include "funcsummary.h"
#include "symbol-table.h"

/************************************************************************/

static enum mstatus status = mstatus_on;

static size_t maplen;

static profile_header *header;

static arc *arc_table;

static pthread_key_t prof_key;

static int dev_zero_fd;

static thread_prof *threads;

static int profile_map_fd;

static size_t thread_maplen;

static const char CPROF_STACK_SIZE[] = "CPROF_STACK_SIZE";
static const char CPROF_ARC_TABLE_SIZE[] = "CPROF_ARC_TABLE_SIZE";
static const char CPROF_DUMP_PREFIX[] = "CPROF_DUMP_PREFIX";

static size_t stack_size;
static size_t arc_table_size;
static char *dump_prefix;

/************************************************************************/

static void profile_exit();
static inline arc *allocate_arc(thread_prof *thread, codeptr_t to);

/* Temporarily installed during determine_frequency():
 * sigsuspend() does not wake up for SIG_IGN signals. */
static void 
alarm_handler(int)
{
}


/* Determine (and return) the number of profctr_t ticks per second. */
static profctr_t 
determine_frequency()
{
	itimerval delay, oldtimer;
	timeval tbefore, tafter;
	profctr_t before, after, delta;
	double elapsed;
	void (*old_alarm_handler)(int);
	sigset_t sigs;
	
	/* We steal the SIGALRM handler as well as ITIMER_REAL. This is very
	 * rude, but unavoidable. */
	
	old_alarm_handler = signal(SIGALRM, alarm_handler);
	
	sigfillset(&sigs);
	sigdelset(&sigs, SIGALRM);
	sigdelset(&sigs, SIGKILL);	/* we aren't allowed to block these */
	sigdelset(&sigs, SIGSTOP);
	
	/* Set this to whatever you want: we use gettimeofday to determine
	 * how much time has actually elapsed. */
	delay.it_interval.tv_sec = 0;
	delay.it_interval.tv_usec = 0;
	delay.it_value.tv_sec = 0;
	delay.it_value.tv_usec = 1000000;
	
	setitimer(ITIMER_REAL, &delay, &oldtimer);
	
	get_timestamp(before);
	gettimeofday(&tbefore, NULL);
	
	sigsuspend(&sigs);
	
	get_timestamp(after);
	gettimeofday(&tafter, NULL);
	
	/* In seconds */
	elapsed = tafter.tv_sec - tbefore.tv_sec
		+ (tafter.tv_usec - tbefore.tv_usec) / 1000000.0;
	
	delta = after - before;
	
	signal(SIGALRM, old_alarm_handler);
	setitimer(ITIMER_REAL, &oldtimer, &delay);
	
	return (profctr_t)(delta / elapsed);
}

static unsigned long 
getenv_ul(const char *name, unsigned long defult)
{
	char *value, *endptr;
	unsigned long ret;
	
	value = getenv(name);
	if (value == NULL || value[0] == '\0')
		return defult;
	
	ret = strtoul(value, &endptr, 10);
	if (*endptr != '\0' || (ret == ULONG_MAX && errno == ERANGE))
		return defult;
	
	return ret;
}

static char *
getenv_str(const char *name, const char *defult, int empty_ok)
{
	const char *value = getenv(name);
	if (!value || (value[0] == '\0' && !empty_ok))
		value = defult;
	
	return strdup(value);
}

static void 
load_environment()
{
	stack_size = getenv_ul(CPROF_STACK_SIZE, STACK_SIZE);
	arc_table_size = getenv_ul(CPROF_ARC_TABLE_SIZE, ARC_TABLE_SIZE);
	dump_prefix = getenv_str(CPROF_DUMP_PREFIX, "cmon.out", 0);
}

static void 
prof_init()
{
	load_environment();
	
	maplen = getpagesize() + arc_table_size * sizeof(arc);
	thread_maplen = stack_size * sizeof(stack_entry);
	
	if (pthread_key_create(&prof_key, NULL) != 0) 
		abort ();
	
	if (atexit(profile_exit) == -1) 
		abort ();
	
	threads = NULL;
	
	dev_zero_fd = open("/dev/zero", O_RDWR);
	if (dev_zero_fd == -1) 
		abort ();
	
	profile_map_fd = open(dump_prefix, O_RDWR | O_CREAT | O_NOCTTY | O_TRUNC, 0664);
	
	if (profile_map_fd == -1) 
		abort ();
	
	ftruncate(profile_map_fd, maplen);
	
	header = (profile_header *)mmap(NULL, maplen, PROT_READ | PROT_WRITE, MAP_SHARED,
	profile_map_fd, 0);
	if (header == (profile_header *)-1) 
		abort ();
	
	arc_table = (arc *)((char *)header + getpagesize());
	
	header->magic = CPROF_MAGIC;
	gettimeofday(&header->start_time, NULL);
	header->frequency = determine_frequency();
	header->num_arcs = arc_table_size;
	header->stack_size = stack_size;
	header->arc_table_addr = (uintptr_t)arc_table;
	header->valid_arcs = 0;
	header->num_threads = 0;
	
	mmap(arc_table + arc_table_size, getpagesize(), PROT_NONE,
		MAP_FIXED | MAP_PRIVATE, dev_zero_fd, 0);

}

static void 
prof_init_if_needed()
{
	static pthread_once_t once_control = PTHREAD_ONCE_INIT;
	pthread_once(&once_control, prof_init);
}


static thread_prof *
new_thread_profile()
{
	prof_init_if_needed();
	
	size_t thread_num = atomic_inc(&header->num_threads);
	off_t mapoff = maplen + thread_num * thread_maplen;
	
	/* We have to extend the file so that it is large enough to hold
	* our additional mapping. Normally we'd use ftruncate, but that
	* causes concurrency problems: we might accidently shorten the file. */
	char c = 0;
	pwrite(profile_map_fd, &c, 1, mapoff + thread_maplen - 1);
	
	thread_prof *thread = (thread_prof *)mmap(NULL, 
		stack_size * sizeof(stack_entry),
		PROT_READ | PROT_WRITE, MAP_SHARED, profile_map_fd, mapoff);
	
	if (thread == (thread_prof *)-1) 
		abort();
	
	/* watch the alignment of stacktop. It's ok in 32-bit systems. */
	thread->stacktop
		= (stack_entry *)((char *)thread + sizeof(thread_prof));
	
	arc *root_arc = allocate_arc(thread, (codeptr_t)NULL);
	
	root_arc->to = (codeptr_t)NULL;
	root_arc->next = NULL;
	root_arc->func_and_children = 0;
	root_arc->count = 0;
	root_arc->chain = NULL;
	
	thread->root_arc = root_arc;
	
	/* Now we have to push a toplevel stack entry, since the rest of
	* menter will look at stacktop->current_arc */
	thread->stacktop->current_arc = thread->root_arc;
	get_timestamp(thread->stacktop->entry_timestamp);
	
	thread->pid = getpid();
	
	pthread_setspecific(prof_key, thread);
	
	/* Put us on the linked list of threads. */
	for (;;) {
		thread_prof *threads_head = threads;
		thread->next = threads_head;
		if (compare_and_swap((void **)&threads, threads_head, thread))
			break;
	}
	
	return thread;
}

static inline arc *
allocate_arc(thread_prof *, codeptr_t to)
{
	arc *result = arc_table + atomic_inc(&header->valid_arcs);
	
	result->to = to;
	
	assert(result->func_and_children == 0);
	assert(result->count == 0);
	assert(result->chain == NULL);
	
	return result;
}

static inline arc *
find_arc(thread_prof *thread, arc *from, codeptr_t to)
{
	for (arc *chain = from->chain; chain != NULL; chain = chain->next) 
		if (chain->to == (codeptr_t)to)
			return chain;

	
	/* So allocate a new arc, and put it on the head of the chain. */
	arc *newarc = allocate_arc(thread, to);
	newarc->next = from->chain;
	from->chain = newarc;
	
	return newarc;
}

static inline thread_prof *
get_thread_profile()
{
	thread_prof *thread = (thread_prof *)pthread_getspecific(prof_key);
	
	if (!thread) 
		thread = new_thread_profile();
	return thread;
}

static codeptr_t
resolve_shared_library_address (codeptr_t address)
{
	// Detect if we have an actual routine address or an address of a GOT.
	// If the latter is the case, follow the address to our routine.
	// This only works after the library has been loaded and relocated.
	const unsigned char *byte_stream = (const unsigned char *)address;
	if (byte_stream[0] == 0xff && byte_stream[1] == 0x25) {
		// jmp * - get the real routine address at the
		// jump destination
		const codeptr_t *indirect_location = *(const codeptr_t **)(byte_stream + 2);
		return *indirect_location;
	}
	return address;
}

extern "C" void __attribute((__stdcall__)) 
__menter_internal (codeptr_t func)
{
	func = resolve_shared_library_address (func);
	thread_prof *thread = get_thread_profile();
	volatile stack_entry *stacktop = thread->stacktop;
	arc *current_arc = find_arc(thread, stacktop->current_arc, func);
	
	stacktop++;
	
	/* Dodge a signal race: We can't bump thread->stacktop until current_arc
	 * is there or a signal handler will pass a bogus arc to find_arc. */
	stacktop->current_arc = current_arc;
	thread->stacktop++;	/* should be atomic */
	/* And if a signal came in before thread->stacktop++, it will have
	 * overwritten stacktop->current_arc, so put it back. */
	stacktop->current_arc = current_arc;
	
	get_timestamp(stacktop->entry_timestamp);
	
	if (status == mstatus_on)
		current_arc->count++;
	
}

extern "C" void __attribute((__stdcall__)) 
__mexit_internal (void)
{
	thread_prof *thread = get_thread_profile();
	volatile stack_entry *stacktop = thread->stacktop;
	arc *current_arc = stacktop->current_arc;
	
	if (status == mstatus_on) {
		/* We have to decrement thread->stacktop before reading the timestamp,
		 * so that any signals that happen after we read the timestamp get
		 * changed against our parent. Note that we have to read the
		 * entry_timestamp before decrementing thread->stacktop or a signal
		 * might overwrite it. 
		 */
		profctr_t entry_timestamp = stacktop->entry_timestamp;
		profctr_t delta;
		
		thread->stacktop--;	/* should be atomic */
		get_timestamp(delta);
		
		delta -= entry_timestamp;
		update_arc_time(current_arc->func_and_children, delta);
	} else
		thread->stacktop--;	/* should be atomic */
}

void 
profile_on()
{
    status = mstatus_on;
}

void 
profile_off()
{
    status = mstatus_off;
}

/* Due to the evil stride, this is not going to be very fast. */
void 
profile_reset() 
{
    mstatus oldstatus = status;
    status = mstatus_off;

    const arc *end = arc_table + header->valid_arcs;
    for (arc *a = arc_table; a < end; a++)  {
		a->func_and_children = 0;
		a->count = 0;
    }

    status = oldstatus;
}

static void
profile_dump()
{
	try {
		FuncSummary::sort_order order = FuncSummary::sort_func;
		ProfileData profile(profile_map_fd);
		SymbolTable symbols("/gnome/bin/nautilus");
		TotalTime tt(profile);

		FuncSummary fs(profile, symbols, order, tt.total_time());
		fs.Output(cout);

//		GprofStack gs(profile, symbols);
//		gs.Output(cout);

//		CallStack cs(profile, symbols);
//		cs.Output(cout);
	} catch (const exception &e) {
		cerr << e.what() << '\n';
	}
	catch (...) {
		cerr << "Unknown exception\n";
	}
	close (profile_map_fd);
	unlink (dump_prefix);
}

static void 
profile_exit()
{
    profile_dump();
    printf("profiler done\n");
}
