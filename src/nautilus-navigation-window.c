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

#include "nautilus-application.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-main.h"
#include "nautilus-signaller.h"
#include "nautilus-location-bar.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-zoom-control.h"
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-property-bag-client.h>
#include <bonobo/bonobo-ui-util.h>
#include <eel/eel-accessibility.h>
#include <eel/eel-debug.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/gnome-window-icon.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-drag-window.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-horizontal-splitter.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-sidebar.h>
#include <libnautilus-private/nautilus-theme.h>
#include <libnautilus-private/nautilus-view-factory.h>
#include <libnautilus-private/nautilus-bonobo-ui.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-undo.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-sidebar-provider.h>
#include <math.h>
#include <sys/time.h>

/* FIXME bugzilla.gnome.org 41243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

/* FIXME bugzilla.gnome.org 41245: hardwired sizes */
#define SIDE_PANE_MINIMUM_WIDTH 1
#define SIDE_PANE_MINIMUM_HEIGHT 400

#define MAX_TITLE_LENGTH 180

/* dock items */
#define LOCATION_BAR_PATH	"/Location Bar"
#define TOOLBAR_PATH            "/Toolbar"
#define MENU_BAR_PATH           "/menu"

enum {
	ARG_0,
	ARG_APP_ID,
	ARG_APP
};

static int side_pane_width_auto_value = SIDE_PANE_MINIMUM_WIDTH;

static void add_sidebar_panels                (NautilusNavigationWindow *window);
static void load_view_as_menu                 (NautilusWindow           *window);
static void side_panel_image_changed_callback (NautilusSidebar          *side_panel,
					       gpointer                  callback_data);

GNOME_CLASS_BOILERPLATE (NautilusNavigationWindow, nautilus_navigation_window,
			 NautilusWindow, NAUTILUS_TYPE_WINDOW)

static void
nautilus_navigation_window_instance_init (NautilusNavigationWindow *window)
{
	window->details = g_new0 (NautilusNavigationWindowDetails, 1);

	window->details->tooltips = gtk_tooltips_new ();
	g_object_ref (G_OBJECT (window->details->tooltips));
	gtk_object_sink (GTK_OBJECT (window->details->tooltips));
	
	window->details->content_paned = nautilus_horizontal_splitter_new ();
	gtk_widget_show (window->details->content_paned);
	bonobo_window_set_contents (BONOBO_WINDOW (window), window->details->content_paned);
}

static void
file_menu_new_window_callback (BonoboUIComponent *component,
			       gpointer user_data,
			       const char *verb)
{
	NautilusWindow *window = NAUTILUS_WINDOW (user_data);

	const gchar    *uri = nautilus_window_get_location (window);

	window = nautilus_application_create_navigation_window (window->application,
						gtk_window_get_screen (GTK_WINDOW (window)));

	nautilus_window_open_location (window, uri, FALSE);
}

