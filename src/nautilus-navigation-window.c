/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *  Copyright (C) 2003 Ximian, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *  	     John Sullivan <sullivan@eazel.com>
 *
 */

/* nautilus-window.c: Implementation of the main window object */

#include <config.h>
#include "nautilus-window-private.h"

#include "nautilus-actions.h"
#include "nautilus-application.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-location-bar.h"
#include "nautilus-query-editor.h"
#include "nautilus-search-bar.h"
#include "nautilus-navigation-window-slot.h"
#include "nautilus-notebook.h"
#include "nautilus-places-sidebar.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-navigation-window-pane.h"

#include "file-manager/fm-tree-view.h"

#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-info.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-view-factory.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-undo.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-search-directory.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <math.h>
#include <sys/time.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_WINDOW
#include <libnautilus-private/nautilus-debug.h>

/* FIXME bugzilla.gnome.org 41243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

#define MAX_TITLE_LENGTH 180

#define MENU_PATH_BOOKMARKS_PLACEHOLDER			"/MenuBar/Other Menus/Bookmarks/Bookmarks Placeholder"

enum {
	ARG_0,
	ARG_APP_ID,
	ARG_APP
};

/* Forward and back buttons on the mouse */
static gboolean mouse_extra_buttons = TRUE;
static int mouse_forward_button = 9;
static int mouse_back_button = 8;

static void mouse_back_button_changed		     (gpointer                  callback_data);
static void mouse_forward_button_changed	     (gpointer                  callback_data);
static void use_extra_mouse_buttons_changed          (gpointer                  callback_data);
static NautilusWindowSlot *create_extra_pane         (NautilusNavigationWindow *window);


G_DEFINE_TYPE (NautilusNavigationWindow, nautilus_navigation_window, NAUTILUS_TYPE_WINDOW)
#define parent_class nautilus_navigation_window_parent_class

static const struct {
	unsigned int keyval;
	const char *action;
} extra_navigation_window_keybindings [] = {
#ifdef HAVE_X11_XF86KEYSYM_H
	{ XF86XK_Back,		NAUTILUS_ACTION_BACK },
	{ XF86XK_Forward,	NAUTILUS_ACTION_FORWARD }
#endif
};

static void
always_use_location_entry_changed (gpointer callback_data)
{
	NautilusNavigationWindow *window;
	GList *walk;
	gboolean use_entry;

	window = NAUTILUS_NAVIGATION_WINDOW (callback_data);

	use_entry = g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);

	for (walk = NAUTILUS_WINDOW(window)->details->panes; walk; walk = walk->next) {
		nautilus_navigation_window_pane_always_use_location_entry (walk->data, use_entry);
	}
}

static void
always_use_browser_changed (gpointer callback_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (callback_data);

	nautilus_navigation_window_update_spatial_menu_item (window);
}

/* Sanity check: highest mouse button value I could find was 14. 5 is our 
 * lower threshold (well-documented to be the one of the button events for the 
 * scrollwheel), so it's hardcoded in the functions below. However, if you have
 * a button that registers higher and want to map it, file a bug and 
 * we'll move the bar. Makes you wonder why the X guys don't have 
 * defined values for these like the XKB stuff, huh?
 */
#define UPPER_MOUSE_LIMIT 14

static void
mouse_back_button_changed (gpointer callback_data)
{
	int new_back_button;

	new_back_button = g_settings_get_int (nautilus_preferences, NAUTILUS_PREFERENCES_MOUSE_BACK_BUTTON);

	/* Bounds checking */
	if (new_back_button < 6 || new_back_button > UPPER_MOUSE_LIMIT)
		return;

	mouse_back_button = new_back_button;
}

static void
mouse_forward_button_changed (gpointer callback_data)
{
	int new_forward_button;

	new_forward_button = g_settings_get_int (nautilus_preferences, NAUTILUS_PREFERENCES_MOUSE_FORWARD_BUTTON);

	/* Bounds checking */
	if (new_forward_button < 6 || new_forward_button > UPPER_MOUSE_LIMIT)
		return;

	mouse_forward_button = new_forward_button;
}

static void
use_extra_mouse_buttons_changed (gpointer callback_data)
{
	mouse_extra_buttons = g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS);
}

void
nautilus_navigation_window_unset_focus_widget (NautilusNavigationWindow *window)
{
	if (window->details->last_focus_widget != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (window->details->last_focus_widget),
					      (gpointer *) &window->details->last_focus_widget);
		window->details->last_focus_widget = NULL;
	}
}

