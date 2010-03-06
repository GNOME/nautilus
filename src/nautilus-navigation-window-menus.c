/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
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
 */

/* nautilus-window-menus.h - implementation of nautilus window menu operations,
 *                           split into separate file just for convenience.
 */
#include <config.h>

#include <locale.h> 

#include "nautilus-actions.h"
#include "nautilus-notebook.h"
#include "nautilus-navigation-action.h"
#include "nautilus-zoom-action.h"
#include "nautilus-view-as-action.h"
#include "nautilus-application.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-file-management-properties.h"
#include "nautilus-property-browser.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include "nautilus-window-bookmarks.h"
#include "nautilus-navigation-window-pane.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-undo-manager.h>
#include <libnautilus-private/nautilus-search-engine.h>
#include <libnautilus-private/nautilus-signaller.h>

#define MENU_PATH_HISTORY_PLACEHOLDER			"/MenuBar/Other Menus/Go/History Placeholder"

#define RESPONSE_FORGET		1000
#define MENU_ITEM_MAX_WIDTH_CHARS 32

static void                  schedule_refresh_go_menu                      (NautilusNavigationWindow   *window);

static void
action_close_all_windows_callback (GtkAction *action, 
				   gpointer user_data)
{
	nautilus_application_close_all_navigation_windows ();
}

static gboolean
should_open_in_new_tab (void)
{
	/* FIXME this is duplicated */
	GdkEvent *event;

	event = gtk_get_current_event ();

	if (event == NULL) {
		return FALSE;
	}

	if (event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) {
		return event->button.button == 2;
	}

	gdk_event_free (event);

	return FALSE;
}

static void
action_back_callback (GtkAction *action, 
		      gpointer user_data) 
{
	nautilus_navigation_window_back_or_forward (NAUTILUS_NAVIGATION_WINDOW (user_data), 
						    TRUE, 0, should_open_in_new_tab ());
}

static void
action_forward_callback (GtkAction *action, 
			 gpointer user_data) 
{
	nautilus_navigation_window_back_or_forward (NAUTILUS_NAVIGATION_WINDOW (user_data), 
			                            FALSE, 0, should_open_in_new_tab ());
}

static void
forget_history_if_yes (GtkDialog *dialog, int response, gpointer callback_data)
{
	if (response == RESPONSE_FORGET) {
		nautilus_forget_history ();
	}
	gtk_object_destroy (GTK_OBJECT (dialog));
}

static void
forget_history_if_confirmed (NautilusWindow *window)
{
	GtkDialog *dialog;

	dialog = eel_create_question_dialog (_("Are you sure you want to clear the list "
					       "of locations you have visited?"),
					     NULL,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					     GTK_STOCK_CLEAR, RESPONSE_FORGET,
					     GTK_WINDOW (window));

	gtk_widget_show (GTK_WIDGET (dialog));
	
	g_signal_connect (dialog, "response",
			  G_CALLBACK (forget_history_if_yes), NULL);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_CANCEL);
}

static void
action_clear_history_callback (GtkAction *action, 
			       gpointer user_data) 
{
	forget_history_if_confirmed (NAUTILUS_WINDOW (user_data));
}

static void
action_split_view_switch_next_pane_callback(GtkAction *action,
					    gpointer user_data)
{
	nautilus_window_pane_switch_to (nautilus_window_get_next_pane (NAUTILUS_WINDOW (user_data)));
}

static void
action_split_view_same_location_callback (GtkAction *action,
					  gpointer user_data)
{
	NautilusWindow *window;
	NautilusWindowPane *next_pane;
	GFile *location;

	window = NAUTILUS_WINDOW (user_data);
	next_pane = nautilus_window_get_next_pane (window);

	if (!next_pane) {
		return;
	}
	location = nautilus_window_slot_get_location (next_pane->active_slot);
	if (location) {
		nautilus_window_slot_go_to (window->details->active_pane->active_slot, location, FALSE);
		g_object_unref (location);
	}
}

static void
action_show_hide_toolbar_callback (GtkAction *action, 
				   gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		nautilus_navigation_window_show_toolbar (window);
	} else {
		nautilus_navigation_window_hide_toolbar (window);
	}
}



static void
action_show_hide_sidebar_callback (GtkAction *action, 
				   gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		nautilus_navigation_window_show_sidebar (window);
	} else {
		nautilus_navigation_window_hide_sidebar (window);
	}
}

