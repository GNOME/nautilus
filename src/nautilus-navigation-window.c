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
#include "nautilus-main.h"
#include "nautilus-location-bar.h"
#include "nautilus-pathbar.h"
#include "nautilus-query-editor.h"
#include "nautilus-search-bar.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-zoom-control.h"
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <glib/gi18n.h>
#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-horizontal-splitter.h>
#include <libnautilus-private/nautilus-icon-info.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-sidebar.h>
#include <libnautilus-private/nautilus-view-factory.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-undo.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-sidebar-provider.h>
#include <libnautilus-private/nautilus-search-directory.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <math.h>
#include <sys/time.h>


/* FIXME bugzilla.gnome.org 41243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

#define MAX_TITLE_LENGTH 180

#define MENU_PATH_BOOKMARKS_PLACEHOLDER			"/MenuBar/Other Menus/Bookmarks/Bookmarks Placeholder"

typedef enum {
	NAUTILUS_BAR_PATH,
	NAUTILUS_BAR_NAVIGATION,
	NAUTILUS_BAR_SEARCH
} NautilusBarMode;

enum {
	ARG_0,
	ARG_APP_ID,
	ARG_APP
};

static int side_pane_width_auto_value = 0;

static void add_sidebar_panels                       (NautilusNavigationWindow *window);
static void load_view_as_menu                        (NautilusWindow           *window);
static void side_panel_image_changed_callback        (NautilusSidebar          *side_panel,
						      gpointer                  callback_data);
static void navigation_bar_location_changed_callback (GtkWidget                *widget,
						      const char               *uri,
						      NautilusNavigationWindow *window);
static void navigation_bar_cancel_callback           (GtkWidget                *widget,
						      NautilusNavigationWindow *window);
static void path_bar_location_changed_callback       (GtkWidget                *widget,
						      GFile                    *path,
						      NautilusNavigationWindow *window);
static void always_use_location_entry_changed        (gpointer                  callback_data);
static void always_use_browser_changed               (gpointer                  callback_data);

static void nautilus_navigation_window_set_bar_mode  (NautilusNavigationWindow *window, 
						      NautilusBarMode           mode);
static void search_bar_activate_callback             (NautilusSearchBar        *bar,
						      NautilusWindow           *window);
static void search_bar_cancel_callback               (GtkWidget                *widget,
						      NautilusNavigationWindow *window);

static void nautilus_navigation_window_show_location_bar_temporarily (NautilusNavigationWindow *window);

static void view_as_menu_switch_views_callback	     (GtkComboBox              *combo_box,
						      NautilusWindow           *window);

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
location_button_toggled_cb (GtkToggleButton *toggle,
			    NautilusNavigationWindow *window)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active (toggle);
	eel_preferences_set_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY, is_active);

	if (is_active) {
		nautilus_navigation_bar_activate (NAUTILUS_NAVIGATION_BAR (window->navigation_bar));
	}
}

static gboolean
location_button_should_be_active (NautilusNavigationWindow *window)
{
	return eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);
}

static GtkWidget *
location_button_create (NautilusNavigationWindow *window)
{
	GtkWidget *image;
	GtkWidget *button;

	image = gtk_image_new_from_stock (GTK_STOCK_EDIT, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);

	button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
			       "image", image,
			       "focus-on-click", FALSE,
			       "active", location_button_should_be_active (window),
			       NULL);

	gtk_widget_set_tooltip_text (button,
				     _("Toggle between button and text-based location bar"));

	g_signal_connect (button, "toggled",
			  G_CALLBACK (location_button_toggled_cb), window);
	return button;
}

static void
nautilus_navigation_window_init (NautilusNavigationWindow *window)
{
	GtkUIManager *ui_manager;
	GtkWidget *toolbar;
	GtkWidget *location_bar;
	GtkWidget *view_as_menu_vbox;
	GtkToolItem *item;
	GtkWidget *hbox, *vbox, *eventbox, *extras_vbox;

	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NAUTILUS_TYPE_NAVIGATION_WINDOW, NautilusNavigationWindowDetails);

	window->details->content_paned = nautilus_horizontal_splitter_new ();
	gtk_table_attach (GTK_TABLE (NAUTILUS_WINDOW (window)->details->table),
			  window->details->content_paned,
			  /* X direction */                   /* Y direction */
			  0, 1,                               3, 4,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_FILL | GTK_SHRINK,
			  0,                                  0);
	gtk_widget_show (window->details->content_paned);

	vbox = gtk_vbox_new (FALSE, 0);
	nautilus_horizontal_splitter_pack2 (
		NAUTILUS_HORIZONTAL_SPLITTER (window->details->content_paned),
		vbox);
	gtk_widget_show (vbox);

	eventbox = gtk_event_box_new ();
	gtk_widget_set_name (eventbox, "nautilus-extra-view-widget");
	gtk_box_pack_start (GTK_BOX (vbox), eventbox, FALSE, FALSE, 0);
	gtk_widget_show (eventbox);
	
	extras_vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (extras_vbox), 6);
	NAUTILUS_WINDOW (window)->details->extra_location_widgets = extras_vbox;
	gtk_container_add (GTK_CONTAINER (eventbox), extras_vbox);

	window->details->content_box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), window->details->content_box, TRUE, TRUE, 0);
	gtk_widget_show (window->details->content_box);
	
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

	/* set up location bar */
	location_bar = gtk_toolbar_new ();
	window->details->location_bar = location_bar;

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (hbox);

	item = gtk_tool_item_new ();
	gtk_container_set_border_width (GTK_CONTAINER (item), GNOME_PAD_SMALL);
	gtk_widget_show (GTK_WIDGET (item));
	gtk_tool_item_set_expand (item, TRUE);
	gtk_container_add (GTK_CONTAINER (item),  hbox);
	gtk_toolbar_insert (GTK_TOOLBAR (location_bar),
			    item, -1);

	window->details->location_button = location_button_create (window);
	gtk_box_pack_start (GTK_BOX (hbox), window->details->location_button, FALSE, FALSE, 0);
	gtk_widget_show (window->details->location_button);

	window->path_bar = g_object_new (NAUTILUS_TYPE_PATH_BAR, NULL);
 	gtk_widget_show (window->path_bar);
	
	g_signal_connect_object (window->path_bar, "path_clicked",
				 G_CALLBACK (path_bar_location_changed_callback), window, 0);
	
	gtk_box_pack_start (GTK_BOX (hbox),
			    window->path_bar,
			    TRUE, TRUE, 0);

	window->navigation_bar = nautilus_location_bar_new (window);
	g_signal_connect_object (window->navigation_bar, "location_changed",
				 G_CALLBACK (navigation_bar_location_changed_callback), window, 0);
	g_signal_connect_object (window->navigation_bar, "cancel",
				 G_CALLBACK (navigation_bar_cancel_callback), window, 0);

	gtk_box_pack_start (GTK_BOX (hbox),
			    window->navigation_bar,
			    TRUE, TRUE, 0);

	window->search_bar = nautilus_search_bar_new ();
	g_signal_connect_object (window->search_bar, "activate",
				 G_CALLBACK (search_bar_activate_callback), window, 0);
	g_signal_connect_object (window->search_bar, "cancel",
				 G_CALLBACK (search_bar_cancel_callback), window, 0);
	gtk_box_pack_start (GTK_BOX (hbox),
			    window->search_bar,
			    TRUE, TRUE, 0);

	/* Option menu for content view types; it's empty here, filled in when a uri is set.
	 * Pack it into vbox so it doesn't grow vertically when location bar does. 
	 */
	view_as_menu_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_widget_show (view_as_menu_vbox);

	item = gtk_tool_item_new ();
	gtk_container_set_border_width (GTK_CONTAINER (item), GNOME_PAD_SMALL);
	gtk_widget_show (GTK_WIDGET (item));
	gtk_container_add (GTK_CONTAINER (item), view_as_menu_vbox);
	gtk_toolbar_insert (GTK_TOOLBAR (location_bar),
			    item, -1);
	
	window->view_as_combo_box = gtk_combo_box_new_text ();
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (window->view_as_combo_box), FALSE);
	gtk_box_pack_end (GTK_BOX (view_as_menu_vbox), window->view_as_combo_box, TRUE, FALSE, 0);
	gtk_widget_show (window->view_as_combo_box);
	g_signal_connect_object (window->view_as_combo_box, "changed",
				 G_CALLBACK (view_as_menu_switch_views_callback), window, 0);

	/* Allocate the zoom control and place on the right next to the menu.
	 * It gets shown later, if the view-frame contains something zoomable.
	 */
	window->zoom_control = nautilus_zoom_control_new ();
	g_signal_connect_object (window->zoom_control, "zoom_in",
				 G_CALLBACK (nautilus_window_zoom_in),
				 window, G_CONNECT_SWAPPED);
	g_signal_connect_object (window->zoom_control, "zoom_out",
				 G_CALLBACK (nautilus_window_zoom_out),
				 window, G_CONNECT_SWAPPED);
	g_signal_connect_object (window->zoom_control, "zoom_to_level",
				 G_CALLBACK (nautilus_window_zoom_to_level),
				 window, G_CONNECT_SWAPPED);
	g_signal_connect_object (window->zoom_control, "zoom_to_default",
				 G_CALLBACK (nautilus_window_zoom_to_default),
				 window, G_CONNECT_SWAPPED);
	
	item = gtk_tool_item_new ();
	gtk_container_set_border_width (GTK_CONTAINER (item), GNOME_PAD_SMALL);
	gtk_widget_show (GTK_WIDGET (item));
	gtk_container_add (GTK_CONTAINER (item),  window->zoom_control);
	gtk_toolbar_insert (GTK_TOOLBAR (location_bar),
			    item, 1);

	gtk_widget_show (location_bar);

	gtk_table_attach (GTK_TABLE (NAUTILUS_WINDOW (window)->details->table),
			  location_bar,
			  /* X direction */                    /* Y direction */
			  0, 1,                                2, 3,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK,  0,
			  0,                                   0);

	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY,
						  always_use_location_entry_changed,
						  window, G_OBJECT (window));

	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER,
						  always_use_browser_changed,
						  window, G_OBJECT (window));
}