gboolean
nautilus_navigation_window_is_in_temporary_navigation_bar (GtkWidget *widget,
				NautilusNavigationWindow *window)
{
	GList *walk;
	gboolean is_in_any = FALSE;

	for (walk = NAUTILUS_WINDOW(window)->details->panes; walk; walk = walk->next) {
		NautilusNavigationWindowPane *pane = walk->data;
		if(gtk_widget_get_ancestor (widget, NAUTILUS_TYPE_NAVIGATION_BAR) != NULL &&
			       pane->temporary_navigation_bar)
			is_in_any = TRUE;
	}
	return is_in_any;
}

gboolean
nautilus_navigation_window_is_in_temporary_search_bar (GtkWidget *widget,
			    NautilusNavigationWindow *window)
{
	GList *walk;
	gboolean is_in_any = FALSE;

	for (walk = NAUTILUS_WINDOW(window)->details->panes; walk; walk = walk->next) {
		NautilusNavigationWindowPane *pane = walk->data;
		if(gtk_widget_get_ancestor (widget, NAUTILUS_TYPE_SEARCH_BAR) != NULL &&
				       pane->temporary_search_bar)
			is_in_any = TRUE;
	}
	return is_in_any;
}

static void
remember_focus_widget (NautilusNavigationWindow *window)
{
	NautilusNavigationWindow *navigation_window;
	GtkWidget *focus_widget;

	navigation_window = NAUTILUS_NAVIGATION_WINDOW (window);

	focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
	if (focus_widget != NULL &&
	    !nautilus_navigation_window_is_in_temporary_navigation_bar (focus_widget, navigation_window) &&
	    !nautilus_navigation_window_is_in_temporary_search_bar (focus_widget, navigation_window)) {
		nautilus_navigation_window_unset_focus_widget (navigation_window);

		navigation_window->details->last_focus_widget = focus_widget;
		g_object_add_weak_pointer (G_OBJECT (focus_widget),
					   (gpointer *) &(NAUTILUS_NAVIGATION_WINDOW (window)->details->last_focus_widget));
	}
}

void
nautilus_navigation_window_restore_focus_widget (NautilusNavigationWindow *window)
{
	if (window->details->last_focus_widget != NULL) {
		if (NAUTILUS_IS_VIEW (window->details->last_focus_widget)) {
			nautilus_view_grab_focus (NAUTILUS_VIEW (window->details->last_focus_widget));
		} else {
			gtk_widget_grab_focus (window->details->last_focus_widget);
		}

		nautilus_navigation_window_unset_focus_widget (window);
	}
}

static void
nautilus_navigation_window_unrealize (GtkWidget *widget)
{
	NautilusNavigationWindow *window;
	
	window = NAUTILUS_NAVIGATION_WINDOW (widget);

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static gboolean
nautilus_navigation_window_state_event (GtkWidget *widget,
					GdkEventWindowState *event)
{
	if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
		g_settings_set_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_MAXIMIZED,
					event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED);
	}

	if (GTK_WIDGET_CLASS (parent_class)->window_state_event != NULL) {
		return GTK_WIDGET_CLASS (parent_class)->window_state_event (widget, event);
	}

	return FALSE;
}

static gboolean
nautilus_navigation_window_key_press_event (GtkWidget *widget,
					    GdkEventKey *event)
{
	NautilusNavigationWindow *window;
	int i;

	window = NAUTILUS_NAVIGATION_WINDOW (widget);

	for (i = 0; i < G_N_ELEMENTS (extra_navigation_window_keybindings); i++) {
		if (extra_navigation_window_keybindings[i].keyval == event->keyval) {
			GtkAction *action;

			action = gtk_action_group_get_action (window->details->navigation_action_group,
							      extra_navigation_window_keybindings[i].action);

			g_assert (action != NULL);
			if (gtk_action_is_sensitive (action)) {
				gtk_action_activate (action);
				return TRUE;
			}

			break;
		}
	}

	return GTK_WIDGET_CLASS (nautilus_navigation_window_parent_class)->key_press_event (widget, event);
}

static gboolean
nautilus_navigation_window_button_press_event (GtkWidget *widget,
					       GdkEventButton *event)
{
	NautilusNavigationWindow *window;
	gboolean handled;

	handled = FALSE;
	window = NAUTILUS_NAVIGATION_WINDOW (widget);

	if (mouse_extra_buttons && (event->button == mouse_back_button)) {
		nautilus_navigation_window_go_back (window);
		handled = TRUE; 
	} else if (mouse_extra_buttons && (event->button == mouse_forward_button)) {
		nautilus_navigation_window_go_forward (window);
		handled = TRUE;
	} else if (GTK_WIDGET_CLASS (nautilus_navigation_window_parent_class)->button_press_event) {
		handled = GTK_WIDGET_CLASS (nautilus_navigation_window_parent_class)->button_press_event (widget, event);
	} else {
		handled = FALSE;
	}
	return handled;
}

