/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-navigation-window-pane.c: Nautilus navigation window pane

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

   Author: Holger Berndt <berndth@gmx.de>
*/

#include "nautilus-navigation-window-pane.h"
#include "nautilus-window-private.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-navigation-bar.h"
#include "nautilus-pathbar.h"
#include "nautilus-location-bar.h"
#include "nautilus-notebook.h"

#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-window-slot-info.h>
#include <libnautilus-private/nautilus-view-factory.h>
#include <libnautilus-private/nautilus-entry.h>

#include <eel/eel-preferences.h>


static void nautilus_navigation_window_pane_init       (NautilusNavigationWindowPane *pane);
static void nautilus_navigation_window_pane_class_init (NautilusNavigationWindowPaneClass *class);
static void nautilus_navigation_window_pane_dispose    (GObject *object);

G_DEFINE_TYPE (NautilusNavigationWindowPane,
	       nautilus_navigation_window_pane,
	       NAUTILUS_TYPE_WINDOW_PANE)
#define parent_class nautilus_navigation_window_pane_parent_class


static void
real_set_active (NautilusWindowPane *pane, gboolean is_active)
{
	NautilusNavigationWindowPane *nav_pane;
	GList *l;

	nav_pane = NAUTILUS_NAVIGATION_WINDOW_PANE (pane);

	/* path bar */
	for (l = NAUTILUS_PATH_BAR (nav_pane->path_bar)->button_list; l; l = l->next) {
		gtk_widget_set_sensitive (gtk_bin_get_child (GTK_BIN (nautilus_path_bar_get_button_from_button_list_entry (l->data))), is_active);
	}

	/* navigation bar (manual entry) */
	nautilus_location_bar_set_active (NAUTILUS_LOCATION_BAR (nav_pane->navigation_bar), is_active);
}

static gboolean
navigation_bar_focus_in_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	NautilusWindowPane *pane;
	pane = NAUTILUS_WINDOW_PANE (user_data);
	nautilus_window_set_active_pane (pane->window, pane);
	return FALSE;
}

static int
bookmark_list_get_uri_index (GList *list, GFile *location)
{
	NautilusBookmark *bookmark;
	GList *l;
	GFile *tmp;
	int i;

	g_return_val_if_fail (location != NULL, -1);

	for (i = 0, l = list; l != NULL; i++, l = l->next) {
		bookmark = NAUTILUS_BOOKMARK (l->data);

		tmp = nautilus_bookmark_get_location (bookmark);
		if (g_file_equal (location, tmp)) {
			g_object_unref (tmp);
			return i;
		}
		g_object_unref (tmp);
	}

	return -1;
}

static void
search_bar_focus_in_callback (NautilusSearchBar *bar,
			      NautilusWindowPane *pane)
{
	nautilus_window_set_active_pane (pane->window, pane);
}


static void
search_bar_activate_callback (NautilusSearchBar *bar,
			      NautilusNavigationWindowPane *pane)
{
	char *uri, *current_uri;
	NautilusDirectory *directory;
	NautilusSearchDirectory *search_directory;
	NautilusQuery *query;
	GFile *location;

	uri = nautilus_search_directory_generate_new_uri ();
	location = g_file_new_for_uri (uri);
	g_free (uri);

	directory = nautilus_directory_get (location);

	g_assert (NAUTILUS_IS_SEARCH_DIRECTORY (directory));

	search_directory = NAUTILUS_SEARCH_DIRECTORY (directory);

	query = nautilus_search_bar_get_query (NAUTILUS_SEARCH_BAR (pane->search_bar));
	if (query != NULL) {
		NautilusWindowSlot *slot = NAUTILUS_WINDOW_PANE (pane)->active_slot;
		if (!nautilus_search_directory_is_indexed (search_directory)) {
			current_uri = nautilus_window_slot_get_location_uri (slot);
			nautilus_query_set_location (query, current_uri);
			g_free (current_uri);
		}
		nautilus_search_directory_set_query (search_directory, query);
		g_object_unref (query);
	}

	nautilus_window_slot_go_to (NAUTILUS_WINDOW_PANE (pane)->active_slot, location, FALSE);

	nautilus_directory_unref (directory);
	g_object_unref (location);
}