static void
pane_show_hide_location_bar (NautilusNavigationWindowPane *pane, gboolean is_active)
{
	if (nautilus_navigation_window_pane_location_bar_showing (pane) != is_active) {
		if (is_active) {
			nautilus_navigation_window_pane_show_location_bar (pane, TRUE);
		} else {
			nautilus_navigation_window_pane_hide_location_bar (pane, TRUE);
		}
	}
}

static void
action_show_hide_location_bar_callback (GtkAction *action, 
					gpointer user_data)
{
	NautilusWindow *window;
	GList *walk;
	gboolean is_active;

	window = NAUTILUS_WINDOW (user_data);

	is_active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	/* Do the active pane first, because this will trigger an update of the menu items,
	 * which in turn relies on the active pane. */
	pane_show_hide_location_bar (NAUTILUS_NAVIGATION_WINDOW_PANE (window->details->active_pane), is_active);

	for (walk = window->details->panes; walk; walk = walk->next) {
		pane_show_hide_location_bar (NAUTILUS_NAVIGATION_WINDOW_PANE (walk->data), is_active);
	}
}

static void
action_show_hide_statusbar_callback (GtkAction *action,
				     gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		nautilus_navigation_window_show_status_bar (window);
	} else {
		nautilus_navigation_window_hide_status_bar (window);
	}
}

static void
action_split_view_callback (GtkAction *action,
			    gpointer user_data)
{
	NautilusNavigationWindow *window;
	gboolean is_active;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	is_active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	if (is_active != nautilus_navigation_window_split_view_showing (window)) {
		NautilusWindow *nautilus_window;

		if (is_active) {
			nautilus_navigation_window_split_view_on (window);
		} else {
			nautilus_navigation_window_split_view_off (window);
		}
		nautilus_window = NAUTILUS_WINDOW (window);
		if (nautilus_window->details->active_pane && nautilus_window->details->active_pane->active_slot) {
			nautilus_view_update_menus (nautilus_window->details->active_pane->active_slot->content_view);
		}
	}
}

void
nautilus_navigation_window_update_show_hide_menu_items (NautilusNavigationWindow *window) 
{
	GtkAction *action;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_SHOW_HIDE_TOOLBAR);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nautilus_navigation_window_toolbar_showing (window));

	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_SHOW_HIDE_SIDEBAR);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nautilus_navigation_window_sidebar_showing (window));
	
	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_SHOW_HIDE_LOCATION_BAR);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nautilus_navigation_window_pane_location_bar_showing (NAUTILUS_NAVIGATION_WINDOW_PANE (NAUTILUS_WINDOW (window)->details->active_pane)));

	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_SHOW_HIDE_STATUSBAR);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nautilus_navigation_window_status_bar_showing (window));

	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_SHOW_HIDE_EXTRA_PANE);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nautilus_navigation_window_split_view_showing (window));
}

void
nautilus_navigation_window_update_spatial_menu_item (NautilusNavigationWindow *window) 
{
	GtkAction *action;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_FOLDER_WINDOW);
	gtk_action_set_visible (action,
				!eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER));
}

static void
action_add_bookmark_callback (GtkAction *action,
			      gpointer user_data)
{
        nautilus_window_add_bookmark_for_current_location (NAUTILUS_WINDOW (user_data));
}

static void
action_edit_bookmarks_callback (GtkAction *action, 
				gpointer user_data)
{
        nautilus_window_edit_bookmarks (NAUTILUS_WINDOW (user_data));
}

void
nautilus_navigation_window_remove_go_menu_callback (NautilusNavigationWindow *window)
{
        if (window->details->refresh_go_menu_idle_id != 0) {
                g_source_remove (window->details->refresh_go_menu_idle_id);
		window->details->refresh_go_menu_idle_id = 0;
        }
}

void
nautilus_navigation_window_remove_go_menu_items (NautilusNavigationWindow *window)
{
	GtkUIManager *ui_manager;
	
	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));
	if (window->details->go_menu_merge_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager,
					  window->details->go_menu_merge_id);
		window->details->go_menu_merge_id = 0;
	}
	if (window->details->go_menu_action_group != NULL) {
		gtk_ui_manager_remove_action_group (ui_manager,
						    window->details->go_menu_action_group);
		window->details->go_menu_action_group = NULL;
	}
}

