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
#include "nautilus-bookmark-parsing.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-file-management-properties.h"
#include "nautilus-property-browser.h"
#include "nautilus-signaller.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-help.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-undo-manager.h>

#define MENU_PATH_HISTORY_PLACEHOLDER			"/MenuBar/Other Menus/Go/History Placeholder"
#define MENU_PATH_BOOKMARKS_PLACEHOLDER			"/MenuBar/Other Menus/Bookmarks/Bookmarks Placeholder"

#define RESPONSE_FORGET		1000

static GtkWindow *bookmarks_window = NULL;
static NautilusBookmarkList *bookmarks = NULL;

static void                  schedule_refresh_go_menu                      (NautilusNavigationWindow   *window);
static void                  append_dynamic_bookmarks                      (NautilusNavigationWindow   *window);
static void                  schedule_refresh_bookmarks_menu               (NautilusNavigationWindow   *window);
static void                  refresh_bookmarks_menu                        (NautilusNavigationWindow   *window);
static void                  add_bookmark_for_current_location             (NautilusNavigationWindow   *window);
static void                  edit_bookmarks                                (NautilusNavigationWindow   *window);
static NautilusBookmarkList *get_bookmark_list                             (void);

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
		detail = _("If you do, you will be doomed to repeat it.");
	} else {
		prompt = _("Are you sure you want to clear the list "
			   "of locations you have visited?");
		detail = _("If you clear the list of locations,"
			   " they will be permanently deleted."); 
	}
					   
	dialog = eel_create_question_dialog (prompt,
					     detail,
					     _("Clear History"), 
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

static void
action_add_bookmark_callback (GtkAction *action,
			      gpointer user_data)
{
        add_bookmark_for_current_location (NAUTILUS_NAVIGATION_WINDOW (user_data));
}

static void
action_edit_bookmarks_callback (GtkAction *action, 
				gpointer user_data)
{
        edit_bookmarks (NAUTILUS_NAVIGATION_WINDOW (user_data));
}

static void
free_bookmark_list (void)
{
	g_object_unref (bookmarks);
}

static NautilusBookmarkList *
get_bookmark_list (void)
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
		nautilus_bookmark_list_delete_items_with_uri (get_bookmark_list (), uri);
	}

	gtk_object_destroy (GTK_OBJECT (dialog));
}

static void
show_bogus_bookmark_window (NautilusWindow *window,
			    NautilusBookmark *bookmark)
{
	GtkDialog *dialog;
	char *uri;
	char *uri_for_display;
	char *prompt;
	char *detail;

	uri = nautilus_bookmark_get_uri (bookmark);
	uri_for_display = eel_format_uri_for_display (uri);
	
	prompt = _("Do you want to remove any bookmarks with the "
		   "non-existing location from your list?");
	detail = g_strdup_printf (_("The location \"%s\" does not exist."), uri_for_display);
	
	dialog = eel_show_yes_no_dialog (prompt, detail,
					 _("Bookmark for Nonexistent Location"),
					 _("Remove"), GTK_STOCK_CANCEL,
					 GTK_WINDOW (window));
	
	eel_gtk_signal_connect_free_data
		(GTK_OBJECT (dialog),
		 "response",
		 G_CALLBACK (remove_bookmarks_for_uri_if_yes),
		 g_strdup (uri));
	
	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_NO);

	g_free (uri);
	g_free (uri_for_display);
	g_free (detail);
}

static GtkWindow *
get_or_create_bookmarks_window (GObject *undo_manager_source)
{
	if (bookmarks_window == NULL) {
		bookmarks_window = create_bookmarks_window (get_bookmark_list(), undo_manager_source);
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
	}
}

/**
 * add_bookmark_for_current_location
 * 
 * Add a bookmark for the displayed location to the bookmarks menu.
 * Does nothing if there's already a bookmark for the displayed location.
 */
static void
add_bookmark_for_current_location (NautilusNavigationWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	nautilus_bookmark_list_append (get_bookmark_list (), 
				       NAUTILUS_WINDOW (window)->current_location_bookmark);
}

