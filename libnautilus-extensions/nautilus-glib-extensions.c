/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-glib-extensions.c - implementation of new functions that conceptually
                                belong in glib. Perhaps some of these will be
                                actually rolled into glib someday.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nautilus-glib-extensions.h"

#include "nautilus-lib-self-check-functions.h"
#include "nautilus-string.h"
#include <ctype.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

/* Legal conversion specifiers, as specified in the C standard. */
#define C_STANDARD_STRFTIME_CHARACTERS "aAbBcdHIjmMpSUwWxXyYZ"
#define C_STANDARD_NUMERIC_STRFTIME_CHARACTERS "dHIjmMSUwWyY"

typedef struct {
	GHashTable *hash_table;
	char *display_name;
	gboolean keys_known_to_be_strings;
} HashTableToFree;

static GList *hash_tables_to_free_at_exit;

/* We will need this for nautilus_unsetenv if there is no unsetenv. */
#if !defined (HAVE_SETENV)
extern char **environ;
#endif

/**
 * nautilus_setenv:
 * 
 * Adds "*name=*value" to the environment
 *
 * Returns: see "putenv" - 
 * 
 **/
int
nautilus_setenv (const char *name, const char *value, gboolean overwrite)
{
#if defined (HAVE_SETENV)
	return setenv (name, value, overwrite);
#else
	char *string;
	
	if (! overwrite && g_getenv (name) != NULL) {
		return 0;
	}
	
	/* This results in a leak when you overwrite existing
	 * settings. It would be fairly easy to fix this by keeping
	 * our own parallel array or hash table.
	 */
	string = g_strconcat (name, "=", value, NULL);
	return putenv (string);
#endif
}

/**
 * nautilus_unsetenv:
 * 
 * Removes *name from the environment
 * 
 **/
void
nautilus_unsetenv (const char *name)
{
#if defined (HAVE_SETENV)
	unsetenv (name);
#else
	int i, len;

	len = strlen (name);
	
	/* Mess directly with the environ array.
	 * This seems to be the only portable way to do this.
	 */
	for (i = 0; environ[i] != NULL; i++) {
		if (strncmp (environ[i], name, len) == 0
		    && environ[i][len + 1] == '=') {
			break;
		}
	}
	while (environ[i] != NULL) {
		environ[i] = environ[i + 1];
		i++;
	}
#endif
}

/**
 * nautilus_g_date_new_tm:
 * 
 * Get a new GDate * for the date represented by a tm struct. 
 * The caller is responsible for g_free-ing the result.
 * @time_pieces: Pointer to a tm struct representing the date to be converted.
 * 
 * Returns: Newly allocated date.
 * 
 **/
GDate *
nautilus_g_date_new_tm (struct tm *time_pieces)
{
	/* tm uses 0-based months; GDate uses 1-based months.
	 * tm_year needs 1900 added to get the full year.
	 */
	return g_date_new_dmy (time_pieces->tm_mday,
			       time_pieces->tm_mon + 1,
			       time_pieces->tm_year + 1900);
}

/**
 * nautilus_strdup_strftime:
 *
 * Cover for standard date-and-time-formatting routine strftime that returns
 * a newly-allocated string of the correct size. The caller is responsible
 * for g_free-ing the returned string.
 *
 * Besides the buffer management, there are two differences between this
 * and the library strftime:
 *
 *   1) The modifiers "-" and "_" between a "%" and a numeric directive
 *      are defined as for the GNU version of strftime. "-" means "do not
 *      pad the field" and "_" means "pad with spaces instead of zeroes".
 *   2) Non-ANSI extensions to strftime are flagged at runtime with a
 *      warning, so it's easy to notice use of the extensions without
 *      testing with multiple versions of the library.
 *
 * @format: format string to pass to strftime. See strftime documentation
 * for details.
 * @time_pieces: date/time, in struct format.
 * 
 * Return value: Newly allocated string containing the formatted time.
 **/
