/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-enumeration.c: Enumeration data structure.

   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
  
   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "eel-enumeration.h"

#include "eel-debug.h"
#include "eel-glib-extensions.h"
#include "eel-lib-self-check-functions.h"
#include "eel-string.h"
#include "eel-i18n.h"

static gboolean suppress_duplicate_registration_warning;

struct EelEnumeration
{
	char *id;
	GPtrArray *entries; /* array of EelEnumerationEntry */
};

static EelEnumeration *
eel_enumeration_new (const char *id)
{
	EelEnumeration *enumeration;

	g_assert (id != NULL);
	g_assert (id[0] != '\0');
	
	enumeration = g_new0 (EelEnumeration, 1);

	enumeration->id = g_strdup (id);
	enumeration->entries = g_ptr_array_new ();

	return enumeration;
}

static void
free_entry (EelEnumerationEntry *entry)
{
	g_free (entry->name);
	g_free (entry->description);
	g_free (entry);
}

static void
eel_enumeration_free (EelEnumeration *enumeration)
{
	if (enumeration == NULL) {
		return;
	}

	g_free (enumeration->id);
	g_ptr_array_foreach (enumeration->entries, (GFunc) free_entry, NULL);
	g_ptr_array_free (enumeration->entries, TRUE);
	g_free (enumeration);
}

char *
eel_enumeration_get_id (const EelEnumeration *enumeration)
{
	g_return_val_if_fail (enumeration != NULL, NULL);

	return g_strdup (enumeration->id);
}

guint
eel_enumeration_get_length (const EelEnumeration *enumeration)
{
	g_return_val_if_fail (enumeration != NULL, 0);

	return enumeration->entries->len;
}

const EelEnumerationEntry *
eel_enumeration_get_nth_entry (const EelEnumeration *enumeration,
			       guint n)
{
	g_return_val_if_fail (enumeration != NULL, NULL);
	g_return_val_if_fail (n < enumeration->entries->len, NULL);

	return (EelEnumerationEntry *) g_ptr_array_index (enumeration->entries, n);
}

int
eel_enumeration_get_name_position (const EelEnumeration *enumeration,
				   const char *name)
{
	int i;

	g_return_val_if_fail (enumeration != NULL, -1);
	g_return_val_if_fail (name != NULL, -1);

	for (i = 0; i < enumeration->entries->len; ++i) {
		EelEnumerationEntry *entry = enumeration->entries->pdata[i];
		if (strcmp (name, entry->name) == 0) {
			return i;
		}
	}

	return -1;
}

