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
#include <locale.h>
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
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return strcmp (string_a == NULL ? "" : string_a,
		       string_b == NULL ? "" : string_b);
}

int
nautilus_strcasecmp (const char *string_a, const char *string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return g_strcasecmp (string_a == NULL ? "" : string_a,
		             string_b == NULL ? "" : string_b);
}

int
nautilus_strcmp_case_breaks_ties (const char *string_a, const char *string_b)
{
	int casecmp_result;

	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	casecmp_result = nautilus_strcasecmp (string_a, string_b);
	if (casecmp_result != 0) {
		return casecmp_result;
	}
	return nautilus_strcmp (string_a, string_b);
}

int
nautilus_strcoll (const char *string_a, const char *string_b)
{
	const char *locale;
	int result;
	
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */

	locale = setlocale (LC_COLLATE, NULL);
	
	if (locale == NULL || strcmp (locale, "C") == 0 || strcmp (locale, "POSIX") == 0) {
		/* If locale is NULL or default "C" or "POSIX" use nautilus sorting */
		return nautilus_strcmp_case_breaks_ties (string_a, string_b);
	} else {
		/* Use locale-specific collated sorting */
		result = strcoll (string_a == NULL ? "" : string_a,
				  string_b == NULL ? "" : string_b);
		if (result != 0) {
			return result;
		}
		return nautilus_strcmp (string_a, string_b);
	}
}

gboolean
nautilus_str_is_empty (const char *string_or_null)
{
	return nautilus_strcmp (string_or_null, NULL) == 0;
}

gboolean
nautilus_str_is_equal (const char *string_a, const char *string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL != ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return nautilus_strcmp (string_a, string_b) == 0;
}

gboolean
nautilus_istr_is_equal (const char *string_a, const char *string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL != ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return nautilus_strcasecmp (string_a, string_b) == 0;
}

int
nautilus_strcmp_compare_func (gconstpointer string_a, gconstpointer string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return nautilus_strcmp ((const char *) string_a,
				(const char *) string_b);
}

int
nautilus_strcoll_compare_func (gconstpointer string_a, gconstpointer string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return nautilus_strcoll ((const char *) string_a,
				 (const char *) string_b);
}