char *
nautilus_strdup_strftime (const char *format, struct tm *time_pieces)
{
	GString *string;
	const char *remainder, *percent;
	char code[3], buffer[512];
	char *piece, *result;
	size_t string_length;
	gboolean strip_leading_zeros, turn_leading_zeros_to_spaces;

	string = g_string_new ("");
	remainder = format;

	/* Walk from % character to % character. */
	for (;;) {
		percent = strchr (remainder, '%');
		if (percent == NULL) {
			g_string_append (string, remainder);
			break;
		}
		nautilus_g_string_append_len (string, remainder,
					      percent - remainder);

		/* Handle the "%" character. */
		remainder = percent + 1;
		switch (*remainder) {
		case '-':
			strip_leading_zeros = TRUE;
			turn_leading_zeros_to_spaces = FALSE;
			remainder++;
			break;
		case '_':
			strip_leading_zeros = FALSE;
			turn_leading_zeros_to_spaces = TRUE;
			remainder++;
			break;
		case '%':
			g_string_append_c (string, '%');
			remainder++;
			continue;
		case '\0':
			g_warning ("Trailing %% passed to nautilus_strdup_strftime");
			g_string_append_c (string, '%');
			continue;
		default:
			strip_leading_zeros = FALSE;
			turn_leading_zeros_to_spaces = FALSE;
			break;
		}
		
		if (strchr (C_STANDARD_STRFTIME_CHARACTERS, *remainder) == NULL) {
			g_warning ("nautilus_strdup_strftime does not support "
				   "non-standard escape code %%%c",
				   *remainder);
		}

		/* Convert code to strftime format. We have a fixed
		 * limit here that each code can expand to a maximum
		 * of 512 bytes, which is probably OK. There's no
		 * limit on the total size of the result string.
		 */
		code[0] = '%';
		code[1] = *remainder;
		code[2] = '\0';
		string_length = strftime (buffer, sizeof (buffer),
					  code, time_pieces);
		if (string_length == 0) {
			/* We could put a warning here, but there's no
			 * way to tell a successful conversion to
			 * empty string from a failure.
			 */
			buffer[0] = '\0';
		}

		/* Strip leading zeros if requested. */
		piece = buffer;
		if (strip_leading_zeros || turn_leading_zeros_to_spaces) {
			if (strchr (C_STANDARD_NUMERIC_STRFTIME_CHARACTERS, *remainder) == NULL) {
				g_warning ("nautilus_strdup_strftime does not support "
					   "modifier for non-numeric escape code %%%c%c",
					   remainder[-1],
					   *remainder);
			}
			if (*piece == '0') {
				do {
					piece++;
				} while (*piece == '0');
				if (!isdigit (*piece)) {
				    piece--;
				}
			}
			if (turn_leading_zeros_to_spaces) {
				memset (buffer, ' ', piece - buffer);
				piece = buffer;
			}
		}
		remainder++;

		/* Add this piece. */
		g_string_append (string, piece);
	}

	/* Extract the string. */
	result = string->str;
	g_string_free (string, FALSE);
	return result;
}

/**
 * nautilus_g_list_exactly_one_item
 *
 * Like g_list_length (list) == 1, only O(1) instead of O(n).
 * @list: List.
 *
 * Return value: TRUE if the list has exactly one item.
 **/
gboolean
nautilus_g_list_exactly_one_item (GList *list)
{
	return list != NULL && list->next == NULL;
}

/**
 * nautilus_g_list_more_than_one_item
 *
 * Like g_list_length (list) > 1, only O(1) instead of O(n).
 * @list: List.
 *
 * Return value: TRUE if the list has more than one item.
 **/
gboolean
nautilus_g_list_more_than_one_item (GList *list)
{
	return list != NULL && list->next != NULL;
}

/**
 * nautilus_g_list_equal
 *
 * Compares two lists to see if they are equal.
 * @list_a: First list.
 * @list_b: Second list.
 *
 * Return value: TRUE if the lists are the same length with the same elements.
 **/
gboolean
nautilus_g_list_equal (GList *list_a, GList *list_b)
{
	GList *p, *q;

	for (p = list_a, q = list_b; p != NULL && q != NULL; p = p->next, q = q->next) {
		if (p->data != q->data) {
			return FALSE;
		}
	}
	return p == NULL && q == NULL;
}

/**
 * nautilus_g_list_copy
 *
 * @list: List to copy.
 * Return value: Shallow copy of @list.
 **/
GList *
nautilus_g_list_copy (GList *list)
{
	GList *p, *result;

	result = NULL;
	
	if (list == NULL) {
		return NULL;
	}

	for (p = g_list_last (list); p != NULL; p = p->prev) {
		result = g_list_prepend (result, p->data);
	}
	return result;
}

