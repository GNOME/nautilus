/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-debug.h: Nautilus debugging aids.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@eazel.com>
*/

#ifndef NAUTILUS_DEBUG_H
#define NAUTILUS_DEBUG_H

#include <glib.h>

#ifdef G_DISABLE_ASSERT

#define nautilus_assert_computed_str(str_expr, expected_str)

#else /* !G_DISABLE_ASSERT */

gboolean nautilus_str_equal_with_free                          (char       *eat_this,
								const char *not_this);

#define nautilus_assert_computed_str(str_expr, expected_str) \
	g_assert (nautilus_str_equal_with_free ((str_expr), (expected_str)))

#endif /* !G_DISABLE_ASSERT */

void     nautilus_stop_in_debugger                             (void);
void     nautilus_make_warnings_and_criticals_stop_in_debugger (const char *first_domain,
								...);
int      nautilus_get_available_file_descriptor_count          (void);

#endif /* NAUTILUS_DEBUG_H */
