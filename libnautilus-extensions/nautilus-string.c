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
nautilus_strlen (const char *string)
{
	return string == NULL ? 0 : strlen (string);
}

char *
nautilus_strchr (const char *haystack, char needle)
{
	return haystack == NULL ? NULL : strchr (haystack, needle);
}

int
nautilus_strcmp (const char *string_a, const char *string_b)
{
	return strcmp (string_a == NULL ? "" : string_a,
		       string_b == NULL ? "" : string_b);
}

int
nautilus_strcasecmp (const char *string_a, const char *string_b)
{
	return g_strcasecmp (string_a == NULL ? "" : string_a,
		             string_b == NULL ? "" : string_b);
}

gboolean
nautilus_str_is_empty (const char *string_or_null)
{
	return nautilus_strcmp (string_or_null, NULL) == 0;
}

int
nautilus_str_compare (gconstpointer string_a, gconstpointer string_b)
{
	return nautilus_strcmp ((const char *) string_a,
				(const char *) string_b);
}

gboolean
nautilus_str_has_prefix (const char *haystack, const char *needle)
{
	const char *h, *n;

	/* Eat one character at a time. */
	h = haystack == NULL ? "" : haystack;
	n = needle == NULL ? "" : needle;
	do {
		if (*n == '\0') {
			return TRUE;
		}
		if (*h == '\0') {
			return FALSE;
		}
	} while (*h++ == *n++);
	return FALSE;
}

gboolean
nautilus_str_has_suffix (const char *haystack, const char *needle)
{
	const char *h, *n;

	if (needle == NULL) {
		return TRUE;
	}
	if (haystack == NULL) {
		return needle[0] == '\0';
	}
		
	/* Eat one character at a time. */
	h = haystack + strlen(haystack);
	n = needle + strlen(needle);
	do {
		if (n == needle) {
			return TRUE;
		}
		if (h == haystack) {
			return FALSE;
		}
	} while (*--h == *--n);
	return FALSE;
}

gboolean
nautilus_istr_has_prefix (const char *haystack, const char *needle)
{
	const char *h, *n;
	char hc, nc;

	/* Eat one character at a time. */
	h = haystack == NULL ? "" : haystack;
	n = needle == NULL ? "" : needle;
	do {
		if (*n == '\0') {
			return TRUE;
		}
		if (*h == '\0') {
			return FALSE;
		}
		hc = *h++;
		nc = *n++;
		if (isupper (hc)) {
			hc = tolower (hc);
		}
		if (isupper (nc)) {
			nc = tolower (nc);
		}
	} while (hc == nc);
	return FALSE;
}

gboolean
nautilus_istr_has_suffix (const char *haystack, const char *needle)
{
	const char *h, *n;
	char hc, nc;

	if (needle == NULL) {
		return TRUE;
	}
	if (haystack == NULL) {
		return needle[0] == '\0';
	}
		
	/* Eat one character at a time. */
	h = haystack + strlen(haystack);
	n = needle + strlen(needle);
	do {
		if (n == needle) {
			return TRUE;
		}
		if (h == haystack) {
			return FALSE;
		}
		hc = *--h;
		nc = *--n;
		if (isupper (hc)) {
			hc = tolower (hc);
		}
		if (isupper (nc)) {
			nc = tolower (nc);
		}
	} while (hc == nc);
	return FALSE;
}

/**
 * nautilus_str_get_prefix:
 * Get a new string containing the first part of an existing string.
 * 
 * @source: The string whose prefix should be extracted.
 * @delimiter: The string that marks the end of the prefix.
 * 
 * Return value: A newly-allocated string that that matches the first part
 * of @source, up to but not including the first occurrence of
 * @delimiter. If @source is NULL, returns NULL. If 
 * @delimiter is NULL, returns a copy of @source.
 * If @delimiter does not occur in @source, returns
 * a copy of @source.
 **/
char *
nautilus_str_get_prefix (const char *source, 
			 const char *delimiter)
{
	char *prefix_start;

	if (source == NULL) {
		return NULL;
	}

	if (delimiter == NULL) {
		return g_strdup (source);
	}

	prefix_start = strstr (source, delimiter);

	if (prefix_start == NULL) {
		return g_strdup ("");
	}

	return g_strndup (source, prefix_start - source);
}


