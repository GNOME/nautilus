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
#include "nautilus-information-panel.h"
#include "nautilus-main.h"
#include "nautilus-signaller.h"
#include "nautilus-switchable-navigation-bar.h"
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
#include <libnautilus-private/nautilus-sidebar-functions.h>
#include <libnautilus-private/nautilus-theme.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus/nautilus-clipboard.h>
#include <libnautilus/nautilus-undo.h>
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

#define NAUTILUS_COMMAND_TOGGLE_FIND_MODE	"/commands/Toggle Find Mode"

#define COMMAND_PATH_TOGGLE_FIND_MODE                   "/commands/Find"
#define COMMAND_PATH_TOGGLE_FIND_MODE_WITH_STATE        "/commands/Toggle Find Mode"

enum {
	ARG_0,
	ARG_APP_ID,
	ARG_APP
};

static int side_pane_width_auto_value = SIDE_PANE_MINIMUM_WIDTH;

static void add_sidebar_panels                  (NautilusNavigationWindow *window);
static void load_view_as_menu                   (NautilusWindow *window);
static void side_panel_view_loaded_callback (NautilusViewFrame *view,
					     gpointer user_data);
static void side_panel_view_failed_callback (NautilusViewFrame *view,
					     gpointer user_data);

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
go_to_callback (GtkWidget *widget,
		const char *uri,
		NautilusNavigationWindow *window)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	nautilus_window_go_to (NAUTILUS_WINDOW (window), uri);
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
navigation_bar_mode_changed_callback (GtkWidget *widget,
				      NautilusSwitchableNavigationBarMode mode,
				      NautilusNavigationWindow *window)
{
	NAUTILUS_WINDOW (window)->details->updating_bonobo_state = TRUE;

	g_assert (mode == NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION 
		  || mode == NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH);

	nautilus_window_ui_freeze (NAUTILUS_WINDOW (window));

	nautilus_bonobo_set_toggle_state (NAUTILUS_WINDOW (window)->details->shell_ui,
					  NAUTILUS_COMMAND_TOGGLE_FIND_MODE,
					  mode == NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH);
	
	NAUTILUS_WINDOW (window)->details->updating_bonobo_state = FALSE;

	nautilus_window_ui_thaw (NAUTILUS_WINDOW (window));
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
side_panel_set_open (GtkWidget *view,
		     gboolean open)
{
	CORBA_Environment ev;
	Bonobo_PropertyBag property_bag;
	Bonobo_Control control;
	
	if (!view || !NAUTILUS_IS_VIEW_FRAME (view)) {
		return;
	}

	control = nautilus_view_frame_get_control (NAUTILUS_VIEW_FRAME (view));
	
	if (control != CORBA_OBJECT_NIL) {
		CORBA_exception_init (&ev);
		property_bag = Bonobo_Control_getProperties (control, &ev);
		if (!BONOBO_EX (&ev) && property_bag != CORBA_OBJECT_NIL) {
			/* For some reason this was implemented as 'close'
			 * before, but open seems more natural */
			bonobo_property_bag_client_set_value_gboolean
				(property_bag, "close", !open, &ev);
			bonobo_object_release_unref (property_bag, NULL);
		}
	}
}

static void
set_current_side_panel (NautilusNavigationWindow *window,
			GtkWidget *panel)
{
	if (window->details->current_side_panel) {
		side_panel_set_open (window->details->current_side_panel, 
				     FALSE);
		eel_remove_weak_pointer (&window->details->current_side_panel);
	}

	side_panel_set_open (panel, TRUE);	
	window->details->current_side_panel = panel;
	eel_add_weak_pointer (&window->details->current_side_panel);
}

static void
side_pane_switch_page_callback (NautilusSidePane *side_pane,
				GtkWidget *panel,
				NautilusNavigationWindow *window)
{
	const char *view_iid;

	set_current_side_panel (window, panel);

	if (NAUTILUS_IS_VIEW_FRAME (panel)) {
		view_iid = nautilus_view_frame_get_view_iid (NAUTILUS_VIEW_FRAME (panel));
		if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_SIDE_PANE_VIEW)) {
			eel_preferences_set (NAUTILUS_PREFERENCES_SIDE_PANE_VIEW,
					     view_iid);
		}
		
	} else {
		if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_SIDE_PANE_VIEW)) {
			eel_preferences_set (NAUTILUS_PREFERENCES_SIDE_PANE_VIEW, "");
		}
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
	
	window->information_panel = nautilus_information_panel_new ();
	
	if (NAUTILUS_WINDOW (window)->details->location != NULL &&
	    NAUTILUS_WINDOW (window)->details->title != NULL) {
		nautilus_information_panel_set_uri (window->information_panel,
						    NAUTILUS_WINDOW (window)->details->location,
						    NAUTILUS_WINDOW (window)->details->title);
	}

	g_signal_connect_object (window->information_panel, "location_changed",
				 G_CALLBACK (go_to_callback), window, 0);

	/* Set up the sidebar panels. */
	nautilus_side_pane_add_panel (NAUTILUS_SIDE_PANE (window->sidebar), 
				      GTK_WIDGET (window->information_panel),
				      _("Information"));

	add_sidebar_panels (window);

	g_signal_connect (window->sidebar,
			  "close_requested",
			  G_CALLBACK (side_pane_close_requested_callback),
			  window);

	g_signal_connect (window->sidebar,
			  "switch_page",
			  G_CALLBACK (side_pane_switch_page_callback),
			  window);
	
	gtk_widget_show (GTK_WIDGET (window->information_panel));
	
	gtk_widget_show (GTK_WIDGET (window->sidebar));
}