static void
show_bogus_history_window (NautilusWindow *window,
			   NautilusBookmark *bookmark)
{
	GFile *file;
	char *uri_for_display;
	char *detail;

	file = nautilus_bookmark_get_location (bookmark);
	uri_for_display = g_file_get_parse_name (file);
	
	detail = g_strdup_printf (_("The location \"%s\" does not exist."), uri_for_display);

	eel_show_warning_dialog (_("The history location doesn't exist."),
				 detail,
				 GTK_WINDOW (window));

	g_object_unref (file);
	g_free (uri_for_display);
	g_free (detail);
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

	label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (proxy)));

	gtk_label_set_use_underline (label, FALSE);
	gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (label, MENU_ITEM_MAX_WIDTH_CHARS);
}

static const char* icon_entries[] = {
	"/MenuBar/Other Menus/Go/Home",
	"/MenuBar/Other Menus/Go/Computer",
	"/MenuBar/Other Menus/Go/Go to Templates",
	"/MenuBar/Other Menus/Go/Go to Trash",
	"/MenuBar/Other Menus/Go/Go to Network",
	"/MenuBar/Other Menus/Go/Go to Location"
};

/**
 * refresh_go_menu:
 * 
 * Refresh list of bookmarks at end of Go menu to match centralized history list.
 * @window: The NautilusWindow whose Go menu will be refreshed.
 **/
static void
refresh_go_menu (NautilusNavigationWindow *window)
{
	GtkUIManager *ui_manager;
	GList *node;
	GtkWidget *menuitem;
	int index;
	int i;
	
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	/* Unregister any pending call to this function. */
	nautilus_navigation_window_remove_go_menu_callback (window);

	/* Remove old set of history items. */
	nautilus_navigation_window_remove_go_menu_items (window);

	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));

	window->details->go_menu_merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	window->details->go_menu_action_group = gtk_action_group_new ("GoMenuGroup");
	g_signal_connect (window->details->go_menu_action_group, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), NULL);

	gtk_ui_manager_insert_action_group (ui_manager,
					    window->details->go_menu_action_group,
					    -1);
	g_object_unref (window->details->go_menu_action_group);

	for (i = 0; i < G_N_ELEMENTS (icon_entries); i++) {
		menuitem = gtk_ui_manager_get_widget (
				ui_manager,
				icon_entries[i]);

		gtk_image_menu_item_set_always_show_image (
				GTK_IMAGE_MENU_ITEM (menuitem), TRUE);
	}
	
	/* Add in a new set of history items. */
	for (node = nautilus_get_history_list (), index = 0;
	     node != NULL && index < 10;
	     node = node->next, index++) {
		nautilus_menus_append_bookmark_to_menu 
			(NAUTILUS_WINDOW (window),
			 NAUTILUS_BOOKMARK (node->data),
			 MENU_PATH_HISTORY_PLACEHOLDER,
			 "history",
			 index,
			 window->details->go_menu_action_group,
			 window->details->go_menu_merge_id,
			 G_CALLBACK (schedule_refresh_go_menu),
			 show_bogus_history_window);
	}
}

static gboolean
refresh_go_menu_idle_callback (gpointer data)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (data));

	refresh_go_menu (NAUTILUS_NAVIGATION_WINDOW (data));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
schedule_refresh_go_menu (NautilusNavigationWindow *window)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	if (window->details->refresh_go_menu_idle_id == 0) {
                window->details->refresh_go_menu_idle_id
                        = g_idle_add (refresh_go_menu_idle_callback,
				      window);
	}	
}

/**
 * nautilus_navigation_window_initialize_go_menu
 * 
 * Wire up signals so we'll be notified when history list changes.
 */
static void 
nautilus_navigation_window_initialize_go_menu (NautilusNavigationWindow *window)
{
	/* Recreate bookmarks part of menu if history list changes
	 */
	g_signal_connect_object (nautilus_signaller_get_current (), "history_list_changed",
				 G_CALLBACK (schedule_refresh_go_menu), window, G_CONNECT_SWAPPED);
}