int
nautilus_strcasecmp_compare_func (gconstpointer string_a, gconstpointer string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return nautilus_strcasecmp ((const char *) string_a,
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
		hc = tolower ((guchar) hc);
		nc = tolower ((guchar) nc);
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
	h = haystack + strlen (haystack);
	n = needle + strlen (needle);
	do {
		if (n == needle) {
			return TRUE;
		}
		if (h == haystack) {
			return FALSE;
		}
		hc = *--h;
		nc = *--n;
		hc = tolower ((guchar) hc);
		nc = tolower ((guchar) nc);
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
	
	result = g_new (char, strlen (source) + 1);
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

	escaped = g_new (char, strlen (string) + underscores + 1);
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

	capitalized[0] = toupper ((guchar) capitalized[0]);

	return capitalized;
}

/* Note: nautilus_string_ellipsize_* that use a length in pixels
 * rather than characters can be found in nautilus_gdk_extensions.h
 * 
 * FIXME bugzilla.eazel.com 5089: 
 * we should coordinate the names of nautilus_string_ellipsize_*
 * and nautilus_str_*_truncate so that they match better and reflect
 * their different behavior.
 */
char *
nautilus_str_middle_truncate (const char *string,
			      guint truncate_length)
{
	char *truncated;
	guint length;
	guint num_left_chars;
	guint num_right_chars;

	const char delimter[] = "...";
	const guint delimter_length = strlen (delimter);
	const guint min_truncate_length = delimter_length + 2;

	if (string == NULL) {
		return NULL;
	}

	/* It doesnt make sense to truncate strings to less than
	 * the size of the delimiter plus 2 characters (one on each
	 * side)
	 */
	if (truncate_length < min_truncate_length) {
		return g_strdup (string);
	}

	length = strlen (string);

	/* Make sure the string is not already small enough. */
	if (length <= truncate_length) {
		return g_strdup (string);
	}

	/* Find the 'middle' where the truncation will occur. */
	num_left_chars = (truncate_length - delimter_length) / 2;
	num_right_chars = truncate_length - num_left_chars - delimter_length + 1;

	truncated = g_new (char, truncate_length + 1);

	strncpy (truncated, string, num_left_chars);
	strncpy (truncated + num_left_chars, delimter, delimter_length);
	strncpy (truncated + num_left_chars + delimter_length, string + length - num_right_chars + 1, num_right_chars);
	
	return truncated;
}

/**
 * nautilus_str_count_characters:
 * Count the number of 'c' characters that occur in 'string.
 * 
 * @string: The string to be scanned.
 * @c: The char to count.
 * 
 * Return value: @count, the 'c' occurance count.
 */
guint
nautilus_str_count_characters (const char	*string,
			       char		c)
{
	guint count = 0;

	while (string && *string != '\0') {
		if (*string == c) {
			count++;
		}

		string++;
	}

	return count;
}

char *
nautilus_str_strip_substring_and_after (const char *string,
					const char *substring)
{
	const char *substring_position;

	g_return_val_if_fail (substring != NULL, g_strdup (string));
	g_return_val_if_fail (substring[0] != '\0', g_strdup (string));

	if (string == NULL) {
		return NULL;
	}

	substring_position = strstr (string, substring);
	if (substring_position == NULL) {
		return g_strdup (string);
	}

	return g_strndup (string,
			  substring_position - string);
}

char *
nautilus_str_replace_substring (const char *string,
				const char *substring,
				const char *replacement)
{
	int substring_length, replacement_length, result_length, remaining_length;
	const char *p, *substring_position;
	char *result, *result_position;

	g_return_val_if_fail (substring != NULL, g_strdup (string));
	g_return_val_if_fail (substring[0] != '\0', g_strdup (string));

	if (string == NULL) {
		return NULL;
	}

	substring_length = strlen (substring);
	replacement_length = nautilus_strlen (replacement);

	result_length = strlen (string);
	for (p = string; ; p = substring_position + substring_length) {
		substring_position = strstr (p, substring);
		if (substring_position == NULL) {
			break;
		}
		result_length += replacement_length - substring_length;
	}

	result = g_malloc (result_length + 1);

	result_position = result;
	for (p = string; ; p = substring_position + substring_length) {
		substring_position = strstr (p, substring);
		if (substring_position == NULL) {
			remaining_length = strlen (p);
			memcpy (result_position, p, remaining_length);
			result_position += remaining_length;
			break;
		}
		memcpy (result_position, p, substring_position - p);
		result_position += substring_position - p;
		memcpy (result_position, replacement, replacement_length);
		result_position += replacement_length;
	}
	g_assert (result_position - result == result_length);
	result_position[0] = '\0';

	return result;
}


/* Removes strings enclosed by the '[' and ']' characters.  Strings
   that have unbalanced open and closed brackets will return the
   string itself. */
char *
nautilus_str_remove_bracketed_text (const char *text)
{
	char *unbracketed_text;
	char *unbracketed_segment, *new_unbracketed_text;
	const char *current_text_location;
	const char *next_open_bracket, *next_close_bracket;
	int bracket_depth;
	

	g_return_val_if_fail (text != NULL, NULL);

	current_text_location = text;
	bracket_depth = 0;
	unbracketed_text = g_strdup ("");
	while (TRUE) {
		next_open_bracket = strchr (current_text_location, '[');
		next_close_bracket = strchr (current_text_location, ']');
		/* No more brackets */
		if (next_open_bracket == NULL &&
		    next_close_bracket == NULL) {
			if (bracket_depth == 0) {
				new_unbracketed_text = g_strconcat (unbracketed_text, 
								    current_text_location, NULL);
				g_free (unbracketed_text);
				return new_unbracketed_text;
			}
			else {
				g_free (unbracketed_text);
				return g_strdup (text);
			}
		}
		/* Close bracket but no open bracket */
		else if (next_open_bracket == NULL) {
			if (bracket_depth == 0) {
				g_free (unbracketed_text);
				return g_strdup (text);
			}
			else {
				current_text_location = next_close_bracket + 1;
				bracket_depth--;
			}
		}
		/* Open bracket but no close bracket */
		else if (next_close_bracket == NULL) {
			g_free (unbracketed_text);
			return g_strdup (text);
		}
		/* Deal with the open bracket, that's first */
		else if (next_open_bracket < next_close_bracket) {
			if (bracket_depth == 0) {
				/* We're out of brackets. Copy until the next bracket */
				unbracketed_segment = g_strndup (current_text_location,
								 next_open_bracket - current_text_location);
				new_unbracketed_text = g_strconcat (unbracketed_text, unbracketed_segment, NULL);
				g_free (unbracketed_text);
				g_free (unbracketed_segment);
				unbracketed_text = new_unbracketed_text;
			}
			current_text_location = next_open_bracket + 1;
			bracket_depth++;
		}
		/* Deal with the close bracket, that's first */
		else {
			if (bracket_depth > 0) {
				bracket_depth--;
				current_text_location = next_close_bracket + 1;
			}
			else {
				g_free (unbracketed_text);
				return g_strdup (text);
			}
		}
	}
	
	     

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

	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_after_prefix (NULL, NULL), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_after_prefix (NULL, "foo"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_after_prefix ("foo", NULL), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_after_prefix ("", ""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_after_prefix ("", "foo"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_after_prefix ("foo", ""), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_after_prefix ("foo", "foo"), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_after_prefix ("foo:", ":"), ":");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_after_prefix ("foo:bar", ":"), ":bar");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_get_after_prefix ("footle:bar", "tle:"), "tle:bar");	

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

	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("foo", 0), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("foo", 1), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("foo", 3), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("foo", 4), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("foo", 5), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("foo", 6), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("foo", 7), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("a_much_longer_foo", 0), "a_much_longer_foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("a_much_longer_foo", 1), "a_much_longer_foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("a_much_longer_foo", 2), "a_much_longer_foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("a_much_longer_foo", 3), "a_much_longer_foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("a_much_longer_foo", 4), "a_much_longer_foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("a_much_longer_foo", 5), "a...o");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("a_much_longer_foo", 6), "a...oo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("a_much_longer_foo", 7), "a_...oo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("a_much_longer_foo", 8), "a_...foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("a_much_longer_foo", 9), "a_m...foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_even", 8), "so...ven");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_odd", 8), "so...odd");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_even", 9), "som...ven");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_odd", 9), "som...odd");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_even", 10), "som...even");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_odd", 10), "som..._odd");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_even", 11), "some...even");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_odd", 11), "some..._odd");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_even", 12), "some..._even");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_odd", 12), "some...g_odd");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_even", 13), "somet..._even");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_odd", 13), "something_odd");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_even", 14), "something_even");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_middle_truncate ("something_odd", 13), "something_odd");

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

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_is_equal (NULL, NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_is_equal (NULL, ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_is_equal ("", ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_is_equal ("", NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_is_equal ("", ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_is_equal ("foo", "foo"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_str_is_equal ("foo", "bar"), FALSE);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_istr_is_equal (NULL, NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_istr_is_equal (NULL, ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_istr_is_equal ("", ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_istr_is_equal ("", NULL), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_istr_is_equal ("", ""), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_istr_is_equal ("foo", "foo"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_istr_is_equal ("foo", "bar"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_istr_is_equal ("Foo", "foo"), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_istr_is_equal ("foo", "Foo"), TRUE);

	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_str_count_characters (NULL, 'x'), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_str_count_characters ("", 'x'), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_str_count_characters (NULL, '\0'), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_str_count_characters ("", '\0'), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_str_count_characters ("foo", 'x'), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_str_count_characters ("foo", 'f'), 1);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_str_count_characters ("foo", 'o'), 2);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_str_count_characters ("xxxx", 'x'), 4);

	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_substring_and_after (NULL, "bar"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_substring_and_after ("", "bar"), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_substring_and_after ("foo", "bar"), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_substring_and_after ("foo bar", "bar"), "foo ");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_substring_and_after ("foo bar xxx", "bar"), "foo ");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_strip_substring_and_after ("bar", "bar"), "");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_replace_substring (NULL, "foo", NULL), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_replace_substring (NULL, "foo", "bar"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_replace_substring ("bar", "foo", NULL), "bar");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_replace_substring ("", "foo", ""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_replace_substring ("", "foo", "bar"), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_replace_substring ("bar", "foo", ""), "bar");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_replace_substring ("xxx", "x", "foo"), "foofoofoo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_replace_substring ("fff", "f", "foo"), "foofoofoo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_replace_substring ("foofoofoo", "foo", "f"), "fff");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_replace_substring ("foofoofoo", "f", ""), "oooooo");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text (""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("[]"), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("["), "[");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("]"), "]");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("[[]"), "[[]");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("foo"), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("foo [bar]"), "foo ");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("foo[ bar]"), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("foo[ bar] baz"), "foo baz");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("foo[ [b]ar] baz"), "foo baz");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("foo[ bar] baz[ bat]"), "foo baz");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("foo[ bar[ baz] bat]"), "foo");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_remove_bracketed_text ("foo[ bar] baz] bat]"), "foo[ bar] baz] bat]");
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
