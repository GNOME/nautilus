/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-dnd.c - Common Drag & drop handling code shared by the icon container
   and the list view.

   Copyright (C) 2000, 2001 Eazel, Inc.

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

/* FIXME: This should really be back in Nautilus, not here in Eel. */

#include <config.h>
#include "nautilus-dnd.h"

#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkseparatormenuitem.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-find-directory.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <stdio.h>
#include <string.h>

#define COMMAND_SPECIFIER "command:"

/* a set of defines stolen from the eel-icon-dnd.c file.
 * These are in microseconds.
 */
#define AUTOSCROLL_TIMEOUT_INTERVAL 100
#define AUTOSCROLL_INITIAL_DELAY 750000

/* drag this close to the view edge to start auto scroll*/
#define AUTO_SCROLL_MARGIN 20

/* the smallest amount of auto scroll used when we just enter the autoscroll
 * margin
 */
#define MIN_AUTOSCROLL_DELTA 5
	 
/* the largest amount of auto scroll used when we are right over the view
 * edge
 */
#define MAX_AUTOSCROLL_DELTA 50

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

	drag_info->drop_occured = FALSE;
	drag_info->need_to_destroy = FALSE;
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


/* Functions to deal with NautilusDragSelectionItems.  */

NautilusDragSelectionItem *
nautilus_drag_selection_item_new (void)
{
	return g_new0 (NautilusDragSelectionItem, 1);
}

static void
drag_selection_item_destroy (NautilusDragSelectionItem *item)
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
		NautilusDragSelectionItem *item;
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
	 * FIXME:
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
	item_uri = gnome_vfs_uri_new (((NautilusDragSelectionItem *)selection_list->data)->uri);
	result = gnome_vfs_uri_is_parent (target_uri, item_uri, FALSE);
	
	gnome_vfs_uri_unref (item_uri);
	gnome_vfs_uri_unref (target_uri);
	
	return result;
}

