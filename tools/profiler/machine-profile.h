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

/* Public, machine-dependent definitions */

#ifndef MACHINE_PROFILE_H_INCLUDED
#define MACHINE_PROFILE_H_INCLUDED

#include <stdint.h>

/* An integer large enough to hold a data or function pointer. */
typedef uintptr_t codeptr_t;

/* An arithmetic used to represent both absolute times
 * and (non-negative) deltas. */
typedef unsigned long long profctr_t;

#endif
