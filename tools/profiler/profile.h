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

/* Public definitions for the profiler. */

#ifndef PROFILE_H_INCLUDED
#define PROFILE_H_INCLUDED

#include <stdint.h>

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "machine-profile.h"

enum { CPROF_MAGIC = 0x4e4f4d43 };

struct profile_header
{
	uint32_t magic;		/* expect CPROF_MAGIC */
	struct timeval start_time;	/* time at which the run started */
	struct timeval end_time;	/* time at which the run was saved */
	
	profctr_t frequency;	/* counts per second */
	
	size_t num_arcs;		/* number of arcs (size of arc table) */
	size_t stack_size;		/* size of each thread's stack */
	
	size_t valid_arcs;		/* number of used arcs
							 * arcs are used from the start of the table */
	uintptr_t arc_table_addr;	/* virtual address of the arc table */
	
	size_t num_threads;		/* number of threads */
};

struct arc
{
    /* FROM is implicit: each arc is on a linked list that belongs
     * to an arc who's TO is our FROM. */

    codeptr_t to;	/* The destination of this arc. */
    struct arc *next;	/* The next node in our FROM's linked list. */

    profctr_t func_and_children;	/* Exit - Enter delta */
    size_t count;	/* number of times this arc has been traversed */
    struct arc *chain;	/* arcs called from the destination of this arc */
};

struct stack_entry
{
    struct arc *current_arc;
    profctr_t entry_timestamp;
};

struct thread_prof
{
    /* Our stack goes upwards through memory. */
    struct stack_entry *stacktop;

    struct arc *root_arc;

    struct thread_prof *next;

    pid_t pid;
};

/* Profiling defaults to on.
 * profile_on and profile_off just do the obvious thing. They don't save or
 * reset counters.
 *
 * profile_reset resets all the counters and elapsed times to 0.
 * profile_save saves the profiling data to the file named 
 */
extern void profile_on(void);
extern void profile_off(void);
extern void profile_reset(void);
extern int profile_save(const char *prefix);

#endif
