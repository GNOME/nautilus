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
#include "nautilus-navigation-action.h"
#include "nautilus-application.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-file-management-properties.h"
#include "nautilus-property-browser.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include "nautilus-window-bookmarks.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-help.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-uidefs.h>
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

static void
action_back_callback (GtkAction *action, 
		      gpointer user_data) 
{
	nautilus_navigation_window_go_back (NAUTILUS_NAVIGATION_WINDOW (user_data));
}

static void
action_forward_callback (GtkAction *action, 
			 gpointer user_data) 
{
	nautilus_navigation_window_go_forward (NAUTILUS_NAVIGATION_WINDOW (user_data));
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
	char *prompt;
	char *detail;

	/* Confirm before forgetting history because it's a rare operation that
	 * is hard to recover from. We don't want people doing it accidentally
	 * when they intended to choose another Go menu item.
	 */
	if ((rand() % 10) == 0) {
		/* This is a little joke, shows up occasionally. I only
		 * implemented this feature so I could use this joke. 
		 */
		prompt = _("Are you sure you want to forget history?");
		/* Translators: This is part of a joke and is paired with "Are you sure you want to forget history?" */
		detail = _("If you do, you will be doomed to repeat it.");
	} else {
		prompt = _("Are you sure you want to clear the list "
			   "of locations you have visited?");
		detail = _("If you clear the list of locations,"
			   " they will be permanently deleted."); 
	}
					   
	dialog = eel_create_question_dialog (prompt,
					     detail,
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
action_show_hide_location_bar_callback (GtkAction *action, 
					gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		nautilus_navigation_window_show_location_bar (window, TRUE);
	} else {
		nautilus_navigation_window_hide_location_bar (window, TRUE);
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
				      nautilus_navigation_window_location_bar_showing (window));

	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_SHOW_HIDE_STATUSBAR);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nautilus_navigation_window_status_bar_showing (window));
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

	label = GTK_LABEL (GTK_BIN (proxy)->child);

	gtk_label_set_use_underline (label, FALSE);
	gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (label, MENU_ITEM_MAX_WIDTH_CHARS);
}

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
	int index;
	
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
action_folder_window_callback (GtkAction *action,
			       gpointer user_data)
{
	NautilusWindow *current_window;
	GFile *current_location;

	current_window = NAUTILUS_WINDOW (user_data);
	current_location = nautilus_window_get_location (current_window);
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

static void
action_search_callback (GtkAction *action,
			gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	nautilus_navigation_window_show_search (window);
}

static const GtkActionEntry navigation_entries[] = {
  /* name, stock id, label */  { "Go", NULL, N_("_Go") },
  /* name, stock id, label */  { "Bookmarks", NULL, N_("_Bookmarks") },
  /* name, stock id, label */  { "New Window", "window-new", N_("New _Window"),
                                 "<control>N", N_("Open another Nautilus window for the displayed location"),
                                 G_CALLBACK (action_new_window_callback) },
  /* name, stock id, label */  { "Folder Window", "folder", N_("Open Folder W_indow"),
                                 NULL, N_("Open a folder window for the displayed location"),
                                 G_CALLBACK (action_folder_window_callback) },
  /* name, stock id, label */  { "Close All Windows", NULL, N_("Close _All Windows"),
                                 "<control><shift>W", N_("Close all Navigation windows"),
                                 G_CALLBACK (action_close_all_windows_callback) },
  /* name, stock id, label */  { "Go to Location", NULL, N_("_Location..."),
                                 "<control>L", N_("Specify a location to open"),
                                 G_CALLBACK (action_go_to_location_callback) },
  /* name, stock id, label */  { "Clear History", NULL, N_("Clea_r History"),
                                 NULL, N_("Clear contents of Go menu and Back/Forward lists"),
                                 G_CALLBACK (action_clear_history_callback) },
  /* name, stock id, label */  { "Add Bookmark", GTK_STOCK_ADD, N_("_Add Bookmark"),
                                 "<control>d", N_("Add a bookmark for the current location to this menu"),
                                 G_CALLBACK (action_add_bookmark_callback) },
  /* name, stock id, label */  { "Edit Bookmarks", NULL, N_("_Edit Bookmarks"),
                                 "<control>b", N_("Display a window that allows editing the bookmarks in this menu"),
                                 G_CALLBACK (action_edit_bookmarks_callback) },
  /* name, stock id, label */  { "Search", "gtk-find", N_("_Search for Files..."),
                                 "<control>F", N_("Locate documents and folders on this computer by name or content"),
                                 G_CALLBACK (action_search_callback) },
		     
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
};

void 
nautilus_navigation_window_initialize_actions (NautilusNavigationWindow *window)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAction *action;
	
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

	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_SEARCH);
	g_object_set (action, "short_label", _("_Search"), NULL);

	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui_manager */
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

