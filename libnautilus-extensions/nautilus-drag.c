/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-drag.c - Common Drag & drop handling code shared by the icon container
   and the list view.

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
   	    Ettore Perazzoli <ettore@gnu.org>
*/

#include <config.h>
#include "nautilus-drag.h"

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/gnome-popup-menu.h>
#include <string.h>
#include <stdio.h>

#include "nautilus-glib-extensions.h"

void
nautilus_drag_init (NautilusDragInfo *drag_info,
		    const GtkTargetEntry *drag_types, int drag_type_count, 
		    GdkBitmap *stipple)
{
	drag_info->target_list = gtk_target_list_new (drag_types,
						   drag_type_count);

	if (stipple != NULL) {
		drag_info->stipple = gdk_bitmap_ref (stipple);
	}
}

void
nautilus_drag_finalize (NautilusDragInfo *drag_info)
{
	gtk_target_list_unref (drag_info->target_list);
	nautilus_drag_destroy_selection_list (drag_info->selection_list);

	if (drag_info->stipple != NULL) {
		gdk_bitmap_unref (drag_info->stipple);
	}

	g_free (drag_info);
}


/* Functions to deal with DragSelectionItems.  */

DragSelectionItem *
nautilus_drag_selection_item_new (void)
{
	return g_new0 (DragSelectionItem, 1);
}

static void
drag_selection_item_destroy (DragSelectionItem *item)
{
	g_free (item->uri);
	g_free (item);
}

void
nautilus_drag_destroy_selection_list (GList *list)
{
	GList *p;

	if (list == NULL)
		return;

	for (p = list; p != NULL; p = p->next)
		drag_selection_item_destroy (p->data);

	g_list_free (list);
}

GList *
nautilus_drag_build_selection_list (GtkSelectionData *data)
{
	GList *result;
	const guchar *p, *oldp;
	int size;

	result = NULL;
	oldp = data->data;
	size = data->length;

	while (size > 0) {
		DragSelectionItem *item;
		guint len;

		/* The list is in the form:

		   name\rx:y:width:height\r\n

		   The geometry information after the first \r is optional.  */

		/* 1: Decode name. */

		p = memchr (oldp, '\r', size);
		if (p == NULL) {
			break;
		}

		item = nautilus_drag_selection_item_new ();

		len = p - oldp;

		item->uri = g_malloc (len + 1);
		memcpy (item->uri, oldp, len);
		item->uri[len] = 0;

		p++;
		if (*p == '\n' || *p == '\0') {
			result = g_list_prepend (result, item);
			if (p == 0) {
				g_warning ("Invalid x-special/gnome-icon-list data received: "
					   "missing newline character.");
				break;
			} else {
				oldp = p + 1;
				continue;
			}
		}

		size -= p - oldp;
		oldp = p;

		/* 2: Decode geometry information.  */

		item->got_icon_position = sscanf (p, "%d:%d:%d:%d%*s",
						  &item->icon_x, &item->icon_y,
						  &item->icon_width, &item->icon_height) == 4;
		if (!item->got_icon_position) {
			g_warning ("Invalid x-special/gnome-icon-list data received: "
				   "invalid icon position specification.");
		}

		result = g_list_prepend (result, item);

		p = memchr (p, '\r', size);
		if (p == NULL || p[1] != '\n') {
			g_warning ("Invalid x-special/gnome-icon-list data received: "
				   "missing newline character.");
			if (p == NULL) {
				break;
			}
		} else {
			p += 2;
		}

		size -= p - oldp;
		oldp = p;
	}

	return result;
}

gboolean
nautilus_drag_items_local (const char *target_uri_string, const GList *selection_list)
{
	/* check if the first item on the list has target_uri_string as a parent
	 * we should really test each item but that would be slow for large selections
	 * and currently dropped items can only be from the same container
	 */
	GnomeVFSURI *target_uri;
	GnomeVFSURI *item_uri;
	gboolean result;

	/* must have at least one item */
	g_assert (selection_list);

	result = FALSE;

	target_uri = gnome_vfs_uri_new (target_uri_string);

	/* get the parent URI of the first item in the selection */
	item_uri = gnome_vfs_uri_new (((DragSelectionItem *)selection_list->data)->uri);
	result = gnome_vfs_uri_is_parent (target_uri, item_uri, FALSE);
	
	gnome_vfs_uri_unref (item_uri);
	gnome_vfs_uri_unref (target_uri);
	
	return result;
}

gboolean
nautilus_drag_can_accept_item (NautilusFile *drop_target_item,
			       const char *item_uri)
{
	if (!nautilus_file_is_directory (drop_target_item)) {
		return FALSE;
	}

	/* target is a directory, find out if it matches the item */
	return !nautilus_file_matches_uri (drop_target_item, item_uri);
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
	for (max = 100; items != NULL && max >=0 ; items = items->next, max--) {
		if (!nautilus_drag_can_accept_item (drop_target_item, 
			((DragSelectionItem *)items->data)->uri)) {
			return FALSE;
		}
	}

	return TRUE;		
}