/**
 * nautilus_str_get_after_prefix:
 * Get a new string containing the part of the string
 * after the prefix
 * @source: The string whose prefix should be extracted.
 * @delimiter: The string that marks the end of the prefix.
 * 
 * Return value: A newly-allocated string that that matches the end
 * of @source, starting right after the first occurr
 * @delimiter. If @source is NULL, returns NULL. If 
 * @delimiter is NULL, returns a copy of @source.
 * If @delimiter does not occur in @source, returns
 * NULL
 **/
char *
nautilus_str_get_after_prefix (const char *source,
			       const char *delimiter)
{
	char *prefix_start;
	
	if (source == NULL) {
		return NULL;
	}
	
	if (delimiter == NULL) {
		return g_strdup (source);
	}
	
	prefix_start = strstr (source, delimiter);
	
	if (prefix_start == NULL) {
		return NULL;
	}
	
	return g_strdup (prefix_start);
}

gboolean
nautilus_str_to_int (const char *string, int *integer)
{
	long result;
	char *parse_end;

	/* Check for the case of an empty string. */
	if (string == NULL || *string == '\0') {
		return FALSE;
	}
	
	/* Call the standard library routine to do the conversion. */
	errno = 0;
	result = strtol (string, &parse_end, 0);

	/* Check that the result is in range. */
	if ((result == G_MINLONG || result == G_MAXLONG) && errno == ERANGE) {
		return FALSE;
	}
	if (result < G_MININT || result > G_MAXINT) {
		return FALSE;
	}

	/* Check that all the trailing characters are spaces. */
	while (*parse_end != '\0') {
		if (!isspace (*parse_end++)) {
			return FALSE;
		}
	}

	/* Return the result. */
	*integer = result;
	return TRUE;
}

/**
 * nautilus_str_strip_chr:
 * Remove all occurrences of a character from a string.
 * 
 * @source: The string to be stripped.
 * @remove_this: The char to remove from @source
 * 
 * Return value: A copy of @source, after removing all occurrences
 * of @remove_this.
 */
char *
nautilus_str_strip_chr (const char *source, char remove_this)
{
	char *result, *out;
	const char *in;
	
        if (source == NULL) {
		return NULL;
	}
	
	result = g_malloc (strlen (source) + 1);
	in = source;
	out = result;
	do {
		if (*in != remove_this) {
			*out++ = *in;
		}
	} while (*in++ != '\0');

        return result;
}

/**
 * nautilus_str_strip_trailing_chr:
 * Remove trailing occurrences of a character from a string.
 * 
 * @source: The string to be stripped.
 * @remove_this: The char to remove from @source
 * 
 * Return value: @source, after removing trailing occurrences
 * of @remove_this.
 */
char *
nautilus_str_strip_trailing_chr (const char *source, char remove_this)
{
	const char *end;
	
        if (source == NULL) {
		return NULL;
	}

	for (end = source + strlen (source); end != source; end--) {
		if (end[-1] != remove_this) {
			break;
		}
	}
	
        return g_strndup (source, end - source);
}

char *   
nautilus_str_strip_trailing_str (const char *source, const char *remove_this)
{
	const char *end;
	if (source == NULL) {
		return NULL;
	}
	if (remove_this == NULL) {
		return g_strdup (source);
	}
	end = source + strlen (source);
	if (strcmp (end - strlen (remove_this), remove_this) != 0) {
		return g_strdup (source);
	}
	else {
		return g_strndup (source, strlen (source) - strlen(remove_this));
	}
	
}

gboolean
nautilus_eat_str_to_int (char *source, int *integer)
{
	gboolean result;

	result = nautilus_str_to_int (source, integer);
	g_free (source);
	return result;
}

/* To use a directory name as a file name, we need to escape any slashes.
   This means that "/" is replaced by "%2F" and "%" is replaced by "%25".
   Later we might share the escaping code with some more generic escaping
   function, but this should do for now.
*/
char *
nautilus_str_escape_slashes (const char *string)
{
	char c;
	const char *in;
	guint length;
	char *result;
	char *out;

	/* Figure out how long the result needs to be. */
	in = string;
	length = 0;
	while ((c = *in++) != '\0')
		switch (c) {
		case '/':
		case '%':
			length += 3;
			break;
		default:
			length += 1;
		}

	/* Create the result string. */
	result = g_malloc (length + 1);
	in = string;
	out = result;	
	while ((c = *in++) != '\0')
		switch (c) {
		case '/':
			*out++ = '%';
			*out++ = '2';
			*out++ = 'F';
			break;
		case '%':
			*out++ = '%';
			*out++ = '2';
			*out++ = '5';
			break;
		default:
			*out++ = c;
		}
	g_assert (out == result + length);
	*out = '\0';

	return result;
}


