/* nautilus-leak-checker.h - simple leak checking library
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

#ifndef LEAK_CHECKER_H
#define LEAK_CHECKER_H

#define LEAK_CHECKER 1

#if LEAK_CHECKER

#define __USE_GNU
#define _GNU_SOURCE
/* need this for dlsym */
#include <pthread.h>
#include <glib.h>

/* This is a leakchecker simpler than MemProf - it tracks all the outstanding
 * allocations and allows you print a total when your app quits. It doesn't actually
 * try to identify leaks like MemProf does. The entire leakchecker machinery runs
 * in the same process as the target app and shares the same heap for it's data
 * structures.
 */

extern void nautilus_leak_checker_init 	(const char 	*app_path);

extern void nautilus_leak_print_leaks 	(int 		stack_grouping_depth, 
					 int 		stack_print_depth,
					 int 		max_count, 
					 gboolean 	sort_by_count);

#endif

#endif