void
nautilus_drag_default_drop_action_for_icons (GdkDragContext *context,
	const char *target_uri_string, const GList *items,
	int *default_action, int *non_default_action)
{
	gboolean same_fs;
	GnomeVFSURI *target_uri;
	GnomeVFSURI *dropped_uri;
	GdkDragAction actions;
	
	if (target_uri_string == NULL) {
		*default_action = 0;
		*non_default_action = 0;
		return;
	}

	actions = context->actions & (GDK_ACTION_MOVE | GDK_ACTION_COPY);
	if (actions == 0) {
		 /* We can't use copy or move, just go with the suggested action.
		  * 
		  * Note: it would be more correct to only choose between move
		  * and copy if both are specified in context->actions.
		  * There is a problem in gtk-dnd.c though where context->actions
		  * gets set only to copy when Control is held down, despite the
		  * fact that bot copy and move were requested.
		  * 
		  */
		*default_action = context->suggested_action;
		*non_default_action = context->suggested_action;

		return;
	}
	
	target_uri = gnome_vfs_uri_new (target_uri_string);

	/* Compare the first dropped uri with the target uri for same fs match. */
	dropped_uri = gnome_vfs_uri_new (((DragSelectionItem *)items->data)->uri);
	same_fs = TRUE;

	gnome_vfs_check_same_fs_uris (target_uri, dropped_uri, &same_fs);
	gnome_vfs_uri_unref (dropped_uri);
	gnome_vfs_uri_unref (target_uri);
	
	if (same_fs) {
		*default_action = GDK_ACTION_MOVE;
		*non_default_action = GDK_ACTION_COPY;
	} else {
		*default_action = GDK_ACTION_COPY;
		*non_default_action = GDK_ACTION_MOVE;
	}
}

/* Encode a "x-special/gnome-icon-list" selection.
   Along with the URIs of the dragged files, this encodes
   the location and size of each icon relative to the cursor.
*/
static void
add_one_gnome_icon_list (const char *uri, int x, int y, int w, int h, 
	gpointer data)
{
	GString *result = (GString *)data;
	char *s;

	s = g_strdup_printf ("%s\r%d:%d:%hu:%hu\r\n",
			     uri, x, y, w, h);
	
	g_string_append (result, s);
	g_free (s);
}

/* Encode a "text/uri-list" selection.  */
static void
add_one_uri_list (const char *uri, int x, int y, int w, int h, 
	gpointer data)
{
	GString *result = (GString *)data;
	g_string_append (result, uri);
	g_string_append (result, "\r\n");
}

/* Common function for drag_data_get_callback calls.
 * Returns FALSE if it doesn't handle drag data
 */
gboolean
nautilus_drag_drag_data_get (GtkWidget *widget,
			     GdkDragContext *context,
			     GtkSelectionData *selection_data,
			     guint info,
			     guint32 time,
			     gpointer container_context,
			     NautilusDragEachSelectedItemIterator each_selected_item_iterator)
{
	GString *result;

	if (info != NAUTILUS_ICON_DND_GNOME_ICON_LIST && info != NAUTILUS_ICON_DND_URI_LIST) {
		/* don't know how to handle */
		return FALSE;
	}
	
	result = g_string_new (NULL);
	
	switch (info) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		each_selected_item_iterator (add_one_gnome_icon_list, container_context, result);
		break;
	case NAUTILUS_ICON_DND_URI_LIST:
		each_selected_item_iterator (add_one_uri_list, container_context, result);
		break;
	default:
		g_assert_not_reached ();
	}
	
	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, result->str, result->len);

	return TRUE;
}

int
nautilus_drag_modifier_based_action (int default_action, int non_default_action)
{
	GdkModifierType modifiers;
	gdk_window_get_pointer (NULL, NULL, NULL, &modifiers);
	
	if ((modifiers & GDK_CONTROL_MASK) != 0) {
		return non_default_action;
	} else if ((modifiers & GDK_SHIFT_MASK) != 0) {
		return GDK_ACTION_LINK;
	} else if ((modifiers & GDK_MOD1_MASK) != 0) {
		return GDK_ACTION_ASK;
	}

	return default_action;
}

/* The menu of DnD actions */
static GnomeUIInfo menu_items[] = {
	GNOMEUIINFO_ITEM_NONE ("_Move here", NULL, NULL),
	GNOMEUIINFO_ITEM_NONE ("_Copy here", NULL, NULL),
	GNOMEUIINFO_ITEM_NONE ("_Link here", NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE ("Cancel", NULL, NULL),
	GNOMEUIINFO_END
};

/* Pops up a menu of actions to perform on dropped files */
GdkDragAction
nautilus_drag_drop_action_ask (GdkDragAction actions)
{
	GtkWidget *menu;
	int action;

	/* Create the menu and set the sensitivity of the items based on the
	 * allowed actions.
	 */
	menu = gnome_popup_menu_new (menu_items);

	gtk_widget_set_sensitive (menu_items[0].widget, (actions & GDK_ACTION_MOVE) != 0);
	gtk_widget_set_sensitive (menu_items[1].widget, (actions & GDK_ACTION_COPY) != 0);
	gtk_widget_set_sensitive (menu_items[2].widget, (actions & GDK_ACTION_LINK) != 0);

	switch (gnome_popup_menu_do_popup_modal (menu, NULL, NULL, NULL, NULL)) {
	case 0:
		action = GDK_ACTION_MOVE;
		break;

	case 1:
		action = GDK_ACTION_COPY;
		break;

	case 2:
		action = GDK_ACTION_LINK;
		break;

	default:
		action = 0; 
	}

	gtk_widget_destroy (menu);

	return action;
}
