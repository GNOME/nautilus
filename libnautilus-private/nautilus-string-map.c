/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-string-map.c: A many-to-one string mapping data structure.
 
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

#include "nautilus-string-map.h"
#include "nautilus-string-list.h"
#include "nautilus-string.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include <string.h>

/* FIXME: The case sensitive flag is not functional yet.  Need to change
 * NautilusStringList to also accept a case_sensitive flag to make it work.
 */

struct _NautilusStringMap
{
	GList		*map;
	gboolean	case_sensitive;
};

typedef struct 
{
	char			*string;
	NautilusStringList	*map_list;
} MapEntry;

/* MapEntry things */
static MapEntry *map_entry_new                       (const char *string,
						      gboolean    case_sensitive);
static void      map_entry_free                      (MapEntry   *map_entry);
static MapEntry *map_entry_list_lookup_mapped_string (GList      *entry_list,
						      const char *mapped_string,
						      gboolean    case_sensitive);
static MapEntry *map_entry_list_lookup               (GList      *entry_list,
						      const char *string);
static gboolean  str_is_equal                        (const char *a,
						      const char *b,
						      gboolean    case_sensitive);

/**
 * nautilus_string_map_new:
 *
 * @case_sensitive: Boolean flag indicating whether the string map is case sensitive.
 *
 * Allocate an empty string map.
 *
 * Return value: A newly allocated string map.
 */
NautilusStringMap *
nautilus_string_map_new (gboolean case_sensitive)
{
	NautilusStringMap * string_map;

	string_map = g_new (NautilusStringMap, 1);

	string_map->map = NULL;
	string_map->case_sensitive = case_sensitive;

	return string_map;
}

/**
 * nautilus_string_map_free:
 *
 * @string_map: A NautilusStringMap
 *
 * Free the string map.
 */
void
nautilus_string_map_free (NautilusStringMap *string_map)
{
	if (string_map == NULL) {
		return;
	}
	
	nautilus_string_map_clear (string_map);

	g_free (string_map);
}

static void
map_for_each_node_free (gpointer	data,
			gpointer	user_data)
{
	MapEntry *map_entry = (MapEntry *) data;
	g_assert (map_entry != NULL);

	map_entry_free (map_entry);
}

/**
 * nautilus_string_map_clear:
 *
 * @string_map: A NautilusStringMap
 *
 * Clear the string map.
 */
void
nautilus_string_map_clear (NautilusStringMap *string_map)
{
	if (string_map == NULL) {
		return;
	}

	g_return_if_fail (string_map != NULL);

	nautilus_g_list_free_deep_custom (string_map->map, map_for_each_node_free, NULL);

	string_map->map = NULL;
}

/**
 * nautilus_string_map_lookup:
 *
 * @string_map: A NautilusStringMap
 * @string: The mapped string to lookup
 *
 * Lookup the mapping for 'string'
 *
 * Return value: A copy of the mapped_to_string or NULL if no mapping exists.
 */
char *
nautilus_string_map_lookup (const NautilusStringMap	*string_map,
			    const char			*string)
{
	MapEntry *map_entry;

	if (string_map == NULL || string == NULL) {
		return NULL;
	}

	map_entry = map_entry_list_lookup (string_map->map, string);

	return map_entry ? g_strdup (map_entry->string) : NULL;
}

/**
 * nautilus_string_map_add:
 *
 * @string_map: A NautilusStringMap
 *
 * Add a mapping from 'string' to 'strings_maps_to'
 */
void
nautilus_string_map_add (NautilusStringMap	*string_map,
			 const char		*string_maps_to,
			 const char		*string)
{
	MapEntry *map_entry;

	g_return_if_fail (string_map != NULL);
	g_return_if_fail (string_maps_to != NULL);
	g_return_if_fail (string != NULL);

	map_entry = map_entry_list_lookup_mapped_string (string_map->map, string_maps_to, string_map->case_sensitive);

	if (map_entry == NULL) {
		map_entry = map_entry_new (string_maps_to, string_map->case_sensitive);

		/* Add a mapping for the string_maps_to to simplify things */
		nautilus_string_list_insert (map_entry->map_list, string_maps_to);

		string_map->map = g_list_append (string_map->map, map_entry);
	}

	nautilus_string_list_insert (map_entry->map_list, string);
}