static void
nautilus_navigation_window_tear_down_sidebar (NautilusNavigationWindow *window)
{
	g_signal_handlers_disconnect_by_func (window->sidebar,
					      side_pane_switch_page_callback,
					      window);
	
	nautilus_navigation_window_set_sidebar_panels (window, NULL);
	gtk_widget_destroy (GTK_WIDGET (window->sidebar));
	window->sidebar = NULL;
	window->information_panel = NULL;
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
	eel_g_object_list_free (window->sidebar_panels);
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
					      NautilusViewFrame *sidebar_panel)
{
	char *label;
	const char *view_iid;
	char *default_iid;

	g_return_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (window));
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (sidebar_panel));
	g_return_if_fail (NAUTILUS_IS_SIDE_PANE (window->sidebar));
	g_return_if_fail (g_list_find (window->sidebar_panels, sidebar_panel) == NULL);	

	label = nautilus_view_frame_get_label (sidebar_panel);
	
	nautilus_side_pane_add_panel (window->sidebar, 
				      GTK_WIDGET (sidebar_panel), 
				      label);
	g_free (label);

	g_object_ref (sidebar_panel);
	window->sidebar_panels = g_list_prepend (window->sidebar_panels, sidebar_panel);

	view_iid = nautilus_view_frame_get_view_iid (sidebar_panel);
	default_iid = eel_preferences_get (NAUTILUS_PREFERENCES_SIDE_PANE_VIEW);

	if (view_iid && default_iid && !strcmp (view_iid, default_iid)) {
		nautilus_side_pane_show_panel (window->sidebar,
					       GTK_WIDGET (sidebar_panel));
	}	

	g_free (default_iid);
}

void
nautilus_navigation_window_remove_sidebar_panel (NautilusNavigationWindow *window, NautilusViewFrame *sidebar_panel)
{
	g_return_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (window));
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (sidebar_panel));

	if (g_list_find (window->sidebar_panels, sidebar_panel) == NULL) {
		return;
	}
	
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
nautilus_navigation_window_set_search_mode (NautilusNavigationWindow *window,
				 gboolean search_mode)
{
	nautilus_switchable_navigation_bar_set_mode
		(NAUTILUS_SWITCHABLE_NAVIGATION_BAR (window->navigation_bar),
		 search_mode
		 ? NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH
		 : NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION);
}

gboolean
nautilus_navigation_window_get_search_mode (NautilusNavigationWindow *window)
{
	return nautilus_switchable_navigation_bar_get_mode 
		(NAUTILUS_SWITCHABLE_NAVIGATION_BAR (window->navigation_bar)) 
	== NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH;
}

void
nautilus_navigation_window_go_home (NautilusNavigationWindow *window)
{
	char *home_uri;

	nautilus_navigation_window_set_search_mode (window, FALSE);

#ifdef WEB_NAVIGATION_ENABLED
	home_uri = eel_preferences_get (NAUTILUS_PREFERENCES_HOME_URI);
#else
	home_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
#endif
	
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
			  NautilusViewIdentifier *identifier,
			  guint index)
{
	GtkWidget *menu_item;
        char *menu_label;

	menu_label = g_strdup (_(identifier->view_as_label));
	menu_item = gtk_menu_item_new_with_mnemonic (menu_label);
	g_free (menu_label);

	g_signal_connect_object (menu_item, "activate",
				 G_CALLBACK (view_as_menu_switch_views_callback),
				 window, 0);

	g_object_set_data (G_OBJECT (menu_item), "viewer index", GINT_TO_POINTER (index));

	gtk_widget_show (menu_item);

	return menu_item;
}

