/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-string-list.h: A collection of strings.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
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
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-string-list.h"

#include "nautilus-glib-extensions.h"
#include <string.h>
#include "nautilus-lib-self-check-functions.h"

static gboolean supress_out_of_bounds_warning;

struct _NautilusStringList
{
	GList *strings;
};

/**
 * nautilus_string_list_new:
 *
 * Construct an empty string list.
 *
 * Returns the string list.
 */
NautilusStringList *
nautilus_string_list_new (void)
{
	NautilusStringList * string_list;

	string_list = g_new (NautilusStringList, 1);

	string_list->strings = NULL;

	return string_list;
}

/**
 * nautilus_string_list_new:
 *
 * Construct an empty string list.
 *
 * Returns the string list.
 */
NautilusStringList *
nautilus_string_list_new_from_string (const char *string)
{
	NautilusStringList * string_list;

	g_return_val_if_fail (string != NULL, NULL);

	string_list = nautilus_string_list_new ();

	nautilus_string_list_insert (string_list, string);

	return string_list;
}

/**
 * nautilus_string_list_new:
 *
 * Construct an empty string list.
 *
 * Returns the string list.
 */
NautilusStringList *
nautilus_string_list_new_from_string_list (const NautilusStringList *other)
{
	NautilusStringList *string_list;
	GList *other_iterator;
	const char *other_string;

	g_return_val_if_fail (other != NULL, NULL);

	string_list = nautilus_string_list_new ();

	for (other_iterator = other->strings; 
	     other_iterator != NULL; 
	     other_iterator = other_iterator->next) {

		other_string = (const char *) other_iterator->data;

		g_assert (other_string != NULL);

		nautilus_string_list_insert (string_list, other_string);
	}

	return string_list;
}

/* Construct a string list from tokens delimited by the given string and delimiter */
NautilusStringList *
nautilus_string_list_new_from_tokens (const char *string,
				      const char *delimiter)
{
	NautilusStringList *string_list;
	char		   **string_array;
	guint		   i;

	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (delimiter != NULL, NULL);

	string_list = nautilus_string_list_new ();

	string_array = g_strsplit (string, delimiter, -1);

	if (string_array) {
		for (i = 0; string_array[i]; i++) {
			nautilus_string_list_insert (string_list, string_array[i]);
		}
		
		g_free (string_array);
	}

	return string_list;
}

void
nautilus_string_list_free (NautilusStringList *string_list)
{
	g_return_if_fail (string_list != NULL);

	nautilus_string_list_clear (string_list);
	g_free (string_list);
}

void
nautilus_string_list_insert (NautilusStringList *string_list,
			     const char	 *string)
{
	g_return_if_fail (string_list != NULL);
	g_return_if_fail (string != NULL);

	string_list->strings = g_list_append (string_list->strings,
					      (gpointer) g_strdup (string));
}

char *
nautilus_string_list_nth (const NautilusStringList *string_list, guint n)
{
	g_return_val_if_fail (string_list != NULL, NULL);

	if (n  < g_list_length (string_list->strings)) {
		const char * s = (const char *) g_list_nth_data (string_list->strings, n);
		
		g_assert (s != NULL);
		
		return g_strdup (s);
	} else if (!supress_out_of_bounds_warning) {
		g_warning ("nautilus_string_list_nth (n = %d) is out of bounds.", n);
	}
	
	return NULL;
}

gboolean
nautilus_string_list_contains (const NautilusStringList *string_list,
			       const char	  *string)
{
	GList *find;

	g_return_val_if_fail (string_list != NULL, FALSE);
	g_return_val_if_fail (string != NULL, FALSE);

	find = g_list_find_custom (string_list->strings, (gpointer) string, (GCompareFunc) strcmp);

	return find == NULL ? FALSE : TRUE;
}

guint
nautilus_string_list_get_length (const NautilusStringList *string_list)
{
	g_return_val_if_fail (string_list != NULL, 0);

	return g_list_length (string_list->strings);
}

void
nautilus_string_list_clear (NautilusStringList *string_list)
{
	g_return_if_fail (string_list != NULL);

	nautilus_g_list_free_deep_custom (string_list->strings, (GFunc) g_free, NULL);

	string_list->strings = NULL;
}

gboolean
nautilus_string_list_equals (const NautilusStringList *a,
			     const NautilusStringList *b)
{
	GList *a_iterator;
	GList *b_iterator;
	const char * a_string;
	const char * b_string;

	g_return_val_if_fail (a != NULL, FALSE);
	g_return_val_if_fail (b != NULL, FALSE);

	for (a_iterator = a->strings, b_iterator = b->strings; 
	     a_iterator != NULL && b_iterator != NULL;
	     a_iterator = a_iterator->next, b_iterator = b_iterator->next) {

		a_string = (const char *) a_iterator->data;
		b_string = (const char *) b_iterator->data;

		if (strcmp (a_string, b_string) != 0) {
			return FALSE;
		}
	}

	return a_iterator == NULL && b_iterator == NULL;
}


#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_string_list (void)
{
	NautilusStringList *fruits;
	NautilusStringList *cities;
	NautilusStringList *cities_copy;
	NautilusStringList *empty;
	NautilusStringList *tokens;
	NautilusStringList *single;

	const char token_string[] = "london:paris:rome";
	const char token_string_thick[] = "london####paris####rome";

	empty = nautilus_string_list_new ();

	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (empty), 0);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (empty, "something"), FALSE);

	/********/

	cities = nautilus_string_list_new ();

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (cities, empty), TRUE);

	nautilus_string_list_insert (cities, "london");
	nautilus_string_list_insert (cities, "paris");
	nautilus_string_list_insert (cities, "rome");

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (cities, empty), FALSE);

	/********/

	cities_copy = nautilus_string_list_new_from_string_list (cities);

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (cities, cities_copy), TRUE);

	nautilus_string_list_free (cities_copy);

	/********/

	fruits = nautilus_string_list_new ();

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (fruits, empty), TRUE);

	nautilus_string_list_insert (fruits, "orange");
	nautilus_string_list_insert (fruits, "apple");
	nautilus_string_list_insert (fruits, "strawberry");
	nautilus_string_list_insert (fruits, "cherry");
	nautilus_string_list_insert (fruits, "bananna");
	nautilus_string_list_insert (fruits, "watermelon");

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (fruits, empty), FALSE);

 	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (fruits, 0), "orange");
 	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (fruits, 2), "strawberry");
 	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (fruits, 3), "cherry");
 	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (fruits, 5), "watermelon");
	supress_out_of_bounds_warning = TRUE;
 	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (fruits, 6), NULL);
	supress_out_of_bounds_warning = FALSE;

	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (fruits), 6);

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (fruits, "orange"), TRUE);
 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (fruits, "apple"), TRUE);
 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (fruits, "watermelon"), TRUE);

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (fruits, "pineapple"), FALSE);

 	nautilus_string_list_clear (fruits);

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (fruits, "orange"), FALSE);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (fruits), 0);
	
	nautilus_string_list_free (fruits);
	nautilus_string_list_free (empty);

	/********/

	tokens = nautilus_string_list_new_from_tokens (token_string, ":");

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (cities, tokens), TRUE);

	nautilus_string_list_free (tokens);

	tokens = nautilus_string_list_new_from_tokens (token_string_thick, "####");

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (cities, tokens), TRUE);

	nautilus_string_list_free (cities);
	nautilus_string_list_free (tokens);

	/********/

	single = nautilus_string_list_new_from_string ("something");

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (single, "something"), TRUE);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (single), 1);

	nautilus_string_list_free (single);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
