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

#include <sys/time.h>
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-string.h"


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
 * @format: format string to pass to strftime. See strftime documentation
 * for details.
 * @time_pieces: date/time, in struct format.
 * 
 * Return value: Newly allocated string containing the formatted time.
 **/
char *
nautilus_strdup_strftime (const char *format, struct tm *time_pieces)
{
	char *result;
	size_t string_length;

	string_length = strftime (NULL, G_MAXINT, format, time_pieces);
	result = g_malloc (string_length + 1);
	strftime (result, string_length + 1, format, time_pieces);

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
	GList *p, *result;

	result = NULL;
	
	if (list == NULL) {
		return NULL;
	}

	for (p = g_list_last (list); p != NULL; p = p->prev) {
		result = g_list_prepend (result, g_strdup (p->data));
	}
	return result;
}


static int
compare_strings (gconstpointer string_a, gconstpointer string_b)
{
        return nautilus_strcmp (string_a, string_b);
}

/**
 * nautilus_g_str_list_sort
 *
 * Sort a list of strings using strcmp.
 *
 * @list: List of strings and/or NULLs.
 * 
 * Return value: @list, sorted.
 **/
GList *
nautilus_g_str_list_sort (GList *list)
{
	return g_list_sort (list, compare_strings);
}

static int
compare_strings_case_insensitive (gconstpointer string_a, gconstpointer string_b)
{
	int insensitive_result;

	insensitive_result =  g_strcasecmp (string_a, string_b);
	if (insensitive_result != 0) {
		return insensitive_result;
	} else {
		return compare_strings (string_a, string_b);
	}
}

/**
 * nautilus_g_str_list_sort_case_insensitive
 *
 * Sort a list of strings using g_strcasecmp.
 *
 * @list: List of strings and/or NULLs.
 * 
 * Return value: @list, sorted.
 **/
GList *
nautilus_g_str_list_sort_case_insensitive (GList *list)
{
	return g_list_sort (list, compare_strings_case_insensitive);
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
 * @removed: The GList * variable pinted to by this argument will be
 * set to the list of elements for which the predicate returned
 * false. */

GList *
nautilus_g_list_partition (GList	          *list,
			   NautilusGPredicateFunc  predicate,
			   gpointer	           user_data,
			   GList                 **failed)
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
	return (gint64)tmp.tv_usec + ((gint64)tmp.tv_sec) * G_GINT64_CONSTANT(1000000);
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
nautilus_test_predicate (char *data,
			 char *user_data)
{
	if (g_strcasecmp (data, user_data) <= 0) {
		return TRUE;
	} else {
		return FALSE;
	}
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
						   (NautilusGPredicateFunc) nautilus_test_predicate,
						   "m",
						   &actual_failed);
	
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_g_str_list_equal (expected_passed, actual_passed), TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_g_str_list_equal (expected_failed, actual_failed), TRUE);
	
	/* Don't free list_to_partition, since it is consumed
	   by nautilus_g_list_partition */
	
	g_list_free (expected_passed);
	g_list_free (actual_passed);
	g_list_free (expected_failed);
	g_list_free (actual_failed);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