static void
search_bar_cancel_callback (GtkWidget *widget,
			    NautilusNavigationWindowPane *pane)
{
	if (nautilus_navigation_window_pane_hide_temporary_bars (pane)) {
		nautilus_navigation_window_restore_focus_widget (NAUTILUS_NAVIGATION_WINDOW (NAUTILUS_WINDOW_PANE (pane)->window));
	}
}

static void
navigation_bar_cancel_callback (GtkWidget *widget,
				NautilusNavigationWindowPane *pane)
{
	if (nautilus_navigation_window_pane_hide_temporary_bars (pane)) {
		nautilus_navigation_window_restore_focus_widget (NAUTILUS_NAVIGATION_WINDOW (NAUTILUS_WINDOW_PANE (pane)->window));
	}
}

static void
navigation_bar_location_changed_callback (GtkWidget *widget,
					  const char *uri,
					  NautilusNavigationWindowPane *pane)
{
	GFile *location;

	if (nautilus_navigation_window_pane_hide_temporary_bars (pane)) {
		nautilus_navigation_window_restore_focus_widget (NAUTILUS_NAVIGATION_WINDOW (NAUTILUS_WINDOW_PANE (pane)->window));
	}

	location = g_file_new_for_uri (uri);
	nautilus_window_slot_go_to (NAUTILUS_WINDOW_PANE (pane)->active_slot, location, FALSE);
	g_object_unref (location);
}

static void
path_bar_location_changed_callback (GtkWidget *widget,
				    GFile *location,
				    NautilusNavigationWindowPane *pane)
{
	NautilusNavigationWindowSlot *slot;
	NautilusWindowPane *win_pane;
	int i;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW_PANE (pane));

	win_pane = NAUTILUS_WINDOW_PANE(pane);

	slot = NAUTILUS_NAVIGATION_WINDOW_SLOT (win_pane->active_slot);

	/* check whether we already visited the target location */
	i = bookmark_list_get_uri_index (slot->back_list, location);
	if (i >= 0) {
		nautilus_navigation_window_back_or_forward (NAUTILUS_NAVIGATION_WINDOW (win_pane->window), TRUE, i, FALSE);
	} else {
		nautilus_window_slot_go_to (win_pane->active_slot, location, FALSE);
	}
}

static gboolean
path_bar_button_pressed_callback (GtkWidget *widget,
				  GdkEventButton *event,
				  NautilusNavigationWindowPane *pane)
{
	NautilusWindowSlot *slot;
	NautilusView *view;
	GFile *location;
	char *uri;

	nautilus_window_set_active_pane (NAUTILUS_WINDOW_PANE (pane)->window, NAUTILUS_WINDOW_PANE (pane));

	g_object_set_data (G_OBJECT (widget), "handle-button-release",
			   GINT_TO_POINTER (TRUE));

	if (event->button == 3) {
		slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW_PANE (pane)->window);
		view = slot->content_view;
		if (view != NULL) {
			location = nautilus_path_bar_get_path_for_button (
				NAUTILUS_PATH_BAR (pane->path_bar), widget);
			if (location != NULL) {
				uri = g_file_get_uri (location);
				nautilus_view_pop_up_location_context_menu (
					view, event, uri);
				g_object_unref (G_OBJECT (location));
				g_free (uri);
				return TRUE;
			}
		}
	}

	return FALSE;
}

static gboolean
path_bar_button_released_callback (GtkWidget *widget,
				   GdkEventButton *event,
				   NautilusNavigationWindowPane *pane)
{
	NautilusWindowSlot *slot;
	NautilusWindowOpenFlags flags;
	GFile *location;
	int mask;
	gboolean handle_button_release;

	mask = event->state & gtk_accelerator_get_default_mod_mask ();
	flags = 0;

	handle_button_release = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
						  "handle-button-release"));

	if (event->type == GDK_BUTTON_RELEASE && handle_button_release) {
		location = nautilus_path_bar_get_path_for_button (NAUTILUS_PATH_BAR (pane->path_bar), widget);

		if (event->button == 2 && mask == 0) {
			flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
		} else if (event->button == 1 && mask == GDK_CONTROL_MASK) {
			flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW;
		}

		if (flags != 0) {
			slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW_PANE (pane)->window);
			nautilus_window_slot_info_open_location (slot, location,
								 NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE,
								 flags, NULL);
			g_object_unref (location);
			return TRUE;
		}

		g_object_unref (location);
	}

	return FALSE;
}

