/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-drag.c - Drag & drop handling code that operated on 
   NautilusFile objects.

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

   Authors: Pavel Cisler <pavel@eazel.com>,
*/

#include <config.h>
#include "nautilus-file-dnd.h"

#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>

gboolean
nautilus_drag_can_accept_item (NautilusFile *drop_target_item,
			       const char *item_uri)
{
	if (nautilus_file_matches_uri (drop_target_item, item_uri)) {
		/* can't accept itself */
		return FALSE;
	}
	
	if (nautilus_file_is_directory (drop_target_item)) {
		/* target is a directory, accept anything */
		return TRUE;
	}

	/* All Nautilus links are assumed to be links to directories.
	 * Therefore, they all can accept drags, like all other
	 * directories to. As with other directories, there can be
	 * errors when the actual copy is attempted due to
	 * permissions.
	 */
	if (nautilus_file_is_nautilus_link (drop_target_item)) {
		return TRUE;
	}
	
	return FALSE;
}
					       
gboolean
nautilus_drag_can_accept_items (NautilusFile *drop_target_item,
				const GList *items)
{
	int max;

	if (drop_target_item == NULL)
		return FALSE;

	g_assert (NAUTILUS_IS_FILE (drop_target_item));

	/* Iterate through selection checking if item will get accepted by the
	 * drop target. If more than 100 items selected, return an over-optimisic
	 * result
	 */
	for (max = 100; items != NULL && max >= 0; items = items->next, max--) {
		if (!nautilus_drag_can_accept_item (drop_target_item, 
			((EelDragSelectionItem *)items->data)->uri)) {
			return FALSE;
		}
	}
	
	return TRUE;
}

void
nautilus_drag_file_receive_dropped_keyword (NautilusFile *file, char *keyword)
{
	GList *keywords, *word;

	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (keyword != NULL);

	/* special case the erase emblem */
	if (strcmp (keyword, NAUTILUS_FILE_DND_ERASE_KEYWORD) == 0) {
		keywords = NULL;
	} else {
		keywords = nautilus_file_get_keywords (file);
		word = g_list_find_custom (keywords, keyword, (GCompareFunc) strcmp);
		if (word == NULL) {
			keywords = g_list_prepend (keywords, g_strdup (keyword));
		} else {
			keywords = g_list_remove_link (keywords, word);
			g_free (word->data);
			g_list_free_1 (word);
		}
	}
	
	nautilus_file_set_keywords (file, keywords);
	eel_g_list_free_deep (keywords);
}