static void
always_use_location_entry_changed (gpointer callback_data)
{
	NautilusNavigationWindow *window;
	gboolean use_entry;

	window = NAUTILUS_NAVIGATION_WINDOW (callback_data);

	use_entry = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);

	if (use_entry) {
		nautilus_navigation_window_set_bar_mode (window, NAUTILUS_BAR_NAVIGATION);
	} else {
		nautilus_navigation_window_set_bar_mode (window, NAUTILUS_BAR_PATH);
	}

	g_signal_handlers_block_by_func (window->details->location_button,
					 G_CALLBACK (location_button_toggled_cb),
					 window);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->details->location_button), use_entry);
	g_signal_handlers_unblock_by_func (window->details->location_button,
					   G_CALLBACK (location_button_toggled_cb),
					   window);
}

static void
always_use_browser_changed (gpointer callback_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (callback_data);

	nautilus_navigation_window_update_spatial_menu_item (window);
}

static int
bookmark_list_get_uri_index (GList *list,
			     GFile *location)
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
path_bar_location_changed_callback (GtkWidget *widget,
				    GFile *location,
				    NautilusNavigationWindow *window)
{
	int i;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	/* check whether we already visited the target location */
	i = bookmark_list_get_uri_index (window->back_list, location);
	if (i >= 0) {
		nautilus_navigation_window_back_or_forward (window, TRUE, i);
	} else {
		nautilus_window_go_to (NAUTILUS_WINDOW (window), location);
	}
}