static GtkWidget *
new_gtk_separator (void)
{
	GtkWidget *result;
	
	result = gtk_menu_item_new ();
	gtk_widget_show (result);
	gtk_widget_set_sensitive (result, FALSE);

	return result;
}

static void
view_as_menu_choose_view_callback (GtkWidget *widget, gpointer data)
{
        NautilusWindow *window;
        
        g_assert (GTK_IS_MENU_ITEM (widget));
        g_assert (NAUTILUS_IS_WINDOW (data));
        
        window = NAUTILUS_WINDOW (data);

	/* Set the option menu back to its previous setting (Don't
	 * leave it on this dialog-producing "View as..."
	 * setting). If the menu choice causes a content view change,
	 * this will be updated again later, in
	 * nautilus_window_load_view_as_menus. Do this right away so
	 * the user never sees the option menu set to "View as
	 * Other...".
	 */
	load_view_as_menu (window);

	nautilus_window_show_view_as_dialog (window);
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

		if (nautilus_window_content_view_matches_iid (NAUTILUS_WINDOW (window), ((NautilusViewIdentifier *)node->data)->iid)) {
			selected_index = index;
		}
        }

	if (selected_index == -1) {
		NautilusViewIdentifier *id;
		/* We're using an extra viewer, add a menu item for it */

		id = nautilus_window_get_content_view_id (window);
                menu_item = create_view_as_menu_item (window, id, index);
		nautilus_view_identifier_free (id);
		
                gtk_menu_shell_append (GTK_MENU_SHELL (new_menu), menu_item);
		selected_index = index;
	}

        /* Add/Show separator before "View as..." if there are any other viewers in menu. */
        if (window->details->short_list_viewers != NULL) {
	        gtk_menu_shell_append (GTK_MENU_SHELL (new_menu), new_gtk_separator ());
        }

	/* Add "View as..." extra bonus choice. */
       	menu_item = gtk_menu_item_new_with_label (_("View as..."));
        g_signal_connect_object (menu_item, "activate",
				 G_CALLBACK (view_as_menu_choose_view_callback), window, 0);
       	gtk_widget_show (menu_item);
       	gtk_menu_shell_append (GTK_MENU_SHELL (new_menu), menu_item);

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

	if (NAUTILUS_NAVIGATION_WINDOW (window)->information_panel) {
		nautilus_information_panel_set_title 
			(NAUTILUS_NAVIGATION_WINDOW (window)->information_panel, title);
	}
}

static void
real_merge_menus (NautilusWindow *nautilus_window)
{
	NautilusNavigationWindow *window;
	GtkWidget *location_bar_box;
	GtkWidget *view_as_menu_vbox;
	BonoboControl *location_bar_wrapper;

	EEL_CALL_PARENT (NAUTILUS_WINDOW_CLASS, 
			 merge_menus, (nautilus_window));

	window = NAUTILUS_NAVIGATION_WINDOW (nautilus_window);
	
	bonobo_ui_util_set_ui (NAUTILUS_WINDOW (window)->details->shell_ui,
			       DATADIR,
			       "nautilus-navigation-window-ui.xml",
			       "nautilus", NULL);

	bonobo_ui_component_freeze 
		(NAUTILUS_WINDOW (window)->details->shell_ui, NULL);

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
	
	window->navigation_bar = nautilus_switchable_navigation_bar_new (window);
	gtk_widget_show (GTK_WIDGET (window->navigation_bar));

	g_signal_connect_object (window->navigation_bar, "location_changed",
				 G_CALLBACK (navigation_bar_location_changed_callback), window, 0);
	g_signal_connect_object (window->navigation_bar, "mode_changed",
				 G_CALLBACK (navigation_bar_mode_changed_callback), window, 0);

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
	g_signal_connect_object (window->zoom_control, "zoom_to_fit",
				 G_CALLBACK (nautilus_window_zoom_to_fit),
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


#ifndef HAVE_MEDUSA
	/* Hide medusa menu items if medusa is not installed */
	nautilus_bonobo_set_hidden (NAUTILUS_WINDOW (window)->details->shell_ui,
				    COMMAND_PATH_TOGGLE_FIND_MODE,
				    TRUE);
	nautilus_bonobo_set_hidden (NAUTILUS_WINDOW (window)->details->shell_ui,
				    COMMAND_PATH_TOGGLE_FIND_MODE_WITH_STATE,
				    TRUE);
	/* Also set these items insensitive so that keyboard shortcuts do not trigger
	   warnings */
	nautilus_bonobo_set_sensitive (NAUTILUS_WINDOW (window)->details->shell_ui,
				       COMMAND_PATH_TOGGLE_FIND_MODE,
				       FALSE);
	nautilus_bonobo_set_sensitive (NAUTILUS_WINDOW (window)->details->shell_ui,
				       COMMAND_PATH_TOGGLE_FIND_MODE_WITH_STATE,
				       FALSE);
#endif

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
zoom_level_changed_callback (NautilusViewFrame *view,
                             NautilusNavigationWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));
	
        /* This is called each time the component successfully completed
         * a zooming operation.
         */
        nautilus_zoom_control_set_zoom_level (NAUTILUS_ZOOM_CONTROL (window->zoom_control),
                                              nautilus_view_frame_get_zoom_level (view));
}