static void
path_bar_button_drag_begin_callback (GtkWidget *widget,
				     GdkEventButton *event,
				     gpointer user_data)
{
	g_object_set_data (G_OBJECT (widget), "handle-button-release",
			   GINT_TO_POINTER (FALSE));
}

static void
path_bar_path_set_callback (GtkWidget *widget,
			    GFile *location,
			    NautilusNavigationWindowPane *pane)
{
	GList *children, *l;
	GtkWidget *child;

	children = gtk_container_get_children (GTK_CONTAINER (widget));

	for (l = children; l != NULL; l = l->next) {
		child = GTK_WIDGET (l->data);

		if (!GTK_IS_TOGGLE_BUTTON (child)) {
			continue;
		}

		if (!g_signal_handler_find (child,
					    G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
					    0, 0, NULL,
					    path_bar_button_pressed_callback,
					    pane)) {
			g_signal_connect (child, "button-press-event",
					  G_CALLBACK (path_bar_button_pressed_callback),
					  pane);
			g_signal_connect (child, "button-release-event",
					  G_CALLBACK (path_bar_button_released_callback),
					  pane);
			g_signal_connect (child, "drag-begin",
					  G_CALLBACK (path_bar_button_drag_begin_callback),
					  pane);
		}
	}

	g_list_free (children);
}

static void
notebook_popup_menu_move_left_cb (GtkMenuItem *menuitem,
				  gpointer user_data)
{
	NautilusNavigationWindowPane *pane;

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (user_data);
	nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (pane->notebook), -1);
}

static void
notebook_popup_menu_move_right_cb (GtkMenuItem *menuitem,
				   gpointer user_data)
{
	NautilusNavigationWindowPane *pane;

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (user_data);
	nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (pane->notebook), 1);
}

static void
notebook_popup_menu_close_cb (GtkMenuItem *menuitem,
			      gpointer user_data)
{
	NautilusWindowPane *pane;
	NautilusWindowSlot *slot;

	pane = NAUTILUS_WINDOW_PANE (user_data);
	slot = pane->active_slot;
	nautilus_window_slot_close (slot);
}

static void
notebook_popup_menu_show (NautilusNavigationWindowPane *pane,
			  GdkEventButton *event)
{
	GtkWidget *popup;
	GtkWidget *item;
	GtkWidget *image;
	int button, event_time;
	gboolean can_move_left, can_move_right;
	NautilusNotebook *notebook;

	notebook = NAUTILUS_NOTEBOOK (pane->notebook);

	can_move_left = nautilus_notebook_can_reorder_current_child_relative (notebook, -1);
	can_move_right = nautilus_notebook_can_reorder_current_child_relative (notebook, 1);

	popup = gtk_menu_new();

	item = gtk_menu_item_new_with_mnemonic (_("Move Tab _Left"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (notebook_popup_menu_move_left_cb),
			  pane);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       item);
	gtk_widget_set_sensitive (item, can_move_left);

	item = gtk_menu_item_new_with_mnemonic (_("Move Tab _Right"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (notebook_popup_menu_move_right_cb),
			  pane);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       item);
	gtk_widget_set_sensitive (item, can_move_right);

	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       gtk_separator_menu_item_new ());

	item = gtk_image_menu_item_new_with_mnemonic (_("_Close Tab"));
	image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (item, "activate",
			  G_CALLBACK (notebook_popup_menu_close_cb), pane);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       item);

	gtk_widget_show_all (popup);

	if (event) {
		button = event->button;
		event_time = event->time;
	} else {
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	/* TODO is this correct? */
	gtk_menu_attach_to_widget (GTK_MENU (popup),
				   pane->notebook,
				   NULL);

	gtk_menu_popup (GTK_MENU (popup), NULL, NULL, NULL, NULL,
			button, event_time);
}

/* emitted when the user clicks the "close" button of tabs */
static void
notebook_tab_close_requested (NautilusNotebook *notebook,
			      NautilusWindowSlot *slot,
			      NautilusWindowPane *pane)
{
	nautilus_window_pane_slot_close (pane, slot);
}

static gboolean
notebook_button_press_cb (GtkWidget *widget,
			  GdkEventButton *event,
			  gpointer user_data)
{
	NautilusNavigationWindowPane *pane;

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (user_data);
	if (GDK_BUTTON_PRESS == event->type && 3 == event->button) {
		notebook_popup_menu_show (pane, event);
		return TRUE;
	}

	return FALSE;
}