static void
hide_temporary_bars (NautilusNavigationWindow *window)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	if (window->details->temporary_location_bar) {
		if (nautilus_navigation_window_location_bar_showing (window)) {
			nautilus_navigation_window_hide_location_bar (window, FALSE);
		}
		window->details->temporary_location_bar = FALSE;
	}
	if (window->details->temporary_navigation_bar) {
		if (NAUTILUS_WINDOW (window)->details->search_mode) {
			nautilus_navigation_window_set_bar_mode (window, NAUTILUS_BAR_SEARCH);
		} else {
			if (!eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY)) {
				nautilus_navigation_window_set_bar_mode (window, NAUTILUS_BAR_PATH);
			}
		}
		window->details->temporary_navigation_bar = FALSE;
	}
	if (window->details->temporary_search_bar) {
		if (!eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY)) {
			nautilus_navigation_window_set_bar_mode (window, NAUTILUS_BAR_PATH);
		} else {
			nautilus_navigation_window_set_bar_mode (window, NAUTILUS_BAR_NAVIGATION);
		}
		window->details->temporary_search_bar = FALSE;
	}
}

static void
navigation_bar_location_changed_callback (GtkWidget *widget,
					  const char *uri,
					  NautilusNavigationWindow *window)
{
	GFile *location;
	
	hide_temporary_bars (window);
	location = g_file_new_for_uri (uri);
	nautilus_window_go_to (NAUTILUS_WINDOW (window), location);
	g_object_unref (location);
}

static void
navigation_bar_cancel_callback (GtkWidget *widget,
				NautilusNavigationWindow *window)
{
	hide_temporary_bars (window);
}

static void
side_pane_close_requested_callback (GtkWidget *widget,
				    gpointer user_data)
{
	NautilusNavigationWindow *window;
	
	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	nautilus_navigation_window_hide_sidebar (window);
}

static void
side_pane_size_allocate_callback (GtkWidget *widget,
				  GtkAllocation *allocation,
				  gpointer user_data)
{
	NautilusNavigationWindow *window;
	
	window = NAUTILUS_NAVIGATION_WINDOW (user_data);
	
	if (allocation->width != window->details->side_pane_width) {
		window->details->side_pane_width = allocation->width;
		if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_SIDEBAR_WIDTH)) {
			eel_preferences_set_integer
				(NAUTILUS_PREFERENCES_SIDEBAR_WIDTH, 
				 allocation->width <= 1 ? 0 : allocation->width);
		}
	}
}

static void
setup_side_pane_width (NautilusNavigationWindow *window)
{
	static gboolean setup_auto_value= TRUE;

	g_return_if_fail (window->sidebar != NULL);
	
	if (setup_auto_value) {
		setup_auto_value = FALSE;
		eel_preferences_add_auto_integer 
			(NAUTILUS_PREFERENCES_SIDEBAR_WIDTH,
			 &side_pane_width_auto_value);
	}

	window->details->side_pane_width = side_pane_width_auto_value;

	/* FIXME bugzilla.gnome.org 41245: Saved in pixels instead of in %? */
        /* FIXME bugzilla.gnome.org 41245: No reality check on the value? */
	
	gtk_paned_set_position (GTK_PANED (window->details->content_paned), 
				side_pane_width_auto_value);
}

static void
set_current_side_panel (NautilusNavigationWindow *window,
			NautilusSidebar *panel)
{
	if (window->details->current_side_panel) {
		nautilus_sidebar_is_visible_changed (window->details->current_side_panel,
						     FALSE);
		eel_remove_weak_pointer (&window->details->current_side_panel);
	}

	if (panel != NULL) {
		nautilus_sidebar_is_visible_changed (panel, TRUE);
	}
	window->details->current_side_panel = panel;
	eel_add_weak_pointer (&window->details->current_side_panel);
}

static void
side_pane_switch_page_callback (NautilusSidePane *side_pane,
				GtkWidget *widget,
				NautilusNavigationWindow *window)
{
	const char *id;
	NautilusSidebar *sidebar;

	sidebar = NAUTILUS_SIDEBAR (widget);

	if (sidebar == NULL) {
		return;
	}
		
	set_current_side_panel (window, sidebar);

	id = nautilus_sidebar_get_sidebar_id (sidebar);
	if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_SIDE_PANE_VIEW)) {
		eel_preferences_set (NAUTILUS_PREFERENCES_SIDE_PANE_VIEW, id);
	}
}

static void
nautilus_navigation_window_set_up_sidebar (NautilusNavigationWindow *window)
{
	window->sidebar = nautilus_side_pane_new ();

	gtk_paned_pack1 (GTK_PANED (window->details->content_paned),
			 GTK_WIDGET (window->sidebar),
			 FALSE, TRUE);

	setup_side_pane_width (window);
	g_signal_connect (window->sidebar, 
			  "size_allocate",
			  G_CALLBACK (side_pane_size_allocate_callback),
			  window);
	
	add_sidebar_panels (window);

	g_signal_connect (window->sidebar,
			  "close_requested",
			  G_CALLBACK (side_pane_close_requested_callback),
			  window);

	g_signal_connect (window->sidebar,
			  "switch_page",
			  G_CALLBACK (side_pane_switch_page_callback),
			  window);
	
	gtk_widget_show (GTK_WIDGET (window->sidebar));
}