gboolean
eel_enumeration_contains_name (const EelEnumeration *enumeration,
			       const char *name)
{
	g_return_val_if_fail (enumeration != NULL, FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	return eel_enumeration_get_name_position (enumeration, name) != -1;
}

guint
eel_enumeration_get_value_for_name (const EelEnumeration *enumeration,
				    const char           *name)
{
	int i;

	g_return_val_if_fail (enumeration != NULL, 0);
	g_return_val_if_fail (name != NULL, 0);

	for (i = 0; i < enumeration->entries->len; ++i) {
		EelEnumerationEntry *entry = enumeration->entries->pdata[i];
		if (strcmp (name, entry->name) == 0) {
			return entry->value;
		}
	}

	g_warning ("No name '%s' in enumeration '%s'", name, enumeration->id);

	return 0;
}

const char *
eel_enumeration_get_name_for_value (const EelEnumeration *enumeration,
				    int                   value)
{
	int i;

	g_return_val_if_fail (enumeration != NULL, 0);

	for (i = 0; i < enumeration->entries->len; ++i) {
		EelEnumerationEntry *entry = enumeration->entries->pdata[i];
		if (value == entry->value) {
			return entry->name;
		}
	}

	g_warning ("No value '%d' in enumeration '%s'", value, enumeration->id);

	return NULL;
}

char **
eel_enumeration_get_names (const EelEnumeration *enumeration)
{
	GPtrArray *names;
	int i;

	g_return_val_if_fail (enumeration != NULL, NULL);

	if (enumeration->entries->len == 0) {
		return NULL;
	}

	names = g_ptr_array_sized_new (enumeration->entries->len + 1);
	for (i = 0; i < enumeration->entries->len; ++i) {
		EelEnumerationEntry *entry = enumeration->entries->pdata[i];
		g_ptr_array_add (names, g_strdup (entry->name));
	}
	g_ptr_array_add (names, NULL);

	return (char **) g_ptr_array_free (names, FALSE);
}

static EelEnumeration *
eel_enumeration_new_from_tokens (const char *id,
				 const char *names,
				 const char *descriptions,
				 const char *values,
				 const char *delimiter)
{
	EelEnumeration *enumeration;
	char **namev;
	char **descriptionv;
	char **valuev;
	int length;
	guint i;

	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (id[0] != '\0', NULL);
	g_return_val_if_fail (names != NULL, NULL);
	g_return_val_if_fail (names[0] != '\0', NULL);
	g_return_val_if_fail (values != NULL, NULL);
	g_return_val_if_fail (values[0] != '\0', NULL);
	g_return_val_if_fail (delimiter != NULL, NULL);
	g_return_val_if_fail (delimiter[0] != '\0', NULL);

	enumeration = eel_enumeration_new (id);

	namev = g_strsplit (names, delimiter, -1);
	valuev = g_strsplit (values, delimiter, -1);

	length = g_strv_length (namev);
	if (g_strv_length (valuev) != length) {
		g_warning ("names and values have different lengths.");
		g_strfreev (namev);
		g_strfreev (valuev);
		return NULL;
	}

	descriptionv = descriptions != NULL ? 
		       g_strsplit (descriptions, delimiter, -1) : NULL;

	if (descriptionv != NULL) {
		if (g_strv_length (descriptionv) != length) {
			g_warning ("names and descriptions have different lengths.");
			g_strfreev (namev);
			g_strfreev (descriptionv);
			g_strfreev (valuev);
			return NULL;
		}
	}

	for (i = 0; i < length; i++) {
		EelEnumerationEntry *entry;
		int value;

		if (!eel_str_to_int (valuev[i], &value)) {
			g_warning ("Could not convert value '%d' to an integer.  Using 0.", i);
			value = 0;
		}

		entry = g_new0 (EelEnumerationEntry, 1);
		entry->name = namev[i];
		entry->description = descriptionv ? descriptionv[i] : NULL;
		entry->value = value;

		g_ptr_array_add (enumeration->entries, entry);
	}

	return enumeration;
}

static EelEnumerationEntry *
dup_entry (const EelEnumerationEntry *entry)
{
	EelEnumerationEntry *res;

	res = g_new0 (EelEnumerationEntry, 1);
	res->name = g_strdup (entry->name);
	res->description = g_strdup (entry->description);
	res->value = entry->value;

	return res;
}

static EelEnumeration *
eel_enumeration_new_from_entries (const char *id,
				  const EelEnumerationEntry entries[],
				  guint n_entries)
{
	EelEnumeration *enumeration;
	guint i;

	g_assert (id != NULL);
	g_assert (id[0] != '\0');
	g_assert (entries != NULL);

	enumeration = eel_enumeration_new (id);

	for (i = 0; i < n_entries; i++) {
		g_ptr_array_add (enumeration->entries, dup_entry (&entries[i]));
	}

	return enumeration;
}

static GHashTable *enumeration_table = NULL;

static void
enumeration_table_free (void)
{
	if (enumeration_table != NULL) {
		g_hash_table_destroy (enumeration_table);
		enumeration_table = NULL;
	}
}

static GHashTable *
enumeration_table_get (void)
{
	if (enumeration_table != NULL) {
		return enumeration_table;
	}

	enumeration_table = g_hash_table_new_full (g_str_hash,
						   g_str_equal,
						   (GDestroyNotify) g_free,
						   (GDestroyNotify) eel_enumeration_free);

	eel_debug_call_at_shutdown (enumeration_table_free);

	return enumeration_table;
}

const EelEnumeration *
eel_enumeration_lookup (const char *id)
{
	GHashTable *table;

	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (id[0] != '\0', NULL);

	table = enumeration_table_get ();
	g_return_val_if_fail (table != NULL, NULL);

	return g_hash_table_lookup (table, id);
}

void
eel_enumeration_register (const char                *id,
			  const EelEnumerationEntry  entries[],
			  guint                      n_entries)
{
	GHashTable *table;
	EelEnumeration *enumeration;

	g_return_if_fail (id != NULL);
	g_return_if_fail (id[0] != '\0');
	g_return_if_fail (entries != NULL);

	table = enumeration_table_get ();
	g_return_if_fail (table != NULL);

	if (eel_enumeration_lookup (id) != NULL) {
		if (!suppress_duplicate_registration_warning) {
			g_warning ("Trying to register duplicate enumeration '%s'.", id);
		}

		return;
	}

	enumeration = eel_enumeration_new_from_entries (id, entries, n_entries);

	g_hash_table_insert (table, g_strdup (id), enumeration);
}


#if !defined (EEL_OMIT_SELF_CHECK)

#define CHECK_ENUMERATION_ENTRY(enumeration, i, name, description, value) \
	EEL_CHECK_INTEGER_RESULT (eel_enumeration_get_name_position (enumeration, name), i); \
	EEL_CHECK_INTEGER_RESULT (eel_enumeration_get_value_for_name (enumeration, name), value); \
	EEL_CHECK_STRING_RESULT (g_strdup (eel_enumeration_get_name_for_value (enumeration, value)), name);

static EelEnumerationEntry speed_tradeoff_enum_entries[] = {
	{ "always",	    "Always",		10 },
	{ "local_only",	    "Local Files Only",	20 },
	{ "never",	    "Never",		30 }
};

static EelEnumerationEntry standard_zoom_levels_enum_entries[] = {
	{ "smallest",	    "25%",	25 },
	{ "smaller",	    "50%",	50 },
	{ "small",	    "75%",	75 },
	{ "standard",	    "100%",	100 },
	{ "large",	    "150%",	150 },
	{ "larger",	    "200%",	200 },
	{ "largest",	    "400%",	400 }
};

static EelEnumerationEntry file_size_enum_entries[] = {
	{ "102400",	    "100 K",	102400 },
	{ "512000",	    "500 K",	512000 },
	{ "1048576",	    "1 MB",	1048576 },
	{ "3145728",	    "3 MB",	3145728 },
	{ "5242880",	    "5 MB",	5242880 },
	{ "10485760",	    "10 MB",	10485760 },
	{ "104857600",	    "100 MB",	104857600 }
};

#define CHECK_REGISTERED_ENUMERATION(enumname) \
G_STMT_START { \
	const EelEnumeration *e; \
	int i; \
	e = eel_enumeration_lookup (#enumname); \
	g_return_if_fail (e != NULL); \
	for (i = 0; i < G_N_ELEMENTS (enumname##_enum_entries); i++) { \
		CHECK_ENUMERATION_ENTRY (e, \
					 i, \
				 	 enumname##_enum_entries[i].name, \
					 enumname##_enum_entries[i].description, \
					 enumname##_enum_entries[i].value); \
	} \
	EEL_CHECK_INTEGER_RESULT (eel_enumeration_get_length (e), i); \
} G_STMT_END

void
eel_self_check_enumeration (void)
{
	EelEnumeration *e;
	char **names;

	/***/
	e = eel_enumeration_new_from_tokens ("id",
					     "single",
					     NULL,
					     "1",
					     ",");
	
	CHECK_ENUMERATION_ENTRY (e, 0, "single", "", 1);
 	EEL_CHECK_STRING_RESULT (eel_enumeration_get_id (e), "id");
 	EEL_CHECK_INTEGER_RESULT (eel_enumeration_get_length (e), 1);
	eel_enumeration_free (e);

	/***/
	e = eel_enumeration_new_from_tokens ("id",
					     "apple,orange,banana",
					     NULL,
					     "1,2,3",
					     ",");
	
	CHECK_ENUMERATION_ENTRY (e, 0, "apple", "", 1);
	CHECK_ENUMERATION_ENTRY (e, 1, "orange", "", 2);
	CHECK_ENUMERATION_ENTRY (e, 2, "banana", "", 3);
 	EEL_CHECK_STRING_RESULT (eel_enumeration_get_id (e), "id");
 	EEL_CHECK_INTEGER_RESULT (eel_enumeration_get_length (e), 3);
	eel_enumeration_free (e);

	/***/
	e = eel_enumeration_new_from_tokens ("id",
					     "foo",
					     NULL,
					     "666",
					     ",");
	CHECK_ENUMERATION_ENTRY (e, 0, "foo", "", 666);
 	EEL_CHECK_STRING_RESULT (eel_enumeration_get_id (e), "id");
 	EEL_CHECK_INTEGER_RESULT (eel_enumeration_get_length (e), 1);
	eel_enumeration_free (e);

	/***/
	e = eel_enumeration_new_from_tokens ("id",
					     "one,two,---,three",
					     "One,Two,---,Three",
					     "1,2,0,3",
					     ",");
	CHECK_ENUMERATION_ENTRY (e, 0, "one", "One", 1);
	CHECK_ENUMERATION_ENTRY (e, 1, "two", "Two", 2);
	CHECK_ENUMERATION_ENTRY (e, 2, "---", "---", 0);
	CHECK_ENUMERATION_ENTRY (e, 3, "three", "Three", 3);
 	EEL_CHECK_INTEGER_RESULT (eel_enumeration_get_length (e), 4);
	eel_enumeration_free (e);

	/***/
	e = eel_enumeration_new_from_tokens ("id",
					     "red,green,blue",
					     "Red Desc,Green Desc,Blue Desc",
					     "10,20,30",
					     ",");
	
	CHECK_ENUMERATION_ENTRY (e, 0, "red", "Red Desc", 10);
	CHECK_ENUMERATION_ENTRY (e, 1, "green", "Green Desc", 20);
	CHECK_ENUMERATION_ENTRY (e, 2, "blue", "Blue Desc", 30);
 	EEL_CHECK_STRING_RESULT (eel_enumeration_get_id (e), "id");
 	EEL_CHECK_INTEGER_RESULT (eel_enumeration_get_length (e), 3);
	
 	EEL_CHECK_BOOLEAN_RESULT (eel_enumeration_contains_name (e, "red"), TRUE);
 	EEL_CHECK_BOOLEAN_RESULT (eel_enumeration_contains_name (e, "green"), TRUE);
 	EEL_CHECK_BOOLEAN_RESULT (eel_enumeration_contains_name (e, "blue"), TRUE);
 	EEL_CHECK_BOOLEAN_RESULT (eel_enumeration_contains_name (e, "pink"), FALSE);
	
	eel_enumeration_free (e);

	/***/
	e = eel_enumeration_new_from_tokens ("id",
					     "red,foo:green,bar:blue,baz",
					     "Red,Desc:Green,Desc:Blue,Desc",
					     "10:20:30",
					     ":");

	CHECK_ENUMERATION_ENTRY (e, 0, "red,foo", "Red,Desc", 10);
	CHECK_ENUMERATION_ENTRY (e, 1, "green,bar", "Green,Desc", 20);
	CHECK_ENUMERATION_ENTRY (e, 2, "blue,baz", "Blue,Desc", 30);
 	EEL_CHECK_STRING_RESULT (eel_enumeration_get_id (e), "id");
 	EEL_CHECK_INTEGER_RESULT (eel_enumeration_get_length (e), 3);
 	EEL_CHECK_BOOLEAN_RESULT (eel_enumeration_contains_name (e, "black"), FALSE);

	names = eel_enumeration_get_names (e);
	EEL_CHECK_INTEGER_RESULT (strcmp(names[2], "blue,baz"), 0);
	g_strfreev (names);
	eel_enumeration_free (e);

	/***/
	suppress_duplicate_registration_warning = TRUE;
	eel_enumeration_register ("speed_tradeoff",
				  speed_tradeoff_enum_entries,
				  G_N_ELEMENTS (speed_tradeoff_enum_entries));
	eel_enumeration_register ("standard_zoom_levels",
				  standard_zoom_levels_enum_entries,
				  G_N_ELEMENTS (standard_zoom_levels_enum_entries));
	eel_enumeration_register ("file_size",
				  file_size_enum_entries,
				  G_N_ELEMENTS (file_size_enum_entries));
	suppress_duplicate_registration_warning = FALSE;

	CHECK_REGISTERED_ENUMERATION(speed_tradeoff);
	CHECK_REGISTERED_ENUMERATION(standard_zoom_levels);
	CHECK_REGISTERED_ENUMERATION(file_size);
}

#endif /* !EEL_OMIT_SELF_CHECK */
