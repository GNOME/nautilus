/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-enumeration.c: Enumeration data structure.

   Copyright (C) 2000 Eazel, Inc.
  
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

#include "nautilus-enumeration.h"

#include "nautilus-glib-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-string.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

struct NautilusEnumeration
{
	NautilusStringList *entries;
	NautilusStringList *descriptions;
	GList *values;
};

/**
 * nautilus_enumeration_new:
 *
 * Return value: A newly constructed empty enumeration.
 */
NautilusEnumeration *
nautilus_enumeration_new (void)
{
	NautilusEnumeration *enumeration;
	
	enumeration = g_new0 (NautilusEnumeration, 1);

	return enumeration;
}

void
nautilus_enumeration_free (NautilusEnumeration *enumeration)
{
	if (enumeration == NULL) {
		return;
	}
	
	nautilus_string_list_free (enumeration->entries);
	nautilus_string_list_free (enumeration->descriptions);
	g_list_free (enumeration->values);
	
	g_free (enumeration);
}

void
nautilus_enumeration_insert (NautilusEnumeration *enumeration,
			     const char *entry,
			     const char *description,
			     int value)
{
	g_return_if_fail (enumeration != NULL);
	g_return_if_fail (entry != NULL);

	if (enumeration->entries == NULL) {
		enumeration->entries =nautilus_string_list_new (TRUE);
	}
	
	if (enumeration->descriptions == NULL) {
		enumeration->descriptions = nautilus_string_list_new (TRUE);
	}

	nautilus_string_list_insert (enumeration->entries, entry);
	nautilus_string_list_insert (enumeration->descriptions, description ? description : "");
	enumeration->values = g_list_append (enumeration->values, GINT_TO_POINTER (value));
}

char *
nautilus_enumeration_get_nth_entry (const NautilusEnumeration *enumeration,
				    guint n)
{
	g_return_val_if_fail (enumeration != NULL, NULL);
	g_return_val_if_fail (n < nautilus_string_list_get_length (enumeration->entries), NULL);

	return nautilus_string_list_nth (enumeration->entries, n);
}

char *
nautilus_enumeration_get_nth_description (const NautilusEnumeration *enumeration,
					  guint n)
{
	g_return_val_if_fail (enumeration != NULL, NULL);
	g_return_val_if_fail (n < nautilus_string_list_get_length (enumeration->descriptions), NULL);

	return nautilus_string_list_nth (enumeration->descriptions, n);
}

int
nautilus_enumeration_get_nth_value (const NautilusEnumeration *enumeration,
				    guint n)
{
	g_return_val_if_fail (enumeration != NULL, 0);
	g_return_val_if_fail (n < g_list_length (enumeration->values), 0);
	
	return GPOINTER_TO_INT (g_list_nth_data (enumeration->values, n));
}

guint
nautilus_enumeration_get_num_entries (const NautilusEnumeration *enumeration)
{
	g_return_val_if_fail (enumeration != NULL, 0);

	return nautilus_string_list_get_length (enumeration->entries);
}

NautilusEnumeration *
nautilus_enumeration_new_from_tokens (const char *entries,
				      const char *descriptions,
				      const char *values,
				      const char *delimiter)
{
	NautilusEnumeration *enumeration;
	NautilusStringList *entry_list;
	NautilusStringList *description_list;
	NautilusStringList *value_list;
	guint i;
	int value;

	g_return_val_if_fail (entries != NULL, NULL);
	g_return_val_if_fail (entries[0] != '\0', NULL);
	g_return_val_if_fail (values != NULL, NULL);
	g_return_val_if_fail (values[0] != '\0', NULL);
	g_return_val_if_fail (delimiter != NULL, NULL);
	g_return_val_if_fail (delimiter[0] != '\0', NULL);

	enumeration = nautilus_enumeration_new ();	

	entry_list = nautilus_string_list_new_from_tokens (entries, delimiter, TRUE);
	value_list = nautilus_string_list_new_from_tokens (values, delimiter, TRUE);

	if (nautilus_string_list_get_length (entry_list)
	    != nautilus_string_list_get_length (value_list)) {
		g_warning ("entries and values have different lengths.");
		nautilus_string_list_free (entry_list);
		nautilus_string_list_free (value_list);
		return NULL;
	}

	description_list = 
		descriptions != NULL ? 
		nautilus_string_list_new_from_tokens (descriptions, delimiter, TRUE) :
		NULL;

	if (description_list != NULL) {
		if (nautilus_string_list_get_length (entry_list)
		    != nautilus_string_list_get_length (description_list)) {
			g_warning ("entries and descriptions have different lengths.");
			nautilus_string_list_free (entry_list);
			nautilus_string_list_free (value_list);
			nautilus_string_list_free (description_list);
			return NULL;
		}
	}

	enumeration->entries = entry_list;

	if (description_list == NULL) {
		description_list = nautilus_string_list_new (TRUE);

		for (i = 0; i < nautilus_string_list_get_length (entry_list); i++) {
			nautilus_string_list_insert (description_list, "");
		}
	}
	
	enumeration->entries = entry_list;
	enumeration->descriptions = description_list;

	for (i = 0; i < nautilus_string_list_get_length (entry_list); i++) {
		if (!nautilus_string_list_nth_as_integer (value_list, i, &value)) {
			g_warning ("Could not convert value '%d' to an integer.  Using 0.", i);
			value = 0;
		}

		enumeration->values = g_list_append (enumeration->values, GINT_TO_POINTER (value));
	}
	nautilus_string_list_free (value_list);
	
	return enumeration;
}

