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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-undo-manager.h>
#include <libnautilus-private/nautilus-search-engine.h>
#include <libnautilus-private/nautilus-signaller.h>

#define MENU_PATH_HISTORY_PLACEHOLDER			"/MenuBar/Other Menus/Go/History Placeholder"
#define MENU_PATH_TABS_PLACEHOLDER			"/MenuBar/Other Menus/Tabs/TabsOpen"

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

void
nautilus_navigation_window_update_tab_menu_item_visibility (NautilusNavigationWindow *window) 
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_TABS);
	gtk_action_set_visible (action,
				eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ENABLE_TABS));

	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_NEW_TAB);
	gtk_action_set_visible (action,
				eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ENABLE_TABS));

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

static void
update_tab_action_sensitivity (NautilusNavigationWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	NautilusNotebook *notebook;
	gboolean sensitive;
	int tab_num;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	notebook = NAUTILUS_NOTEBOOK (window->notebook);
	action_group = window->details->navigation_action_group;

	action = gtk_action_group_get_action (action_group, "TabsPrevious");
	sensitive = nautilus_notebook_can_set_current_page_relative (notebook, -1);
	g_object_set (action, "sensitive", sensitive, NULL);

	action = gtk_action_group_get_action (action_group, "TabsNext");
	sensitive = nautilus_notebook_can_set_current_page_relative (notebook, 1);
	g_object_set (action, "sensitive", sensitive, NULL);

	action = gtk_action_group_get_action (action_group, "TabsMoveLeft");
	sensitive = nautilus_notebook_can_reorder_current_child_relative (notebook, -1);
	g_object_set (action, "sensitive", sensitive, NULL);

	action = gtk_action_group_get_action (action_group, "TabsMoveRight");
	sensitive = nautilus_notebook_can_reorder_current_child_relative (notebook, 1);
	g_object_set (action, "sensitive", sensitive, NULL);

	action_group = window->details->tabs_menu_action_group;
	tab_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	action = gtk_action_group_get_action (action_group, "Tab0");
	if (tab_num >= 0 && action != NULL) {
		gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), tab_num);
	}
}

static void
tab_menu_action_activate_callback (GtkAction *action,
				   gpointer user_data)
{
	int num;
	GtkWidget *notebook;
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);
	notebook = window->notebook;

	num = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), num);		       
}

static void
reload_tab_menu (NautilusNavigationWindow *window)
{
	GtkRadioAction *action;
	GtkUIManager *ui_manager;
	int i;
	gchar action_name[80];
	gchar *action_label;
	gchar accelerator[80];
	GSList *radio_group;
	NautilusWindowSlot *slot;
	GtkNotebook *notebook;
	
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	/* Remove old tab menu items */
	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));
	if (window->details->tabs_menu_merge_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager,
					  window->details->tabs_menu_merge_id);
		window->details->tabs_menu_merge_id = 0;
	}
	if (window->details->tabs_menu_action_group != NULL) {
		gtk_ui_manager_remove_action_group (ui_manager,
						    window->details->tabs_menu_action_group);
		window->details->tabs_menu_action_group = NULL;
	}

	/* Add new tab menu items */
	window->details->tabs_menu_merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	window->details->tabs_menu_action_group = gtk_action_group_new ("TabsMenuGroup");

	g_signal_connect (window->details->tabs_menu_action_group, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), NULL);
	
	gtk_ui_manager_insert_action_group (ui_manager,
					    window->details->tabs_menu_action_group,
					    -1);
	g_object_unref (window->details->tabs_menu_action_group);

	notebook = GTK_NOTEBOOK (window->notebook);
	radio_group = NULL;
	for (i = 0; i < gtk_notebook_get_n_pages (notebook); i++) {

		snprintf(action_name, sizeof (action_name), "Tab%d", i);

		slot = nautilus_window_get_slot_for_content_box (NAUTILUS_WINDOW (window),
								 gtk_notebook_get_nth_page (notebook, i));
		if (slot) {
			action_label = g_strdup (slot->title);
		} else {
			/* Give the action a generic label. This should only happen when the tab is created
			 * and the slot has not yet be created, so if all goes to plan then the action label
			 * will be updated when the slot is created. */
			action_label = g_strdup_printf ("Tab %d", i);
		}

		action = gtk_radio_action_new (action_name, action_label, NULL, NULL, i);

		g_free (action_label);
		action_label = NULL;
		
		gtk_radio_action_set_group (action, radio_group);
		radio_group = gtk_radio_action_get_group (action);
		
		g_signal_connect (action, "activate", 
				  G_CALLBACK (tab_menu_action_activate_callback),
				  window);

		/* Use Alt+(Number) keyboard accelerators for first 10 tabs */
		if (i < 10) {
			snprintf(accelerator, sizeof (accelerator), "<Alt>%d", (i+1)%10);
		} else {
			accelerator[0] = '\0';
		}
		gtk_action_group_add_action_with_accel (window->details->tabs_menu_action_group, 
							GTK_ACTION (action),
							accelerator);
		
		g_object_unref (action);
		
		gtk_ui_manager_add_ui (ui_manager, 
				       window->details->tabs_menu_merge_id,
				       MENU_PATH_TABS_PLACEHOLDER,
				       action_name,
				       action_name,
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);
	}

	update_tab_action_sensitivity (window);
}