/**
 * nautilus_g_str_list_equal
 *
 * Compares two lists of C strings to see if they are equal.
 * @list_a: First list.
 * @list_b: Second list.
 *
 * Return value: TRUE if the lists contain the same strings.
 **/
gboolean
nautilus_g_str_list_equal (GList *list_a, GList *list_b)
{
	GList *p, *q;

	for (p = list_a, q = list_b; p != NULL && q != NULL; p = p->next, q = q->next) {
		if (nautilus_strcmp (p->data, q->data) != 0) {
			return FALSE;
		}
	}
	return p == NULL && q == NULL;
}

/**
 * nautilus_g_str_list_copy
 *
 * @list: List of strings and/or NULLs to copy.
 * Return value: Deep copy of @list.
 **/
GList *
nautilus_g_str_list_copy (GList *list)
{
	GList *node, *result;

	result = NULL;
	
	for (node = g_list_last (list); node != NULL; node = node->prev) {
		result = g_list_prepend (result, g_strdup (node->data));
	}
	return result;
}

/**
 * nautilus_g_str_list_alphabetize
 *
 * Sort a list of strings using locale-sensitive rules.
 *
 * @list: List of strings and/or NULLs.
 * 
 * Return value: @list, sorted.
 **/
GList *
nautilus_g_str_list_alphabetize (GList *list)
{
	return g_list_sort (list, (GCompareFunc) nautilus_strcoll);
}

/**
 * nautilus_g_list_free_deep_custom
 *
 * Frees the elements of a list and then the list, using a custom free function.
 *
 * @list: List of elements that can be freed with the provided free function.
 * @element_free_func: function to call with the data pointer and user_data to free it.
 * @user_data: User data to pass to element_free_func
 **/
void
nautilus_g_list_free_deep_custom (GList *list, GFunc element_free_func, gpointer user_data)
{
	g_list_foreach (list, element_free_func, user_data);
	g_list_free (list);
}

/**
 * nautilus_g_list_free_deep
 *
 * Frees the elements of a list and then the list.
 * @list: List of elements that can be freed with g_free.
 **/
void
nautilus_g_list_free_deep (GList *list)
{
	nautilus_g_list_free_deep_custom (list, (GFunc) g_free, NULL);
}

/**
 * nautilus_g_list_free_deep_custom
 *
 * Frees the elements of a list and then the list, using a custom free function.
 *
 * @list: List of elements that can be freed with the provided free function.
 * @element_free_func: function to call with the data pointer and user_data to free it.
 * @user_data: User data to pass to element_free_func
 **/
void
nautilus_g_slist_free_deep_custom (GSList *list, GFunc element_free_func, gpointer user_data)
{
	g_slist_foreach (list, element_free_func, user_data);
	g_slist_free (list);
}

/**
 * nautilus_g_slist_free_deep
 *
 * Frees the elements of a list and then the list.
 * @list: List of elements that can be freed with g_free.
 **/
void
nautilus_g_slist_free_deep (GSList *list)
{
	nautilus_g_slist_free_deep_custom (list, (GFunc) g_free, NULL);
}


/**
 * nautilus_g_strv_find
 * 
 * Get index of string in array of strings.
 * 
 * @strv: NULL-terminated array of strings.
 * @find_me: string to search for.
 * 
 * Return value: index of array entry in @strv that
 * matches @find_me, or -1 if no matching entry.
 */
int
nautilus_g_strv_find (char **strv, const char *find_me)
{
	int index;

	g_return_val_if_fail (find_me != NULL, -1);
	
	for (index = 0; strv[index] != NULL; ++index) {
		if (strcmp (strv[index], find_me) == 0) {
			return index;
		}
	}

	return -1;
}

/**
 * nautilus_g_list_safe_for_each
 * 
 * A version of g_list_foreach that works if the passed function
 * deletes the current element.
 * 
 * @list: List to iterate.
 * @function: Function to call on each element.
 * @user_data: Data to pass to function.
 */
void
nautilus_g_list_safe_for_each (GList *list, GFunc function, gpointer user_data)
{
	GList *p, *next;

	for (p = list; p != NULL; p = next) {
		next = p->next;
		(* function) (p->data, user_data);
	}
}

