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

/* We use the "str" abbrevation to mean char * string, since
 * "string" usually means g_string instead. We use the "istr"
 * abbreviation to mean a case-insensitive char *.
 */

/* NULL is allowed for all the str parameters to these functions. */

/* Versions of basic string functions that allow NULL. */
size_t   nautilus_strlen                 (const char    *string);
char *   nautilus_strchr                 (const char    *haystack,
					  char           needle);
int      nautilus_strcmp                 (const char    *string_a,
					  const char    *string_b);
int      nautilus_strcasecmp             (const char    *string_a,
					  const char    *string_b);

/* GCompareFunc version. */
int      nautilus_str_compare            (gconstpointer  string_a,
					  gconstpointer  string_b);
int      nautilus_istr_compare           (gconstpointer  string_a,
					  gconstpointer  string_b);

/* Other basic string operations. */
gboolean nautilus_str_is_empty           (const char    *string_or_null);
gboolean nautilus_str_has_prefix         (const char    *target,
					  const char    *prefix);
char *   nautilus_str_get_prefix         (const char    *source,
					  const char    *delimiter);
char *   nautilus_str_get_after_prefix   (const char    *source,
					  const char    *delimiter);
gboolean nautilus_istr_has_prefix        (const char    *target,
					  const char    *prefix);
gboolean nautilus_str_has_suffix         (const char    *target,
					  const char    *suffix);
gboolean nautilus_istr_has_suffix        (const char    *target,
					  const char    *suffix);
char *   nautilus_str_get_prefix         (const char    *source,
					  const char    *delimiter);
char *   nautilus_str_strip_chr          (const char    *string,
					  char           remove_this);
char *   nautilus_str_strip_trailing_chr (const char    *string,
					  char           remove_this);
char *   nautilus_str_strip_trailing_str (const char    *string,
					  const char    *remove_this);

/* Conversions to and from strings. */
gboolean nautilus_str_to_int             (const char    *string,
					  int           *integer);
gboolean nautilus_eat_str_to_int         (char          *string_gets_freed,
					  int           *integer);

/* Escape function for slashes */
char *   nautilus_str_escape_slashes     (const char    *string);

/* Escape function for '_' character. */
char *   nautilus_str_double_underscores (const char    *string);

/* Capitalize a string */
char *   nautilus_str_capitalize         (const char    *string);

#endif /* NAUTILUS_STRING_H */