static void
nautilus_navigation_window_tear_down_sidebar (NautilusNavigationWindow *window)
{
	GList *node, *next;
	NautilusSidebar *sidebar_panel;
	
	g_signal_handlers_disconnect_by_func (window->sidebar,
					      side_pane_switch_page_callback,
					      window);

	for (node = window->sidebar_panels; node != NULL; node = next) {
		next = node->next;

		sidebar_panel = NAUTILUS_SIDEBAR (node->data);
		
		nautilus_navigation_window_remove_sidebar_panel (window,
								 sidebar_panel);
        }

	gtk_widget_destroy (GTK_WIDGET (window->sidebar));
	window->sidebar = NULL;
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
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_MAXIMIZED,
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

static void
nautilus_navigation_window_destroy (GtkObject *object)
{
	NautilusNavigationWindow *window;
	
	window = NAUTILUS_NAVIGATION_WINDOW (object);

	window->sidebar = NULL;
	g_list_foreach (window->sidebar_panels, (GFunc)g_object_unref, NULL);
	g_list_free (window->sidebar_panels);
	window->sidebar_panels = NULL;

	window->view_as_combo_box = NULL;
	window->navigation_bar = NULL;
	window->path_bar = NULL;
	window->zoom_control = NULL;

	window->details->content_paned = NULL;

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
nautilus_navigation_window_finalize (GObject *object)
{
	NautilusNavigationWindow *window;
	
	window = NAUTILUS_NAVIGATION_WINDOW (object);

	nautilus_navigation_window_remove_go_menu_callback (window);
	nautilus_navigation_window_clear_back_list (window);
	nautilus_navigation_window_clear_forward_list (window);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*
 * Main API
 */

void
nautilus_navigation_window_add_sidebar_panel (NautilusNavigationWindow *window,
					      NautilusSidebar *sidebar_panel)
{
	const char *sidebar_id;
	char *label;
	char *tooltip;
	char *default_id;
	GdkPixbuf *icon;

	g_return_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (window));
	g_return_if_fail (NAUTILUS_IS_SIDEBAR (sidebar_panel));
	g_return_if_fail (NAUTILUS_IS_SIDE_PANE (window->sidebar));
	g_return_if_fail (g_list_find (window->sidebar_panels, sidebar_panel) == NULL);	

	label = nautilus_sidebar_get_tab_label (sidebar_panel);
	tooltip = nautilus_sidebar_get_tab_tooltip (sidebar_panel);
	nautilus_side_pane_add_panel (window->sidebar, 
				      GTK_WIDGET (sidebar_panel), 
				      label,
				      tooltip);
	g_free (tooltip);
	g_free (label);

	icon = nautilus_sidebar_get_tab_icon (sidebar_panel);
	nautilus_side_pane_set_panel_image (NAUTILUS_NAVIGATION_WINDOW (window)->sidebar,
					    GTK_WIDGET (sidebar_panel),
					    icon);
	if (icon) {
		g_object_unref (icon);
	}

	g_signal_connect (sidebar_panel, "tab_icon_changed",
			  (GCallback)side_panel_image_changed_callback, window);

	window->sidebar_panels = g_list_prepend (window->sidebar_panels,
						 g_object_ref (sidebar_panel));

	/* Show if default */
	sidebar_id = nautilus_sidebar_get_sidebar_id (sidebar_panel);
	default_id = eel_preferences_get (NAUTILUS_PREFERENCES_SIDE_PANE_VIEW);
	if (sidebar_id && default_id && !strcmp (sidebar_id, default_id)) {
		nautilus_side_pane_show_panel (window->sidebar,
					       GTK_WIDGET (sidebar_panel));
	}	
	g_free (default_id);
}

void
nautilus_navigation_window_remove_sidebar_panel (NautilusNavigationWindow *window,
						 NautilusSidebar *sidebar_panel)
{
	g_return_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (window));
	g_return_if_fail (NAUTILUS_IS_SIDEBAR (sidebar_panel));

	if (g_list_find (window->sidebar_panels, sidebar_panel) == NULL) {
		return;
	}

	g_signal_handlers_disconnect_by_func (sidebar_panel, side_panel_image_changed_callback, window);
	
	nautilus_side_pane_remove_panel (window->sidebar, 
					 GTK_WIDGET (sidebar_panel));
	window->sidebar_panels = g_list_remove (window->sidebar_panels, sidebar_panel);
	g_object_unref (sidebar_panel);
}

void
nautilus_navigation_window_go_back (NautilusNavigationWindow *window)
{
	nautilus_navigation_window_back_or_forward (window, TRUE, 0);
}

void
nautilus_navigation_window_go_forward (NautilusNavigationWindow *window)
{
	nautilus_navigation_window_back_or_forward (window, FALSE, 0);
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
activate_nth_short_list_item (NautilusWindow *window, guint index)
{
	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (index < g_list_length (window->details->short_list_viewers));

	nautilus_window_set_content_view (window, 
					  g_list_nth_data (window->details->short_list_viewers, index));
}

static void
activate_extra_viewer (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (window->details->extra_viewer != NULL);
	
	nautilus_window_set_content_view (window, window->details->extra_viewer);	
}

static void
view_as_menu_switch_views_callback (GtkComboBox *combo_box, NautilusWindow *window)
{         
	int active;
	g_assert (GTK_IS_COMBO_BOX (combo_box));
	g_assert (NAUTILUS_IS_WINDOW (window));

	active = gtk_combo_box_get_active (combo_box);

	if (active < 0) {
		return;
	} else if (active < GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo_box), "num viewers"))) {
		activate_nth_short_list_item (window, active);
	} else {
		activate_extra_viewer (window);
	}
}

