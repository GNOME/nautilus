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

#include <config.h>
#include "nautilus-string.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "nautilus-lib-self-check-functions.h"

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
nautilus_has_prefix (const char *haystack_null_allowed, const char *needle_null_allowed)
{
	const char *h, *n;

	/* Eat one character at a time. */
	h = haystack_null_allowed == NULL ? "" : haystack_null_allowed;
	n = needle_null_allowed == NULL ? "" : needle_null_allowed;
	do {
		if (*n == '\0')
			return TRUE;
		if (*h == '\0')
			return FALSE;
	} while (*h++ == *n++);
	return FALSE;
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
	errno = 0;
	result = strtol (string, &parse_end, 0);

	/* Check that the result is in range. */
	if ((result == G_MINLONG || result == G_MAXLONG) && errno == ERANGE)
		return FALSE;
	if (result < G_MININT || result > G_MAXINT)
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

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

static int
call_string_to_int (const char *string)
{
	int integer;

	integer = 9999;
	nautilus_string_to_int (string, &integer);
	return integer;
}

static int
call_eat_string_to_int (char *string)
{
	int integer;

	integer = 9999;
	nautilus_eat_string_to_int (string, &integer);
	return integer;
}

void
nautilus_self_check_string (void)
{
	int integer;

	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_strlen (NULL), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_strlen (""), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_strlen ("abc"), 3);

	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_strcmp (NULL, NULL), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_strcmp (NULL, ""), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_strcmp ("", NULL), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_strcmp ("a", "a"), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_strcmp ("aaab", "aaab"), 0);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_strcmp (NULL, "a") < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_strcmp ("a", NULL) > 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_strcmp ("", "a") < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_strcmp ("a", "") > 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_strcmp ("a", "b") < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_strcmp ("a", "ab") < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_strcmp ("ab", "a") > 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_strcmp ("aaa", "aaab") < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_strcmp ("aaab", "aaa") > 0, TRUE);

	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_eat_strcmp (NULL, NULL), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_eat_strcmp (NULL, ""), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_eat_strcmp (g_strdup (""), NULL), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_eat_strcmp (g_strdup ("a"), "a"), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_eat_strcmp (g_strdup ("aaab"), "aaab"), 0);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_eat_strcmp (NULL, "a") < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_eat_strcmp (g_strdup ("a"), NULL) > 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_eat_strcmp (g_strdup (""), "a") < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_eat_strcmp (g_strdup ("a"), "") > 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_eat_strcmp (g_strdup ("a"), "b") < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_eat_strcmp (g_strdup ("a"), "ab") < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_eat_strcmp (g_strdup ("ab"), "a") > 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_eat_strcmp (g_strdup ("aaa"), "aaab") < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_eat_strcmp (g_strdup ("aaab"), "aaa") > 0, TRUE);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix (NULL, NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix (NULL, ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix ("", NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix ("a", "a"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix ("aaab", "aaab"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix (NULL, "a"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix ("a", NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix ("", "a"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix ("a", ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix ("a", "b"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix ("a", "ab"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix ("ab", "a"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix ("aaa", "aaab"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_has_prefix ("aaab", "aaa"), TRUE);

	#define TEST_INTEGER_CONVERSION_FUNCTIONS(string, boolean_result, integer_result) \
		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_to_int (string, &integer), boolean_result); \
		NAUTILUS_CHECK_INTEGER_RESULT (call_string_to_int (string), integer_result); \
		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_eat_string_to_int (g_strdup (string), &integer), boolean_result); \
		NAUTILUS_CHECK_INTEGER_RESULT (call_eat_string_to_int (g_strdup (string)), integer_result);

	TEST_INTEGER_CONVERSION_FUNCTIONS (NULL, FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("a", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS (".", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("0", TRUE, 0)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("1", TRUE, 1)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("+1", TRUE, 1)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("-1", TRUE, -1)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("2147483647", TRUE, 2147483647)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("2147483648", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("+2147483647", TRUE, 2147483647)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("+2147483648", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("-2147483648", TRUE, INT_MIN)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("-2147483649", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("1a", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("0.0", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("1e1", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("21474836470", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("+21474836470", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("-21474836480", FALSE, 9999)
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