static void
nautilus_navigation_window_destroy (GtkWidget *object)
{
	NautilusNavigationWindow *window;
	
	window = NAUTILUS_NAVIGATION_WINDOW (object);

	nautilus_navigation_window_unset_focus_widget (window);

	window->details->content_paned = NULL;
	window->details->split_view_hpane = NULL;

	GTK_WIDGET_CLASS (parent_class)->destroy (object);
}

static void
nautilus_navigation_window_finalize (GObject *object)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (object);

	nautilus_navigation_window_remove_go_menu_callback (window);

	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      always_use_browser_changed,
					      window);
	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      always_use_location_entry_changed,
					      window);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*
 * Main API
 */

void
nautilus_navigation_window_go_back (NautilusNavigationWindow *window)
{
	nautilus_navigation_window_back_or_forward (window, TRUE, 0, FALSE);
}

void
nautilus_navigation_window_go_forward (NautilusNavigationWindow *window)
{
	nautilus_navigation_window_back_or_forward (window, FALSE, 0, FALSE);
}

void
nautilus_navigation_window_allow_back (NautilusNavigationWindow *window, gboolean allow)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_BACK);
	
	gtk_action_set_sensitive (action, allow);
}

void
nautilus_navigation_window_allow_forward (NautilusNavigationWindow *window, gboolean allow)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      NAUTILUS_ACTION_FORWARD);
	
	gtk_action_set_sensitive (action, allow);
}

static void
real_sync_title (NautilusWindow *window,
		 NautilusWindowSlot *slot)
{
	NautilusNavigationWindow *navigation_window;
	NautilusNavigationWindowPane *pane;
	NautilusNotebook *notebook;
	char *full_title;
	char *window_title;

	navigation_window = NAUTILUS_NAVIGATION_WINDOW (window);

	EEL_CALL_PARENT (NAUTILUS_WINDOW_CLASS,
			 sync_title, (window, slot));

	if (slot == window->details->active_pane->active_slot) {
		/* if spatial mode is default, we keep "File Browser" in the window title
		 * to recognize browser windows. Otherwise, we default to the directory name.
		 */
		if (!g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER)) {
			full_title = g_strdup_printf (_("%s - File Browser"), slot->title);
			window_title = eel_str_middle_truncate (full_title, MAX_TITLE_LENGTH);
			g_free (full_title);
		} else {
			window_title = eel_str_middle_truncate (slot->title, MAX_TITLE_LENGTH);
		}

		gtk_window_set_title (GTK_WINDOW (window), window_title);
		g_free (window_title);
	}

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (slot->pane);
	notebook = NAUTILUS_NOTEBOOK (pane->notebook);
	nautilus_notebook_sync_tab_label (notebook, slot);
}

static NautilusIconInfo *
real_get_icon (NautilusWindow *window,
	       NautilusWindowSlot *slot)
{
	return nautilus_file_get_icon (slot->viewed_file, 48,
					NAUTILUS_FILE_ICON_FLAGS_IGNORE_VISITING |
					NAUTILUS_FILE_ICON_FLAGS_USE_MOUNT_ICON);
}

static void
real_sync_allow_stop (NautilusWindow *window,
		      NautilusWindowSlot *slot)
{
	NautilusNavigationWindow *navigation_window;
	NautilusNotebook *notebook;

	navigation_window = NAUTILUS_NAVIGATION_WINDOW (window);
	nautilus_navigation_window_set_spinner_active (navigation_window, slot->allow_stop);

	notebook = NAUTILUS_NOTEBOOK (NAUTILUS_NAVIGATION_WINDOW_PANE (slot->pane)->notebook);
	nautilus_notebook_sync_loading (notebook, slot);
}

static void
real_prompt_for_location (NautilusWindow *window, const char *initial)
{
	NautilusNavigationWindowPane *pane;

	remember_focus_widget (NAUTILUS_NAVIGATION_WINDOW (window));

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (window->details->active_pane);

	nautilus_navigation_window_pane_show_location_bar_temporarily (pane);
	nautilus_navigation_window_pane_show_navigation_bar_temporarily (pane);
	
	if (initial) {
		nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (pane->navigation_bar),
						      initial);
	}
}