void
nautilus_navigation_window_update_split_view_actions_sensitivity (NautilusNavigationWindow *window)
{
	NautilusWindow *win;
	GtkActionGroup *action_group;
	GtkAction *action;
	gboolean have_multiple_panes;
	gboolean next_pane_is_in_same_location;
	GFile *active_pane_location;
	GFile *next_pane_location;
	NautilusWindowPane *next_pane;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	action_group = window->details->navigation_action_group;
	win = NAUTILUS_WINDOW (window);

	/* collect information */
	have_multiple_panes = (win->details->panes && win->details->panes->next);
	if (win->details->active_pane->active_slot) {
		active_pane_location = nautilus_window_slot_get_location (win->details->active_pane->active_slot);
	}
	else {
		active_pane_location = NULL;
	}
	next_pane = nautilus_window_get_next_pane (win);
	if (next_pane && next_pane->active_slot) {
		next_pane_location = nautilus_window_slot_get_location (next_pane->active_slot);
		next_pane_is_in_same_location = (active_pane_location && next_pane_location &&
						 g_file_equal (active_pane_location, next_pane_location));
	}
	else {
		next_pane_location = NULL;
		next_pane_is_in_same_location = FALSE;
	}

	/* switch to next pane */
	action = gtk_action_group_get_action (action_group, "SplitViewNextPane");
	gtk_action_set_sensitive (action, have_multiple_panes);

	/* same location */
	action = gtk_action_group_get_action (action_group, "SplitViewSameLocation");
	gtk_action_set_sensitive (action, have_multiple_panes && !next_pane_is_in_same_location);

	/* clean up */
	if (active_pane_location) {
		g_object_unref (active_pane_location);
	}
	if (next_pane_location) {
		g_object_unref (next_pane_location);
	}
}

static void
action_new_window_callback (GtkAction *action,
			    gpointer user_data)
{
	NautilusWindow *current_window;
	NautilusWindow *new_window;

	current_window = NAUTILUS_WINDOW (user_data);
	new_window = nautilus_application_create_navigation_window (
				current_window->application,
				NULL,
				gtk_window_get_screen (GTK_WINDOW (current_window)));
	nautilus_window_go_home (new_window);
}

static void
action_new_tab_callback (GtkAction *action,
			 gpointer user_data)
{
	NautilusWindow *window;
	NautilusWindowSlot *current_slot;
	NautilusWindowSlot *new_slot;
	NautilusWindowOpenFlags flags;
	GFile *location;
	int new_slot_position;
	char *scheme;

	window = NAUTILUS_WINDOW (user_data);
	current_slot = window->details->active_pane->active_slot;
	location = nautilus_window_slot_get_location (current_slot);

	if (location != NULL) {
		flags = 0;

		new_slot_position = eel_preferences_get_enum (NAUTILUS_PREFERENCES_NEW_TAB_POSITION);
		if (new_slot_position == NAUTILUS_NEW_TAB_POSITION_END) {
			flags = NAUTILUS_WINDOW_OPEN_SLOT_APPEND;
		}

		scheme = g_file_get_uri_scheme (location);
		if (!strcmp (scheme, "x-nautilus-search")) {
			g_object_unref (location);
			location = g_file_new_for_path (g_get_home_dir ());
		}
		g_free (scheme);

		new_slot = nautilus_window_open_slot (current_slot->pane, flags);
		nautilus_window_set_active_slot (window, new_slot);
		nautilus_window_slot_go_to (new_slot, location, FALSE);
		g_object_unref (location);
	}
}

static void
action_folder_window_callback (GtkAction *action,
			       gpointer user_data)
{
	NautilusWindow *current_window;
	NautilusWindowSlot *slot;
	GFile *current_location;

	current_window = NAUTILUS_WINDOW (user_data);
	slot = current_window->details->active_pane->active_slot;
	current_location = nautilus_window_slot_get_location (slot);
	nautilus_application_present_spatial_window (
			current_window->application,
			current_window,
			NULL,
			current_location,
			gtk_window_get_screen (GTK_WINDOW (current_window)));
	if (current_location != NULL) {
		g_object_unref (current_location);
	}
}

static void
action_go_to_location_callback (GtkAction *action,
				gpointer user_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);

	nautilus_window_prompt_for_location (window, NULL);
}

/* The ctrl-f Keyboard shortcut always enables, rather than toggles
   the search mode */
static void
action_show_search_callback (GtkAction *action,
			     gpointer user_data)
{
	GtkAction *search_action;
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	search_action =
		gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_SEARCH);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (search_action))) {
		/* Already visible, just show it */
		nautilus_navigation_window_show_search (window);
	} else {
		/* Otherwise, enable */
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (search_action),
					      TRUE);
	}
}