static gboolean
notebook_popup_menu_cb (GtkWidget *widget,
			gpointer user_data)
{
	NautilusNavigationWindowPane *pane;

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (user_data);
	notebook_popup_menu_show (pane, NULL);
	return TRUE;
}

static gboolean
notebook_switch_page_cb (GtkNotebook *notebook,
			 GtkWidget *page,
			 unsigned int page_num,
			 NautilusNavigationWindowPane *pane)
{
	NautilusWindowSlot *slot;
	GtkWidget *widget;

	widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (pane->notebook), page_num);
	g_assert (widget != NULL);

	/* find slot corresponding to the target page */
	slot = nautilus_window_pane_get_slot_for_content_box (NAUTILUS_WINDOW_PANE (pane), widget);
	g_assert (slot != NULL);

	nautilus_window_set_active_slot (slot->pane->window, slot);

	return FALSE;
}

void
nautilus_navigation_window_pane_remove_page (NautilusNavigationWindowPane *pane, int page_num)
{
	GtkNotebook *notebook;
	notebook = GTK_NOTEBOOK (pane->notebook);

	g_signal_handlers_block_by_func (notebook,
					 G_CALLBACK (notebook_switch_page_cb),
					 pane);
	gtk_notebook_remove_page (notebook, page_num);
	g_signal_handlers_unblock_by_func (notebook,
					   G_CALLBACK (notebook_switch_page_cb),
					   pane);
}

void
nautilus_navigation_window_pane_add_slot_in_tab (NautilusNavigationWindowPane *pane, NautilusWindowSlot *slot, NautilusWindowOpenSlotFlags flags)
{
	NautilusNotebook *notebook;

	notebook = NAUTILUS_NOTEBOOK (pane->notebook);
	g_signal_handlers_block_by_func (notebook,
					 G_CALLBACK (notebook_switch_page_cb),
					 pane);
	nautilus_notebook_add_tab (notebook,
				   slot,
				   (flags & NAUTILUS_WINDOW_OPEN_SLOT_APPEND) != 0 ?
				   -1 :
				   gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook)) + 1,
				   FALSE);
	g_signal_handlers_unblock_by_func (notebook,
					   G_CALLBACK (notebook_switch_page_cb),
					   pane);
}

static void
real_sync_location_widgets (NautilusWindowPane *pane)
{
	NautilusNavigationWindowSlot *navigation_slot;
	NautilusNavigationWindowPane *navigation_pane;
	NautilusWindowSlot *slot;

	slot = pane->active_slot;
	navigation_pane = NAUTILUS_NAVIGATION_WINDOW_PANE (pane);

	/* Change the location bar and path bar to match the current location. */
	if (slot->location != NULL) {
		char *uri;

		/* this may be NULL if we just created the slot */
		uri = nautilus_window_slot_get_location_uri (slot);
		nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (navigation_pane->navigation_bar), uri);
		g_free (uri);
		nautilus_path_bar_set_path (NAUTILUS_PATH_BAR (navigation_pane->path_bar), slot->location);
	}

	/* Update window global UI if this is the active pane */
	if (pane == pane->window->details->active_pane) {
		nautilus_window_update_up_button (pane->window);

		/* Check if the back and forward buttons need enabling or disabling. */
		navigation_slot = NAUTILUS_NAVIGATION_WINDOW_SLOT (pane->window->details->active_pane->active_slot);
		nautilus_navigation_window_allow_back (NAUTILUS_NAVIGATION_WINDOW (pane->window),
						       navigation_slot->back_list != NULL);
		nautilus_navigation_window_allow_forward (NAUTILUS_NAVIGATION_WINDOW (pane->window),
							  navigation_slot->forward_list != NULL);
	}
}