/**
 * nautilus_g_list_partition
 * 
 * Parition a list into two parts depending on whether the data
 * elements satisfy a provided predicate. Order is preserved in both
 * of the resulting lists, and the original list is consumed. A list
 * of the items that satisfy the predicate is returned, and the list
 * of items not satisfying the predicate is returned via the failed
 * out argument.
 * 
 * @list: List to partition.
 * @predicate: Function to call on each element.
 * @user_data: Data to pass to function.  
 * @failed: The GList * variable pointed to by this argument will be
 * set to the list of elements for which the predicate returned
 * false. */

GList *
nautilus_g_list_partition (GList *list,
			   NautilusPredicateFunction  predicate,
			   gpointer user_data,
			   GList **failed)
{
	GList *predicate_true;
	GList *predicate_false;
	GList *reverse;
	GList *p;
	GList *next;

	predicate_true = NULL;
	predicate_false = NULL;

	reverse = g_list_reverse (list);

	for (p = reverse; p != NULL; p = next) {
		next = p->next;
		
		if (next != NULL) {
			next->prev = NULL;
		}

		if (predicate (p->data, user_data)) {
			p->next = predicate_true;
 			if (predicate_true != NULL) {
				predicate_true->prev = p;
			}
			predicate_true = p;
		} else {
			p->next = predicate_false;
 			if (predicate_false != NULL) {
				predicate_false->prev = p;
			}
			predicate_false = p;
		}
	}

	*failed = predicate_false;
	return predicate_true;
}



/**
 * nautilus_g_ptr_array_new_from_list
 * 
 * Copies (shallow) a list of pointers into an array of pointers.
 * 
 * @list: List to copy.
 * 
 * Return value: GPtrArray containing a copy of all the pointers
 * from @list
 */
GPtrArray *
nautilus_g_ptr_array_new_from_list (GList *list)
{
	GPtrArray *array;
	int size;
	int index;
	GList *p;

	array = g_ptr_array_new ();
	size = g_list_length ((GList *)list);

	g_ptr_array_set_size (array, size);

	for (p = list, index = 0; p != NULL; p = p->next, index++) {
		g_ptr_array_index (array, index) = p->data;
	}

	return array;
}

/**
 * nautilus_g_ptr_array_sort
 * 
 * Sorts @array using a qsort algorithm. Allows passing in
 * a pass-thru context that can be used by the @sort_function
 * 
 * @array: pointer array to sort
 * @sort_function: sort function
 * @context: sort context passed to the sort_function
 */
void
nautilus_g_ptr_array_sort (GPtrArray *array,
			   NautilusCompareFunction sort_function,
			   void *context)
{
	size_t count, r, l, j;
	void **base, **lp, **rp, **ip, **jp, **tmpp;
	void *tmp;

	count = array->len;

	if (count < 2) {
		return;
	}

	r = count;
	l = (r / 2) + 1;

	base = (void **) array->pdata;
	lp = base + (l - 1);
	rp = base + (r - 1);

	for (;;) {
		if (l > 1) {
			l--;
			lp--;
		} else {
			tmp = *lp;
			*lp = *rp;
			*rp = tmp;

			if (--r == 1) {
				return;
			}

			rp--;
		}

		j = l;

		jp = base + (j - 1);
		
		while (j * 2 <= r) {
			j *= 2;
			
			ip = jp;
			jp = base + (j - 1);
			
			if (j < r) {
				tmpp = jp + 1;
				if (sort_function(*jp, *tmpp, context) < 0) {
					j++;
					jp = tmpp;
				}
			}
			
			if (sort_function (*ip, *jp, context) >= 0) {
				break;
			}

			tmp = *ip;
			*ip = *jp;
			*jp = tmp;
		}
	}
}

/**
 * nautilus_g_ptr_array_search
 * 
 * Does a binary search through @array looking for an item
 * that matches a predicate consisting of a @search_function and
 * @context. May be used to find an insertion point 
 * 
 * @array: pointer array to search
 * @search_function: search function called on elements
 * @context: search context passed to the @search_function containing
 * the key or other data we are matching
 * @match_only: if TRUE, only returns a match if the match is exact,
 * if FALSE, returns either an index of a match or an index of the
 * slot a new item should be inserted in
 * 
 * Return value: index of the match or -1 if not found
 */
