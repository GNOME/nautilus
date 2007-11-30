/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2005 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: John Sullivan <sullivan@eazel.com>
 *         Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <locale.h> 

#include "nautilus-actions.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-window-bookmarks.h"
#include "nautilus-window-private.h"
#include <libnautilus-private/nautilus-undo-manager.h>
#include <eel/eel-debug.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gtk-extensions.h>

#define MENU_ITEM_MAX_WIDTH_CHARS 32

static GtkWindow *bookmarks_window = NULL;
static NautilusBookmarkList *bookmarks = NULL;

static void schedule_refresh_bookmarks_menu (NautilusWindow *window);

static void
free_bookmark_list (void)
{
	g_object_unref (bookmarks);
}

NautilusBookmarkList *
nautilus_get_bookmark_list (void)
{
        if (bookmarks == NULL) {
                bookmarks = nautilus_bookmark_list_new ();
                eel_debug_call_at_shutdown (free_bookmark_list);
        }
	
        return bookmarks;
}


static void
remove_bookmarks_for_uri_if_yes (GtkDialog *dialog, int response, gpointer callback_data)
{
	const char *uri;

	g_assert (GTK_IS_DIALOG (dialog));
	g_assert (callback_data != NULL);

	if (response == GTK_RESPONSE_YES) {
		uri = callback_data;
		nautilus_bookmark_list_delete_items_with_uri (nautilus_get_bookmark_list (), uri);
	}

	gtk_object_destroy (GTK_OBJECT (dialog));
}

static void
show_bogus_bookmark_window (NautilusWindow *window,
			    NautilusBookmark *bookmark)
{
	GtkDialog *dialog;
	GFile *location;
	char *uri_for_display;
	char *prompt;
	char *detail;

	location = nautilus_bookmark_get_location (bookmark);
	uri_for_display = g_file_get_parse_name (location);
	
	prompt = _("Do you want to remove any bookmarks with the "
		   "non-existing location from your list?");
	detail = g_strdup_printf (_("The location \"%s\" does not exist."), uri_for_display);
	
	dialog = eel_show_yes_no_dialog (prompt, detail,
					 _("Bookmark for Nonexistent Location"),
					 GTK_STOCK_CANCEL,
					 GTK_WINDOW (window));

	g_signal_connect_data (dialog,
			       "response",
			       G_CALLBACK (remove_bookmarks_for_uri_if_yes),
			       g_file_get_uri (location),
			       (GClosureNotify)g_free,
			       0);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_NO);

	g_object_unref (location);
	g_free (uri_for_display);
	g_free (detail);
}

static GtkWindow *
get_or_create_bookmarks_window (GObject *undo_manager_source)
{
	if (bookmarks_window == NULL) {
		bookmarks_window = create_bookmarks_window (nautilus_get_bookmark_list(), undo_manager_source);
	} else {
		edit_bookmarks_dialog_set_signals (undo_manager_source);
	}

	return bookmarks_window;
}

/**
 * nautilus_bookmarks_exiting:
 * 
 * Last chance to save state before app exits.
 * Called when application exits; don't call from anywhere else.
 **/
void
nautilus_bookmarks_exiting (void)
{
	if (bookmarks_window != NULL) {
		nautilus_bookmarks_window_save_geometry (bookmarks_window);
		gtk_widget_destroy (GTK_WIDGET (bookmarks_window));
	}
}

/**
 * add_bookmark_for_current_location
 * 
 * Add a bookmark for the displayed location to the bookmarks menu.
 * Does nothing if there's already a bookmark for the displayed location.
 */
void
nautilus_window_add_bookmark_for_current_location (NautilusWindow *window)
{
	NautilusBookmark *bookmark;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	bookmark = window->current_location_bookmark;

	if (!nautilus_bookmark_list_contains (nautilus_get_bookmark_list (), bookmark)) {
		nautilus_bookmark_list_append (nautilus_get_bookmark_list (), bookmark); 
	}
}

void
nautilus_window_edit_bookmarks (NautilusWindow *window)
{
	GtkWindow *dialog;

	dialog = get_or_create_bookmarks_window (G_OBJECT (window));

	gtk_window_set_screen (
		dialog, gtk_window_get_screen (GTK_WINDOW (window)));
        gtk_window_present (dialog);
}