void 
nautilus_navigation_window_show_search (NautilusNavigationWindow *window)
{
	NautilusNavigationWindowPane *pane;

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (NAUTILUS_WINDOW (window)->details->active_pane);
	if (!nautilus_navigation_window_pane_search_bar_showing (pane)) {
		remember_focus_widget (window);

		nautilus_navigation_window_pane_show_location_bar_temporarily (pane);
		nautilus_navigation_window_pane_set_bar_mode (pane, NAUTILUS_BAR_SEARCH);
		pane->temporary_search_bar = TRUE;
		nautilus_search_bar_clear (NAUTILUS_SEARCH_BAR (pane->search_bar));
	}

	nautilus_search_bar_grab_focus (NAUTILUS_SEARCH_BAR (pane->search_bar));
}

void
nautilus_navigation_window_hide_search (NautilusNavigationWindow *window)
{
	NautilusNavigationWindowPane *pane = NAUTILUS_NAVIGATION_WINDOW_PANE (NAUTILUS_WINDOW (window)->details->active_pane);
	if (nautilus_navigation_window_pane_search_bar_showing (pane)) {
		if (nautilus_navigation_window_pane_hide_temporary_bars (pane)) {
			nautilus_navigation_window_restore_focus_widget (window);
		}
	}
}

/* This updates the UI state of the search button, but does not
   in itself trigger a search action */
void
nautilus_navigation_window_set_search_button (NautilusNavigationWindow *window,
					      gboolean state)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->details->navigation_action_group,
					      "Search");

	/* Block callback so we don't activate the action and thus focus the
	   search entry */
	g_object_set_data (G_OBJECT (action), "blocked", GINT_TO_POINTER (1));
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), state);
	g_object_set_data (G_OBJECT (action), "blocked", NULL);
}

gboolean
nautilus_navigation_window_toolbar_showing (NautilusNavigationWindow *window)
{
	if (window->details->toolbar != NULL) {
		return gtk_widget_get_visible (window->details->toolbar);
	}
	/* If we're not visible yet we haven't changed visibility, so its TRUE */
	return TRUE;
}

void
nautilus_navigation_window_hide_status_bar (NautilusNavigationWindow *window)
{
	gtk_widget_hide (NAUTILUS_WINDOW (window)->details->statusbar);

	nautilus_navigation_window_update_show_hide_menu_items (window);
	g_settings_set_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_START_WITH_STATUS_BAR, FALSE);
}

void
nautilus_navigation_window_show_status_bar (NautilusNavigationWindow *window)
{
	gtk_widget_show (NAUTILUS_WINDOW (window)->details->statusbar);

	nautilus_navigation_window_update_show_hide_menu_items (window);
	g_settings_set_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_START_WITH_STATUS_BAR, TRUE);
}

gboolean
nautilus_navigation_window_status_bar_showing (NautilusNavigationWindow *window)
{
	if (NAUTILUS_WINDOW (window)->details->statusbar != NULL) {
		return gtk_widget_get_visible (NAUTILUS_WINDOW (window)->details->statusbar);
	}
	/* If we're not visible yet we haven't changed visibility, so its TRUE */
	return TRUE;
}


void
nautilus_navigation_window_hide_toolbar (NautilusNavigationWindow *window)
{
	gtk_widget_hide (window->details->toolbar);
	nautilus_navigation_window_update_show_hide_menu_items (window);
	g_settings_set_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_START_WITH_TOOLBAR, FALSE);
}

void
nautilus_navigation_window_show_toolbar (NautilusNavigationWindow *window)
{
	gtk_widget_show (window->details->toolbar);
	nautilus_navigation_window_update_show_hide_menu_items (window);
	g_settings_set_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_START_WITH_TOOLBAR, TRUE);
}

/**
 * nautilus_navigation_window_get_base_page_index:
 * @window:	Window to get index from
 *
 * Returns the index of the base page in the history list.
 * Base page is not the currently displayed page, but the page
 * that acts as the base from which the back and forward commands
 * navigate from.
 */
gint 
nautilus_navigation_window_get_base_page_index (NautilusNavigationWindow *window)
{
	NautilusNavigationWindowSlot *slot;
	gint forward_count;

	slot = NAUTILUS_NAVIGATION_WINDOW_SLOT (NAUTILUS_WINDOW (window)->details->active_pane->active_slot);

	forward_count = g_list_length (slot->forward_list); 

	/* If forward is empty, the base it at the top of the list */
	if (forward_count == 0) {
		return 0;
	}

	/* The forward count indicate the relative postion of the base page
	 * in the history list
	 */ 
	return forward_count;
}

/**
 * nautilus_navigation_window_show:
 * @widget: a #GtkWidget.
 *
 * Call parent and then show/hide window items
 * base on user prefs.
 */