static void
navigation_bar_location_changed_callback (GtkWidget *widget,
					  const char *uri,
					  NautilusNavigationWindow *window)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	if (window->details->temporary_navigation_bar) {
		if (nautilus_navigation_window_location_bar_showing (window)) {
			nautilus_navigation_window_hide_location_bar (window, FALSE);
		}
		window->details->temporary_navigation_bar = FALSE;
	}

	nautilus_window_go_to (NAUTILUS_WINDOW (window), uri);
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
				 allocation->width);
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

	if (window->details->throbber_property_bag != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (window->details->throbber_property_bag, NULL);
		window->details->throbber_property_bag = CORBA_OBJECT_NIL;
	}

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
nautilus_navigation_window_destroy (GtkObject *object)
{
	NautilusNavigationWindow *window;
	
	window = NAUTILUS_NAVIGATION_WINDOW (object);

	window->sidebar = NULL;
	g_list_foreach (window->sidebar_panels, (GFunc)g_object_unref, NULL);
	window->sidebar_panels = NULL;

	window->view_as_option_menu = NULL;
	window->navigation_bar = NULL;
	window->zoom_control = NULL;

	window->details->content_paned = NULL;

	if (window->details->tooltips) {
		g_object_unref (G_OBJECT (window->details->tooltips));
		window->details->tooltips = NULL;
	}

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
nautilus_navigation_window_finalize (GObject *object)
{
	NautilusNavigationWindow *window;
	
	window = NAUTILUS_NAVIGATION_WINDOW (object);

	nautilus_navigation_window_remove_bookmarks_menu_callback (window);
	nautilus_navigation_window_clear_back_list (window);
	nautilus_navigation_window_clear_forward_list (window);

 	g_free (window->details);

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
	char *default_id;
	GdkPixbuf *icon;

	g_return_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (window));
	g_return_if_fail (NAUTILUS_IS_SIDEBAR (sidebar_panel));
	g_return_if_fail (NAUTILUS_IS_SIDE_PANE (window->sidebar));
	g_return_if_fail (g_list_find (window->sidebar_panels, sidebar_panel) == NULL);	

	label = nautilus_sidebar_get_tab_label (sidebar_panel);
	nautilus_side_pane_add_panel (window->sidebar, 
				      GTK_WIDGET (sidebar_panel), 
				      label);
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

	
	g_object_ref (sidebar_panel);
	window->sidebar_panels = g_list_prepend (window->sidebar_panels, sidebar_panel);


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
nautilus_navigation_window_go_home (NautilusNavigationWindow *window)
{
	char *home_uri;

	home_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
	
	g_assert (home_uri != NULL);
	nautilus_window_go_to (NAUTILUS_WINDOW (window), home_uri);
	g_free (home_uri);
}

void
nautilus_navigation_window_allow_back (NautilusNavigationWindow *window, gboolean allow)
{
	nautilus_window_ui_freeze (NAUTILUS_WINDOW (window));

	nautilus_bonobo_set_sensitive (NAUTILUS_WINDOW (window)->details->shell_ui,
				       NAUTILUS_COMMAND_BACK, allow);
	/* Have to handle non-standard Back button explicitly (it's
	 * non-standard to support right-click menu).
	 */
	gtk_widget_set_sensitive 
		(GTK_WIDGET (window->details->back_button_item), allow);

	nautilus_window_ui_thaw (NAUTILUS_WINDOW (window));
}

void
nautilus_navigation_window_allow_forward (NautilusNavigationWindow *window, gboolean allow)
{
	nautilus_window_ui_freeze (NAUTILUS_WINDOW (window));

	nautilus_bonobo_set_sensitive (NAUTILUS_WINDOW (window)->details->shell_ui,
				       NAUTILUS_COMMAND_FORWARD, allow);

	/* Have to handle non-standard Forward button explicitly (it's
	 * non-standard to support right-click menu).
	 */
	gtk_widget_set_sensitive 
		(GTK_WIDGET (window->details->forward_button_item), allow);

	nautilus_window_ui_thaw (NAUTILUS_WINDOW (window));
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
view_as_menu_switch_views_callback (GtkWidget *widget, gpointer data)
{
        NautilusWindow *window;
        int viewer_index;
        
        g_assert (GTK_IS_MENU_ITEM (widget));
        g_assert (NAUTILUS_IS_WINDOW (data));

        window = NAUTILUS_WINDOW (data);

        if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "extra viewer")) == TRUE) {
        	activate_extra_viewer (window);
        } else {
		viewer_index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "viewer index"));
		activate_nth_short_list_item (window, viewer_index);
        }
}

static GtkWidget *
create_view_as_menu_item (NautilusWindow *window, 
			  const char *identifier,
			  guint index)
{
	GtkWidget *menu_item;
        char *menu_label;
	const NautilusViewInfo *info;

	info = nautilus_view_factory_lookup (identifier);

	/* BONOBOTODO: nicer way for the labels */
	menu_label = g_strdup_printf (_("View as %s"), _(info->label));
	menu_item = gtk_menu_item_new_with_mnemonic (menu_label);
	g_free (menu_label);

	g_signal_connect_object (menu_item, "activate",
				 G_CALLBACK (view_as_menu_switch_views_callback),
				 window, 0);

	g_object_set_data (G_OBJECT (menu_item), "viewer index", GINT_TO_POINTER (index));

	gtk_widget_show (menu_item);

	return menu_item;
}