static void
action_show_hide_search_callback (GtkAction *action,
				  gpointer user_data)
{
	NautilusNavigationWindow *window;

	/* This is used when toggling the action for updating the UI
	   state only, not actually activating the action */
	if (g_object_get_data (G_OBJECT (action), "blocked") != NULL) {
		return;
	}

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		nautilus_navigation_window_show_search (window);
	} else {
		NautilusWindowSlot *slot;
		GFile *location = NULL;

		slot = NAUTILUS_WINDOW (window)->details->active_pane->active_slot;

		/* Use the location bar as the return location */
		if (slot->query_editor == NULL){
			location = nautilus_window_slot_get_location (slot);
		/* Use the search location as the return location */
		} else {
			NautilusQuery *query;
			char *uri;

			query = nautilus_query_editor_get_query (slot->query_editor);
			if (query != NULL) {
				uri = nautilus_query_get_location (query);
				if (uri != NULL) {
					location = g_file_new_for_uri (uri);
					g_free (uri);
				}
				g_object_unref (query);
			}
		}

		/* Last try: use the home directory as the return location */
		if (location == NULL) {
			location = g_file_new_for_path (g_get_home_dir ());
		}

		nautilus_window_go_to (NAUTILUS_WINDOW (window), location);
		g_object_unref (location);

		nautilus_navigation_window_hide_search (window);
	}
}

static void
action_tabs_previous_callback (GtkAction *action,
			       gpointer user_data)
{
	NautilusNavigationWindowPane *pane;

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (NAUTILUS_WINDOW (user_data)->details->active_pane);
	nautilus_notebook_set_current_page_relative (NAUTILUS_NOTEBOOK (pane->notebook), -1);
}

static void
action_tabs_next_callback (GtkAction *action,
			   gpointer user_data)
{
	NautilusNavigationWindowPane *pane;

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (NAUTILUS_WINDOW (user_data)->details->active_pane);
	nautilus_notebook_set_current_page_relative (NAUTILUS_NOTEBOOK (pane->notebook), 1);
}

static void
action_tabs_move_left_callback (GtkAction *action,
				gpointer user_data)
{
	NautilusNavigationWindowPane *pane;

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (NAUTILUS_WINDOW (user_data)->details->active_pane);
	nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (pane->notebook), -1);
}

static void
action_tabs_move_right_callback (GtkAction *action,
				 gpointer user_data)
{
	NautilusNavigationWindowPane *pane;

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (NAUTILUS_WINDOW (user_data)->details->active_pane);
	nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (pane->notebook), 1);
}

static void
action_tab_change_action_activate_callback (GtkAction *action, gpointer user_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);
	if (window && window->details->active_pane) {
		GtkNotebook *notebook;
		notebook = GTK_NOTEBOOK (NAUTILUS_NAVIGATION_WINDOW_PANE (window->details->active_pane)->notebook);
		if (notebook) {
			int num;
			num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action), "num"));
			if (num < gtk_notebook_get_n_pages (notebook)) {
				gtk_notebook_set_current_page (notebook, num);
			}
		}
	}
}