static void
nautilus_navigation_window_show (GtkWidget *widget)
{
	NautilusNavigationWindow *window;
	gboolean show_location_bar;
	gboolean always_use_location_entry;
	GList *walk;

	window = NAUTILUS_NAVIGATION_WINDOW (widget);

	/* Initially show or hide views based on preferences; once the window is displayed
	 * these can be controlled on a per-window basis from View menu items.
	 */

	if (g_settings_get_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_START_WITH_TOOLBAR)) {
		nautilus_navigation_window_show_toolbar (window);
	} else {
		nautilus_navigation_window_hide_toolbar (window);
	}

	show_location_bar = g_settings_get_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_START_WITH_LOCATION_BAR);
	always_use_location_entry = g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);
	for (walk = NAUTILUS_WINDOW(window)->details->panes; walk; walk = walk->next) {
		NautilusNavigationWindowPane *pane = walk->data;
		if (show_location_bar) {
			nautilus_navigation_window_pane_show_location_bar (pane, FALSE);
		} else {
			nautilus_navigation_window_pane_hide_location_bar (pane, FALSE);
		}

		if (always_use_location_entry) {
			nautilus_navigation_window_pane_set_bar_mode (pane, NAUTILUS_BAR_NAVIGATION);
		} else {
			nautilus_navigation_window_pane_set_bar_mode (pane, NAUTILUS_BAR_PATH);
		}
	}

	if (g_settings_get_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_START_WITH_SIDEBAR)) {
		nautilus_navigation_window_show_sidebar (window);
	} else {
		nautilus_navigation_window_hide_sidebar (window);
	}

	if (g_settings_get_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_START_WITH_STATUS_BAR)) {
		nautilus_navigation_window_show_status_bar (window);
	} else {
		nautilus_navigation_window_hide_status_bar (window);
	}

	GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static void
nautilus_navigation_window_save_geometry (NautilusNavigationWindow *window)
{
	char *geometry_string;
	gboolean is_maximized;

	g_assert (NAUTILUS_IS_WINDOW (window));

	if (gtk_widget_get_window (GTK_WIDGET (window))) {
		geometry_string = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));
		is_maximized = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)))
				& GDK_WINDOW_STATE_MAXIMIZED;

		if (!is_maximized) {
			g_settings_set_string
				(nautilus_window_state, NAUTILUS_WINDOW_STATE_GEOMETRY,
				 geometry_string);
		}
		g_free (geometry_string);

		g_settings_set_boolean
			(nautilus_window_state, NAUTILUS_WINDOW_STATE_MAXIMIZED,
			 is_maximized);
	}
}

static void
real_window_close (NautilusWindow *window)
{
	nautilus_navigation_window_save_geometry (NAUTILUS_NAVIGATION_WINDOW (window));
}

static void
real_get_min_size (NautilusWindow *window,
		   guint *min_width, guint *min_height)
{
	if (min_width) {
		*min_width = NAUTILUS_NAVIGATION_WINDOW_MIN_WIDTH;
	}
	if (min_height) {
		*min_height = NAUTILUS_NAVIGATION_WINDOW_MIN_HEIGHT;
	}
}

static void
real_get_default_size (NautilusWindow *window,
		       guint *default_width, guint *default_height)
{
	if (default_width) {
		*default_width = NAUTILUS_NAVIGATION_WINDOW_DEFAULT_WIDTH;
	}

	if (default_height) {
		*default_height = NAUTILUS_NAVIGATION_WINDOW_DEFAULT_HEIGHT;
	}
}

static NautilusWindowSlot *
real_open_slot (NautilusWindowPane *pane,
		NautilusWindowOpenSlotFlags flags)
{
	NautilusWindowSlot *slot;

	slot = (NautilusWindowSlot *) g_object_new (NAUTILUS_TYPE_NAVIGATION_WINDOW_SLOT, NULL);
	slot->pane = pane;

	nautilus_navigation_window_pane_add_slot_in_tab (NAUTILUS_NAVIGATION_WINDOW_PANE (pane), slot, flags);
	gtk_widget_show (slot->content_box);

	return slot;
}

static void
real_close_slot (NautilusWindowPane *pane,
		 NautilusWindowSlot *slot)
{
	int page_num;
	GtkNotebook *notebook;

	notebook = GTK_NOTEBOOK (NAUTILUS_NAVIGATION_WINDOW_PANE (pane)->notebook);

	page_num = gtk_notebook_page_num (notebook, slot->content_box);
	g_assert (page_num >= 0);

	nautilus_navigation_window_pane_remove_page (NAUTILUS_NAVIGATION_WINDOW_PANE (pane), page_num);

	gtk_notebook_set_show_tabs (notebook,
				    gtk_notebook_get_n_pages (notebook) > 1);

	EEL_CALL_PARENT (NAUTILUS_WINDOW_CLASS,
			 close_slot, (pane, slot));
}

