/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-string.c: String routines to augment <string.h>.

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "nautilus-string.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

size_t
nautilus_strlen (const char *string_null_allowed)
{
	return string_null_allowed == NULL ? 0 : strlen (string_null_allowed);
}

char *
nautilus_strchr (const char *haystack_null_allowed, char needle)
{
	return haystack_null_allowed == NULL ? NULL : strchr (haystack_null_allowed, needle);
}

int
nautilus_strcmp (const char *string_a_null_allowed, const char *string_b_null_allowed)
{
	return strcmp (string_a_null_allowed == NULL ? "" : string_a_null_allowed,
		       string_b_null_allowed == NULL ? "" : string_b_null_allowed);
}

int
nautilus_eat_strcmp (char *string_a, const char *string_b)
{
	int result;

	result = nautilus_strcmp (string_a, string_b);
	g_free (string_a);
	return result;
}

gboolean
nautilus_string_to_int (const char *string, int *integer)
{
	long result;
	char *parse_end;

	/* Check for the case of an empty string. */
	if (string == NULL || *string == '\0')
		return FALSE;
	
	/* Call the standard library routine to do the conversion. */
	result = strtol (string, &parse_end, 0);

	/* Check that the result is in range. */
	if ((result == LONG_MIN || result == LONG_MAX) && errno == ERANGE)
		return FALSE;
	if (result < INT_MIN || result > INT_MAX)
		return FALSE;

	/* Check that all the trailing characters are spaces. */
	while (*parse_end != '\0')
		if (!isspace (*parse_end++))
			return FALSE;

	/* Return the result. */
	*integer = result;
	return TRUE;
}

gboolean
nautilus_eat_string_to_int (char *string, int *integer)
{
	gboolean result;

	result = nautilus_string_to_int (string, integer);
	g_free (string);
	return result;
}