char *
nautilus_str_double_underscores (const char *string)
{
	int underscores;
	const char *p;
	char *q;
	char *escaped;
	
	if (string == NULL) {
		return NULL;
	}
	
	underscores = 0;
	for (p = string; *p != '\0'; p++) {
		underscores += (*p == '_');
	}
	
	if (underscores == 0) {
		return g_strdup (string);
	}

	escaped = g_malloc (strlen (string) + underscores + 1);
	for (p = string, q = escaped; *p != '\0'; p++, q++) {
		/* Add an extra underscore. */
		if (*p == '_') {
			*q++ = '_';
		}
		*q = *p;
	}
	*q = '\0';
	
	return escaped;
}

char *
nautilus_str_capitalize (const char *string)
{
	char *capitalized;

	if (string == NULL) {
		return NULL;
	}

	capitalized = g_strdup (string);

	if (islower (capitalized[0])) {
		capitalized[0] = toupper (capitalized[0]);
	}

	return capitalized;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

static int
call_str_to_int (const char *string)
{
	int integer;

	integer = 9999;
	nautilus_str_to_int (string, &integer);
	return integer;
}

static int
call_eat_str_to_int (char *string)
{
	int integer;

	integer = 9999;
	nautilus_eat_str_to_int (string, &integer);
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

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix (NULL, NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix (NULL, ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix ("", NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix ("a", "a"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix ("aaab", "aaab"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix (NULL, "a"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix ("a", NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix ("", "a"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix ("a", ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix ("a", "b"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix ("a", "ab"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix ("ab", "a"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix ("aaa", "aaab"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_prefix ("aaab", "aaa"), TRUE);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix (NULL, NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix (NULL, ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("", NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("", ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("a", ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("", "a"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("a", "a"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("aaab", "aaab"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix (NULL, "a"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("a", NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("", "a"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("a", ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("a", "b"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("a", "ab"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("ab", "a"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("ab", "b"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("aaa", "baaa"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_has_suffix ("baaa", "aaa"), TRUE);

	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_prefix (NULL, NULL), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_prefix (NULL, "foo"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_prefix ("foo", NULL), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_prefix ("", ""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_prefix ("", "foo"), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_prefix ("foo", ""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_prefix ("foo", "foo"), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_prefix ("foo:", ":"), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_prefix ("foo:bar", ":"), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_prefix ("footle:bar", "tle:"), "foo");	

	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_chr (NULL, '_'), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_chr ("", '_'), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_chr ("foo", '_'), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_chr ("_foo", '_'), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_chr ("foo_", '_'), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_chr ("_foo__", '_'), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_chr ("_f_o__o_", '_'), "foo");
        
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_chr (NULL, '_'), NULL);	
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_chr ("", '_'), "");	
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_chr ("foo", '_'), "foo");	
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_chr ("_foo", '_'), "_foo");	
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_chr ("foo_", '_'), "foo");	
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_chr ("_foo__", '_'), "_foo");	
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_chr ("_f_o__o_", '_'), "_f_o__o");	

	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_str (NULL, NULL), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_str (NULL, "bar"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_str ("bar", NULL), "bar");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_str ("", ""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_str ("", "bar"), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_str ("bar", ""), "bar");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_str ("foo", "bar"), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_str ("foo bar", "bar"), "foo ");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_trailing_str ("bar", "bar"), "");
	
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_double_underscores (NULL), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_double_underscores (""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_double_underscores ("_"), "__");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_double_underscores ("foo"), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_double_underscores ("foo_bar"), "foo__bar");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_double_underscores ("foo_bar_2"), "foo__bar__2");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_double_underscores ("_foo"), "__foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_double_underscores ("foo_"), "foo__");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_capitalize (NULL), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_capitalize (""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_capitalize ("foo"), "Foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_capitalize ("Foo"), "Foo");

	#define TEST_INTEGER_CONVERSION_FUNCTIONS(string, boolean_result, integer_result) \
		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_to_int (string, &integer), boolean_result); \
		NAUTILUS_CHECK_INTEGER_RESULT (call_str_to_int (string), integer_result); \
		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_eat_str_to_int (g_strdup (string), &integer), boolean_result); \
		NAUTILUS_CHECK_INTEGER_RESULT (call_eat_str_to_int (g_strdup (string)), integer_result);

	
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