static void 
nautilus_navigation_window_initialize_tabs_menu (NautilusNavigationWindow *window)
{
	g_signal_connect_object (window->notebook, "page-added",
				 G_CALLBACK (reload_tab_menu), window, G_CONNECT_SWAPPED);
	g_signal_connect_object (window->notebook, "page-removed",
				 G_CALLBACK (reload_tab_menu), window, G_CONNECT_SWAPPED);
	g_signal_connect_object (window->notebook, "page-reordered",
				 G_CALLBACK (reload_tab_menu), window, G_CONNECT_SWAPPED);
	g_signal_connect_object (window->notebook, "switch-page",
				 G_CALLBACK (update_tab_action_sensitivity), window,
				 G_CONNECT_SWAPPED | G_CONNECT_AFTER);

	reload_tab_menu (window);
}

/* Update the label displayed in the "Tabs" menu. This is called when the title of
 * a slot changes. */
void
nautilus_navigation_window_sync_tab_menu_title (NautilusNavigationWindow *window,
						NautilusWindowSlot *slot)
{
	int tab_num;
	GtkNotebook *notebook;
	GtkAction *action;
	GtkActionGroup *action_group;
	char action_name[80];

	notebook = GTK_NOTEBOOK (window->notebook);

	/* Find the tab number for that slot. It should (almost?) always be the current
	 * tab, so check that first in order to avoid searching through the entire tab 
	 * list. */
	tab_num = gtk_notebook_get_current_page (notebook);
	if (slot->content_box != gtk_notebook_get_nth_page (notebook, tab_num)) {
		tab_num = gtk_notebook_page_num (notebook, slot->content_box);
	}

	g_return_if_fail (tab_num >= 0);

	/* Find the action associated with that tab */
	action_group = window->details->tabs_menu_action_group;
	snprintf (action_name, sizeof (action_name), "Tab%d", tab_num);
	action = gtk_action_group_get_action (action_group, action_name);

	/* Update the label */
	g_return_if_fail (action);
	g_object_set (action, "label", slot->title, NULL);
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

	if (!eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ENABLE_TABS)) {
		return;
	}

	window = NAUTILUS_WINDOW (user_data);
	current_slot = window->details->active_slot;
	location = nautilus_window_slot_get_location (current_slot);

	window = NAUTILUS_WINDOW (current_slot->window);

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

		new_slot = nautilus_window_open_slot (window, flags);
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
	slot = current_window->details->active_slot;
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

static void
action_search_callback (GtkAction *action,
			gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	nautilus_navigation_window_show_search (window);
}

static void
action_tabs_previous_callback (GtkAction *action,
			       gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);
	nautilus_notebook_set_current_page_relative (NAUTILUS_NOTEBOOK (window->notebook), -1);
}

static void
action_tabs_next_callback (GtkAction *action,
			   gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);
	nautilus_notebook_set_current_page_relative (NAUTILUS_NOTEBOOK (window->notebook), 1);
}

static void
action_tabs_move_left_callback (GtkAction *action,
				gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);
	nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (window->notebook), -1);
}

static void
action_tabs_move_right_callback (GtkAction *action,
				 gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);
	nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (window->notebook), 1);
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
  /* name, stock id, label */  { "Edit Bookmarks", NULL, N_("_Edit Bookmarks..."),
                                 "<control>b", N_("Display a window that allows editing the bookmarks in this menu"),
                                 G_CALLBACK (action_edit_bookmarks_callback) },
  /* name, stock id, label */  { "Search", "gtk-find", N_("_Search for Files..."),
                                 "<control>F", N_("Locate documents and folders on this computer by name or content"),
                                 G_CALLBACK (action_search_callback) },

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
	nautilus_navigation_window_update_tab_menu_item_visibility (window);

        nautilus_navigation_window_initialize_go_menu (window);
        nautilus_navigation_window_initialize_tabs_menu (window);
}