gboolean
nautilus_drag_items_in_trash (const GList *selection_list)
{
	/* check if the first item on the list is in trash.
	 * FIXME:
	 * we should really test each item but that would be slow for large selections
	 * and currently dropped items can only be from the same container
	 */
	return eel_uri_is_in_trash (((NautilusDragSelectionItem *)selection_list->data)->uri);
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
	GnomeVFSResult result;

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

	/* Check for trash URI.  We do a find_directory for any Trash directory. */
	if (eel_uri_is_trash (target_uri_string)) {
		result = gnome_vfs_find_directory (NULL, GNOME_VFS_DIRECTORY_KIND_TRASH,
						   &target_uri, TRUE, FALSE, 0777);
		if (result != GNOME_VFS_OK) {
			*default_action = 0;
			*non_default_action = 0;
			return;
		}

		/* Only move to Trash */
		*default_action = GDK_ACTION_MOVE;
		*non_default_action = GDK_ACTION_MOVE;
		return;

	} else if (eel_str_has_prefix (target_uri_string, COMMAND_SPECIFIER)) {
		*default_action = GDK_ACTION_MOVE;
		*non_default_action = GDK_ACTION_MOVE;
		return;
	} else {
		target_uri = gnome_vfs_uri_new (target_uri_string);
	}

	if (target_uri == NULL) {
		*default_action = 0;
		*non_default_action = 0;
		return;
	}

	/* Compare the first dropped uri with the target uri for same fs match. */
	dropped_uri = gnome_vfs_uri_new (((NautilusDragSelectionItem *)items->data)->uri);
	same_fs = TRUE;

	gnome_vfs_check_same_fs_uris (dropped_uri, target_uri, &same_fs);
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
add_one_gnome_icon (const char *uri, int x, int y, int w, int h, 
		    gpointer data)
{
	GString *result;
	char *s;

	result = (GString *) data;
	s = g_strdup_printf ("%s\r%d:%d:%hu:%hu\r\n",
			     uri, x, y, w, h);
	g_string_append (result, s);
	g_free (s);
}

/* Encode a "_NETSCAPE_URL_" selection.
 * As far as I can tell, Netscape is expecting a single
 * URL to be returned.  I cannot discover a way to construct
 * a list to be returned that Netscape can understand.
 * GMC also fails to do this as well.
 */
static void
add_one_netscape_url (const char *url, int x, int y, int w, int h, gpointer data)
{
	GString *result;

	result = (GString *) data;
	if (result->len == 0) {
		g_string_append (result, url);
	}
}

static gboolean
is_path_that_gnome_uri_list_extract_filenames_can_parse (const char *path)
{
	if (path == NULL || path [0] == '\0') {
		return FALSE;
	}

	/* It strips leading and trailing spaces. So it can't handle
	 * file names with leading and trailing spaces.
	 */
	if (g_ascii_isspace (path [0])) {
		return FALSE;
	}
	if (g_ascii_isspace (path [strlen (path) - 1])) {
		return FALSE;
	}

	/* # works as a comment delimiter, and \r and \n are used to
	 * separate the lines, so it can't handle file names with any
	 * of these.
	 */
	if (strchr (path, '#') != NULL
	    || strchr (path, '\r') != NULL
	    || strchr (path, '\n') != NULL) {
		return FALSE;
	}

	return TRUE;
}

/* Encode a "text/plain" selection; this is a broken URL -- just
 * "file:" with a path after it (no escaping or anything). We are
 * trying to make the old gnome_uri_list_extract_filenames function
 * happy, so this is coded to its idiosyncrasises.
 */
static void
add_one_compatible_uri (const char *uri, int x, int y, int w, int h, gpointer data)
{
	GString *result;
	char *local_path;
	
	result = (GString *) data;

	/* For URLs that do not have a file: scheme, there's no harm
	 * in passing the real URL. But for URLs that do have a file:
	 * scheme, we have to send a URL that will work with the old
	 * gnome-libs function or nothing will be able to understand
	 * it.
	 */
	if (!eel_istr_has_prefix (uri, "file:")) {
		g_string_append (result, uri);
		g_string_append (result, "\r\n");
	} else {
		local_path = gnome_vfs_get_local_path_from_uri (uri);

		/* Check for characters that confuse the old
		 * gnome_uri_list_extract_filenames implementation, and just leave
		 * out any paths with those in them.
		 */
		if (is_path_that_gnome_uri_list_extract_filenames_can_parse (local_path)) {
			g_string_append (result, "file:");
			g_string_append (result, local_path);
			g_string_append (result, "\r\n");
		}

		g_free (local_path);
	}
}

/* Common function for drag_data_get_callback calls.
 * Returns FALSE if it doesn't handle drag data */
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
		
	switch (info) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		result = g_string_new (NULL);
		(* each_selected_item_iterator) (add_one_gnome_icon, container_context, result);
		break;
		
	case NAUTILUS_ICON_DND_URI_LIST:
	case NAUTILUS_ICON_DND_TEXT:
		result = g_string_new (NULL);
		(* each_selected_item_iterator) (add_one_compatible_uri, container_context, result);
		break;
		
	case NAUTILUS_ICON_DND_URL:
		result = g_string_new (NULL);
		(* each_selected_item_iterator) (add_one_netscape_url, container_context, result);
		break;
		
	default:
		return FALSE;
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

typedef struct
{
	GMainLoop *loop;
	GdkDragAction chosen;
} DropActionMenuData;

static void
menu_deactivate_callback (GtkWidget *menu,
			  gpointer   data)
{
	DropActionMenuData *damd;
	
	damd = data;

	if (g_main_loop_is_running (damd->loop))
		g_main_loop_quit (damd->loop);
}

static void
drop_action_activated_callback (GtkWidget  *menu_item,
				gpointer    data)
{
	DropActionMenuData *damd;
	
	damd = data;

	damd->chosen = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item),
							   "action"));

	if (g_main_loop_is_running (damd->loop))
		g_main_loop_quit (damd->loop);
}

static void
append_drop_action_menu_item (GtkWidget          *menu,
			      const char         *text,
			      GdkDragAction       action,
			      gboolean            sensitive,
			      DropActionMenuData *damd)
{
	GtkWidget *menu_item;

	menu_item = gtk_menu_item_new_with_mnemonic (text);
	gtk_widget_set_sensitive (menu_item, sensitive);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	g_object_set_data (G_OBJECT (menu_item),
			   "action",
			   GINT_TO_POINTER (action));
	
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (drop_action_activated_callback),
			  damd);

	gtk_widget_show (menu_item);
}