int
nautilus_g_ptr_array_search (GPtrArray *array, 
			     NautilusSearchFunction search_function,
			     void *context,
			     gboolean match_only)
{
	int r, l;
	int resulting_index;
	int compare_result;
	void *item;

	r = array->len - 1;
	item = NULL;
	resulting_index = 0;
	compare_result = 0;
	
	for (l = 0; l <= r; ) {
		resulting_index = (l + r) / 2;

		item = g_ptr_array_index (array, resulting_index);

		compare_result = (search_function) (item, context);
		
		if (compare_result > 0) {
			r = resulting_index - 1;
		} else if (compare_result < 0) {
			l = resulting_index + 1;
		} else {
			return resulting_index;
		}
	}

	if (compare_result < 0) {
		resulting_index++;
	}

	if (match_only && compare_result != 0) {
		return -1;
	}

	return resulting_index;
}

/**
 * nautilus_get_system_time
 * 
 * Return value: number of microseconds since the machine was turned on
 */
gint64
nautilus_get_system_time (void)
{
	struct timeval tmp;

	gettimeofday (&tmp, NULL);
	return (gint64)tmp.tv_usec + (gint64)tmp.tv_sec * G_GINT64_CONSTANT (1000000);
}

static void
print_key_string (gpointer key, gpointer value, gpointer callback_data)
{
	g_assert (callback_data == NULL);

	g_print ("--> %s\n", (char *) key);
}

static void
free_hash_tables_at_exit (void)
{
	GList *p;
	HashTableToFree *hash_table_to_free;
	guint size;

	for (p = hash_tables_to_free_at_exit; p != NULL; p = p->next) {
		hash_table_to_free = p->data;

		size = g_hash_table_size (hash_table_to_free->hash_table);
		if (size != 0) {
			if (hash_table_to_free->keys_known_to_be_strings) {
				g_print ("\n--- Hash table keys for warning below:\n");
				g_hash_table_foreach (hash_table_to_free->hash_table,
						      print_key_string,
						      NULL);
			}
			g_warning ("\"%s\" hash table still has %u element%s at quit time%s",
				   hash_table_to_free->display_name, size,
				   size == 1 ? "" : "s",
				   hash_table_to_free->keys_known_to_be_strings
				   ? " (keys above)" : "");
		}

		g_hash_table_destroy (hash_table_to_free->hash_table);
		g_free (hash_table_to_free->display_name);
		g_free (hash_table_to_free);
	}
	g_list_free (hash_tables_to_free_at_exit);
	hash_tables_to_free_at_exit = NULL;
}

GHashTable *
nautilus_g_hash_table_new_free_at_exit (GHashFunc hash_func,
					GCompareFunc key_compare_func,
					const char *display_name)
{
	GHashTable *hash_table;
	HashTableToFree *hash_table_to_free;

	if (hash_tables_to_free_at_exit == NULL) {
		g_atexit (free_hash_tables_at_exit);
	}

	hash_table = g_hash_table_new (hash_func, key_compare_func);

	hash_table_to_free = g_new (HashTableToFree, 1);
	hash_table_to_free->hash_table = hash_table;
	hash_table_to_free->display_name = g_strdup (display_name);
	hash_table_to_free->keys_known_to_be_strings =
		hash_func == g_str_hash;

	hash_tables_to_free_at_exit = g_list_prepend
		(hash_tables_to_free_at_exit, hash_table_to_free);

	return hash_table;
}

typedef struct {
	GList *keys;
	GList *values;
} FlattenedHashTable;

static void
flatten_hash_table_element (gpointer key, gpointer value, gpointer callback_data)
{
	FlattenedHashTable *flattened_table;

	flattened_table = callback_data;
	flattened_table->keys = g_list_prepend
		(flattened_table->keys, key);
	flattened_table->values = g_list_prepend
		(flattened_table->values, value);
}

void
nautilus_g_hash_table_safe_for_each (GHashTable *hash_table,
				     GHFunc callback,
				     gpointer callback_data)
{
	FlattenedHashTable flattened;
	GList *p, *q;

	flattened.keys = NULL;
	flattened.values = NULL;

	g_hash_table_foreach (hash_table,
			      flatten_hash_table_element,
			      &flattened);

	for (p = flattened.keys, q = flattened.values;
	     p != NULL;
	     p = p->next, q = q->next) {
		(* callback) (p->data, q->data, callback_data);
	}

	g_list_free (flattened.keys);
	g_list_free (flattened.values);
}