static void
load_view_as_menu (NautilusWindow *window)
{
	GList *node;
	int index;
	int selected_index = -1;
	GtkTreeModel *model;
	GtkListStore *store;
	const NautilusViewInfo *info;
	GtkComboBox* combo_box;
	
    	combo_box = GTK_COMBO_BOX (NAUTILUS_NAVIGATION_WINDOW (window)->view_as_combo_box);
	/* Clear the contents of ComboBox in a wacky way because there
	 * is no function to clear all items and also no function to obtain
	 * the number of items in a combobox.
	 */
	model = gtk_combo_box_get_model (combo_box);
	g_return_if_fail (GTK_IS_LIST_STORE (model));
	store = GTK_LIST_STORE (model);
	gtk_list_store_clear (store);

        /* Add a menu item for each view in the preferred list for this location. */
        for (node = window->details->short_list_viewers, index = 0; 
             node != NULL; 
             node = node->next, ++index) {
		info = nautilus_view_factory_lookup (node->data);
		gtk_combo_box_append_text (combo_box, _(info->view_combo_label));

		if (nautilus_window_content_view_matches_iid (NAUTILUS_WINDOW (window), (char *)node->data)) {
			selected_index = index;
		}
        }
	g_object_set_data (G_OBJECT (combo_box), "num viewers", GINT_TO_POINTER (index));
	if (selected_index == -1) {
		const char *id;
		/* We're using an extra viewer, add a menu item for it */

		id = nautilus_window_get_content_view_id (window);
		info = nautilus_view_factory_lookup (id);
		gtk_combo_box_append_text (GTK_COMBO_BOX (NAUTILUS_NAVIGATION_WINDOW (window)->view_as_combo_box),
					   _(info->view_combo_label));
		selected_index = index;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (NAUTILUS_NAVIGATION_WINDOW (window)->view_as_combo_box), selected_index);
}

static void
real_load_view_as_menu (NautilusWindow *window)
{
	EEL_CALL_PARENT (NAUTILUS_WINDOW_CLASS,
			 load_view_as_menu, (window));

	load_view_as_menu (window);
}

static gboolean
real_set_title (NautilusWindow *window, const char *title)
{
	char *full_title;
	char *window_title;
	gboolean changed;

	changed = EEL_CALL_PARENT_WITH_RETURN_VALUE
		(NAUTILUS_WINDOW_CLASS, set_title, (window, title));

	if (changed) {
		full_title = g_strdup_printf (_("%s - File Browser"), title);

		window_title = eel_str_middle_truncate (full_title, MAX_TITLE_LENGTH);
		gtk_window_set_title (GTK_WINDOW (window), window_title);
		g_free (window_title);
		g_free (full_title);
	}

	return changed;
}

static NautilusIconInfo *
real_get_icon (NautilusWindow *window)
{
	return nautilus_icon_info_lookup_from_name ("file-manager", 48);
}

static void
zoom_level_changed_callback (NautilusView *view,
                             NautilusNavigationWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));
	
        /* This is called each time the component successfully completed
         * a zooming operation.
         */
        nautilus_zoom_control_set_zoom_level (NAUTILUS_ZOOM_CONTROL (window->zoom_control),
                                              nautilus_view_get_zoom_level (view));
}

static void
connect_view (NautilusNavigationWindow *window, NautilusView *view)
{
	g_signal_connect (view, "zoom_level_changed",
			  G_CALLBACK (zoom_level_changed_callback),
			  window);
}

static void
disconnect_view (NautilusNavigationWindow *window, NautilusView *view)
{
	if (!view) {
		return;
	}
	
	g_signal_handlers_disconnect_by_func
		(view, 
		 G_CALLBACK (zoom_level_changed_callback), 
		 window);	
}

static void
real_set_content_view_widget (NautilusWindow *nautilus_window,
			      NautilusView *new_view)
{
	
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (nautilus_window);

	disconnect_view (window, nautilus_window->content_view);

	EEL_CALL_PARENT (NAUTILUS_WINDOW_CLASS, 
			 set_content_view_widget, 
			 (nautilus_window, new_view));


	if (new_view == NULL) {
		return;	       
	}

	connect_view (window, new_view);

	gtk_container_add (GTK_CONTAINER (window->details->content_box),
			   GTK_WIDGET (new_view));

	if (new_view != NULL && nautilus_view_supports_zooming (new_view)) {
		gtk_widget_show (window->zoom_control);
	} else {
		gtk_widget_hide (window->zoom_control);
	}

        /* Update displayed view in menu. Only do this if we're not switching
         * locations though, because if we are switching locations we'll
         * install a whole new set of views in the menu later (the current
         * views in the menu are for the old location).
         */
	if (nautilus_window->details->pending_location == NULL) {
		load_view_as_menu (nautilus_window);
	}
}

static void
real_set_throbber_active (NautilusWindow *window, gboolean active)
{
	nautilus_navigation_window_set_throbber_active
		(NAUTILUS_NAVIGATION_WINDOW (window), active);
}

