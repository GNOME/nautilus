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

#include <string.h>
#include "nautilus-glib-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-string.h"

static gboolean supress_out_of_bounds_warning;

struct _NautilusStringList
{
	GList		*strings;
	GCompareFunc	compare_function;
};

static gboolean str_is_equal (const char *a,
			      const char *b,
			      gboolean    case_sensitive);

/**
 * nautilus_string_list_new:
 *
 * @case_sensitive: Flag indicating whether the new string list is case sensitive.
 *
 * Return value: A newly constructed string list.
 */
NautilusStringList *
nautilus_string_list_new (gboolean case_sensitive)
{
	NautilusStringList * string_list;

	string_list = g_new (NautilusStringList, 1);

	string_list->strings = NULL;
	string_list->compare_function = case_sensitive
		? nautilus_strcmp_compare_func
		: nautilus_strcasecmp_compare_func;
	/* FIXME: If these lists are seen by users, we want to use
	 * strcoll, not strcasecmp.
	 */

	return string_list;
}

/**
 * nautilus_string_list_new_from_string:
 *
 * @other_or_null: A NautilusStringList or NULL
 * @case_sensitive: Flag indicating whether the new string list is case sensitive.
 *
 * Return value: A newly constructed string list with one entry 'string'.
 */
NautilusStringList *
nautilus_string_list_new_from_string (const char	*string,
				      gboolean          case_sensitive)
{
	NautilusStringList * string_list;

	g_return_val_if_fail (string != NULL, NULL);

	string_list = nautilus_string_list_new (case_sensitive);

	nautilus_string_list_insert (string_list, string);

	return string_list;
}

/**
 * nautilus_string_list_new_from_string_list:
 *
 * @other_or_null: A NautilusStringList or NULL
 * @case_sensitive: Flag indicating whether the new string list is case sensitive.
 *
 * Return value: A newly allocated string list that is equal to other.
 */
NautilusStringList *
nautilus_string_list_new_from_string_list (const NautilusStringList	*other_or_null,
					   gboolean			case_sensitive)
{
	NautilusStringList *string_list;

	if (other_or_null == NULL) {
		return NULL;
	}

	string_list = nautilus_string_list_new (case_sensitive);

	nautilus_string_list_assign_from_string_list (string_list, other_or_null);

	return string_list;
}

/* Construct a string list from tokens delimited by the given string and delimiter */
NautilusStringList *
nautilus_string_list_new_from_tokens (const char	*string,
				      const char	*delimiter,
				      gboolean          case_sensitive)
{
	NautilusStringList *string_list;

	g_return_val_if_fail (delimiter != NULL, NULL);

	string_list = nautilus_string_list_new (case_sensitive);

	if (string != NULL) {
		char  **string_array;
		int i;

		string_array = g_strsplit (string, delimiter, -1);
		
		if (string_array) {
			for (i = 0; string_array[i]; i++) {
				nautilus_string_list_insert (string_list, string_array[i]);
			}
			
			g_strfreev (string_array);
		}
	}

	return string_list;
}

void
nautilus_string_list_assign_from_string_list (NautilusStringList       *string_list,
					      const NautilusStringList *other_or_null)
{
	GList		*other_iterator;
	const char	*other_string;

	g_return_if_fail (string_list != NULL);

	nautilus_string_list_clear (string_list);

	if (other_or_null == NULL) {
		return;
	}

	for (other_iterator = other_or_null->strings; 
	     other_iterator != NULL; 
	     other_iterator = other_iterator->next) {
		
		other_string = (const char *) other_iterator->data;

		nautilus_string_list_insert (string_list, other_string);
	}
}

