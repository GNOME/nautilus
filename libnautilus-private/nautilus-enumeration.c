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
 * Return value: A newly constructed string list.
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

#if !defined (NAUTILUS_OMIT_SELF_CHECK)
void
nautilus_self_check_enumeration (void)
{
}
#endif /* !NAUTILUS_OMIT_SELF_CHECK */