static void
load_view_as_menu (NautilusWindow *window)
{
	GtkWidget *new_menu;
	GtkWidget *menu_item;
	GList *node;
	int index;
	int selected_index = -1;

        new_menu = gtk_menu_new ();
	
        /* Add a menu item for each view in the preferred list for this location. */
        for (node = window->details->short_list_viewers, index = 0; 
             node != NULL; 
             node = node->next, ++index) {
        	/* Menu item in option menu. This doesn't use Bonobo, for various
        	 * historical and technical reasons.
        	 */
                menu_item = create_view_as_menu_item (window, node->data, index);
                gtk_menu_shell_append (GTK_MENU_SHELL (new_menu), menu_item);

		if (nautilus_window_content_view_matches_iid (NAUTILUS_WINDOW (window), (char *)node->data)) {
			selected_index = index;
		}
        }

	if (selected_index == -1) {
		const char *id;
		/* We're using an extra viewer, add a menu item for it */

		id = nautilus_window_get_content_view_id (window);
                menu_item = create_view_as_menu_item (window, id, index);
		
                gtk_menu_shell_append (GTK_MENU_SHELL (new_menu), menu_item);
		selected_index = index;
	}

        /* We create and attach a new menu here because adding/removing
         * items from existing menu screws up the size of the option menu.
         */
        gtk_option_menu_set_menu (GTK_OPTION_MENU (NAUTILUS_NAVIGATION_WINDOW (window)->view_as_option_menu),
                                  new_menu);

	gtk_option_menu_set_history (GTK_OPTION_MENU (NAUTILUS_NAVIGATION_WINDOW (window)->view_as_option_menu), selected_index);
}

static void
real_load_view_as_menu (NautilusWindow *window)
{
	EEL_CALL_PARENT (NAUTILUS_WINDOW_CLASS,
			 load_view_as_menu, (window));

	load_view_as_menu (window);
}

static void
real_set_title (NautilusWindow *window, const char *title)
{
	char *full_title;
	char *window_title;
	
	EEL_CALL_PARENT (NAUTILUS_WINDOW_CLASS,
			 set_title, (window, title));
	
	full_title = g_strdup_printf (_("File Browser: %s"), title);

	window_title = eel_str_middle_truncate (full_title, MAX_TITLE_LENGTH);
	gtk_window_set_title (GTK_WINDOW (window), window_title);
	g_free (window_title);
	g_free (full_title);
}

static void
real_merge_menus (NautilusWindow *nautilus_window)
{
	NautilusNavigationWindow *window;
	GtkWidget *location_bar_box;
	GtkWidget *view_as_menu_vbox;
	BonoboControl *location_bar_wrapper;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("New Window", file_menu_new_window_callback),
		BONOBO_UI_VERB_END
	};

	EEL_CALL_PARENT (NAUTILUS_WINDOW_CLASS, 
			 merge_menus, (nautilus_window));

	window = NAUTILUS_NAVIGATION_WINDOW (nautilus_window);
	
	bonobo_ui_util_set_ui (NAUTILUS_WINDOW (window)->details->shell_ui,
			       DATADIR,
			       "nautilus-navigation-window-ui.xml",
			       "nautilus", NULL);

	bonobo_ui_component_freeze 
		(NAUTILUS_WINDOW (window)->details->shell_ui, NULL);

	bonobo_ui_component_add_verb_list_with_data (nautilus_window->details->shell_ui,
						     verbs, window);

	nautilus_navigation_window_initialize_menus_part_1 (window);
	nautilus_navigation_window_initialize_toolbars (window);

	/* Set initial sensitivity of some buttons & menu items 
	 * now that they're all created.
	 */
	nautilus_navigation_window_allow_back (window, FALSE);
	nautilus_navigation_window_allow_forward (window, FALSE);

	/* set up location bar */
	location_bar_box = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (location_bar_box), GNOME_PAD_SMALL);
	
	window->navigation_bar = nautilus_location_bar_new (window);
	gtk_widget_show (GTK_WIDGET (window->navigation_bar));

	g_signal_connect_object (window->navigation_bar, "location_changed",
				 G_CALLBACK (navigation_bar_location_changed_callback), window, 0);

	gtk_box_pack_start (GTK_BOX (location_bar_box), window->navigation_bar,
			    TRUE, TRUE, GNOME_PAD_SMALL);

	/* Option menu for content view types; it's empty here, filled in when a uri is set.
	 * Pack it into vbox so it doesn't grow vertically when location bar does. 
	 */
	view_as_menu_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_widget_show (view_as_menu_vbox);
	gtk_box_pack_end (GTK_BOX (location_bar_box), view_as_menu_vbox, FALSE, FALSE, 0);
	
	window->view_as_option_menu = gtk_option_menu_new ();
	gtk_box_pack_end (GTK_BOX (view_as_menu_vbox), window->view_as_option_menu, TRUE, FALSE, 0);
	gtk_widget_show (window->view_as_option_menu);

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
	gtk_box_pack_end (GTK_BOX (location_bar_box), window->zoom_control, FALSE, FALSE, 0);

	gtk_widget_show (location_bar_box);

	/* Wrap the location bar in a control and set it up. */
	location_bar_wrapper = bonobo_control_new (location_bar_box);
	bonobo_ui_component_object_set (NAUTILUS_WINDOW (window)->details->shell_ui,
					"/Location Bar/Wrapper",
					BONOBO_OBJREF (location_bar_wrapper),
					NULL);

	bonobo_object_unref (location_bar_wrapper);

	bonobo_ui_component_thaw (NAUTILUS_WINDOW (window)->details->shell_ui,
				  NULL);
}