int
nautilus_enumeration_get_entry_position (const NautilusEnumeration *enumeration,
					 const char *entry)
{
	g_return_val_if_fail (enumeration != NULL, NAUTILUS_STRING_LIST_NOT_FOUND);
	g_return_val_if_fail (entry != NULL, NAUTILUS_STRING_LIST_NOT_FOUND);

	if (enumeration->entries == NULL) {
		return NAUTILUS_STRING_LIST_NOT_FOUND;
	}

	return nautilus_string_list_get_index_for_string (enumeration->entries, entry);
}

int
nautilus_enumeration_get_value_position (const NautilusEnumeration *enumeration,
					 int value)
{
	GList *node;
	int pos;

	g_return_val_if_fail (enumeration != NULL, NAUTILUS_STRING_LIST_NOT_FOUND);

	if (enumeration->values == NULL) {
		return NAUTILUS_STRING_LIST_NOT_FOUND;
	}

	for (node = enumeration->values, pos = 0; node != NULL; node = node->next, pos++) {
		if (GPOINTER_TO_INT (node->data) == value) {
			return pos;
		}
	}

	return NAUTILUS_STRING_LIST_NOT_FOUND;
}

NautilusStringList *
nautilus_enumeration_get_entries (const NautilusEnumeration *enumeration)
{
	g_return_val_if_fail (enumeration != NULL, NULL);

	if (enumeration->entries == NULL) {
		return NULL;
	}

	return nautilus_string_list_new_from_string_list (enumeration->entries, TRUE);
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

#define CHECK_ENUMERATION_ENTRY(enumeration, i, entry, description, value) \
 	NAUTILUS_CHECK_STRING_RESULT (nautilus_enumeration_get_nth_entry (enumeration, i), entry); \
 	NAUTILUS_CHECK_STRING_RESULT (nautilus_enumeration_get_nth_description (enumeration, i), description); \
 	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_enumeration_get_nth_value (enumeration, i), value); \
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_enumeration_get_entry_position (enumeration, entry), i); \
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_enumeration_get_value_position (enumeration, value), i);

void
nautilus_self_check_enumeration (void)
{
	NautilusEnumeration *e;
	NautilusStringList *entries;
	NautilusStringList *entries_test;

	/***/
	e = nautilus_enumeration_new ();
	nautilus_enumeration_insert (e, "foo", NULL, 0);
	CHECK_ENUMERATION_ENTRY (e, 0, "foo", "", 0);
 	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_enumeration_get_num_entries (e), 1);

	nautilus_enumeration_insert (e, "bar", NULL, 1);
	CHECK_ENUMERATION_ENTRY (e, 1, "bar", "", 1);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_enumeration_get_num_entries (e), 2);

	nautilus_enumeration_free (e);

	/***/
	e = nautilus_enumeration_new_from_tokens ("apple,orange,banana",
						  NULL,
						  "1,2,3",
						  ",");
	
	CHECK_ENUMERATION_ENTRY (e, 0, "apple", "", 1);
	CHECK_ENUMERATION_ENTRY (e, 1, "orange", "", 2);
	CHECK_ENUMERATION_ENTRY (e, 2, "banana", "", 3);
 	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_enumeration_get_num_entries (e), 3);
	nautilus_enumeration_free (e);

	/***/
	e = nautilus_enumeration_new_from_tokens ("foo",
						  NULL,
						  "666",
						  ",");
	CHECK_ENUMERATION_ENTRY (e, 0, "foo", "", 666);
 	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_enumeration_get_num_entries (e), 1);
	nautilus_enumeration_free (e);

	/***/
	e = nautilus_enumeration_new_from_tokens ("red,green,blue",
						  "Red Desc,Green Desc,Blue Desc",
						  "10,20,30",
						  ",");

	CHECK_ENUMERATION_ENTRY (e, 0, "red", "Red Desc", 10);
	CHECK_ENUMERATION_ENTRY (e, 1, "green", "Green Desc", 20);
	CHECK_ENUMERATION_ENTRY (e, 2, "blue", "Blue Desc", 30);
 	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_enumeration_get_num_entries (e), 3);
	nautilus_enumeration_free (e);

	/***/
	e = nautilus_enumeration_new_from_tokens ("red,foo:green,bar:blue,baz",
						  "Red,Desc:Green,Desc:Blue,Desc",
						  "10:20:30",
						  ":");

	CHECK_ENUMERATION_ENTRY (e, 0, "red,foo", "Red,Desc", 10);
	CHECK_ENUMERATION_ENTRY (e, 1, "green,bar", "Green,Desc", 20);
	CHECK_ENUMERATION_ENTRY (e, 2, "blue,baz", "Blue,Desc", 30);
 	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_enumeration_get_num_entries (e), 3);

	entries = nautilus_enumeration_get_entries (e);
	entries_test = nautilus_string_list_new_from_tokens ("red,foo:green,bar:blue,baz", ":", TRUE);

 	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_string_list_equals (entries, entries_test), TRUE);

	nautilus_string_list_free (entries);
	nautilus_string_list_free (entries_test);
	nautilus_enumeration_free (e);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