/* side pane helpers */

static void
side_pane_size_allocate_callback (GtkWidget *widget,
				  GtkAllocation *allocation,
				  gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	if (allocation->width != window->details->side_pane_width) {
		window->details->side_pane_width = allocation->width;

		DEBUG ("Saving sidebar width: %d", allocation->width);
		
		g_settings_set_int (nautilus_window_state,
				    NAUTILUS_WINDOW_STATE_SIDEBAR_WIDTH,
				    allocation->width <= 1 ? 0 : allocation->width);
	}
}

static void
setup_side_pane_width (NautilusNavigationWindow *window)
{
	g_return_if_fail (window->details->sidebar != NULL);

	window->details->side_pane_width =
		g_settings_get_int (nautilus_window_state,
				    NAUTILUS_WINDOW_STATE_SIDEBAR_WIDTH);

	gtk_paned_set_position (GTK_PANED (window->details->content_paned),
				window->details->side_pane_width);
}

static gboolean
sidebar_id_is_valid (const gchar *sidebar_id)
{
	return (g_strcmp0 (sidebar_id, NAUTILUS_NAVIGATION_WINDOW_SIDEBAR_PLACES) == 0 ||
		g_strcmp0 (sidebar_id, NAUTILUS_NAVIGATION_WINDOW_SIDEBAR_TREE) == 0);
}

static void
nautilus_navigation_window_set_up_sidebar (NautilusNavigationWindow *window)
{
	GtkWidget *sidebar;

	DEBUG ("Setting up sidebar id %s", window->details->sidebar_id);

	window->details->sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

	gtk_paned_pack1 (GTK_PANED (window->details->content_paned),
			 GTK_WIDGET (window->details->sidebar),
			 FALSE, FALSE);

	setup_side_pane_width (window);
	g_signal_connect (window->details->sidebar, 
			  "size_allocate",
			  G_CALLBACK (side_pane_size_allocate_callback),
			  window);

	if (g_strcmp0 (window->details->sidebar_id, NAUTILUS_NAVIGATION_WINDOW_SIDEBAR_PLACES) == 0) {
		sidebar = nautilus_places_sidebar_new (NAUTILUS_WINDOW_INFO (window));
	} else if (g_strcmp0 (window->details->sidebar_id, NAUTILUS_NAVIGATION_WINDOW_SIDEBAR_TREE) == 0) {
		sidebar = nautilus_tree_sidebar_new (NAUTILUS_WINDOW_INFO (window));
	} else {
		g_assert_not_reached ();
	}

	gtk_box_pack_start (GTK_BOX (window->details->sidebar), sidebar, TRUE, TRUE, 0);
	gtk_widget_show (sidebar);
	gtk_widget_show (GTK_WIDGET (window->details->sidebar));
}

static void
nautilus_navigation_window_tear_down_sidebar (NautilusNavigationWindow *window)
{
	DEBUG ("Destroying sidebar");

	gtk_widget_destroy (GTK_WIDGET (window->details->sidebar));
	window->details->sidebar = NULL;
}

void
nautilus_navigation_window_hide_sidebar (NautilusNavigationWindow *window)
{
	DEBUG ("Called hide_sidebar()");

	if (window->details->sidebar == NULL) {
		return;
	}

	nautilus_navigation_window_tear_down_sidebar (window);
	nautilus_navigation_window_update_show_hide_menu_items (window);

	g_settings_set_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_START_WITH_SIDEBAR, FALSE);
}

void
nautilus_navigation_window_show_sidebar (NautilusNavigationWindow *window)
{
	DEBUG ("Called show_sidebar()");

	if (window->details->sidebar != NULL) {
		return;
	}

	nautilus_navigation_window_set_up_sidebar (window);
	nautilus_navigation_window_update_show_hide_menu_items (window);
	g_settings_set_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_START_WITH_SIDEBAR, TRUE);
}

gboolean
nautilus_navigation_window_sidebar_showing (NautilusNavigationWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (window), FALSE);

	return (window->details->sidebar != NULL)
		&& gtk_widget_get_visible (gtk_paned_get_child1 (GTK_PANED (window->details->content_paned)));
}

static void
side_pane_id_changed (NautilusNavigationWindow *window)
{
	gchar *sidebar_id;

	sidebar_id = g_settings_get_string (nautilus_window_state,
					    NAUTILUS_WINDOW_STATE_SIDE_PANE_VIEW);

	DEBUG ("Sidebar id changed to %s", sidebar_id);

	if (g_strcmp0 (sidebar_id, window->details->sidebar_id) == 0) {
		g_free (sidebar_id);
		return;
	}

	if (!sidebar_id_is_valid (sidebar_id)) {
		g_free (sidebar_id);
		return;
	}

	g_free (window->details->sidebar_id);
	window->details->sidebar_id = sidebar_id;

	if (window->details->sidebar != NULL) {
		/* refresh the sidebar setting */
		nautilus_navigation_window_tear_down_sidebar (window);
		nautilus_navigation_window_set_up_sidebar (window);
	}
}

