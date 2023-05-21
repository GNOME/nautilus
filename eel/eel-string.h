/*
   eel-string.h: String routines to augment <string.h>.

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
   see <http://www.gnu.org/licenses/>.

   Authors: Darin Adler <darin@eazel.com>
*/

#pragma once

#include <glib.h>
#include <string.h>
#include <stdarg.h>

/* We use the "str" abbrevation to mean char * string, since
 * "string" usually means g_string instead. We use the "istr"
 * abbreviation to mean a case-insensitive char *.
 */


/* NULL is allowed for all the str parameters to these functions. */

/* Capitalize a string */
char *   eel_str_capitalize                (const char    *str);

/**
 * eel_str_get_common_prefix:
 * @str: set of strings
 * @min_required_len: the minimum number of characters required in the prefix
 *
 * Returns: the common prefix for a set of strings, or NULL if there isn't a
 * common prefix of length min_required_len
 */
char *   eel_str_get_common_prefix         (GList *strs, int min_required_len);