static void
edit_bookmarks (NautilusNavigationWindow *window)
{
	GtkWindow *dialog;

	dialog = get_or_create_bookmarks_window (G_OBJECT (window));

	gtk_window_set_screen (
		dialog, gtk_window_get_screen (GTK_WINDOW (window)));
        gtk_window_present (dialog);
}

static void
refresh_bookmarks_menu (NautilusNavigationWindow *window)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	/* Unregister any pending call to this function. */
	nautilus_navigation_window_remove_bookmarks_menu_callback (window);

	nautilus_navigation_window_remove_bookmarks_menu_items (window);
	append_dynamic_bookmarks (window);
}

/**
 * nautilus_navigation_window_initialize_bookmarks_menu
 * 
 * Fill in bookmarks menu with stored bookmarks, and wire up signals
 * so we'll be notified when bookmark list changes.
 */
static void 
nautilus_navigation_window_initialize_bookmarks_menu (NautilusNavigationWindow *window)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	/* Construct the initial set of bookmarks. */
	refresh_bookmarks_menu (window);

	/* Recreate dynamic part of menu if bookmark list changes */
	g_signal_connect_object (get_bookmark_list (), "contents_changed",
				 G_CALLBACK (schedule_refresh_bookmarks_menu),
				 window, G_CONNECT_SWAPPED);

	/* Recreate static & dynamic parts of menu if icon theme changes */
	g_signal_connect_object (nautilus_icon_factory_get (), "icons_changed",
				 G_CALLBACK (schedule_refresh_bookmarks_menu),
				 window, G_CONNECT_SWAPPED);
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

	gtk_ui_manager_insert_action_group (ui_manager,
					    window->details->go_menu_action_group,
					    -1);

	
	/* Add in a new set of history items. */
	for (node = nautilus_get_history_list (), index = 0;
	     node != NULL && index < 10;
	     node = node->next, index++) {
		nautilus_menus_append_bookmark_to_menu 
			(NAUTILUS_WINDOW (window),
			 NAUTILUS_BOOKMARK (node->data),
			 MENU_PATH_HISTORY_PLACEHOLDER,
			 index,
			 window->details->go_menu_action_group,
			 window->details->go_menu_merge_id,
			 G_CALLBACK (schedule_refresh_go_menu),
			 show_bogus_bookmark_window);
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
	 * or if icon theme changes.
	 */
	g_signal_connect_object (nautilus_signaller_get_current (), "history_list_changed",
				 G_CALLBACK (schedule_refresh_go_menu), window, G_CONNECT_SWAPPED);
	g_signal_connect_object (nautilus_icon_factory_get (), "icons_changed",
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
				gtk_window_get_screen (GTK_WINDOW (current_window)));
	nautilus_window_go_home (new_window);
}

static void
action_go_to_location_callback (GtkAction *action,
				gpointer user_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);

	nautilus_window_prompt_for_location (window);
}			   

static GtkActionEntry navigation_entries[] = {
  { "Go", NULL, N_("_Go") },               /* name, stock id, label */
  { "Bookmarks", NULL, N_("_Bookmarks") },               /* name, stock id, label */
  { "New Window", NULL, N_("Open New _Window"),               /* name, stock id, label */
    "<control>N", N_("Open another Nautilus window for the displayed location"),
    G_CALLBACK (action_new_window_callback) },
  { "Close All Windows", NULL, N_("Close _All Windows"),               /* name, stock id, label */
    "<control><shift>W", N_("Close all Navigation windows"),
    G_CALLBACK (action_close_all_windows_callback) },
  { "Go to Location", NULL, N_("_Location..."), /* name, stock id, label */
    "<control>L", N_("Specify a location to open"),
    G_CALLBACK (action_go_to_location_callback) },
  { "Clear History", NULL, N_("_Clear History"), /* name, stock id, label */
    NULL, N_("Clear contents of Go menu and Back/Forward lists"),
    G_CALLBACK (action_clear_history_callback) },
  { "Add Bookmark", GTK_STOCK_ADD, N_("_Add Bookmark"), /* name, stock id, label */
    "<control>d", N_("Add a bookmark for the current location to this menu"),
    G_CALLBACK (action_add_bookmark_callback) },
  { "Edit Bookmarks", NULL, N_("_Edit Bookmarks"), /* name, stock id, label */
    "<control>b", N_("Display a window that allows editing the bookmarks in this menu"),
    G_CALLBACK (action_edit_bookmarks_callback) },
};