static void
nautilus_navigation_window_show_location_bar_temporarily (NautilusNavigationWindow *window)
{
	if (!nautilus_navigation_window_location_bar_showing (window)) {
		nautilus_navigation_window_show_location_bar (window, FALSE);
		window->details->temporary_location_bar = TRUE;
	}
}

static void
nautilus_navigation_window_show_navigation_bar_temporarily (NautilusNavigationWindow *window)
{
	if (nautilus_navigation_window_path_bar_showing (window)
	    || nautilus_navigation_window_search_bar_showing (window)) {
		nautilus_navigation_window_set_bar_mode (window, NAUTILUS_BAR_NAVIGATION);
		window->details->temporary_navigation_bar = TRUE;
	}
	nautilus_navigation_bar_activate 
		(NAUTILUS_NAVIGATION_BAR (window->navigation_bar));
}

static void
real_prompt_for_location (NautilusWindow *window, const char *initial)
{
	nautilus_navigation_window_show_location_bar_temporarily (NAUTILUS_NAVIGATION_WINDOW (window));
	nautilus_navigation_window_show_navigation_bar_temporarily (NAUTILUS_NAVIGATION_WINDOW (window));
	
	if (initial) {
		nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (NAUTILUS_NAVIGATION_WINDOW (window)->navigation_bar),
						      initial);
	}
}

static void
search_bar_activate_callback (NautilusSearchBar *bar,
			      NautilusWindow *window)
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

	query = nautilus_search_bar_get_query (NAUTILUS_SEARCH_BAR (NAUTILUS_NAVIGATION_WINDOW (window)->search_bar));
	if (query != NULL) {
		if (!nautilus_search_directory_is_indexed (search_directory)) {
			current_uri = nautilus_window_get_location_uri (window);
			nautilus_query_set_location (query, current_uri);
			g_free (current_uri);
		}
		nautilus_search_directory_set_query (search_directory, query);
		g_object_unref (query);
	}
	
	nautilus_window_go_to (window, location);
	
	nautilus_directory_unref (directory);
	g_object_unref (location);
}

static void
search_bar_cancel_callback (GtkWidget *widget,
			    NautilusNavigationWindow *window)
{
	hide_temporary_bars (window);
}

void 
nautilus_navigation_window_show_search (NautilusNavigationWindow *window)
{
	if (!nautilus_navigation_window_search_bar_showing (window)) {
		nautilus_navigation_window_show_location_bar_temporarily (window);
		nautilus_navigation_window_set_bar_mode (window, NAUTILUS_BAR_SEARCH);
		window->details->temporary_search_bar = TRUE;
		nautilus_search_bar_clear (NAUTILUS_SEARCH_BAR (window->search_bar));
	}
	
	nautilus_search_bar_grab_focus (NAUTILUS_SEARCH_BAR (window->search_bar));
}

static void
query_editor_changed_callback (NautilusSearchBar *bar,
			       NautilusQuery *query,
			       gboolean reload,
			       NautilusWindow *window)
{
	NautilusDirectory *directory;

	directory = nautilus_directory_get_for_file (window->details->viewed_file);
	g_assert (NAUTILUS_IS_SEARCH_DIRECTORY (directory));

	nautilus_search_directory_set_query (NAUTILUS_SEARCH_DIRECTORY (directory),
					     query);
	if (reload) {
		nautilus_window_reload (window);
	}

	nautilus_directory_unref (directory);
}

static void
real_set_search_mode (NautilusWindow *window, gboolean search_mode,
		      NautilusSearchDirectory *search_directory)
{
	NautilusNavigationWindow *nav_window;
	GtkWidget *query_editor;
	NautilusQuery *query;

	nav_window = NAUTILUS_NAVIGATION_WINDOW (window);

	if (!search_mode) {
		nav_window->details->temporary_search_bar = TRUE;
		hide_temporary_bars (nav_window);
		return;
	}

	if (nautilus_search_directory_is_saved_search (search_directory)) {
		query_editor = nautilus_query_editor_new (TRUE,
							  nautilus_search_directory_is_indexed (search_directory));
	} else {
		nautilus_navigation_window_show_location_bar_temporarily (nav_window);
		nautilus_navigation_window_set_bar_mode (nav_window, NAUTILUS_BAR_SEARCH);
		nav_window->details->temporary_search_bar = FALSE;

		query_editor = nautilus_query_editor_new_with_bar (FALSE,
								   nautilus_search_directory_is_indexed (search_directory),
								   NAUTILUS_SEARCH_BAR (nav_window->search_bar));
	}

	g_signal_connect_object (query_editor, "changed",
				 G_CALLBACK (query_editor_changed_callback), window, 0);
	
	query = nautilus_search_directory_get_query (search_directory);
	if (query != NULL) {
		nautilus_query_editor_set_query (NAUTILUS_QUERY_EDITOR (query_editor),
						 query);
		g_object_unref (query);
	}else {
		nautilus_query_editor_set_default_query (NAUTILUS_QUERY_EDITOR (query_editor));
	}
	
	nautilus_window_add_extra_location_widget (window, query_editor);
	gtk_widget_show (query_editor);
	nautilus_query_editor_grab_focus (NAUTILUS_QUERY_EDITOR (query_editor));
}

void
nautilus_navigation_window_clear_forward_list (NautilusNavigationWindow *window)
{
	eel_g_object_list_free (window->forward_list);
	window->forward_list = NULL;
}

void
nautilus_navigation_window_clear_back_list (NautilusNavigationWindow *window)
{
	eel_g_object_list_free (window->back_list);
	window->back_list = NULL;
}