static void
zoom_parameters_changed_callback (NautilusViewFrame *view,
                                  NautilusNavigationWindow *window)
{
        g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));
	
        /* This callback is invoked via the "zoom_parameters_changed"
         * signal of the BonoboZoomableFrame.
         * 
         * You can rely upon this callback being called in the following
         * situations:
         *
         * - a zoomable component has been set in the NautilusViewFrame;
         *   in this case nautilus_view_frame_set_to_component() emits the
         *   "zoom_parameters_changed" signal after creating the
         *   BonoboZoomableFrame and binding it to the Bonobo::Zoomable.
         *
         *   This means that we can use the following call to
         *   nautilus_zoom_control_set_parameters() to display the zoom
         *   control when a new zoomable component has been loaded.
         *
         * - a new file has been loaded by the zoomable component; this is
         *   not 100% guaranteed since it's up to the component to emit this
         *   signal, but I consider it "good behaviour" of a component to
         *   emit this signal after loading a new file.
         */

	nautilus_zoom_control_set_parameters
		(NAUTILUS_ZOOM_CONTROL (window->zoom_control),
		 nautilus_view_frame_get_min_zoom_level (view),
		 nautilus_view_frame_get_max_zoom_level (view),
		 nautilus_view_frame_get_has_min_zoom_level (view),
		 nautilus_view_frame_get_has_max_zoom_level (view),
		 nautilus_view_frame_get_preferred_zoom_levels (view));

        /* "zoom_parameters_changed" always implies "zoom_level_changed",
         * but you won't get both signals, so we need to pass it down.
         */
        zoom_level_changed_callback (view, window);
}

static void
connect_view (NautilusNavigationWindow *window, NautilusViewFrame *view)
{
	g_signal_connect (view, "zoom_parameters_changed",
			  G_CALLBACK (zoom_parameters_changed_callback),
			  window);
	g_signal_connect (view, "zoom_level_changed",
			  G_CALLBACK (zoom_level_changed_callback),
			  window);
}

static void
disconnect_view (NautilusNavigationWindow *window, NautilusViewFrame *view)
{
	if (!view) {
		return;
	}
	
	g_signal_handlers_disconnect_by_func 
		(G_OBJECT (view), 
		 G_CALLBACK (zoom_parameters_changed_callback), 
		 window);
	g_signal_handlers_disconnect_by_func
		(view, 
		 G_CALLBACK (zoom_level_changed_callback), 
		 window);	
}