static const GtkActionEntry navigation_entries[] = {
  /* name, stock id, label */  { "Go", NULL, N_("_Go") },
  /* name, stock id, label */  { "Bookmarks", NULL, N_("_Bookmarks") },
  /* name, stock id, label */  { "Tabs", NULL, N_("_Tabs") },
  /* name, stock id, label */  { "New Window", "window-new", N_("New _Window"),
                                 "<control>N", N_("Open another Nautilus window for the displayed location"),
                                 G_CALLBACK (action_new_window_callback) },
  /* name, stock id, label */  { "New Tab", "tab-new", N_("New _Tab"),
                                 "<control>T", N_("Open another tab for the displayed location"),
                                 G_CALLBACK (action_new_tab_callback) },
  /* name, stock id, label */  { "Folder Window", "folder", N_("Open Folder W_indow"),
                                 NULL, N_("Open a folder window for the displayed location"),
                                 G_CALLBACK (action_folder_window_callback) },
  /* name, stock id, label */  { "Close All Windows", NULL, N_("Close _All Windows"),
                                 "<control>Q", N_("Close all Navigation windows"),
                                 G_CALLBACK (action_close_all_windows_callback) },
  /* name, stock id, label */  { "Go to Location", NULL, N_("_Location..."),
                                 "<control>L", N_("Specify a location to open"),
                                 G_CALLBACK (action_go_to_location_callback) },
  /* name, stock id, label */  { "Clear History", NULL, N_("Clea_r History"),
                                 NULL, N_("Clear contents of Go menu and Back/Forward lists"),
                                 G_CALLBACK (action_clear_history_callback) },
  /* name, stock id, label */  { "SplitViewNextPane", NULL, N_("S_witch to Other Pane"),
				 "F6", N_("Move focus to the other pane in a split view window"),
				 G_CALLBACK (action_split_view_switch_next_pane_callback) },
  /* name, stock id, label */  { "SplitViewSameLocation", NULL, N_("Sa_me Location as Other Pane"),
				 NULL, N_("Go to the same location as in the extra pane"),
				 G_CALLBACK (action_split_view_same_location_callback) },
  /* name, stock id, label */  { "Add Bookmark", GTK_STOCK_ADD, N_("_Add Bookmark"),
                                 "<control>d", N_("Add a bookmark for the current location to this menu"),
                                 G_CALLBACK (action_add_bookmark_callback) },
  /* name, stock id, label */  { "Edit Bookmarks", NULL, N_("_Edit Bookmarks..."),
                                 "<control>b", N_("Display a window that allows editing the bookmarks in this menu"),
                                 G_CALLBACK (action_edit_bookmarks_callback) },
	{ "TabsPrevious", NULL, N_("_Previous Tab"), "<control>Page_Up",
	  N_("Activate previous tab"),
	  G_CALLBACK (action_tabs_previous_callback) },
	{ "TabsNext", NULL, N_("_Next Tab"), "<control>Page_Down",
	  N_("Activate next tab"),
	  G_CALLBACK (action_tabs_next_callback) },
	{ "TabsMoveLeft", NULL, N_("Move Tab _Left"), "<shift><control>Page_Up",
	  N_("Move current tab to left"),
	  G_CALLBACK (action_tabs_move_left_callback) },
	{ "TabsMoveRight", NULL, N_("Move Tab _Right"), "<shift><control>Page_Down",
	  N_("Move current tab to right"),
	  G_CALLBACK (action_tabs_move_right_callback) },
	{ "ShowSearch", NULL, N_("S_how Search"), "<control>f",
	  N_("Show search"),
	  G_CALLBACK (action_show_search_callback) }
};

static const GtkToggleActionEntry navigation_toggle_entries[] = {
  /* name, stock id */     { "Show Hide Toolbar", NULL,
  /* label, accelerator */   N_("_Main Toolbar"), NULL,
  /* tooltip */              N_("Change the visibility of this window's main toolbar"),
                             G_CALLBACK (action_show_hide_toolbar_callback),
  /* is_active */            TRUE }, 
  /* name, stock id */     { "Show Hide Sidebar", NULL,
  /* label, accelerator */   N_("_Side Pane"), "F9",
  /* tooltip */              N_("Change the visibility of this window's side pane"),
                             G_CALLBACK (action_show_hide_sidebar_callback),
  /* is_active */            TRUE }, 
  /* name, stock id */     { "Show Hide Location Bar", NULL,
  /* label, accelerator */   N_("Location _Bar"), NULL,
  /* tooltip */              N_("Change the visibility of this window's location bar"),
                             G_CALLBACK (action_show_hide_location_bar_callback),
  /* is_active */            TRUE }, 
  /* name, stock id */     { "Show Hide Statusbar", NULL,
  /* label, accelerator */   N_("St_atusbar"), NULL,
  /* tooltip */              N_("Change the visibility of this window's statusbar"),
                             G_CALLBACK (action_show_hide_statusbar_callback),
  /* is_active */            TRUE },
  /* name, stock id */     { "Search", "gtk-find",
  /* label, accelerator */   N_("_Search for Files..."),
			     /* Accelerator is in ShowSearch */"",
  /* tooltip */              N_("Search documents and folders by name"),
                             G_CALLBACK (action_show_hide_search_callback),
  /* is_active */            FALSE },
  /* name, stock id */     { NAUTILUS_ACTION_SHOW_HIDE_EXTRA_PANE, NULL,
  /* label, accelerator */   N_("E_xtra Pane"), "F3",
  /* tooltip */              N_("Open an extra folder view side-by-side"),
                             G_CALLBACK (action_split_view_callback),
  /* is_active */            FALSE },
};