gboolean
nautilus_navigation_window_pane_hide_temporary_bars (NautilusNavigationWindowPane *pane)
{
	NautilusWindowSlot *slot;
	NautilusDirectory *directory;
	gboolean success;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW_PANE (pane));

	slot = NAUTILUS_WINDOW_PANE(pane)->active_slot;
	success = FALSE;

	if (pane->temporary_location_bar) {
		if (nautilus_navigation_window_pane_location_bar_showing (pane)) {
			nautilus_navigation_window_pane_hide_location_bar (pane, FALSE);
		}
		pane->temporary_location_bar = FALSE;
		success = TRUE;
	}
	if (pane->temporary_navigation_bar) {
		directory = nautilus_directory_get (slot->location);

		if (NAUTILUS_IS_SEARCH_DIRECTORY (directory)) {
			nautilus_navigation_window_pane_set_bar_mode (pane, NAUTILUS_BAR_SEARCH);
		} else {
			if (!eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY)) {
				nautilus_navigation_window_pane_set_bar_mode (pane, NAUTILUS_BAR_PATH);
			}
		}
		pane->temporary_navigation_bar = FALSE;
		success = TRUE;

		nautilus_directory_unref (directory);
	}
	if (pane->temporary_search_bar) {
		NautilusNavigationWindow *window;

		if (!eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY)) {
			nautilus_navigation_window_pane_set_bar_mode (pane, NAUTILUS_BAR_PATH);
		} else {
			nautilus_navigation_window_pane_set_bar_mode (pane, NAUTILUS_BAR_NAVIGATION);
		}
		window = NAUTILUS_NAVIGATION_WINDOW (NAUTILUS_WINDOW_PANE (pane)->window);
		nautilus_navigation_window_set_search_button (window, FALSE);
		pane->temporary_search_bar = FALSE;
		success = TRUE;
	}

	return success;
}

void
nautilus_navigation_window_pane_always_use_location_entry (NautilusNavigationWindowPane *pane, gboolean use_entry)
{
	if (use_entry) {
		nautilus_navigation_window_pane_set_bar_mode (pane, NAUTILUS_BAR_NAVIGATION);
	} else {
		nautilus_navigation_window_pane_set_bar_mode (pane, NAUTILUS_BAR_PATH);
	}
}