static void
real_merge_menus_2 (NautilusWindow *nautilus_window)
{
	NautilusNavigationWindow *window;

	EEL_CALL_PARENT (NAUTILUS_WINDOW_CLASS, 
			 merge_menus_2, (nautilus_window));

	window = NAUTILUS_NAVIGATION_WINDOW (nautilus_window);
	
	nautilus_navigation_window_initialize_menus_part_2 (window);
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

	nautilus_horizontal_splitter_pack2 (
		NAUTILUS_HORIZONTAL_SPLITTER (window->details->content_paned),
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
nautilus_navigation_window_show_location_bar_temporarily (NautilusNavigationWindow *window, 
							  gboolean in_search_mode)
{
	if (!nautilus_navigation_window_location_bar_showing (window)) {
		nautilus_navigation_window_show_location_bar (window, FALSE);
		window->details->temporary_navigation_bar = TRUE;
	}
	nautilus_navigation_bar_activate 
		(NAUTILUS_NAVIGATION_BAR (window->navigation_bar));
}

static void
real_prompt_for_location (NautilusWindow *window)
{
	if (!window->details->updating_bonobo_state) {
		nautilus_navigation_window_show_location_bar_temporarily (NAUTILUS_NAVIGATION_WINDOW (window), FALSE);
	}
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
	NautilusSidebarProvider *provider;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	if (window->sidebar == NULL) {
		return;
	}

 	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_SIDEBAR_PROVIDER);
	
	for (p = providers; p != NULL; p = p->next) {
		provider = NAUTILUS_SIDEBAR_PROVIDER (p->data);
		
		sidebar_panel = nautilus_sidebar_provider_create (provider,
								  NAUTILUS_WINDOW_INFO (window));
		nautilus_navigation_window_add_sidebar_panel (window,
							      sidebar_panel);
		
		g_object_unref (sidebar_panel);
	}
	
	current = nautilus_side_pane_get_current_panel (window->sidebar);
	set_current_side_panel
		(window, 
		 NAUTILUS_SIDEBAR (current));
}

static void 
show_dock_item (NautilusNavigationWindow *window, const char *dock_item_path)
{
	nautilus_window_ui_freeze (NAUTILUS_WINDOW (window));

	nautilus_bonobo_set_hidden (NAUTILUS_WINDOW (window)->details->shell_ui,
				    dock_item_path,
				    FALSE);
	nautilus_navigation_window_update_show_hide_menu_items (window);

	nautilus_window_ui_thaw (NAUTILUS_WINDOW (window));
}

static void 
hide_dock_item (NautilusNavigationWindow *window, const char *dock_item_path)
{
	nautilus_window_ui_freeze (NAUTILUS_WINDOW (window));

	nautilus_bonobo_set_hidden (NAUTILUS_WINDOW (window)->details->shell_ui,
				    dock_item_path,
				    TRUE);
	nautilus_navigation_window_update_show_hide_menu_items (window);

	nautilus_window_ui_thaw (NAUTILUS_WINDOW (window));
}

static gboolean
dock_item_showing (NautilusNavigationWindow *window, const char *dock_item_path)
{
	return !nautilus_bonobo_get_hidden (NAUTILUS_WINDOW (window)->details->shell_ui,
					    dock_item_path);
}

void 
nautilus_navigation_window_hide_location_bar (NautilusNavigationWindow *window, gboolean save_preference)
{
	window->details->temporary_navigation_bar = FALSE;
	hide_dock_item (window, LOCATION_BAR_PATH);
	if (save_preference &&
	    eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR, FALSE);
	}
}

