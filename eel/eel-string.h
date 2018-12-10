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

/* Escape function for '_' character. */
char *   eel_str_double_underscores        (const char    *str);

/* Capitalize a string */
char *   eel_str_capitalize                (const char    *str);

/**
 * eel_str_middle_truncate:
 * @string: the string to truncate
 * @truncate_length: the length limit at which to truncate
 *
 * If @string is longer than @truncate_length, replaces the middle with an
 * ellipsis so the resulting string is exactly @truncate_length characters
 * in length. Otherwise, returns a copy of @string.
 *
 * Do not use to ellipsize whole labels, only substrings that appear in them,
 * e.g. file names.
 *
 * Returns: @string, truncated at the middle to @truncate_length or a copy
 * if it was not longer than @truncate_length.
 */
gchar   *eel_str_middle_truncate           (const gchar   *string,
                                            guint          truncate_length);


/* Remove all characters after the passed-in substring. */
char *   eel_str_strip_substring_and_after (const char    *str,
					    const char    *substring);

/* Replace all occurrences of substring with replacement. */
char *   eel_str_replace_substring         (const char    *str,
					    const char    *substring,
					    const char    *replacement);

/**
 * eel_str_get_common_prefix:
 * @str: set of strings
 * @min_required_len: the minimum number of characters required in the prefix
 *
 * Returns: the common prefix for a set of strings, or NULL if there isn't a
 * common prefix of length min_required_len
 */
char *   eel_str_get_common_prefix         (GList *strs, int min_required_len);
