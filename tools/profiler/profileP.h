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

#ifndef PROFILE_PRIVATE_H_INCLUDED
#define PROFILE_PRIVATE_H_INCLUDED

#include "profile.h"

#define ARC_TABLE_SIZE	(16 * 1024 * 1024)	/* global */
#define STACK_SIZE	1024	/* 12k per-thread */
/* And we put the thread_prof structure onto the stack. */

enum mstatus
{
    mstatus_on = 0,
    mstatus_off,
    mstatus_error
};

#ifndef INLINE1
#define INLINE1 inline
#endif

#ifndef INLINE2
#define INLINE2 inline
#endif

#endif