gboolean
nautilus_g_hash_table_remove_deep_custom (GHashTable *hash_table, gconstpointer key,
					  GFunc key_free_func, gpointer key_free_data,
					  GFunc value_free_func, gpointer value_free_data)
{
	gpointer key_in_table;
	gpointer value;

	/* It would sure be nice if we could do this with a single lookup.
	 */
	if (g_hash_table_lookup_extended (hash_table, key,
					  &key_in_table, &value)) {
		g_hash_table_remove (hash_table, key);
		if (key_free_func != NULL) {
			(* key_free_func) (key_in_table, key_free_data);
		}
		/* handle key == value, don't double free */
		if (value_free_func != NULL && value != key_in_table) {
			(* value_free_func) (value, value_free_data);
		}
		return TRUE;
	} else {
		return FALSE;
	}
}

gboolean
nautilus_g_hash_table_remove_deep (GHashTable *hash_table, gconstpointer key)
{
	return nautilus_g_hash_table_remove_deep_custom
		(hash_table, key, (GFunc) g_free, NULL, (GFunc) g_free, NULL);
}

typedef struct {
	GFunc	  key_free_func;
	gpointer  key_free_data;
	GFunc	  value_free_func;
	gpointer  value_free_data;
} HashTableFreeFuncs;

static gboolean
destroy_deep_helper (gpointer key, gpointer value, gpointer data)
{
	HashTableFreeFuncs *free_funcs;

	free_funcs = (HashTableFreeFuncs *) data;
	
	if (free_funcs->key_free_func != NULL) {
		(* free_funcs->key_free_func) (key, free_funcs->key_free_data);
	}
	/* handle key == value, don't double free */
	if (free_funcs->value_free_func != NULL && value != key) {
		(* free_funcs->value_free_func) (value, free_funcs->value_free_data);
	}
	return TRUE;
}

void
nautilus_g_hash_table_destroy_deep_custom (GHashTable *hash_table,
					   GFunc key_free_func, gpointer key_free_data,
					   GFunc value_free_func, gpointer value_free_data)
{
	HashTableFreeFuncs free_funcs;
	
	g_return_if_fail (hash_table != NULL);

	free_funcs.key_free_func = key_free_func;
	free_funcs.key_free_data = key_free_data;
	free_funcs.value_free_func = value_free_func;
	free_funcs.value_free_data = value_free_data;
	
	g_hash_table_foreach_remove (hash_table, destroy_deep_helper, &free_funcs);

	g_hash_table_destroy (hash_table);
}

void
nautilus_g_hash_table_destroy_deep (GHashTable *hash_table)
{
	nautilus_g_hash_table_destroy_deep_custom (hash_table, (GFunc) g_free, NULL, (GFunc) g_free, NULL);
}

/* This is something like the new g_string_append_len function from
 * GLib 2.0, without the ability to deal with NUL character that the
 * GLib 2.0 function has. It's limited in other ways too, so it's
 * best to delete this when we move to GLib 2.0.
 */
void
nautilus_g_string_append_len (GString *string,
			      const char *new_text,
			      int len)
{
	int old_string_len, new_text_len;

	old_string_len = string->len;
	g_string_append (string, new_text);

	new_text_len = strlen (new_text);
	if (len < new_text_len) {
		g_string_erase (string,
				old_string_len + len,
				new_text_len - len);
	}
}

/* By strange parallel evolution there is actually going to be a
 * function to do this in GNOME 2.0.
 */
char *
nautilus_shell_quote (const char *string)
{
	const char *p;
	GString *quoted_string;
	char *quoted_str;

	/* All kinds of ways to do this fancier.
	 * - Detect when quotes aren't needed at all.
	 * - Use double quotes when they would look nicer.
	 * - Avoid sequences of quote/unquote in a row (like when you quote "'''").
	 * - Do it higher speed with strchr.
	 * - Allocate the GString with g_string_sized_new.
	 */

	g_return_val_if_fail (string != NULL, NULL);

	quoted_string = g_string_new ("'");

	for (p = string; *p != '\0'; p++) {
		if (*p == '\'') {
			/* Get out of quotes, do a quote, then back in. */
			g_string_append (quoted_string, "'\\''");
		} else {
			g_string_append_c (quoted_string, *p);
		}
	}

	g_string_append_c (quoted_string, '\'');

	/* Let go of the GString. */
	quoted_str = quoted_string->str;
	g_string_free (quoted_string, FALSE);

	return quoted_str;
}