/* Pops up a menu of actions to perform on dropped files */
GdkDragAction
nautilus_drag_drop_action_ask (GdkDragAction actions)
{
	GtkWidget *menu;
	GtkWidget *menu_item;
	DropActionMenuData damd;
	
	/* Create the menu and set the sensitivity of the items based on the
	 * allowed actions.
	 */
	menu = gtk_menu_new ();
	
	append_drop_action_menu_item (menu, _("_Move here"),
				      GDK_ACTION_MOVE,
				      (actions & GDK_ACTION_MOVE) != 0,
				      &damd);

	append_drop_action_menu_item (menu, _("_Copy here"),
				      GDK_ACTION_COPY,
				      (actions & GDK_ACTION_COPY) != 0,
				      &damd);
	
	append_drop_action_menu_item (menu, _("_Link here"),
				      GDK_ACTION_LINK,
				      (actions & GDK_ACTION_LINK) != 0,
				      &damd);

	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	menu_item = gtk_menu_item_new_with_mnemonic (_("Cancel"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	
	damd.chosen = 0;
	damd.loop = g_main_loop_new (NULL, FALSE);

	g_signal_connect (menu, "deactivate",
			  G_CALLBACK (menu_deactivate_callback),
			  &damd);
	
	gtk_grab_add (menu);
	
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			NULL, NULL, 0, GDK_CURRENT_TIME);
	
	g_main_loop_run (damd.loop);

	gtk_grab_remove (menu);
	
	g_main_loop_unref (damd.loop);
	
	gtk_object_sink (GTK_OBJECT (menu));
	
	return damd.chosen;
}

gboolean
nautilus_drag_autoscroll_in_scroll_region (GtkWidget *widget)
{
	float x_scroll_delta, y_scroll_delta;

	nautilus_drag_autoscroll_calculate_delta (widget, &x_scroll_delta, &y_scroll_delta);

	return x_scroll_delta != 0 || y_scroll_delta != 0;
}


void
nautilus_drag_autoscroll_calculate_delta (GtkWidget *widget, float *x_scroll_delta, float *y_scroll_delta)
{
	int x, y;

	g_assert (GTK_IS_WIDGET (widget));

	gdk_window_get_pointer (widget->window, &x, &y, NULL);

	/* Find out if we are anywhere close to the tree view edges
	 * to see if we need to autoscroll.
	 */
	*x_scroll_delta = 0;
	*y_scroll_delta = 0;
	
	if (x < AUTO_SCROLL_MARGIN) {
		*x_scroll_delta = (float)(x - AUTO_SCROLL_MARGIN);
	}

	if (x > widget->allocation.width - AUTO_SCROLL_MARGIN) {
		if (*x_scroll_delta != 0) {
			/* Already trying to scroll because of being too close to 
			 * the top edge -- must be the window is really short,
			 * don't autoscroll.
			 */
			return;
		}
		*x_scroll_delta = (float)(x - (widget->allocation.width - AUTO_SCROLL_MARGIN));
	}

	if (y < AUTO_SCROLL_MARGIN) {
		*y_scroll_delta = (float)(y - AUTO_SCROLL_MARGIN);
	}

	if (y > widget->allocation.height - AUTO_SCROLL_MARGIN) {
		if (*y_scroll_delta != 0) {
			/* Already trying to scroll because of being too close to 
			 * the top edge -- must be the window is really narrow,
			 * don't autoscroll.
			 */
			return;
		}
		*y_scroll_delta = (float)(y - (widget->allocation.height - AUTO_SCROLL_MARGIN));
	}

	if (*x_scroll_delta == 0 && *y_scroll_delta == 0) {
		/* no work */
		return;
	}

	/* Adjust the scroll delta to the proper acceleration values depending on how far
	 * into the sroll margins we are.
	 * FIXME bugzilla.eazel.com 2486:
	 * we could use an exponential acceleration factor here for better feel
	 */
	if (*x_scroll_delta != 0) {
		*x_scroll_delta /= AUTO_SCROLL_MARGIN;
		*x_scroll_delta *= (MAX_AUTOSCROLL_DELTA - MIN_AUTOSCROLL_DELTA);
		*x_scroll_delta += MIN_AUTOSCROLL_DELTA;
	}
	
	if (*y_scroll_delta != 0) {
		*y_scroll_delta /= AUTO_SCROLL_MARGIN;
		*y_scroll_delta *= (MAX_AUTOSCROLL_DELTA - MIN_AUTOSCROLL_DELTA);
		*y_scroll_delta += MIN_AUTOSCROLL_DELTA;
	}

}



void
nautilus_drag_autoscroll_start (NautilusDragInfo *drag_info,
				GtkWidget        *widget,
				GtkFunction       callback,
				gpointer          user_data)
{
	if (nautilus_drag_autoscroll_in_scroll_region (widget)) {
		if (drag_info->auto_scroll_timeout_id == 0) {
			drag_info->waiting_to_autoscroll = TRUE;
			drag_info->start_auto_scroll_in = eel_get_system_time() 
				+ AUTOSCROLL_INITIAL_DELAY;
			
			drag_info->auto_scroll_timeout_id = gtk_timeout_add
				(AUTOSCROLL_TIMEOUT_INTERVAL,
				 callback,
			 	 user_data);
		}
	} else {
		if (drag_info->auto_scroll_timeout_id != 0) {
			gtk_timeout_remove (drag_info->auto_scroll_timeout_id);
			drag_info->auto_scroll_timeout_id = 0;
		}
	}
}

void
nautilus_drag_autoscroll_stop (NautilusDragInfo *drag_info)
{
	if (drag_info->auto_scroll_timeout_id != 0) {
		gtk_timeout_remove (drag_info->auto_scroll_timeout_id);
		drag_info->auto_scroll_timeout_id = 0;
	}
}