void
nautilus_string_list_free (NautilusStringList *string_list_or_null)
{
	if (string_list_or_null == NULL) {
		return;
	}
	
	nautilus_string_list_clear (string_list_or_null);
	g_free (string_list_or_null);
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

/**
 * nautilus_string_list_modify_nth
 *
 * @string_list: A NautilusStringList
 * @n: Index of string to modify.
 * @string: New value for the string.
 *
 * Modify the nth value of a string in the collection.
 */
void
nautilus_string_list_modify_nth (NautilusStringList *string_list,
				 guint n,
				 const char *string)
{
	GList* nth;

	g_return_if_fail (string_list != NULL);
	g_return_if_fail (string != NULL);

	if (n >= g_list_length (string_list->strings)) {
		if (!supress_out_of_bounds_warning) {
			g_warning ("nautilus_string_list_nth (n = %d) is out of bounds.", n);
		}

		return;
	}

	nth = g_list_nth (string_list->strings, n);
	g_assert (nth != NULL);

	g_free (nth->data);
	nth->data = g_strdup (string);
}

/**
 * nautilus_string_list_remove_nth
 *
 * @string_list: A NautilusStringList
 * @n: Index of string to modify.
 *
 * Remove the nth string in the collection.
 */
void
nautilus_string_list_remove_nth (NautilusStringList *string_list,
				 guint n)
{
	GList* nth;

	g_return_if_fail (string_list != NULL);

	if (n >= g_list_length (string_list->strings)) {
		if (!supress_out_of_bounds_warning) {
			g_warning ("nautilus_string_list_nth (n = %d) is out of bounds.", n);
		}

		return;
	}

	nth = g_list_nth (string_list->strings, n);
	g_assert (nth != NULL);
	g_free (nth->data);
	string_list->strings = g_list_remove_link (string_list->strings, nth);
}

gboolean
nautilus_string_list_contains (const NautilusStringList	*string_list,
			       const char		*string)
{
	GList *find;

	if (string_list == NULL) {
		return FALSE;
	}

	g_return_val_if_fail (string != NULL, FALSE);

	find = g_list_find_custom (string_list->strings, (gpointer) string,
				   string_list->compare_function);

	return find == NULL ? FALSE : TRUE;
}

/**
 * nautilus_string_list_get_longest_string:
 *
 * @string_list: A NautilusStringList
 * @test_function: Function to use for testing the strings.
 * @callback_data: Data to pass to test function.
 *
 * Return value: Returns the first string in the collection for 
 * which the test function returns TRUE.  If the no string matches, the 
 * result is NULL.
 */
char *
nautilus_string_list_find_by_function (const NautilusStringList *string_list,
				       NautilusStringListTestFunction test_function,
				       gpointer callback_data)
{
	GList *iterator;

	if (string_list == NULL) {
		return NULL;
	}

	g_return_val_if_fail (test_function != NULL, FALSE);
	
	for (iterator = string_list->strings; iterator; iterator = iterator->next) {
		if ((* test_function) (string_list, iterator->data, callback_data)) {
			return g_strdup (iterator->data);
		}
	}

	return NULL;
}

guint
nautilus_string_list_get_length (const NautilusStringList *string_list)
{
	if (string_list == NULL) {
		return 0;
	}

	return g_list_length (string_list->strings);
}

void
nautilus_string_list_clear (NautilusStringList *string_list)
{
	g_return_if_fail (string_list != NULL);

	nautilus_g_list_free_deep (string_list->strings);
	string_list->strings = NULL;
}

gboolean
nautilus_string_list_equals (const NautilusStringList *a,
			     const NautilusStringList *b)
{
	GList *a_iterator;
	GList *b_iterator;

	g_return_val_if_fail (a != NULL, FALSE);
	g_return_val_if_fail (b != NULL, FALSE);

	for (a_iterator = a->strings, b_iterator = b->strings; 
	     a_iterator != NULL && b_iterator != NULL;
	     a_iterator = a_iterator->next, b_iterator = b_iterator->next) {
		if (!str_is_equal ((const char *) a_iterator->data, (const char *) b_iterator->data,
				   a->compare_function == b->compare_function)) {
			return FALSE;
		}
	}
	
	return a_iterator == NULL && b_iterator == NULL;
}

/**
 * nautilus_string_list_as_g_list:
 *
 * @string_list: A NautilusStringList
 *
 * Return value: A GList of strings that must deep free the result with
 * nautilus_g_list_free_deep()
 */
GList *
nautilus_string_list_as_g_list (const NautilusStringList *string_list)
{
	guint i;
	GList *copy = NULL;

	g_return_val_if_fail (string_list != NULL, NULL);

	for (i = 0; i < nautilus_string_list_get_length (string_list); i++) {
		copy = g_list_append (copy, nautilus_string_list_nth (string_list, i));
	}

	return copy;
}

/**
 * nautilus_string_list_get_index_for_string:
 *
 * @string_list: A NautilusStringList
 * @string: The string to look for
 *
 * Return value: An int with the index of the given string or 
 * NAUTILUS_STRING_LIST_NOT_FOUND if the string aint found.
 */
int
nautilus_string_list_get_index_for_string (const NautilusStringList	*string_list,
					   const char			*string)
{
	int	n = 0;
	GList	*iterator;

	g_return_val_if_fail (string_list != NULL, NAUTILUS_STRING_LIST_NOT_FOUND);
	g_return_val_if_fail (string != NULL, NAUTILUS_STRING_LIST_NOT_FOUND);

	for (iterator = string_list->strings; iterator != NULL; iterator = iterator->next) {
		if (str_is_equal ((const char *) iterator->data, string,
				  string_list->compare_function == nautilus_strcmp_compare_func)) {
			return n;
		}
		
		n++;
	}

	return NAUTILUS_STRING_LIST_NOT_FOUND;
}

/**
 * nautilus_string_list_as_tokens
 *
 * @string_list: A NautilusStringList
 * @delimiter: The string to use a delimeter, can be NULL.
 *
 * Return value: An newly allocated string concatenation of all the items in the list.
 * The string is delimited by 'delimiter'.
 */
char *
nautilus_string_list_as_concatenated_string (const NautilusStringList *string_list,
					     const char               *delimiter)
{
	char *result = NULL;
	int length;
	
	g_return_val_if_fail (string_list != NULL, NULL);
	
	length = nautilus_string_list_get_length (string_list);
	
	if (length > 0) {
		int	n;
		GList	*iterator;
		GString	*tokens;

		n = 0;
		
		tokens = g_string_new (NULL);
		
		for (iterator = string_list->strings; iterator != NULL; iterator = iterator->next) {
			const char *current = (const char *) iterator->data;
			
			g_string_append (tokens, current);

			
			n++;

			if (delimiter && (n != length)) {
				g_string_append (tokens, delimiter);
			}
		}

		result = tokens->str;

		g_string_free (tokens, FALSE);
	}

	return result;
}

void
nautilus_string_list_sort (NautilusStringList *string_list)
{
	g_return_if_fail (string_list != NULL);

	string_list->strings = g_list_sort (string_list->strings, string_list->compare_function);
}

/**
 * nautilus_string_list_sort_by_function
 *
 * @string_list: A NautilusStringList
 * @compare_function: Function to use for comparing the strings.
 *
 * Sort the strings using the given compare function.
 */
void
nautilus_string_list_sort_by_function (NautilusStringList *string_list,
				       GCompareFunc compare_function)
{
	g_return_if_fail (string_list != NULL);

	string_list->strings = g_list_sort (string_list->strings, compare_function);
}

void
nautilus_string_list_remove_duplicates (NautilusStringList *string_list)
{
	GList	*new_strings = NULL;
	GList	*iterator;

	g_return_if_fail (string_list != NULL);
	
	for (iterator = string_list->strings; iterator != NULL; iterator = iterator->next) {
		const char *string = (const char *) iterator->data;
		g_assert (string != NULL);

		if (g_list_find_custom (new_strings, (gpointer) string, string_list->compare_function) == NULL) {
			new_strings = g_list_append (new_strings, g_strdup (string));
		}
	}

	nautilus_string_list_clear (string_list);

	string_list->strings = new_strings;
}

void
nautilus_string_list_for_each (const NautilusStringList *string_list,
			       GFunc                     function,
			       gpointer                  user_data)
{
	g_return_if_fail (string_list != NULL);

	g_list_foreach (string_list->strings, function, user_data);
}

/**
 * nautilus_string_list_get_longest_string:
 *
 * @string_list: A NautilusStringList
 *
 * Return value: A copy of the longest string in the collection.  Need to g_free() it.
 */
char *
nautilus_string_list_get_longest_string (const NautilusStringList *string_list)
{
	int	longest_length = 0;
	int	longest_index = 0;
	GList	*iterator;
	int	i;

	g_return_val_if_fail (string_list != NULL, NULL);

	if (string_list->strings == NULL) {
		return NULL;
	}
	
	for (iterator = string_list->strings, i = 0; iterator != NULL; iterator = iterator->next, i++) {
		int current_length = nautilus_strlen ((const char *) iterator->data);
		
		if (current_length > longest_length) {
			longest_index = i;
			longest_length = current_length;
		}
	}

	return nautilus_string_list_nth (string_list, longest_index);
}

/**
 * nautilus_string_list_get_longest_string_length:
 *
 * @string_list: A NautilusStringList
 *
 * Return value: The length of the longest string in the collection.
 */
int
nautilus_string_list_get_longest_string_length (const NautilusStringList *string_list)
{
	int	longest_length = 0;
	GList	*iterator;
	int	i;

	g_return_val_if_fail (string_list != NULL, 0);

	if (string_list->strings == NULL) {
		return 0;
	}
	
	for (iterator = string_list->strings, i = 0; iterator != NULL; iterator = iterator->next, i++) {
		int current_length = nautilus_strlen ((const char *) iterator->data);
		
		if (current_length > longest_length) {
			longest_length = current_length;
		}
	}

	return longest_length;
}

static gboolean
str_is_equal (const char	*a,
	      const char	*b,
	      gboolean		case_sensitive)
{
	return case_sensitive ? nautilus_str_is_equal (a, b) : nautilus_istr_is_equal (a, b);
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

static gboolean
test_dog (const NautilusStringList *string_list,
	     const char *string,
	     gpointer callback_data)
{
	return nautilus_str_is_equal (string, "dog");
}

static gboolean
test_data (const NautilusStringList *string_list,
	   const char *string,
	   gpointer callback_data)
{
	return nautilus_str_is_equal (string, callback_data);
}

static gboolean
test_true (const NautilusStringList *string_list,
	   const char *string,
	   gpointer callback_data)
{
	return TRUE;
}

static gboolean
test_false (const NautilusStringList *string_list,
	    const char *string,
	    gpointer callback_data)
{
	return FALSE;
}

static int
compare_number (gconstpointer string_a,
		gconstpointer string_b)
{
	int a;
	int b;
	
	g_return_val_if_fail (string_a != NULL, 0);
	g_return_val_if_fail (string_b != NULL, 0);

	g_return_val_if_fail (nautilus_str_to_int (string_a, &a), 0);
	g_return_val_if_fail (nautilus_str_to_int (string_b, &b), 0);

 	if (a < b) {
 		return -1;
	}

 	if (a == b) {
 		return 0;
 	}

	return 1;
}

void
nautilus_self_check_string_list (void)
{
	NautilusStringList *fruits;
	NautilusStringList *cities;
	NautilusStringList *cities_copy;
	NautilusStringList *empty;
	NautilusStringList *single;

	/*
	 * nautilus_string_list_contains
	 */
	empty = nautilus_string_list_new (TRUE);

	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (empty), 0);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (empty, "something"), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (NULL, "something"), FALSE);

	/*
	 * nautilus_string_list_new
	 */
	cities = nautilus_string_list_new (TRUE);

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (cities, empty), TRUE);
	
	nautilus_string_list_insert (cities, "london");
	nautilus_string_list_insert (cities, "paris");
	nautilus_string_list_insert (cities, "rome");

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (cities, empty), FALSE);

	/*
	 * nautilus_string_list_new_from_string_list
	 */
	cities_copy = nautilus_string_list_new_from_string_list (cities, TRUE);

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (cities, cities_copy), TRUE);

	nautilus_string_list_free (cities_copy);
	nautilus_string_list_free (cities);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_new_from_string_list (NULL, TRUE) == NULL, TRUE);

	/*
	 * nautilus_string_list_insert,
	 * nautilus_string_list_nth,
	 * nautilus_string_list_contains,
	 * nautilus_string_list_get_length
	 */
	fruits = nautilus_string_list_new (TRUE);

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

	/*
	 * nautilus_string_list_new_from_string
	 */
	single = nautilus_string_list_new_from_string ("something", TRUE);

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (single, "something"), TRUE);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (single), 1);

	nautilus_string_list_free (single);


	/*
	 * nautilus_string_list_as_g_list
	 */
	{
		guint			i;
		GList			*glist;
		GList			*glist_iterator;
		NautilusStringList	*string_list;

		string_list = nautilus_string_list_new (TRUE);
		
		nautilus_string_list_insert (string_list, "orange");
		nautilus_string_list_insert (string_list, "apple");
		nautilus_string_list_insert (string_list, "strawberry");
		nautilus_string_list_insert (string_list, "cherry");
		nautilus_string_list_insert (string_list, "bananna");
		nautilus_string_list_insert (string_list, "watermelon");
		
		glist = nautilus_string_list_as_g_list (string_list);

		NAUTILUS_CHECK_BOOLEAN_RESULT (g_list_length (glist) == nautilus_string_list_get_length (string_list),
					       TRUE);

		for (i = 0, glist_iterator = glist;
		     i < nautilus_string_list_get_length (string_list); 
		     i++, glist_iterator = glist_iterator->next) {
			char *s1 = nautilus_string_list_nth (string_list, i);
			const char *s2 = (const char *) glist_iterator->data;

			NAUTILUS_CHECK_INTEGER_RESULT (nautilus_strcmp (s1, s2), 0);

			g_free (s1);
		}

		nautilus_string_list_free (string_list);

		nautilus_g_list_free_deep (glist);
	}
	
	/*
	 * nautilus_string_list_get_index_for_string
	 *
	 */

	{
		NautilusStringList *fruits;

		fruits = nautilus_string_list_new (TRUE);
		
		nautilus_string_list_insert (fruits, "orange");
		nautilus_string_list_insert (fruits, "apple");
		nautilus_string_list_insert (fruits, "strawberry");
		nautilus_string_list_insert (fruits, "cherry");
		nautilus_string_list_insert (fruits, "bananna");
		nautilus_string_list_insert (fruits, "watermelon");

		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_index_for_string (fruits, "orange"), 0);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_index_for_string (fruits, "apple"), 1);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_index_for_string (fruits, "strawberry"), 2);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_index_for_string (fruits, "cherry"), 3);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_index_for_string (fruits, "bananna"), 4);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_index_for_string (fruits, "watermelon"), 5);

		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_index_for_string (fruits, "papaya"), NAUTILUS_STRING_LIST_NOT_FOUND);

		nautilus_string_list_free (fruits);
	}

	/*
	 * nautilus_string_list_as_concatenated_string
	 *
	 */
	{
		NautilusStringList *l;

		l = nautilus_string_list_new (TRUE);
		
		nautilus_string_list_insert (l, "x");

		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_as_concatenated_string (l, NULL), "x");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_as_concatenated_string (l, ":"), "x");

		nautilus_string_list_insert (l, "y");
		nautilus_string_list_insert (l, "z");

		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_as_concatenated_string (l, NULL), "xyz");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_as_concatenated_string (l, ""), "xyz");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_as_concatenated_string (l, ":"), "x:y:z");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_as_concatenated_string (l, "abc"), "xabcyabcz");

		nautilus_string_list_free (l);
	}


	/*
	 * nautilus_string_list_sort
	 *
	 */
	{
		NautilusStringList *l;

		l = nautilus_string_list_new (TRUE);
		
		nautilus_string_list_insert (l, "dog");
		nautilus_string_list_insert (l, "cat");
		nautilus_string_list_insert (l, "bird");

		nautilus_string_list_sort (l);

		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (l, 0), "bird");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (l, 1), "cat");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (l, 2), "dog");

		nautilus_string_list_free (l);
	}

	/*
	 * nautilus_string_list_remove_duplicates
	 *
	 */
	{
		NautilusStringList *l;

		l = nautilus_string_list_new (TRUE);

		nautilus_string_list_remove_duplicates (l);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (l), 0);
		
		nautilus_string_list_insert (l, "foo");
		nautilus_string_list_insert (l, "bar");
		nautilus_string_list_insert (l, "bar");
		nautilus_string_list_insert (l, "foo");
		nautilus_string_list_insert (l, "foo");

		nautilus_string_list_remove_duplicates (l);

		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (l), 2);
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (l, 0), "foo");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (l, 1), "bar");

		nautilus_string_list_clear (l);

		nautilus_string_list_remove_duplicates (l);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (l), 0);

		nautilus_string_list_insert (l, "single");
		nautilus_string_list_remove_duplicates (l);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (l), 1);
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (l, 0), "single");

		nautilus_string_list_clear (l);

		nautilus_string_list_free (l);
	}

	/*
	 * nautilus_string_list_assign_from_string_list
	 *
	 */
	{
		NautilusStringList *l;
		NautilusStringList *other;

		l = nautilus_string_list_new (TRUE);
		other = nautilus_string_list_new (TRUE);

		/* assign an other with some items */
		nautilus_string_list_insert (other, "dog");
		nautilus_string_list_insert (other, "cat");
		nautilus_string_list_insert (other, "mouse");
		nautilus_string_list_assign_from_string_list (l, other);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (l), 3);
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (l, 0), "dog");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (l, 1), "cat");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (l, 2), "mouse");

		/* assign an other with 1 item */
		nautilus_string_list_clear (other);
		nautilus_string_list_insert (other, "something");
		nautilus_string_list_assign_from_string_list (l, other);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (l), 1);
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (l, 0), "something");
		
		/* assign an empty other */
		nautilus_string_list_clear (other);
		nautilus_string_list_assign_from_string_list (l, other);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (l), 0);

		nautilus_string_list_free (l);
		nautilus_string_list_free (other);
	}


	/*
	 * nautilus_string_list_get_longest_string
	 *
	 */
	{
		NautilusStringList *l;

		l = nautilus_string_list_new (TRUE);

		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_get_longest_string (l), NULL);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_longest_string_length (l), 0);

		nautilus_string_list_insert (l, "a");
		nautilus_string_list_insert (l, "bb");
		nautilus_string_list_insert (l, "ccc");
		nautilus_string_list_insert (l, "dddd");

		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_get_longest_string (l), "dddd");
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_longest_string_length (l), strlen ("dddd"));

		nautilus_string_list_clear (l);

		nautilus_string_list_insert (l, "foo");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_get_longest_string (l), "foo");
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_longest_string_length (l), strlen ("foo"));

		nautilus_string_list_free (l);

	}

	/*
	 * case insensitive tests
	 *
	 */
	{
		NautilusStringList *l;

		l = nautilus_string_list_new (FALSE);

		nautilus_string_list_insert (l, "foo");
		nautilus_string_list_insert (l, "bar");

		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (l, "Foo"), TRUE);
		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (l, "foO"), TRUE);
		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (l, "fOo"), TRUE);
		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_contains (l, "foo"), TRUE);

		nautilus_string_list_clear (l);

		nautilus_string_list_insert (l, "Foo");
		nautilus_string_list_insert (l, "Foo");
		nautilus_string_list_insert (l, "fOo");
		nautilus_string_list_insert (l, "foO");
		nautilus_string_list_remove_duplicates (l);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (l), 1);

		nautilus_string_list_free (l);
	}

	/*
	 * nautilus_string_list_new_from_tokens
	 */
	{
		NautilusStringList *lines;
		NautilusStringList *thick_lines;

		const char lines_string[] = "This\nAre\nSome\n\nLines";
		const char thick_lines_string[] = "This####Are####Some########Lines";
		const int num_lines = nautilus_str_count_characters (lines_string, '\n') + 1;

		lines = nautilus_string_list_new_from_tokens (lines_string, "\n", TRUE);
		thick_lines = nautilus_string_list_new_from_tokens (thick_lines_string, "####", TRUE);

		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (lines, thick_lines), TRUE);

		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (lines), num_lines);
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (lines, 0), "This");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (lines, 1), "Are");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (lines, 2), "Some");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (lines, 3), "");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (lines, 4), "Lines");
		
		nautilus_string_list_free (lines);
		nautilus_string_list_free (thick_lines);
	}

	/*
	 * nautilus_string_list_modify_nth
	 */
	{
		NautilusStringList *list;

		list = nautilus_string_list_new (TRUE);
		nautilus_string_list_insert (list, "dog");
		nautilus_string_list_insert (list, "cat");
		nautilus_string_list_insert (list, "mouse");

		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (list, 0), "dog");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (list, 2), "mouse");
		nautilus_string_list_modify_nth (list, 2, "rat");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (list, 2), "rat");
		nautilus_string_list_modify_nth (list, 0, "pig");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (list, 0), "pig");
		
		nautilus_string_list_free (list);
	}

	/*
	 * nautilus_string_list_remove_nth
	 */
	{
		NautilusStringList *list;

		list = nautilus_string_list_new (TRUE);
		nautilus_string_list_insert (list, "dog");
		nautilus_string_list_insert (list, "cat");
		nautilus_string_list_insert (list, "mouse");
		nautilus_string_list_insert (list, "bird");
		nautilus_string_list_insert (list, "pig");
		
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (list), 5);
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (list, 0), "dog");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (list, 4), "pig");

		nautilus_string_list_remove_nth (list, 2);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (list), 4);
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (list, 2), "bird");

		nautilus_string_list_remove_nth (list, 3);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (list), 3);
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (list, 0), "dog");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (list, 2), "bird");

		nautilus_string_list_remove_nth (list, 0);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_string_list_get_length (list), 2);
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (list, 0), "cat");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_nth (list, 1), "bird");
		
		nautilus_string_list_free (list);
	}

	/*
	 * nautilus_string_list_find_by_function
	 */
	{
		NautilusStringList *list;

		list = nautilus_string_list_new (TRUE);
		nautilus_string_list_insert (list, "house");
		nautilus_string_list_insert (list, "street");
		nautilus_string_list_insert (list, "car");
		nautilus_string_list_insert (list, "dog");
		
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_find_by_function (NULL, test_dog, NULL), NULL);
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_find_by_function (list, test_dog, NULL), "dog");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_find_by_function (list, test_false, NULL), NULL);
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_find_by_function (list, test_true, NULL), "house");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_string_list_find_by_function (list, test_data, "car"), "car");

		nautilus_string_list_free (list);
	}

	/*
	 * nautilus_string_list_sort_by_function
	 */
	{
		NautilusStringList *sorted_list;
		NautilusStringList *list;

		sorted_list = nautilus_string_list_new (TRUE);
		nautilus_string_list_insert (sorted_list, "0");
		nautilus_string_list_insert (sorted_list, "1");
		nautilus_string_list_insert (sorted_list, "2");
		nautilus_string_list_insert (sorted_list, "3");
		nautilus_string_list_insert (sorted_list, "4");

		list = nautilus_string_list_new (TRUE);
		nautilus_string_list_insert (list, "4");
		nautilus_string_list_insert (list, "2");
		nautilus_string_list_insert (list, "1");
		nautilus_string_list_insert (list, "0");
		nautilus_string_list_insert (list, "3");

		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (list, sorted_list), FALSE);

		nautilus_string_list_sort_by_function (list, compare_number);
		
		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (list, sorted_list), TRUE);

		nautilus_string_list_free (list);
		nautilus_string_list_free (sorted_list);
	}
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