int nautilus_g_round (double d)
{
	double val = floor (d + .5);

	/* The tests are needed because the result of floating-point to integral
	 * conversion is undefined if the floating point value is not representable
	 * in the new type. E.g. the magnititude is too large or a negative
	 * floating-point value being converted to an unsigned.
	 */
	g_return_val_if_fail (val <= INT_MAX, INT_MAX);
	g_return_val_if_fail (val >= INT_MIN, INT_MIN);

	return val;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

static void 
check_tm_to_g_date (time_t time)
{
	struct tm *before_conversion;
	struct tm after_conversion;
	GDate *date;

	before_conversion = localtime (&time);
	date = nautilus_g_date_new_tm (before_conversion);

	g_date_to_struct_tm (date, &after_conversion);

	g_date_free (date);

	NAUTILUS_CHECK_INTEGER_RESULT (after_conversion.tm_mday,
				       before_conversion->tm_mday);
	NAUTILUS_CHECK_INTEGER_RESULT (after_conversion.tm_mon,
				       before_conversion->tm_mon);
	NAUTILUS_CHECK_INTEGER_RESULT (after_conversion.tm_year,
				       before_conversion->tm_year);
}

static gboolean
nautilus_test_predicate (gpointer data,
			 gpointer callback_data)
{
	return g_strcasecmp (data, callback_data) <= 0;
}

static char *
test_strftime (const char *format,
	       int year,
	       int month,
	       int day,
	       int hour,
	       int minute,
	       int second)
{
	struct tm time_pieces;

	time_pieces.tm_sec = second;
	time_pieces.tm_min = minute;
	time_pieces.tm_hour = hour;
	time_pieces.tm_mday = day;
	time_pieces.tm_mon = month - 1;
	time_pieces.tm_year = year - 1900;
	time_pieces.tm_isdst = -1;
	mktime (&time_pieces);

	return nautilus_strdup_strftime (format, &time_pieces);
}

void
nautilus_self_check_glib_extensions (void)
{
	char **strv;
	GList *compare_list_1;
	GList *compare_list_2;
	GList *compare_list_3;
	GList *compare_list_4;
	GList *compare_list_5;
	gint64 time1, time2;
	GList *list_to_partition;
	GList *expected_passed;
	GList *expected_failed;
	GList *actual_passed;
	GList *actual_failed;
	char *huge_string;
	
	check_tm_to_g_date (0);			/* lower limit */
	check_tm_to_g_date ((time_t) -1);	/* upper limit */
	check_tm_to_g_date (time (NULL));	/* current time */

	strv = g_strsplit ("zero|one|two|three|four", "|", 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_g_strv_find (strv, "zero"), 0);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_g_strv_find (strv, "one"), 1);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_g_strv_find (strv, "four"), 4);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_g_strv_find (strv, "five"), -1);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_g_strv_find (strv, ""), -1);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_g_strv_find (strv, "o"), -1);
	g_strfreev (strv);

	/* nautilus_get_system_time */
	time1 = nautilus_get_system_time ();
	time2 = nautilus_get_system_time ();
	NAUTILUS_CHECK_BOOLEAN_RESULT (time1 - time2 > -1000, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (time1 - time2 <= 0, TRUE);

	/* nautilus_g_str_list_equal */

	/* We g_strdup because identical string constants can be shared. */

	compare_list_1 = NULL;
	compare_list_1 = g_list_append (compare_list_1, g_strdup ("Apple"));
	compare_list_1 = g_list_append (compare_list_1, g_strdup ("zebra"));
	compare_list_1 = g_list_append (compare_list_1, g_strdup ("!@#!@$#@$!"));

	compare_list_2 = NULL;
	compare_list_2 = g_list_append (compare_list_2, g_strdup ("Apple"));
	compare_list_2 = g_list_append (compare_list_2, g_strdup ("zebra"));
	compare_list_2 = g_list_append (compare_list_2, g_strdup ("!@#!@$#@$!"));

	compare_list_3 = NULL;
	compare_list_3 = g_list_append (compare_list_3, g_strdup ("Apple"));
	compare_list_3 = g_list_append (compare_list_3, g_strdup ("zebra"));

	compare_list_4 = NULL;
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("Apple"));
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("zebra"));
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("!@#!@$#@$!"));
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("foobar"));

	compare_list_5 = NULL;
	compare_list_5 = g_list_append (compare_list_5, g_strdup ("Apple"));
	compare_list_5 = g_list_append (compare_list_5, g_strdup ("zzzzzebraaaaaa"));
	compare_list_5 = g_list_append (compare_list_5, g_strdup ("!@#!@$#@$!"));

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_g_str_list_equal (compare_list_1, compare_list_2), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_g_str_list_equal (compare_list_1, compare_list_3), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_g_str_list_equal (compare_list_1, compare_list_4), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_g_str_list_equal (compare_list_1, compare_list_5), FALSE);

	nautilus_g_list_free_deep (compare_list_1);
	nautilus_g_list_free_deep (compare_list_2);
	nautilus_g_list_free_deep (compare_list_3);
	nautilus_g_list_free_deep (compare_list_4);
	nautilus_g_list_free_deep (compare_list_5);

	/* nautilus_g_list_partition */

	list_to_partition = NULL;
	list_to_partition = g_list_append (list_to_partition, "Cadillac");
	list_to_partition = g_list_append (list_to_partition, "Pontiac");
	list_to_partition = g_list_append (list_to_partition, "Ford");
	list_to_partition = g_list_append (list_to_partition, "Range Rover");
	
	expected_passed = NULL;
	expected_passed = g_list_append (expected_passed, "Cadillac");
	expected_passed = g_list_append (expected_passed, "Ford");
	
	expected_failed = NULL;
	expected_failed = g_list_append (expected_failed, "Pontiac");
	expected_failed = g_list_append (expected_failed, "Range Rover");
	
	actual_passed = nautilus_g_list_partition (list_to_partition, 
						   nautilus_test_predicate,
						   "m",
						   &actual_failed);
	
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_g_str_list_equal (expected_passed, actual_passed), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_g_str_list_equal (expected_failed, actual_failed), TRUE);
	
	/* Don't free "list_to_partition", since it is consumed
	 * by nautilus_g_list_partition.
	 */
	
	g_list_free (expected_passed);
	g_list_free (actual_passed);
	g_list_free (expected_failed);
	g_list_free (actual_failed);

	/* nautilus_strdup_strftime */
	huge_string = g_new (char, 10000+1);
	memset (huge_string, 'a', 10000);
	huge_string[10000] = '\0';

	NAUTILUS_CHECK_STRING_RESULT (test_strftime ("", 2000, 1, 1, 0, 0, 0), "");
	NAUTILUS_CHECK_STRING_RESULT (test_strftime (huge_string, 2000, 1, 1, 0, 0, 0), huge_string);
	NAUTILUS_CHECK_STRING_RESULT (test_strftime ("%%", 2000, 1, 1, 1, 0, 0), "%");
	NAUTILUS_CHECK_STRING_RESULT (test_strftime ("%%%%", 2000, 1, 1, 1, 0, 0), "%%");
	/* localizers: These strings are part of the strftime
	 * self-check code and must be changed to match what strtfime
	 * yields -- usually just omitting the AM part is all that's
	 * needed.
	 */
	NAUTILUS_CHECK_STRING_RESULT (test_strftime ("%m/%d/%y, %I:%M %p", 2000, 1, 1, 1, 0, 0), _("01/01/00, 01:00 AM"));
	NAUTILUS_CHECK_STRING_RESULT (test_strftime ("%-m/%-d/%y, %-I:%M %p", 2000, 1, 1, 1, 0, 0), _("1/1/00, 1:00 AM"));
	NAUTILUS_CHECK_STRING_RESULT (test_strftime ("%_m/%_d/%y, %_I:%M %p", 2000, 1, 1, 1, 0, 0), _(" 1/ 1/00,  1:00 AM"));

	g_free (huge_string);

	/* nautilus_shell_quote */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_shell_quote (""), "''");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_shell_quote ("a"), "'a'");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_shell_quote ("'"), "''\\'''");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_shell_quote ("'a"), "''\\''a'");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_shell_quote ("a'"), "'a'\\'''");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_shell_quote ("a'a"), "'a'\\''a'");
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