void 
nautilus_navigation_window_initialize_actions (NautilusNavigationWindow *window)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAction *action;
	int i;
	
	action_group = gtk_action_group_new ("NavigationActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	window->details->navigation_action_group = action_group;
	gtk_action_group_add_actions (action_group, 
				      navigation_entries, G_N_ELEMENTS (navigation_entries),
				      window);
	gtk_action_group_add_toggle_actions (action_group, 
					     navigation_toggle_entries, G_N_ELEMENTS (navigation_toggle_entries),
					     window);

	action = g_object_new (NAUTILUS_TYPE_NAVIGATION_ACTION,
			       "name", "Back",
			       "label", _("_Back"),
			       "stock_id", GTK_STOCK_GO_BACK,
			       "tooltip", _("Go to the previous visited location"),
			       "arrow-tooltip", _("Back history"),
			       "window", window,
			       "direction", NAUTILUS_NAVIGATION_DIRECTION_BACK,
			       "is_important", TRUE,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (action_back_callback), window);
	gtk_action_group_add_action_with_accel (action_group,
						action,
						"<alt>Left");
	g_object_unref (action);

	action = g_object_new (NAUTILUS_TYPE_NAVIGATION_ACTION,
			       "name", "Forward",
			       "label", _("_Forward"),
			       "stock_id", GTK_STOCK_GO_FORWARD,
			       "tooltip", _("Go to the next visited location"),
			       "arrow-tooltip", _("Forward history"),
			       "window", window,
			       "direction", NAUTILUS_NAVIGATION_DIRECTION_FORWARD,
			       "is_important", TRUE,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (action_forward_callback), window);
	gtk_action_group_add_action_with_accel (action_group,
						action,
						"<alt>Right");

	g_object_unref (action);

	action = g_object_new (NAUTILUS_TYPE_ZOOM_ACTION,
			       "name", "Zoom",
			       "label", _("_Zoom"),
			       "window", window,
			       "is_important", FALSE,
			       NULL);
	gtk_action_group_add_action (action_group,
				     action);
	g_object_unref (action);

	action = g_object_new (NAUTILUS_TYPE_VIEW_AS_ACTION,
			       "name", "ViewAs",
			       "label", _("_View As"),
			       "window", window,
			       "is_important", FALSE,
			       NULL);
	gtk_action_group_add_action (action_group,
				     action);
	g_object_unref (action);

	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));

	/* Alt+N for the first 10 tabs */
	for (i = 0; i < 10; ++i) {
		gchar action_name[80];
		gchar accelerator[80];

		snprintf(action_name, sizeof (action_name), "Tab%d", i);
		action = gtk_action_new (action_name, NULL, NULL, NULL);
		g_object_set_data (G_OBJECT (action), "num", GINT_TO_POINTER (i));
		g_signal_connect (action, "activate",
				G_CALLBACK (action_tab_change_action_activate_callback), window);
		snprintf(accelerator, sizeof (accelerator), "<alt>%d", (i+1)%10);
		gtk_action_group_add_action_with_accel (action_group, action, accelerator);
		g_object_unref (action);
		gtk_ui_manager_add_ui (ui_manager,
				gtk_ui_manager_new_merge_id (ui_manager),
				"/",
				action_name,
				action_name,
				GTK_UI_MANAGER_ACCELERATOR,
				FALSE);

	}

	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_SEARCH);
	g_object_set (action, "short_label", _("_Search"), NULL);

	action = gtk_action_group_get_action (action_group, "ShowSearch");
	gtk_action_set_sensitive (action, TRUE);

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui_manager */

	g_signal_connect (window, "loading_uri",
			  G_CALLBACK (nautilus_navigation_window_update_split_view_actions_sensitivity),
			  NULL);

	nautilus_navigation_window_update_split_view_actions_sensitivity (window);
}


/**
 * nautilus_window_initialize_menus
 * 
 * Create and install the set of menus for this window.
 * @window: A recently-created NautilusWindow.
 */
void 
nautilus_navigation_window_initialize_menus (NautilusNavigationWindow *window)
{
	GtkUIManager *ui_manager;
	const char *ui;

	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));

	ui = nautilus_ui_string_get ("nautilus-navigation-window-ui.xml");
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

	nautilus_navigation_window_update_show_hide_menu_items (window);
	nautilus_navigation_window_update_spatial_menu_item (window);

	nautilus_navigation_window_initialize_go_menu (window);
}