static GtkToggleActionEntry navigation_toggle_entries[] = {
  { "Show Hide Sidebar", NULL,                 /* name, stock id */
    N_("_Side Pane"), "F9",                    /* label, accelerator */     
    N_("Change the visibility of this window's sidebar"),                       /* tooltip */
    G_CALLBACK (action_show_hide_sidebar_callback),
    TRUE}, /* is_active */
  { "Show Hide Location Bar", NULL,                 /* name, stock id */
    N_("Location _Bar"), NULL,                    /* label, accelerator */     
    N_("Change the visibility of this window's location bar"),                    /* tooltip */
    G_CALLBACK (action_show_hide_location_bar_callback),
    TRUE  },                                    /* is_active */
  { "Show Hide Statusbar", NULL,                 /* name, stock id */
    N_("St_atusbar"), NULL,                    /* label, accelerator */     
    N_("Change the visibility of this window's statusbar"),                    /* tooltip */
    G_CALLBACK (action_show_hide_statusbar_callback),
    TRUE  },                                    /* is_active */
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

        nautilus_navigation_window_initialize_go_menu (window);
        nautilus_navigation_window_initialize_bookmarks_menu (window);

}

void
nautilus_navigation_window_remove_bookmarks_menu_callback (NautilusNavigationWindow *window)
{
        if (window->details->refresh_bookmarks_menu_idle_id != 0) {
                g_source_remove (window->details->refresh_bookmarks_menu_idle_id);
		window->details->refresh_bookmarks_menu_idle_id = 0;
        }
}

void
nautilus_navigation_window_remove_bookmarks_menu_items (NautilusNavigationWindow *window)
{
	GtkUIManager *ui_manager;
	
	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));
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
append_dynamic_bookmarks (NautilusNavigationWindow *window)
{
        NautilusBookmarkList *bookmarks;
	guint bookmark_count;
	guint index;
	GtkUIManager *ui_manager;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));
	g_assert (window->details->bookmarks_merge_id == 0);
	g_assert (window->details->bookmarks_action_group == NULL);

	bookmarks = get_bookmark_list ();

	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));
	
	window->details->bookmarks_merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	window->details->bookmarks_action_group = gtk_action_group_new ("BookmarksGroup");

	gtk_ui_manager_insert_action_group (ui_manager,
					    window->details->bookmarks_action_group,
					    -1);

	/* append new set of bookmarks */
	bookmark_count = nautilus_bookmark_list_length (bookmarks);
	for (index = 0; index < bookmark_count; ++index) {
		nautilus_menus_append_bookmark_to_menu
			(NAUTILUS_WINDOW (window), 
			 nautilus_bookmark_list_item_at (bookmarks, index),
			 MENU_PATH_BOOKMARKS_PLACEHOLDER,
			 index,
			 window->details->bookmarks_action_group,
			 window->details->bookmarks_merge_id,
			 G_CALLBACK (schedule_refresh_bookmarks_menu), 
			 show_bogus_bookmark_window);
	}
}

static gboolean
refresh_bookmarks_menu_idle_callback (gpointer data)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (data));

	refresh_bookmarks_menu (NAUTILUS_NAVIGATION_WINDOW (data));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
schedule_refresh_bookmarks_menu (NautilusNavigationWindow *window)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	if (window->details->refresh_bookmarks_menu_idle_id == 0) {
                window->details->refresh_bookmarks_menu_idle_id
                        = g_idle_add (refresh_bookmarks_menu_idle_callback,
				      window);
	}	
}


