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
#include <libnautilus/nautilus-string-list.h>
#include "nautilus-lib-self-check-functions.h"

#include <string.h>

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
nautilus_string_list_new ()
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
nautilus_string_list_new_from_string (const gchar *string)
{
	NautilusStringList * string_list;

	g_assert (string != NULL);

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
	NautilusStringList * string_list;
	GList *other_iterator;

	g_assert (other != NULL);

	string_list = nautilus_string_list_new ();

	for (other_iterator = other->strings; 
	     other_iterator != NULL; 
	     other_iterator = other_iterator->next) 
	{
		const gchar * other_string = (const gchar *) other_iterator->data;

		g_assert (other_string != NULL);

		nautilus_string_list_insert (string_list, other_string);
	}

	return string_list;
}

/* Construct a string list from tokens delimted by the given string and delimeter */
NautilusStringList *
nautilus_string_list_new_from_tokens (const gchar               *string,
				      const gchar               *delimeter)
{
	NautilusStringList *string_list;
	gchar		   **string_array;
	guint		   i;

	g_assert (string != NULL);
	g_assert (delimeter != NULL);

	string_list = nautilus_string_list_new ();

	string_array = g_strsplit (string, delimeter, -1);

	if (string_array)
	{
		for (i = 0; string_array[i]; i++) 
		{
			nautilus_string_list_insert (string_list, string_array[i]);
		}
		
		g_free (string_array);
	}

	return string_list;
}

void
nautilus_string_list_free (NautilusStringList *string_list)
{
	g_assert (string_list != NULL);

	if (string_list->strings)
	{
		g_list_foreach (string_list->strings, (GFunc) g_free, NULL);
		g_list_free (string_list->strings);
	}

	g_free (string_list);
}

void
nautilus_string_list_insert (NautilusStringList *string_list,
			     const gchar	 *string)
{
	g_assert (string_list != NULL);
	g_assert (string != NULL);

	string_list->strings = g_list_append (string_list->strings,
					      (gpointer) g_strdup (string));
}

gchar *
nautilus_string_list_nth (const NautilusStringList *string_list, guint n)
{
	g_assert (string_list != NULL);

	if (string_list->strings)
	{
		if (n  < g_list_length (string_list->strings))
		{
			const gchar * s = (const gchar *) g_list_nth_data (string_list->strings, n);
			
			g_assert (s != NULL);

			return g_strdup (s);
		}
		else
		{
			g_warning ("nautilus_string_list_nth (n = %d) is out of bounds.", n);
		}
	}
	
	return NULL;
}

gboolean
nautilus_string_list_contains (const NautilusStringList *string_list,
			       const gchar	  *string)
{
	GList *find;

	g_assert (string_list != NULL);
	g_assert (string != NULL);

	find = g_list_find_custom (string_list->strings, (gpointer) string, (GCompareFunc) strcmp);

	return find ? TRUE : FALSE;
}

guint
nautilus_string_list_get_length (const NautilusStringList *string_list)
{
	g_assert (string_list != NULL);

	return (string_list->strings ? g_list_length (string_list->strings) : 0);
}

void
nautilus_string_list_clear (NautilusStringList *string_list)
{
	g_assert (string_list != NULL);

	if (string_list->strings)
	{
		g_list_foreach (string_list->strings, (GFunc) g_free, NULL);
		g_list_free (string_list->strings);
	}

	string_list->strings = NULL;
}

gboolean
nautilus_string_list_equals (const NautilusStringList *a,
			     const NautilusStringList *b)
{
	GList *a_iterator;
	GList *b_iterator;

	g_assert (a != NULL);
	g_assert (b != NULL);

	if (nautilus_string_list_get_length (a) != nautilus_string_list_get_length (b))
	{
		return FALSE;
	}
		
	for (a_iterator = a->strings, b_iterator = b->strings; 
	     (a_iterator != NULL) && (b_iterator != NULL);
	     a_iterator = a_iterator->next,b_iterator = b_iterator->next) 
	{
		const gchar * a_string = (const gchar *) a_iterator->data;
		const gchar * b_string = (const gchar *) b_iterator->data;

		if (strcmp (a_string, b_string) != 0)
		{
			return FALSE;
		}
	}

	return TRUE;
}


#if !defined (NAUTILUS_OMIT_SELF_CHECK)

#include <stdio.h>

void
nautilus_self_check_string_list (void)
{
	NautilusStringList * fruits;
	NautilusStringList * cities;
	NautilusStringList * cities_copy;
	NautilusStringList * empty;
	NautilusStringList * tokens;
	NautilusStringList * single;

	const gchar *token_string = "london:paris:rome";
	const gchar *token_string_thick = "london####paris####rome";

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
 	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (fruits, 6), NULL);

	printf ("You should see an out of bounds warning.  Its ok!\n");

	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (fruits), 6);

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (fruits, "orange"), TRUE);
 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (fruits, "apple"), TRUE);
 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (fruits, "watermelon"), TRUE);

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (fruits, "pineapple"), FALSE);

 	nautilus_string_list_clear (fruits);

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (fruits, "orange"), FALSE);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (fruits), 0);
	
	/********/

	tokens = nautilus_string_list_new_from_tokens (token_string, ":");

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (cities, tokens), TRUE);

	nautilus_string_list_free (tokens);

	tokens = nautilus_string_list_new_from_tokens (token_string_thick, "####");

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (cities, tokens), TRUE);

	/********/
	single = nautilus_string_list_new_from_string ("something");

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (single, "something"), TRUE);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (single), 1);

	nautilus_string_list_free (fruits);
	nautilus_string_list_free (cities);
	nautilus_string_list_free (cities_copy);
	nautilus_string_list_free (empty);
	nautilus_string_list_free (tokens);
	nautilus_string_list_free (single);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
