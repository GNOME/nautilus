/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-navigation-window-slot.c: Nautilus navigation window slot
 
   Copyright (C) 2008 Free Software Foundation, Inc.
  
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
  
   Author: Christian Neumair <cneumair@gnome.org>
*/

#include "nautilus-window-slot.h"
#include "nautilus-navigation-window-slot.h"
#include "nautilus-window-private.h"
#include "nautilus-search-bar.h"
#include <libnautilus-private/nautilus-window-slot-info.h>
#include <libnautilus-private/nautilus-file.h>
#include <eel/eel-gtk-macros.h>

static void nautilus_navigation_window_slot_init       (NautilusNavigationWindowSlot *slot);
static void nautilus_navigation_window_slot_class_init (NautilusNavigationWindowSlotClass *class);

G_DEFINE_TYPE (NautilusNavigationWindowSlot, nautilus_navigation_window_slot, NAUTILUS_TYPE_WINDOW_SLOT)
#define parent_class nautilus_navigation_window_slot_parent_class

gboolean
nautilus_navigation_window_slot_should_close_with_mount (NautilusNavigationWindowSlot *slot,
							 GMount *mount)
{
	NautilusBookmark *bookmark;
	GFile *mount_location, *bookmark_location;
	GList *l;
	gboolean close_with_mount;

	mount_location = g_mount_get_root (mount);

	close_with_mount = TRUE;

	for (l = slot->back_list; l != NULL; l = l->next) {
		bookmark = NAUTILUS_BOOKMARK (l->data);

		bookmark_location = nautilus_bookmark_get_location (bookmark);
		close_with_mount &= g_file_has_prefix (bookmark_location, mount_location);
		g_object_unref (bookmark_location);

		if (!close_with_mount) {
			break;
		}
	}

	close_with_mount &= g_file_has_prefix (NAUTILUS_WINDOW_SLOT (slot)->location, mount_location);

	/* we could also consider the forward list here, but since the “go home” request
	 * in nautilus-window-manager-views.c:mount_removed_callback() would discard those
	 * anyway, we don't consider them.
	 */

	g_object_unref (mount_location);

	return close_with_mount;
}

void
nautilus_navigation_window_slot_clear_forward_list (NautilusNavigationWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW_SLOT (slot));

	eel_g_object_list_free (slot->forward_list);
	slot->forward_list = NULL;
}

void
nautilus_navigation_window_slot_clear_back_list (NautilusNavigationWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW_SLOT (slot));

	eel_g_object_list_free (slot->back_list);
	slot->back_list = NULL;
}

static void
query_editor_changed_callback (NautilusSearchBar *bar,
			       NautilusQuery *query,
			       gboolean reload,
			       NautilusWindowSlot *slot)
{
	NautilusDirectory *directory;

	g_assert (NAUTILUS_IS_FILE (slot->viewed_file));

	directory = nautilus_directory_get_for_file (slot->viewed_file);
	g_assert (NAUTILUS_IS_SEARCH_DIRECTORY (directory));

	nautilus_search_directory_set_query (NAUTILUS_SEARCH_DIRECTORY (directory),
					     query);
	if (reload) {
		nautilus_window_slot_reload (slot);
	}

	nautilus_directory_unref (directory);
}


static void
nautilus_navigation_window_slot_update_query_editor (NautilusWindowSlot *slot)
{
	NautilusDirectory *directory;
	NautilusSearchDirectory *search_directory;
	NautilusQuery *query;
	NautilusNavigationWindow *navigation_window;
	GtkWidget *query_editor;

	g_assert (slot->window != NULL);
	navigation_window = NAUTILUS_NAVIGATION_WINDOW (slot->window);

	query_editor = NULL;

	directory = nautilus_directory_get (slot->location);
	if (NAUTILUS_IS_SEARCH_DIRECTORY (directory)) {
		search_directory = NAUTILUS_SEARCH_DIRECTORY (directory);

		if (nautilus_search_directory_is_saved_search (search_directory)) {
			query_editor = nautilus_query_editor_new (TRUE,
								  nautilus_search_directory_is_indexed (search_directory));
		} else {
			query_editor = nautilus_query_editor_new_with_bar (FALSE,
									   nautilus_search_directory_is_indexed (search_directory),
									   slot->window->details->active_slot == slot,
									   NAUTILUS_SEARCH_BAR (navigation_window->search_bar),
									   slot);
		}
	}

	slot->query_editor = NAUTILUS_QUERY_EDITOR (query_editor);

	if (query_editor != NULL) {
		g_signal_connect_object (query_editor, "changed",
					 G_CALLBACK (query_editor_changed_callback), slot, 0);
		
		query = nautilus_search_directory_get_query (search_directory);
		if (query != NULL) {
			nautilus_query_editor_set_query (NAUTILUS_QUERY_EDITOR (query_editor),
							 query);
			g_object_unref (query);
		} else {
			nautilus_query_editor_set_default_query (NAUTILUS_QUERY_EDITOR (query_editor));
		}

		nautilus_window_slot_add_extra_location_widget (slot, query_editor);
		gtk_widget_show (query_editor);
		nautilus_query_editor_grab_focus (NAUTILUS_QUERY_EDITOR (query_editor));
	}

	nautilus_directory_unref (directory);
}

static void
nautilus_navigation_window_slot_active (NautilusWindowSlot *slot)
{
	NautilusNavigationWindow *window;
	NautilusNavigationWindowSlot *navigation_slot;
	int page_num;

	navigation_slot = NAUTILUS_NAVIGATION_WINDOW_SLOT (slot);
	window = NAUTILUS_NAVIGATION_WINDOW (slot->window);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook),
					  slot->content_box);
	g_assert (page_num >= 0);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page_num);

	EEL_CALL_PARENT (NAUTILUS_WINDOW_SLOT_CLASS, active, (slot));

	if (slot->viewed_file != NULL) {
		nautilus_navigation_window_load_extension_toolbar_items (window);
	}
}
 
static void
nautilus_navigation_window_slot_dispose (GObject *object)
{
	NautilusNavigationWindowSlot *slot;

	slot = NAUTILUS_NAVIGATION_WINDOW_SLOT (object);

	nautilus_navigation_window_slot_clear_forward_list (slot);
	nautilus_navigation_window_slot_clear_back_list (slot);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
nautilus_navigation_window_slot_init (NautilusNavigationWindowSlot *slot)
{
}

static void
nautilus_navigation_window_slot_class_init (NautilusNavigationWindowSlotClass *class)
{
	NAUTILUS_WINDOW_SLOT_CLASS (class)->active = nautilus_navigation_window_slot_active; 
	NAUTILUS_WINDOW_SLOT_CLASS (class)->update_query_editor = nautilus_navigation_window_slot_update_query_editor; 

	G_OBJECT_CLASS (class)->dispose = nautilus_navigation_window_slot_dispose;
}