void 
nautilus_navigation_window_show_location_bar (NautilusNavigationWindow *window, gboolean save_preference)
{
	show_dock_item (window, LOCATION_BAR_PATH);
	if (save_preference &&
	    eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR, TRUE);
	}
}

gboolean
nautilus_navigation_window_location_bar_showing (NautilusNavigationWindow *window)
{
	return dock_item_showing (window, LOCATION_BAR_PATH);
}

gboolean
nautilus_navigation_window_toolbar_showing (NautilusNavigationWindow *window)
{
	return dock_item_showing (window, TOOLBAR_PATH);
}

void 
nautilus_navigation_window_hide_sidebar (NautilusNavigationWindow *window)
{
	if (NAUTILUS_IS_DESKTOP_WINDOW (window) || window->sidebar == NULL) {
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
	if (NAUTILUS_IS_DESKTOP_WINDOW (window) || window->sidebar != NULL) {
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
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR)) {
		nautilus_navigation_window_show_location_bar (window, FALSE);
	} else {
		nautilus_navigation_window_hide_location_bar (window, FALSE);
	}

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_SIDEBAR)) {
		nautilus_navigation_window_show_sidebar (window);
	} else {
		nautilus_navigation_window_hide_sidebar (window);
	}

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR)) {
		nautilus_window_show_status_bar (NAUTILUS_WINDOW (window));
	} else {
		nautilus_window_hide_status_bar (NAUTILUS_WINDOW (window));
	}

	GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static void
nautilus_navigation_window_save_geometry (NautilusNavigationWindow *window)
{
	char *geometry_string;

	g_assert (NAUTILUS_IS_WINDOW (window));

	if (GTK_WIDGET(window)->window &&
	    !(gdk_window_get_state (GTK_WIDGET(window)->window) & GDK_WINDOW_STATE_MAXIMIZED)) {
		geometry_string = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));
		
		if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_SAVED_GEOMETRY)) {
			eel_preferences_set
				(NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_SAVED_GEOMETRY, 
				 geometry_string);
		}
		g_free (geometry_string);
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

	G_OBJECT_CLASS (class)->finalize = nautilus_navigation_window_finalize;
	GTK_OBJECT_CLASS (class)->destroy = nautilus_navigation_window_destroy;
	GTK_WIDGET_CLASS (class)->show = nautilus_navigation_window_show;
	GTK_WIDGET_CLASS (class)->unrealize = nautilus_navigation_window_unrealize;
	NAUTILUS_WINDOW_CLASS (class)->merge_menus = real_merge_menus;
	NAUTILUS_WINDOW_CLASS (class)->merge_menus_2 = real_merge_menus_2;
	NAUTILUS_WINDOW_CLASS (class)->load_view_as_menu = real_load_view_as_menu;
	NAUTILUS_WINDOW_CLASS (class)->set_content_view_widget = real_set_content_view_widget;
	NAUTILUS_WINDOW_CLASS (class)->set_throbber_active = real_set_throbber_active;
	NAUTILUS_WINDOW_CLASS (class)->prompt_for_location = real_prompt_for_location;
	NAUTILUS_WINDOW_CLASS (class)->set_title = real_set_title;
	NAUTILUS_WINDOW_CLASS(class)->get_default_size = real_get_default_size;
	NAUTILUS_WINDOW_CLASS (class)->close = real_window_close;
}
