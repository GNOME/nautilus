/*
 * Cprof profiler tool
 * (C) Copyright 1999-2000 Corel Corporation   
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
 */

#ifndef MACHINE_PROFILE_PRIVATE_H_INCLUDED
#define MACHINE_PROFILE_PRIVATE_H_INCLUDED

#include "machine-profile.h"

extern "C" {
	void __attribute((__stdcall__)) __menter_internal (codeptr_t func);
	void __attribute((__stdcall__)) __mexit_internal (void);
}
/* Note that we don't issue a serialising instruction (e.g. cpuid)
 * before reading the counter. Since we are profiling entire functions,
 * it doesn't matter.
 * Don't use this to determine how many cycles a single instruction takes.
 */
#define get_timestamp(t) \
	do							\
	{							\
		__asm__ __volatile__("pushl %%ebx\n"		\
				     "cpuid\n"			\
				     "popl %%ebx\n"		\
				     "rdtsc"			\
				     : "=A" (t) : : "cx");	\
	} while (0)

#define update_arc_time(arc_time, delta) \
	do { (arc_time) += (delta); } while (0)

static inline size_t atomic_inc(volatile size_t *x)
{
    size_t result;
    __asm__("lock; xaddl %1, %0"
	    : "=m" (*x), "=rm" (result)
	    : "1" (1));

    return result;
}

static inline int compare_and_swap(void **dest, void *old, void *_new)
{
    char ret;
    unsigned long readval;

    __asm__("lock; cmpxchgl %3, %1; sete %0"
	    : "=q" (ret), "=m" (*dest), "=a" (readval)
	    : "r" (_new), "m" (*dest), "a" (old));

    return ret;
}

#endif