static void
side_panel_image_changed_callback (NautilusSidebar *side_panel,
                                   gpointer callback_data)
{
        NautilusWindow *window;
	GdkPixbuf *icon;

        window = NAUTILUS_WINDOW (callback_data);

	icon = nautilus_sidebar_get_tab_icon (side_panel);
        nautilus_side_pane_set_panel_image (NAUTILUS_NAVIGATION_WINDOW (window)->sidebar,
                                            GTK_WIDGET (side_panel),
                                            icon);
	if (icon != NULL) {
		g_object_unref (icon);
	}
}

/**
 * add_sidebar_panels:
 * @window:	A NautilusNavigationWindow
 *
 * Adds all sidebars available
 *
 */
static void
add_sidebar_panels (NautilusNavigationWindow *window)
{
	GtkWidget *current;
	GList *providers;
	GList *p;
	NautilusSidebar *sidebar_panel;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	if (window->sidebar == NULL) {
		return;
	}

 	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_SIDEBAR_PROVIDER);
	
	for (p = providers; p != NULL; p = p->next) {
		NautilusSidebarProvider *provider;

		provider = NAUTILUS_SIDEBAR_PROVIDER (p->data);
		
		sidebar_panel = nautilus_sidebar_provider_create (provider,
								  NAUTILUS_WINDOW_INFO (window));
		nautilus_navigation_window_add_sidebar_panel (window,
							      sidebar_panel);
		
		g_object_unref (sidebar_panel);
	}

	nautilus_module_extension_list_free (providers);

	current = nautilus_side_pane_get_current_panel (window->sidebar);
	set_current_side_panel
		(window, 
		 NAUTILUS_SIDEBAR (current));
}

void 
nautilus_navigation_window_hide_location_bar (NautilusNavigationWindow *window, gboolean save_preference)
{
	window->details->temporary_location_bar = FALSE;
	gtk_widget_hide (window->details->location_bar);
	nautilus_navigation_window_update_show_hide_menu_items (window);
	if (save_preference &&
	    eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR, FALSE);
	}
}

void 
nautilus_navigation_window_show_location_bar (NautilusNavigationWindow *window, gboolean save_preference)
{
	gtk_widget_show (window->details->location_bar);
	nautilus_navigation_window_update_show_hide_menu_items (window);
	if (save_preference &&
	    eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR, TRUE);
	}
}

gboolean
nautilus_navigation_window_location_bar_showing (NautilusNavigationWindow *window)
{
	if (window->details->location_bar != NULL) {
		return GTK_WIDGET_VISIBLE (window->details->location_bar);
	}
	/* If we're not visible yet we haven't changed visibility, so its TRUE */
	return TRUE;
}

gboolean
nautilus_navigation_window_search_bar_showing (NautilusNavigationWindow *window)
{
	if (window->search_bar != NULL) {
		return GTK_WIDGET_VISIBLE (window->search_bar);
	}
	/* If we're not visible yet we haven't changed visibility, so its TRUE */
	return TRUE;
}

static void
nautilus_navigation_window_set_bar_mode (NautilusNavigationWindow *window,
					 NautilusBarMode mode)
{
	switch (mode) {

	case NAUTILUS_BAR_PATH:
		gtk_widget_show (window->path_bar);
		gtk_widget_hide (window->navigation_bar);
		gtk_widget_hide (window->search_bar);
		break;

	case NAUTILUS_BAR_NAVIGATION:
		gtk_widget_show (window->navigation_bar);
		gtk_widget_hide (window->path_bar);
		gtk_widget_hide (window->search_bar);
		break;

	case NAUTILUS_BAR_SEARCH:
		gtk_widget_show (window->search_bar);
		gtk_widget_hide (window->path_bar);
		gtk_widget_hide (window->navigation_bar);
		break;
	}
}

gboolean
nautilus_navigation_window_path_bar_showing (NautilusNavigationWindow *window)
{
	if (window->path_bar != NULL) {
		return GTK_WIDGET_VISIBLE (window->path_bar);
	}
	/* If we're not visible yet we haven't changed visibility, so its TRUE */
	return TRUE;
}

gboolean
nautilus_navigation_window_toolbar_showing (NautilusNavigationWindow *window)
{
	if (window->details->toolbar != NULL) {
		return GTK_WIDGET_VISIBLE (window->details->toolbar);
	}
	/* If we're not visible yet we haven't changed visibility, so its TRUE */
	return TRUE;
}

void 
nautilus_navigation_window_hide_status_bar (NautilusNavigationWindow *window)
{
	gtk_widget_hide (NAUTILUS_WINDOW (window)->details->statusbar);

	nautilus_navigation_window_update_show_hide_menu_items (window);
	if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR) &&
	    eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR, FALSE);
	}
}

void 
nautilus_navigation_window_show_status_bar (NautilusNavigationWindow *window)
{
	gtk_widget_show (NAUTILUS_WINDOW (window)->details->statusbar);

	nautilus_navigation_window_update_show_hide_menu_items (window);
	if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR) &&
	    !eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR, TRUE);
	}
}

gboolean
nautilus_navigation_window_status_bar_showing (NautilusNavigationWindow *window)
{
	if (NAUTILUS_WINDOW (window)->details->statusbar != NULL) {
		return GTK_WIDGET_VISIBLE (NAUTILUS_WINDOW (window)->details->statusbar);
	}
	/* If we're not visible yet we haven't changed visibility, so its TRUE */
	return TRUE;
}