/* MapEntry things */
static MapEntry *
map_entry_new (const char	*string,
	       gboolean		case_sensitive)
{
	MapEntry *map_entry;

	g_return_val_if_fail (string != NULL, NULL);

	map_entry = g_new (MapEntry, 1);
	map_entry->map_list = nautilus_string_list_new (case_sensitive);
	map_entry->string = g_strdup (string);

	return map_entry;
}

static void
map_entry_free (MapEntry *map_entry)
{
	g_return_if_fail (map_entry);

	nautilus_string_list_free (map_entry->map_list);
	map_entry->map_list = NULL;
	g_free (map_entry->string);
	g_free (map_entry);
}

static MapEntry *
map_entry_list_lookup_mapped_string (GList	*entry_list,
				     const char	*mapped_string,
				     gboolean	case_sensitive)
{
	GList *iterator;

	g_return_val_if_fail (mapped_string != NULL, NULL);

	if (entry_list == NULL) {
		return NULL;
	}

	for (iterator = entry_list; iterator != NULL; iterator = iterator->next) {
		MapEntry *map_entry = (MapEntry *) iterator->data;
		g_assert (map_entry != NULL);

		if (str_is_equal (map_entry->string, mapped_string, case_sensitive)) {
			return map_entry;
		}
	}

	return NULL;
}

static MapEntry *
map_entry_list_lookup (GList		*entry_list,
		       const char	*string)
{
	GList *iterator;

	g_return_val_if_fail (string != NULL, NULL);

	if (entry_list == NULL) {
		return NULL;
	}

	for (iterator = entry_list; iterator != NULL; iterator = iterator->next) {
		MapEntry *map_entry = (MapEntry *) iterator->data;
		g_assert (map_entry != NULL);

		if (nautilus_string_list_contains (map_entry->map_list, string)) {
			return map_entry;
		}
	}

	return NULL;
}

static gboolean
str_is_equal (const char	*a,
	      const char	*b,
	      gboolean		case_sensitive)
{
	return case_sensitive ? nautilus_str_is_equal (a, b) : nautilus_istr_is_equal (a, b);
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_string_map (void)
{
	NautilusStringMap *map;

	map = nautilus_string_map_new (TRUE);

	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "foo"), NULL);
	
	nautilus_string_map_clear (map);
	
	nautilus_string_map_add (map, "animal", "dog");
	nautilus_string_map_add (map, "animal", "cat");
	nautilus_string_map_add (map, "animal", "mouse");
	nautilus_string_map_add (map, "human", "geek");
	nautilus_string_map_add (map, "human", "nerd");
	nautilus_string_map_add (map, "human", "lozer");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "animal"), "animal");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "cat"), "animal");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "dog"), "animal");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "mouse"), "animal");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "human"), "human");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "geek"), "human");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "nerd"), "human");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "lozer"), "human");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "Lozer"), NULL);
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "HuMaN"), NULL);

	nautilus_string_map_free (map);

	/*
	 * case insensitive tests
	 *
	 */
	map = nautilus_string_map_new (FALSE);

	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "foo"), NULL);
	
	nautilus_string_map_clear (map);
	
	nautilus_string_map_add (map, "animal", "dog");
	nautilus_string_map_add (map, "animal", "cat");
	nautilus_string_map_add (map, "animal", "mouse");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "animal"), "animal");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "Animal"), "animal");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "cat"), "animal");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "CAT"), "animal");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "dog"), "animal");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_string_map_lookup (map, "DoG"), "animal");

	nautilus_string_map_free (map);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