void
nautilus_window_remove_bookmarks_menu_callback (NautilusWindow *window)
{
        if (window->details->refresh_bookmarks_menu_idle_id != 0) {
                g_source_remove (window->details->refresh_bookmarks_menu_idle_id);
		window->details->refresh_bookmarks_menu_idle_id = 0;
        }
}

static void
remove_bookmarks_menu_items (NautilusWindow *window)
{
	GtkUIManager *ui_manager;
	
	ui_manager = nautilus_window_get_ui_manager (window);
	if (window->details->bookmarks_merge_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager,
					  window->details->bookmarks_merge_id);
		window->details->bookmarks_merge_id = 0;
	}
	if (window->details->bookmarks_action_group != NULL) {
		gtk_ui_manager_remove_action_group (ui_manager,
						    window->details->bookmarks_action_group);
		window->details->bookmarks_action_group = NULL;
	}
}

static void
connect_proxy_cb (GtkActionGroup *action_group,
                  GtkAction *action,
                  GtkWidget *proxy,
                  gpointer dummy)
{
	GtkLabel *label;

	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	label = GTK_LABEL (GTK_BIN (proxy)->child);

	gtk_label_set_use_underline (label, FALSE);
	gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (label, MENU_ITEM_MAX_WIDTH_CHARS);
}

static void
update_bookmarks (NautilusWindow *window)
{
        NautilusBookmarkList *bookmarks;
	NautilusBookmark *bookmark;
	guint bookmark_count;
	guint index;
	GtkUIManager *ui_manager;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (window->details->bookmarks_merge_id == 0);
	g_assert (window->details->bookmarks_action_group == NULL);

	bookmarks = nautilus_get_bookmark_list ();

	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));
	
	window->details->bookmarks_merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	window->details->bookmarks_action_group = gtk_action_group_new ("BookmarksGroup");
	g_signal_connect (window->details->bookmarks_action_group, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), NULL);

	gtk_ui_manager_insert_action_group (ui_manager,
					    window->details->bookmarks_action_group,
					    -1);
	g_object_unref (window->details->bookmarks_action_group);

	/* append new set of bookmarks */
	bookmark_count = nautilus_bookmark_list_length (bookmarks);
	for (index = 0; index < bookmark_count; ++index) {
		bookmark = nautilus_bookmark_list_item_at (bookmarks, index);

		if (nautilus_bookmark_uri_known_not_to_exist (bookmark)) {
			continue;
		}

		nautilus_menus_append_bookmark_to_menu
			(NAUTILUS_WINDOW (window),
			 bookmark,
			 NAUTILUS_WINDOW_GET_CLASS (window)->bookmarks_placeholder,
			 "dynamic",
			 index,
			 window->details->bookmarks_action_group,
			 window->details->bookmarks_merge_id,
			 G_CALLBACK (schedule_refresh_bookmarks_menu), 
			 show_bogus_bookmark_window);
	}
}

static void
refresh_bookmarks_menu (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Unregister any pending call to this function. */
	nautilus_window_remove_bookmarks_menu_callback (window);

	remove_bookmarks_menu_items (window);
	update_bookmarks (window);
}


static gboolean
refresh_bookmarks_menu_idle_callback (gpointer data)
{
	g_assert (NAUTILUS_IS_WINDOW (data));

	refresh_bookmarks_menu (NAUTILUS_WINDOW (data));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
schedule_refresh_bookmarks_menu (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	if (window->details->refresh_bookmarks_menu_idle_id == 0) {
                window->details->refresh_bookmarks_menu_idle_id
                        = g_idle_add (refresh_bookmarks_menu_idle_callback,
				      window);
	}	
}

/**
 * nautilus_window_initialize_bookmarks_menu
 * 
 * Fill in bookmarks menu with stored bookmarks, and wire up signals
 * so we'll be notified when bookmark list changes.
 */
void 
nautilus_window_initialize_bookmarks_menu (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Construct the initial set of bookmarks. */
	/* We do this in an idle, since this is called in the constructor
	 * where we don't know the real type of the window */
	schedule_refresh_bookmarks_menu (window);

	/* Recreate dynamic part of menu if bookmark list changes */
	g_signal_connect_object (nautilus_get_bookmark_list (), "contents_changed",
				 G_CALLBACK (schedule_refresh_bookmarks_menu),
				 window, G_CONNECT_SWAPPED);
}