void
nautilus_navigation_window_pane_setup (NautilusNavigationWindowPane *pane)
{
	GtkWidget *hbox;
	NautilusEntry *entry;
	GtkSizeGroup *header_size_group;

	pane->widget = gtk_vbox_new (FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 12);
	pane->location_bar = hbox;
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
	gtk_box_pack_start (GTK_BOX (pane->widget), hbox,
			    FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	header_size_group = NAUTILUS_NAVIGATION_WINDOW (NAUTILUS_WINDOW_PANE (pane)->window)->details->header_size_group;

	pane->path_bar = g_object_new (NAUTILUS_TYPE_PATH_BAR, NULL);
	gtk_size_group_add_widget (header_size_group, pane->path_bar);
	gtk_widget_show (pane->path_bar);

	g_signal_connect_object (pane->path_bar, "path_clicked",
				 G_CALLBACK (path_bar_location_changed_callback), pane, 0);
	g_signal_connect_object (pane->path_bar, "path_set",
				 G_CALLBACK (path_bar_path_set_callback), pane, 0);

	gtk_box_pack_start (GTK_BOX (hbox),
			    pane->path_bar,
			    TRUE, TRUE, 0);

	pane->navigation_bar = nautilus_location_bar_new (pane);
	gtk_size_group_add_widget (header_size_group, pane->navigation_bar);
	g_signal_connect_object (pane->navigation_bar, "location_changed",
				 G_CALLBACK (navigation_bar_location_changed_callback), pane, 0);
	g_signal_connect_object (pane->navigation_bar, "cancel",
				 G_CALLBACK (navigation_bar_cancel_callback), pane, 0);
	entry = nautilus_location_bar_get_entry (NAUTILUS_LOCATION_BAR (pane->navigation_bar));
	g_signal_connect (entry, "focus-in-event",
			G_CALLBACK (navigation_bar_focus_in_callback), pane);

	gtk_box_pack_start (GTK_BOX (hbox),
			    pane->navigation_bar,
			    TRUE, TRUE, 0);

	pane->search_bar = nautilus_search_bar_new ();
	gtk_size_group_add_widget (header_size_group, pane->search_bar);
	g_signal_connect_object (pane->search_bar, "activate",
				 G_CALLBACK (search_bar_activate_callback), pane, 0);
	g_signal_connect_object (pane->search_bar, "cancel",
				 G_CALLBACK (search_bar_cancel_callback), pane, 0);
	g_signal_connect_object (pane->search_bar, "focus-in",
				 G_CALLBACK (search_bar_focus_in_callback), pane, 0);
	gtk_box_pack_start (GTK_BOX (hbox),
			    pane->search_bar,
			    TRUE, TRUE, 0);

	pane->notebook = g_object_new (NAUTILUS_TYPE_NOTEBOOK, NULL);
	gtk_box_pack_start (GTK_BOX (pane->widget), pane->notebook,
			    TRUE, TRUE, 0);
	g_signal_connect (pane->notebook,
			  "tab-close-request",
			  G_CALLBACK (notebook_tab_close_requested),
			  pane);
	g_signal_connect_after (pane->notebook,
				"button_press_event",
				G_CALLBACK (notebook_button_press_cb),
				pane);
	g_signal_connect (pane->notebook, "popup-menu",
			  G_CALLBACK (notebook_popup_menu_cb),
			  pane);
	g_signal_connect (pane->notebook,
			  "switch-page",
			  G_CALLBACK (notebook_switch_page_cb),
			  pane);

	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (pane->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (pane->notebook), FALSE);
	gtk_widget_show (pane->notebook);
	gtk_container_set_border_width (GTK_CONTAINER (pane->notebook), 0);

	/* Ensure that the view has some minimal size and that other parts
	 * of the UI (like location bar and tabs) don't request more and
	 * thus affect the default position of the split view paned.
	 */
	gtk_widget_set_size_request (pane->widget, 60, 60);
}


void
nautilus_navigation_window_pane_show_location_bar_temporarily (NautilusNavigationWindowPane *pane)
{
	if (!nautilus_navigation_window_pane_location_bar_showing (pane)) {
		nautilus_navigation_window_pane_show_location_bar (pane, FALSE);
		pane->temporary_location_bar = TRUE;
	}
}

void
nautilus_navigation_window_pane_show_navigation_bar_temporarily (NautilusNavigationWindowPane *pane)
{
	if (nautilus_navigation_window_pane_path_bar_showing (pane)
	    || nautilus_navigation_window_pane_search_bar_showing (pane)) {
		nautilus_navigation_window_pane_set_bar_mode (pane, NAUTILUS_BAR_NAVIGATION);
		pane->temporary_navigation_bar = TRUE;
	}
	nautilus_navigation_bar_activate
		(NAUTILUS_NAVIGATION_BAR (pane->navigation_bar));
}

gboolean
nautilus_navigation_window_pane_path_bar_showing (NautilusNavigationWindowPane *pane)
{
	if (pane->path_bar != NULL) {
		return gtk_widget_get_visible (pane->path_bar);
	}
	/* If we're not visible yet we haven't changed visibility, so its TRUE */
	return TRUE;
}

void
nautilus_navigation_window_pane_set_bar_mode (NautilusNavigationWindowPane *pane,
					      NautilusBarMode mode)
{
	GtkWidget *focus_widget;
	NautilusNavigationWindow *window;

	switch (mode) {

	case NAUTILUS_BAR_PATH:
		gtk_widget_show (pane->path_bar);
		gtk_widget_hide (pane->navigation_bar);
		gtk_widget_hide (pane->search_bar);
		break;

	case NAUTILUS_BAR_NAVIGATION:
		gtk_widget_show (pane->navigation_bar);
		gtk_widget_hide (pane->path_bar);
		gtk_widget_hide (pane->search_bar);
		break;

	case NAUTILUS_BAR_SEARCH:
		gtk_widget_show (pane->search_bar);
		gtk_widget_hide (pane->path_bar);
		gtk_widget_hide (pane->navigation_bar);
		break;
	}

	window = NAUTILUS_NAVIGATION_WINDOW (NAUTILUS_WINDOW_PANE (pane)->window);
	focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
	if (focus_widget != NULL && !nautilus_navigation_window_is_in_temporary_navigation_bar (focus_widget, window) &&
				    !nautilus_navigation_window_is_in_temporary_search_bar (focus_widget, window)) {
		if (mode == NAUTILUS_BAR_NAVIGATION || mode == NAUTILUS_BAR_PATH) {
			nautilus_navigation_window_set_search_button (window, FALSE);
		} else {
			nautilus_navigation_window_set_search_button (window, TRUE);
		}
	}
}

gboolean
nautilus_navigation_window_pane_search_bar_showing (NautilusNavigationWindowPane *pane)
{
	if (pane->search_bar != NULL) {
		return gtk_widget_get_visible (pane->search_bar);
	}
	/* If we're not visible yet we haven't changed visibility, so its TRUE */
	return TRUE;
}

void
nautilus_navigation_window_pane_hide_location_bar (NautilusNavigationWindowPane *pane, gboolean save_preference)
{
	pane->temporary_location_bar = FALSE;
	gtk_widget_hide(pane->location_bar);
	nautilus_navigation_window_update_show_hide_menu_items(
			NAUTILUS_NAVIGATION_WINDOW (NAUTILUS_WINDOW_PANE (pane)->window));
	if (save_preference && eel_preferences_key_is_writable(NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR)) {
		eel_preferences_set_boolean(NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR, FALSE);
	}
}

void
nautilus_navigation_window_pane_show_location_bar (NautilusNavigationWindowPane *pane, gboolean save_preference)
{
	gtk_widget_show(pane->location_bar);
	nautilus_navigation_window_update_show_hide_menu_items(NAUTILUS_NAVIGATION_WINDOW (NAUTILUS_WINDOW_PANE (pane)->window));
	if (save_preference && eel_preferences_key_is_writable(NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR)) {
		eel_preferences_set_boolean(NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR, TRUE);
	}
}

gboolean
nautilus_navigation_window_pane_location_bar_showing (NautilusNavigationWindowPane *pane)
{
	if (!NAUTILUS_IS_NAVIGATION_WINDOW_PANE (pane)) {
		return FALSE;
	}
	if (pane->location_bar != NULL) {
		return gtk_widget_get_visible (pane->location_bar);
	}
	/* If we're not visible yet we haven't changed visibility, so its TRUE */
	return TRUE;
}

static void
nautilus_navigation_window_pane_init (NautilusNavigationWindowPane *pane)
{
}

static void
nautilus_navigation_window_pane_show (NautilusWindowPane *pane)
{
	NautilusNavigationWindowPane *npane = NAUTILUS_NAVIGATION_WINDOW_PANE (pane);

	gtk_widget_show (npane->widget);
}

/* either called due to slot change, or due to location change in the current slot. */
static void
real_sync_search_widgets (NautilusWindowPane *window_pane)
{
	NautilusWindowSlot *slot;
	NautilusDirectory *directory;
	NautilusSearchDirectory *search_directory;
	NautilusNavigationWindowPane *pane;

	pane = NAUTILUS_NAVIGATION_WINDOW_PANE (window_pane);
	slot = window_pane->active_slot;
	search_directory = NULL;

	directory = nautilus_directory_get (slot->location);
	if (NAUTILUS_IS_SEARCH_DIRECTORY (directory)) {
		search_directory = NAUTILUS_SEARCH_DIRECTORY (directory);
	}

	if (search_directory != NULL &&
	    !nautilus_search_directory_is_saved_search (search_directory)) {
		nautilus_navigation_window_pane_show_location_bar_temporarily (pane);
		nautilus_navigation_window_pane_set_bar_mode (pane, NAUTILUS_BAR_SEARCH);
		pane->temporary_search_bar = FALSE;
	} else {
		pane->temporary_search_bar = TRUE;
		nautilus_navigation_window_pane_hide_temporary_bars (pane);
	}
	nautilus_directory_unref (directory);
}

static void
nautilus_navigation_window_pane_class_init (NautilusNavigationWindowPaneClass *class)
{
	G_OBJECT_CLASS (class)->dispose = nautilus_navigation_window_pane_dispose;
	NAUTILUS_WINDOW_PANE_CLASS (class)->show = nautilus_navigation_window_pane_show;
	NAUTILUS_WINDOW_PANE_CLASS (class)->set_active = real_set_active;
	NAUTILUS_WINDOW_PANE_CLASS (class)->sync_search_widgets = real_sync_search_widgets;
	NAUTILUS_WINDOW_PANE_CLASS (class)->sync_location_widgets = real_sync_location_widgets;
}

static void
nautilus_navigation_window_pane_dispose (GObject *object)
{
	NautilusNavigationWindowPane *pane = NAUTILUS_NAVIGATION_WINDOW_PANE (object);

	gtk_widget_destroy (pane->widget);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

NautilusNavigationWindowPane *
nautilus_navigation_window_pane_new (NautilusWindow *window)
{
	NautilusNavigationWindowPane *pane;

	pane = g_object_new (NAUTILUS_TYPE_NAVIGATION_WINDOW_PANE, NULL);
	NAUTILUS_WINDOW_PANE(pane)->window = window;

	return pane;
}