void 
nautilus_navigation_window_hide_toolbar (NautilusNavigationWindow *window)
{
	gtk_widget_hide (window->details->toolbar);
	nautilus_navigation_window_update_show_hide_menu_items (window);
	if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_TOOLBAR) &&
	    eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_TOOLBAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_TOOLBAR, FALSE);
	}
}

void 
nautilus_navigation_window_show_toolbar (NautilusNavigationWindow *window)
{
	gtk_widget_show (window->details->toolbar);
	nautilus_navigation_window_update_show_hide_menu_items (window);
	if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_TOOLBAR) &&
	    !eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_TOOLBAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_TOOLBAR, TRUE);
	}
}

void 
nautilus_navigation_window_hide_sidebar (NautilusNavigationWindow *window)
{
	if (window->sidebar == NULL) {
		return;
	}

	nautilus_navigation_window_tear_down_sidebar (window);
	nautilus_navigation_window_update_show_hide_menu_items (window);

	if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_SIDEBAR) &&
	    eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_SIDEBAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_SIDEBAR, FALSE);
	}
}

void 
nautilus_navigation_window_show_sidebar (NautilusNavigationWindow *window)
{
	if (window->sidebar != NULL) {
		return;
	}

	nautilus_navigation_window_set_up_sidebar (window);
	nautilus_navigation_window_update_show_hide_menu_items (window);
	if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_SIDEBAR) &&
	    !eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_SIDEBAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_SIDEBAR, TRUE);
	}
}

gboolean
nautilus_navigation_window_sidebar_showing (NautilusNavigationWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (window), FALSE);

	return (window->sidebar != NULL)
		&& nautilus_horizontal_splitter_is_hidden (NAUTILUS_HORIZONTAL_SPLITTER (window->details->content_paned));
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
	gint forward_count;
	
	forward_count = g_list_length (window->forward_list); 

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
 * @widget:	GtkWidget
 *
 * Call parent and then show/hide window items
 * base on user prefs.
 */
static void
nautilus_navigation_window_show (GtkWidget *widget)
{	
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (widget);

	/* Initially show or hide views based on preferences; once the window is displayed
	 * these can be controlled on a per-window basis from View menu items. 
	 */

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_TOOLBAR)) {
		nautilus_navigation_window_show_toolbar (window);
	} else {
		nautilus_navigation_window_hide_toolbar (window);
	}

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR)) {
		nautilus_navigation_window_show_location_bar (window, FALSE);
	} else {
		nautilus_navigation_window_hide_location_bar (window, FALSE);
	}

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY)) {
		nautilus_navigation_window_set_bar_mode (window, NAUTILUS_BAR_NAVIGATION);
	} else {
		nautilus_navigation_window_set_bar_mode (window, NAUTILUS_BAR_PATH);
	}
	
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_SIDEBAR)) {
		nautilus_navigation_window_show_sidebar (window);
	} else {
		nautilus_navigation_window_hide_sidebar (window);
	}

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR)) {
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

	if (GTK_WIDGET(window)->window) {
		geometry_string = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));
		is_maximized = gdk_window_get_state (GTK_WIDGET (window)->window)
				& GDK_WINDOW_STATE_MAXIMIZED;
		
		if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_SAVED_GEOMETRY) &&
		    !is_maximized) {
			eel_preferences_set
				(NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_SAVED_GEOMETRY, 
				 geometry_string);
		}
		g_free (geometry_string);

		if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_MAXIMIZED)) {
			eel_preferences_set_boolean
				(NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_MAXIMIZED, 
				 is_maximized);
		}
	}
}



static void
real_window_close (NautilusWindow *window)
{
	nautilus_navigation_window_save_geometry (NAUTILUS_NAVIGATION_WINDOW (window));
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

static void
nautilus_navigation_window_class_init (NautilusNavigationWindowClass *class)
{
	NAUTILUS_WINDOW_CLASS (class)->window_type = NAUTILUS_WINDOW_NAVIGATION;
	NAUTILUS_WINDOW_CLASS (class)->bookmarks_placeholder = MENU_PATH_BOOKMARKS_PLACEHOLDER;
	
	G_OBJECT_CLASS (class)->finalize = nautilus_navigation_window_finalize;
	GTK_OBJECT_CLASS (class)->destroy = nautilus_navigation_window_destroy;
	GTK_WIDGET_CLASS (class)->show = nautilus_navigation_window_show;
	GTK_WIDGET_CLASS (class)->unrealize = nautilus_navigation_window_unrealize;
	GTK_WIDGET_CLASS (class)->window_state_event = nautilus_navigation_window_state_event;
	GTK_WIDGET_CLASS (class)->key_press_event = nautilus_navigation_window_key_press_event;
	NAUTILUS_WINDOW_CLASS (class)->load_view_as_menu = real_load_view_as_menu;
	NAUTILUS_WINDOW_CLASS (class)->set_content_view_widget = real_set_content_view_widget;
	NAUTILUS_WINDOW_CLASS (class)->set_throbber_active = real_set_throbber_active;
	NAUTILUS_WINDOW_CLASS (class)->prompt_for_location = real_prompt_for_location;
	NAUTILUS_WINDOW_CLASS (class)->set_search_mode = real_set_search_mode;
	NAUTILUS_WINDOW_CLASS (class)->set_title = real_set_title;
	NAUTILUS_WINDOW_CLASS (class)->get_icon = real_get_icon;
	NAUTILUS_WINDOW_CLASS (class)->get_default_size = real_get_default_size;
	NAUTILUS_WINDOW_CLASS (class)->close = real_window_close;

	g_type_class_add_private (G_OBJECT_CLASS (class), sizeof (NautilusNavigationWindowDetails));
}
