/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-string.h: String routines to augment <string.h>.

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Darin Adler <darin@eazel.com>
*/

#ifndef NAUTILUS_STRING_H
#define NAUTILUS_STRING_H

#include <glib.h>
#include <string.h>

/* Versions of basic string functions that allow NULL. */
size_t   nautilus_strlen            (const char *string_null_allowed);
char *   nautilus_strchr            (const char *haystack_null_allowed,
				     char        needle);
int      nautilus_strcmp            (const char *string_a_null_allowed,
				     const char *string_b_null_allowed);

/* Versions of basic string functions that free their parameters. */
int      nautilus_eat_strcmp        (char       *string_a_null_allowed_gets_freed,
				     const char *string_b_null_allowed);

/* Conversions to and from strings. */
gboolean nautilus_string_to_int     (const char *string,
				     int        *integer);
gboolean nautilus_eat_string_to_int (char       *string_gets_freed,
				     int        *integer);

#endif /* NAUTILUS_STRING_H */