static void
real_set_content_view_widget (NautilusWindow *nautilus_window,
			      NautilusViewFrame *new_view)
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

	if (new_view != NULL && nautilus_view_frame_get_is_zoomable (new_view)) {
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
	nautilus_navigation_window_set_search_mode 
		(window, in_search_mode);
	nautilus_switchable_navigation_bar_activate 
		(NAUTILUS_SWITCHABLE_NAVIGATION_BAR (window->navigation_bar));
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

static int
compare_view_identifier_with_iid (gconstpointer passed_view_identifier,
                                  gconstpointer passed_iid)
{
        return strcmp (((NautilusViewIdentifier *) passed_view_identifier)->iid,
                       (char *) passed_iid);
}

static void
disconnect_and_destroy_sidebar_panel (NautilusNavigationWindow *window, 
                                      NautilusViewFrame *view)
{
        g_object_ref (view);

	g_signal_handlers_disconnect_by_func 
		(view, 
		 G_CALLBACK (side_panel_view_failed_callback),
		 window);
	g_signal_handlers_disconnect_by_func 
		(view, 
		 G_CALLBACK (side_panel_view_loaded_callback),
		 window);

	nautilus_window_disconnect_extra_view (NAUTILUS_WINDOW (window), view);
        nautilus_navigation_window_remove_sidebar_panel (window, view);
	gtk_object_destroy (GTK_OBJECT (view));
        g_object_unref (view);
}

static void
set_side_panel_image (NautilusWindow *window,
                      NautilusViewFrame *side_panel,
                      const char *image_name)
{
        GdkPixbuf *pixbuf;
        char *image_path;

        pixbuf = NULL;
        
        if (image_name && image_name[0]) {
                image_path = nautilus_theme_get_image_path (image_name);
                if (image_path) {
                        pixbuf = gdk_pixbuf_new_from_file (image_path, NULL);
                        g_free (image_path);
                }
        }

        nautilus_side_pane_set_panel_image (NAUTILUS_NAVIGATION_WINDOW (window)->sidebar,
                                            GTK_WIDGET (side_panel),
                                            pixbuf);
        
        if (pixbuf) {
                g_object_unref (pixbuf);
        }
}

static void
side_panel_image_changed_callback (BonoboListener *listener,
                                   const char *event_name,
                                   const CORBA_any *arg,
                                   CORBA_Environment *ev,
                                   gpointer callback_data)
{
        NautilusViewFrame *side_panel;
        NautilusWindow *window;

        side_panel = NAUTILUS_VIEW_FRAME (callback_data);        
        window = NAUTILUS_WINDOW (g_object_get_data (G_OBJECT (side_panel),
                                                     "nautilus-window"));

        set_side_panel_image (window, side_panel, BONOBO_ARG_GET_STRING (arg));
}

static void
report_side_panel_failure_to_user (NautilusWindow *window, NautilusViewFrame *view_frame)
{
	char *message;
	char *detail;
	char *label;

	label = nautilus_window_get_view_frame_label (view_frame);

        if (label == NULL) {
                message = g_strdup
                        (_("One of the side panels encountered an error and can't continue."));
		detail = _("Unfortunately I couldn't tell which one.");
        } else {
                message = g_strdup_printf
                        (_("The %s side panel encountered an error and can't continue."), label);
                detail = _("If this keeps happening, you might want to turn this panel off.");
        }

	eel_show_error_dialog (message, detail, _("Side Panel Failed"), GTK_WINDOW (window));

	g_free (label);
	g_free (message);
}

static void
side_panel_view_failed_callback (NautilusViewFrame *view,
				 gpointer user_data)
{
	NautilusWindow *window;
	const char *current_iid;
	
	g_warning ("A view failed. The UI will handle this with a dialog but this should be debugged.");

	window = NAUTILUS_WINDOW (user_data);	

	report_side_panel_failure_to_user (window, view);
	current_iid = nautilus_view_frame_get_view_iid (view);
	disconnect_and_destroy_sidebar_panel (NAUTILUS_NAVIGATION_WINDOW (window), view);
}

static void
connect_side_panel (NautilusWindow *window,
                    NautilusViewFrame *side_panel)
{
        Bonobo_Control control;
        Bonobo_PropertyBag property_bag;
        CORBA_Environment ev;
        char *image_name;
        
        g_object_set_data (G_OBJECT (side_panel),
                           "nautilus-window",
                           window);
        
        control = nautilus_view_frame_get_control (side_panel);

	g_signal_connect_object (side_panel, 
				 "failed",
				 G_CALLBACK (side_panel_view_failed_callback),
				 window, 0);

        if (control != CORBA_OBJECT_NIL) {
                CORBA_exception_init (&ev);
                property_bag = Bonobo_Control_getProperties (control, &ev);
                if (property_bag != CORBA_OBJECT_NIL) {                        
                        bonobo_event_source_client_add_listener 
                                (property_bag,
                                 side_panel_image_changed_callback,
                                 "Bonobo/Property:change:tab_image",
                                 NULL,
                                 side_panel);
                        
                        /* Set the initial tab image */
                        image_name = bonobo_property_bag_client_get_value_string
                                (property_bag, 
                                 "tab_image", 
                                 NULL);
                        set_side_panel_image (window, side_panel, image_name);
                        g_free (image_name);
                        
                        bonobo_object_release_unref (property_bag, NULL);
                }
                CORBA_exception_free (&ev);
        }
}

static void
side_panel_view_loaded_callback (NautilusViewFrame *view,
				gpointer user_data)
{
	NautilusWindow *window;
	
	window = NAUTILUS_WINDOW (user_data);

	connect_side_panel (window, view);
}

void
nautilus_navigation_window_set_sidebar_panels (NautilusNavigationWindow *window,
                                               GList *passed_identifier_list)
{
	GList *identifier_list;
	GList *node, *next, *found_node;
	NautilusViewFrame *sidebar_panel;
	NautilusViewIdentifier *identifier;
	const char *current_iid;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	/* Make a copy of the list so we can remove items from it. */
	identifier_list = g_list_copy (passed_identifier_list);
	
	/* Remove panels from the window that don't appear in the list. */
	for (node = window->sidebar_panels; node != NULL; node = next) {
		next = node->next;

		sidebar_panel = NAUTILUS_VIEW_FRAME (node->data);
		
		found_node = g_list_find_custom (identifier_list,
						 (char *) nautilus_view_frame_get_view_iid (sidebar_panel),
						 compare_view_identifier_with_iid);
		if (found_node == NULL) {
			current_iid = nautilus_view_frame_get_view_iid (sidebar_panel);
			disconnect_and_destroy_sidebar_panel (window, sidebar_panel);
		} else {
                        identifier = (NautilusViewIdentifier *) found_node->data;

                        /* Right panel, make sure it has the right name. */
                        /* FIXME: Is this set_label necessary? Shouldn't it already
                         * have the right label here?
                         */
                        nautilus_view_frame_set_label (sidebar_panel, identifier->name);

                        /* Since this was found, there's no need to add it in the loop below. */
			identifier_list = g_list_remove_link (identifier_list, found_node);
			g_list_free_1 (found_node);
		}
        }

	/* Add panels to the window that were in the list, but not the window. */
	for (node = identifier_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		
		identifier = (NautilusViewIdentifier *) node->data;

                /* Create and load the panel. */
		sidebar_panel = nautilus_view_frame_new (NAUTILUS_WINDOW (window)->details->ui_container,
                                                         NAUTILUS_WINDOW (window)->application->undo_manager,
							 NAUTILUS_WINDOW_GET_CLASS (window)->window_type);
                
                eel_accessibility_set_name (sidebar_panel, _("Side Pane"));
                eel_accessibility_set_description
                        (sidebar_panel, _("Contains a side pane view"));
                

		nautilus_view_frame_set_label (sidebar_panel, identifier->name);
		nautilus_window_connect_extra_view (NAUTILUS_WINDOW (window), 
						    sidebar_panel,
						    identifier);
		g_signal_connect_object (sidebar_panel, 
					 "view_loaded",
					 G_CALLBACK (side_panel_view_loaded_callback),
					 G_OBJECT (window), 0);
		
		nautilus_view_frame_load_view (sidebar_panel, identifier->iid);

		connect_side_panel (NAUTILUS_WINDOW (window), sidebar_panel);
		
		nautilus_navigation_window_add_sidebar_panel (window, sidebar_panel);
                gtk_object_sink (GTK_OBJECT (sidebar_panel));
	}

	g_list_free (identifier_list);
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
	GList *identifier_list;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	if (window->sidebar == NULL) {
		return;
	}

	identifier_list = nautilus_sidebar_get_all_sidebar_panel_view_identifiers ();
	nautilus_navigation_window_set_sidebar_panels (window, identifier_list);
	nautilus_view_identifier_list_free (identifier_list);

	set_current_side_panel
		(window, 
		 nautilus_side_pane_get_current_panel (window->sidebar));
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

	GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static void 
real_get_default_size(NautilusWindow *window, guint *default_width, guint *default_height)
{
	
    if(default_width) {
	   *default_width = NAUTILUS_NAVIGATION_WINDOW_DEFAULT_WIDTH;
	}
	
	if(default_height) {
       *default_height = NAUTILUS_NAVIGATION_WINDOW_DEFAULT_HEIGHT;	
	}
}

static void
nautilus_navigation_window_class_init (NautilusNavigationWindowClass *class)
{
	NAUTILUS_WINDOW_CLASS (class)->window_type = Nautilus_WINDOW_NAVIGATION;

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
}