static void
nautilus_navigation_window_init (NautilusNavigationWindow *window)
{
	GtkUIManager *ui_manager;
	GtkWidget *toolbar;
	NautilusWindow *win;
	NautilusNavigationWindowPane *pane;
	GtkWidget *hpaned;
	GtkWidget *vbox;

	win = NAUTILUS_WINDOW (window);

	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NAUTILUS_TYPE_NAVIGATION_WINDOW, NautilusNavigationWindowDetails);

	pane = nautilus_navigation_window_pane_new (win);
	win->details->panes = g_list_prepend (win->details->panes, pane);

	window->details->header_size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
	gtk_size_group_set_ignore_hidden (window->details->header_size_group, FALSE);

	window->details->content_paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_table_attach (GTK_TABLE (NAUTILUS_WINDOW (window)->details->table),
			  window->details->content_paned,
			  /* X direction */                   /* Y direction */
			  0, 1,                               3, 4,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0,                                  0);
	gtk_widget_show (window->details->content_paned);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_paned_pack2 (GTK_PANED (window->details->content_paned), vbox,
			 TRUE, FALSE);
	gtk_widget_show (vbox);

	hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (vbox), hpaned, TRUE, TRUE, 0);
	gtk_widget_show (hpaned);
	window->details->split_view_hpane = hpaned;

	gtk_box_pack_start (GTK_BOX (vbox), win->details->statusbar, FALSE, FALSE, 0);
	gtk_widget_show (win->details->statusbar);

	nautilus_navigation_window_pane_setup (pane);

	gtk_paned_pack1 (GTK_PANED(hpaned), pane->widget, TRUE, FALSE);
	gtk_widget_show (pane->widget);

	/* this has to be done after the location bar has been set up,
	 * but before menu stuff is being called */
	nautilus_window_set_active_pane (win, NAUTILUS_WINDOW_PANE (pane));

	nautilus_navigation_window_initialize_actions (window);

	nautilus_navigation_window_initialize_menus (window);

	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));
	toolbar = gtk_ui_manager_get_widget (ui_manager, "/Toolbar");
	window->details->toolbar = toolbar;
	gtk_table_attach (GTK_TABLE (NAUTILUS_WINDOW (window)->details->table),
			  toolbar,
			  /* X direction */                   /* Y direction */
			  0, 1,                               1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0,
			  0,                                  0);
	gtk_widget_show (toolbar);

	nautilus_navigation_window_initialize_toolbars (window);

	/* Set initial sensitivity of some buttons & menu items
	 * now that they're all created.
	 */
	nautilus_navigation_window_allow_back (window, FALSE);
	nautilus_navigation_window_allow_forward (window, FALSE);

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY,
				  G_CALLBACK(always_use_location_entry_changed),
				  window);

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER,
				  G_CALLBACK(always_use_browser_changed),
				  window);

	g_signal_connect_swapped (nautilus_window_state,
				  "changed::" NAUTILUS_WINDOW_STATE_SIDE_PANE_VIEW,
				  G_CALLBACK (side_pane_id_changed),
				  window);

	side_pane_id_changed (window);
}

