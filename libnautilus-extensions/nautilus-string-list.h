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

#ifndef NAUTILUS_STRING_LIST_H
#define NAUTILUS_STRING_LIST_H

#include <glib.h>

#define NAUTILUS_STRING_LIST_NOT_FOUND -1

/* Opaque type declaration. */
typedef struct _NautilusStringList NautilusStringList;

/* Test function for finding strings in the list */
typedef gboolean (*NautilusStringListTestFunction) (const NautilusStringList *string_list,
						    const char *string,
						    gpointer callback_data);

/* Construct an empty string list. */
NautilusStringList *nautilus_string_list_new                       (gboolean                        case_sensitive);

/* Construct a string list with a single element */
NautilusStringList *nautilus_string_list_new_from_string           (const char                     *string,
								    gboolean                        case_sensitive);

/* Construct a string list that is a copy of another string list */
NautilusStringList *nautilus_string_list_new_from_string_list      (const NautilusStringList       *other_or_null,
								    gboolean                        case_sensitive);

/* Construct a string list from tokens delimited by the given string and delimeter */
NautilusStringList *nautilus_string_list_new_from_tokens           (const char                     *string,
								    const char                     *delimiter,
								    gboolean                        case_sensitive);

/* Assign the contents another string list.  The other string list can be null. */
void                nautilus_string_list_assign_from_string_list   (NautilusStringList             *string_list,
								    const NautilusStringList       *other_or_null);
/* Free a string list */
void                nautilus_string_list_free                      (NautilusStringList             *string_list_or_null);

/* Insert a string into the collection. */
void                nautilus_string_list_insert                    (NautilusStringList             *string_list,
								    const char                     *string);
/* Clear the collection. */
void                nautilus_string_list_clear                     (NautilusStringList             *string_list);

/* Access the nth string in the collection.  Returns an strduped string. */
char *              nautilus_string_list_nth                       (const NautilusStringList       *string_list,
								    guint                           n);
/* Modify the nth string in the collection. */
void                nautilus_string_list_modify_nth                (NautilusStringList             *string_list,
								    guint                           n,
								    const char                     *string);
/* Remove the nth string in the collection. */
void                nautilus_string_list_remove_nth                (NautilusStringList             *string_list,
								    guint                           n);
/* Does the string list contain the given string ? */
gboolean            nautilus_string_list_contains                  (const NautilusStringList       *string_list,
								    const char                     *string);
/* Find a string using the given test function ? */
char *              nautilus_string_list_find_by_function          (const NautilusStringList       *string_list,
								    NautilusStringListTestFunction  test_function,
								    gpointer                        callback_data);
/* How many strings are currently in the collection ? */
guint               nautilus_string_list_get_length                (const NautilusStringList       *string_list);

/* Get the index for the given string.  Return NAUTILUS_STRING_LIST_NOT_FOUND if not found. */
int                 nautilus_string_list_get_index_for_string      (const NautilusStringList       *string_list,
								    const char                     *string);
/* Does the string list a equal string list b ? */
gboolean            nautilus_string_list_equals                    (const NautilusStringList       *a,
								    const NautilusStringList       *b);
/* Return the string list in a GList.  Must deep free the result with nautilus_g_list_free_deep() */
GList *             nautilus_string_list_as_g_list                 (const NautilusStringList       *string_list);

/* Return the string list as a concatenation of all the items delimeted by delimeter. */
char *              nautilus_string_list_as_concatenated_string    (const NautilusStringList       *string_list,
								    const char                     *delimiter);
/* Sort the string collection. */
void                nautilus_string_list_sort                      (NautilusStringList             *string_list);

/* Sort the string collection using a comparison function. */
void                nautilus_string_list_sort_by_function          (NautilusStringList             *string_list,
								    GCompareFunc                    compare_function);
/* Remove duplicate strings from the collection. */
void                nautilus_string_list_remove_duplicates         (NautilusStringList             *string_list);

/* Invoke the given function for each string in the collection. */
void                nautilus_string_list_for_each                  (const NautilusStringList       *string_list,
								    GFunc                           function,
								    gpointer                        user_data);

/* Return the longest string in the collection. */
char *              nautilus_string_list_get_longest_string        (const NautilusStringList       *string_list);

/* Return the length of the longest string in the collection. */
int                 nautilus_string_list_get_longest_string_length (const NautilusStringList       *string_list);

#endif /* NAUTILUS_STRING_LIST_H */