static void
nautilus_navigation_window_class_init (NautilusNavigationWindowClass *class)
{
	NAUTILUS_WINDOW_CLASS (class)->window_type = NAUTILUS_WINDOW_NAVIGATION;
	NAUTILUS_WINDOW_CLASS (class)->bookmarks_placeholder = MENU_PATH_BOOKMARKS_PLACEHOLDER;
	
	G_OBJECT_CLASS (class)->finalize = nautilus_navigation_window_finalize;
	GTK_WIDGET_CLASS (class)->destroy = nautilus_navigation_window_destroy;
	GTK_WIDGET_CLASS (class)->show = nautilus_navigation_window_show;
	GTK_WIDGET_CLASS (class)->unrealize = nautilus_navigation_window_unrealize;
	GTK_WIDGET_CLASS (class)->window_state_event = nautilus_navigation_window_state_event;
	GTK_WIDGET_CLASS (class)->key_press_event = nautilus_navigation_window_key_press_event;
	GTK_WIDGET_CLASS (class)->button_press_event = nautilus_navigation_window_button_press_event;
	NAUTILUS_WINDOW_CLASS (class)->sync_allow_stop = real_sync_allow_stop;
	NAUTILUS_WINDOW_CLASS (class)->prompt_for_location = real_prompt_for_location;
	NAUTILUS_WINDOW_CLASS (class)->sync_title = real_sync_title;
	NAUTILUS_WINDOW_CLASS (class)->get_icon = real_get_icon;
	NAUTILUS_WINDOW_CLASS (class)->get_min_size = real_get_min_size;
	NAUTILUS_WINDOW_CLASS (class)->get_default_size = real_get_default_size;
	NAUTILUS_WINDOW_CLASS (class)->close = real_window_close;

	NAUTILUS_WINDOW_CLASS (class)->open_slot = real_open_slot;
	NAUTILUS_WINDOW_CLASS (class)->close_slot = real_close_slot;

	g_type_class_add_private (G_OBJECT_CLASS (class), sizeof (NautilusNavigationWindowDetails));

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_MOUSE_BACK_BUTTON,
				  G_CALLBACK(mouse_back_button_changed),
				  NULL);

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_MOUSE_FORWARD_BUTTON,
				  G_CALLBACK(mouse_forward_button_changed),
				  NULL);

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS,
				  G_CALLBACK(use_extra_mouse_buttons_changed),
				  NULL);
}

static NautilusWindowSlot *
create_extra_pane (NautilusNavigationWindow *window)
{
	NautilusWindow *win;
	NautilusNavigationWindowPane *pane;
	NautilusWindowSlot *slot;
	GtkPaned *paned;

	win = NAUTILUS_WINDOW (window);

	/* New pane */
	pane = nautilus_navigation_window_pane_new (win);
	win->details->panes = g_list_append (win->details->panes, pane);

	nautilus_navigation_window_pane_setup (pane);

	paned = GTK_PANED (window->details->split_view_hpane);
	if (gtk_paned_get_child1 (paned) == NULL) {
		gtk_paned_pack1 (paned, pane->widget, TRUE, FALSE);
	} else {
		gtk_paned_pack2 (paned, pane->widget, TRUE, FALSE);
	}

	/* slot */
	slot = nautilus_window_open_slot (NAUTILUS_WINDOW_PANE (pane),
					  NAUTILUS_WINDOW_OPEN_SLOT_APPEND);
	NAUTILUS_WINDOW_PANE (pane)->active_slot = slot;

	return slot;
}

void
nautilus_navigation_window_split_view_on (NautilusNavigationWindow *window)
{
	NautilusWindow *win;
	NautilusNavigationWindowPane *pane;
	NautilusWindowSlot *slot, *old_active_slot;
	GFile *location;
	GtkAction *action;

	win = NAUTILUS_WINDOW (window);

	old_active_slot = nautilus_window_get_active_slot (win);
	slot = create_extra_pane (window);
	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (slot->pane);

	location = NULL;
	if (old_active_slot != NULL) {
		location = nautilus_window_slot_get_location (old_active_slot);
		if (location != NULL) {
			if (g_file_has_uri_scheme (location, "x-nautilus-search")) {
				g_object_unref (location);
				location = NULL;
			}
		}
	}
	if (location == NULL) {
		location = g_file_new_for_path (g_get_home_dir ());
	}

	nautilus_window_slot_go_to (slot, location, FALSE);
	g_object_unref (location);

	action = gtk_action_group_get_action (NAUTILUS_NAVIGATION_WINDOW (NAUTILUS_WINDOW_PANE (pane)->window)->details->navigation_action_group,
					      NAUTILUS_ACTION_SHOW_HIDE_LOCATION_BAR);
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		nautilus_navigation_window_pane_show_location_bar (pane, TRUE);
	} else {
		nautilus_navigation_window_pane_hide_location_bar (pane, TRUE);
	}
}

void
nautilus_navigation_window_split_view_off (NautilusNavigationWindow *window)
{
	NautilusWindow *win;
	NautilusWindowPane *pane, *active_pane;
	GList *l, *next;

	win = NAUTILUS_WINDOW (window);

	g_return_if_fail (win);

	active_pane = win->details->active_pane;

	/* delete all panes except the first (main) pane */
	for (l = win->details->panes; l != NULL; l = next) {
		next = l->next;
		pane = l->data;
		if (pane != active_pane) {
			nautilus_window_close_pane (pane);
		}
	}

	nautilus_navigation_window_update_show_hide_menu_items (window);
	nautilus_navigation_window_update_split_view_actions_sensitivity (window);
}

gboolean
nautilus_navigation_window_split_view_showing (NautilusNavigationWindow *window)
{
	return g_list_length (NAUTILUS_WINDOW (window)->details->panes) > 1;
}
