/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
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

#include "nautilus-main.h"
#include "nautilus-application.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-sidebar.h"
#include "nautilus-signaller.h"
#include "nautilus-switchable-navigation-bar.h"
#include "nautilus-throbber.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-zoom-control.h"
#include <bonobo/bonobo-ui-util.h>
#include <ctype.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-geometry.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-any-width-bin.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-generous-bin.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-horizontal-splitter.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-mime-actions.h>
#include <libnautilus-extensions/nautilus-program-choosing.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus/nautilus-clipboard.h>
#include <libnautilus/nautilus-undo.h>
#include <math.h>

/* FIXME bugzilla.eazel.com 1243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

/* Milliseconds */
#define STATUS_BAR_CLEAR_TIMEOUT 5000

/* GNOME Dock Items */
#define LOCATION_BAR_PATH	"/Location Bar"
#define TOOL_BAR_PATH           "/Tool Bar"
#define STATUS_BAR_PATH         "/status"
#define MENU_BAR_PATH           "/menu"

/* default web search uri - FIXME bugzilla.eazel.com 2465: this will be changed to point to the Eazel service */
#define DEFAULT_SEARCH_WEB_URI "http://www.google.com"


enum {
	ARG_0,
	ARG_APP_ID,
	ARG_APP
};

/* Other static variables */
static GList *history_list = NULL;

static void nautilus_window_initialize_class        (NautilusWindowClass *klass);
static void nautilus_window_initialize              (NautilusWindow      *window);
static void nautilus_window_destroy                 (GtkObject           *object);
static void nautilus_window_set_arg                 (GtkObject           *object,
						     GtkArg              *arg,
						     guint                arg_id);
static void nautilus_window_get_arg                 (GtkObject           *object,
						     GtkArg              *arg,
						     guint                arg_id);
static void nautilus_window_size_request            (GtkWidget           *widget,
						     GtkRequisition      *requisition);
static void nautilus_window_realize                 (GtkWidget           *widget);
static void update_sidebar_panels_from_preferences  (NautilusWindow      *window);
static void sidebar_panels_changed_callback         (gpointer             user_data);
static void nautilus_window_show                    (GtkWidget           *widget);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusWindow,
				   nautilus_window,
				   BONOBO_TYPE_WIN)

static void
nautilus_window_initialize_class (NautilusWindowClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	object_class->destroy = nautilus_window_destroy;
	object_class->get_arg = nautilus_window_get_arg;
	object_class->set_arg = nautilus_window_set_arg;
	
	widget_class->show = nautilus_window_show;
	
	gtk_object_add_arg_type ("NautilusWindow::app_id",
				 GTK_TYPE_STRING,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
				 ARG_APP_ID);
	gtk_object_add_arg_type ("NautilusWindow::app",
				 GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
				 ARG_APP);
	
	widget_class->realize = nautilus_window_realize;
	widget_class->size_request = nautilus_window_size_request;	
}

static void
nautilus_window_initialize (NautilusWindow *window)
{
	window->details = g_new0 (NautilusWindowDetails, 1);

	gtk_quit_add_destroy (1, GTK_OBJECT (window));
	
	/* Keep track of any sidebar panel changes */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SIDEBAR_PANELS_NAMESPACE,
					   sidebar_panels_changed_callback,
					   window);

	/* Keep the main event loop alive as long as the window exists */
	nautilus_main_event_loop_register (GTK_OBJECT (window));
}

static gboolean
nautilus_window_clear_status (gpointer callback_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (callback_data);
	bonobo_ui_component_set_status (window->details->shell_ui, "", NULL);
	window->status_bar_clear_id = 0;
	return FALSE;
}

void
nautilus_window_set_status (NautilusWindow *window, const char *text)
{
	if (window->status_bar_clear_id != 0) {
		g_source_remove (window->status_bar_clear_id);
	}
	
	if (text != NULL && text[0] != '\0') {
		bonobo_ui_component_set_status (window->details->shell_ui, text, NULL);
		window->status_bar_clear_id = g_timeout_add
			(STATUS_BAR_CLEAR_TIMEOUT, nautilus_window_clear_status, window);
	} else {
		window->status_bar_clear_id = 0;
	}
}

void
nautilus_window_goto_uri (NautilusWindow *window, const char *uri)
{
	nautilus_window_open_location (window, uri);
}

static void
goto_uri_callback (GtkWidget *widget,
		   const char *uri,
		   GtkWidget *window)
{
	nautilus_window_goto_uri (NAUTILUS_WINDOW (window), uri);
}

static void
navigation_bar_mode_changed_callback (GtkWidget *widget,
				      NautilusSwitchableNavigationBarMode mode,
				      GtkWidget *window)
{
	nautilus_window_update_find_menu_item (NAUTILUS_WINDOW (window));

#ifdef UIH
	switch (mode) {
	case NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (NAUTILUS_WINDOW (window)->search_local_button), FALSE);
		break;
	case NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (NAUTILUS_WINDOW (window)->search_local_button), TRUE);
		break;
	default:
		g_assert_not_reached ();
	}
#endif
}

void
nautilus_window_zoom_in (NautilusWindow *window)
{
	if (window->content_view != NULL) {
		nautilus_view_frame_zoom_in (window->content_view);
	}
}

void
nautilus_window_zoom_to_level (NautilusWindow *window, double level)
{
	if (window->content_view != NULL) {
		nautilus_view_frame_set_zoom_level (window->content_view, level);
	}
}

void
nautilus_window_zoom_out (NautilusWindow *window)
{
	if (window->content_view != NULL) {
		nautilus_view_frame_zoom_out (window->content_view);
	}
}

void
nautilus_window_zoom_to_fit (NautilusWindow *window)
{
	if (window->content_view != NULL) {
		nautilus_view_frame_zoom_to_fit (window->content_view);
	}
}

#ifdef UIH

/* This is our replacement for gnome_app_set_statusbar.
 * It uses nautilus_any_width_bin to make text changes in the
 * status bar not affect the width of the window.
 */
static void
install_status_bar (GnomeApp *app,
		    GtkWidget *status_bar)
{
	GtkWidget *bin;

	g_assert (GNOME_IS_APP (app));
	g_assert (GTK_IS_WIDGET (status_bar));
	g_assert (app->statusbar == NULL);

	app->statusbar = status_bar;
	gtk_widget_show (status_bar);

	bin = nautilus_any_width_bin_new ();
	gtk_container_set_border_width (GTK_CONTAINER (bin), 0);
	gtk_widget_show (bin);

	gtk_container_add (GTK_CONTAINER (bin), status_bar);
	gtk_box_pack_start (GTK_BOX (app->vbox), bin, FALSE, FALSE, 0);
}

#endif

/* Code should never force the window taller than this size.
 * (The user can still stretch the window taller if desired).
 */
static guint
get_max_forced_height (void)
{
	return (gdk_screen_height () * 90) / 100;
}

/* Code should never force the window wider than this size.
 * (The user can still stretch the window wider if desired).
 */
static guint
get_max_forced_width (void)
{
	return (gdk_screen_width () * 90) / 100;
}

static void
set_initial_window_geometry (NautilusWindow *window)
{
	guint max_width_for_screen, max_height_for_screen;

	/* Don't let GTK determine the minimum size
	 * automatically. It will insist that the window be
	 * really wide based on some misguided notion about
	 * the content view area. Also, it might start the
	 * window wider (or taller) than the screen, which
	 * is evil. So we choose semi-arbitrary initial and
	 * minimum widths instead of letting GTK decide.
	 */

	max_width_for_screen = get_max_forced_width ();
	max_height_for_screen = get_max_forced_height ();

	gtk_widget_set_usize (GTK_WIDGET (window), 
			      MIN (NAUTILUS_WINDOW_MIN_WIDTH, 
			           max_width_for_screen),
			      MIN (NAUTILUS_WINDOW_MIN_HEIGHT, 
			           max_height_for_screen));

	gtk_window_set_default_size (GTK_WINDOW (window), 
				     MIN (NAUTILUS_WINDOW_DEFAULT_WIDTH, 
				          max_width_for_screen), 
				     MIN (NAUTILUS_WINDOW_DEFAULT_HEIGHT, 
				          max_height_for_screen));

	gtk_window_set_policy (GTK_WINDOW (window), 
			       FALSE,  /* don't let window be stretched 
			                  smaller than usize */
			       TRUE,   /* do let the window be stretched 
			                  larger than default size */
			       FALSE); /* don't shrink the window 
			                  automatically to fit contents */

}

static void
nautilus_window_constructed (NautilusWindow *window)
{
	GtkWidget *location_bar_box;
	GtkWidget *view_as_menu_vbox;
  	int sidebar_width;
	BonoboControl *location_bar_wrapper;

	/* set up location bar */
	location_bar_box = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (location_bar_box), GNOME_PAD_SMALL);
	
	window->navigation_bar = nautilus_switchable_navigation_bar_new ();
	gtk_widget_show (GTK_WIDGET (window->navigation_bar));

	gtk_signal_connect (GTK_OBJECT (window->navigation_bar), "location_changed",
			    goto_uri_callback, window);

	gtk_signal_connect (GTK_OBJECT (window->navigation_bar), "mode_changed",
			    navigation_bar_mode_changed_callback, window);

	gtk_box_pack_start (GTK_BOX (location_bar_box), window->navigation_bar,
			    TRUE, TRUE, GNOME_PAD_SMALL);

	/* Option menu for content view types; it's empty here, filled in when a uri is set.
	 * Pack it into vbox so it doesn't grow vertically when location bar does. 
	 */
	view_as_menu_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_widget_show (view_as_menu_vbox);
	gtk_box_pack_end (GTK_BOX (location_bar_box), view_as_menu_vbox, FALSE, FALSE, 0);
	
	window->view_as_option_menu = gtk_option_menu_new();
	gtk_box_pack_end (GTK_BOX (view_as_menu_vbox), window->view_as_option_menu, TRUE, FALSE, 0);
	gtk_widget_show (window->view_as_option_menu);
	
	/* Allocate the zoom control and place on the right next to the menu.
	 * It gets shown later, if the view-frame contains something zoomable.
	 */
	window->zoom_control = nautilus_zoom_control_new ();
	gtk_signal_connect_object (GTK_OBJECT (window->zoom_control), "zoom_in", nautilus_window_zoom_in, GTK_OBJECT (window));
	gtk_signal_connect_object (GTK_OBJECT (window->zoom_control), "zoom_out", nautilus_window_zoom_out, GTK_OBJECT (window));
	gtk_signal_connect_object (GTK_OBJECT (window->zoom_control), "zoom_to_level", nautilus_window_zoom_to_level, GTK_OBJECT (window));
	gtk_signal_connect_object (GTK_OBJECT (window->zoom_control), "zoom_to_fit", nautilus_window_zoom_to_fit, GTK_OBJECT (window));
	gtk_box_pack_end (GTK_BOX (location_bar_box), window->zoom_control, FALSE, FALSE, 0);
	
	gtk_widget_show (location_bar_box);
	
	/* FIXME bugzilla.eazel.com 1243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
        if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		window->content_hbox = gtk_widget_new (NAUTILUS_TYPE_GENEROUS_BIN, NULL);
	} else {
		set_initial_window_geometry (window);
	
		window->content_hbox = nautilus_horizontal_splitter_new ();

		/* FIXME bugzilla.eazel.com 1245: No constant for the default? */
		/* FIXME bugzilla.eazel.com 1245: Saved in pixels instead of in %? */
		/* FIXME bugzilla.eazel.com 1245: No reality check on the value? */
		/* FIXME bugzilla.eazel.com 1245: get_enum? why not get_integer? */
		sidebar_width = nautilus_preferences_get_enum (NAUTILUS_PREFERENCES_SIDEBAR_WIDTH, 148);
		e_paned_set_position (E_PANED (window->content_hbox), sidebar_width);
	}
	gtk_widget_show (window->content_hbox);
	bonobo_win_set_contents (BONOBO_WIN (window), window->content_hbox);
	
	/* set up the index panel */
	
	window->sidebar = nautilus_sidebar_new ();
	gtk_widget_show (GTK_WIDGET (window->sidebar));
	gtk_signal_connect (GTK_OBJECT (window->sidebar), "location_changed",
			    goto_uri_callback, window);
	
	/* FIXME bugzilla.eazel.com 1243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
        if (!NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		e_paned_pack1 (E_PANED(window->content_hbox), GTK_WIDGET(window->sidebar), FALSE, FALSE);
	}
	
	/* CORBA and Bonobo setup */
	window->details->ui_container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (window->details->ui_container,
				     BONOBO_WIN (window));


	/* Load the user interface from the XML file. */
	window->details->shell_ui = bonobo_ui_component_new ("Nautilus Shell");
	bonobo_ui_component_set_container
		(window->details->shell_ui,
		 bonobo_object_corba_objref (BONOBO_OBJECT (window->details->ui_container)));

	bonobo_ui_component_freeze (window->details->shell_ui, NULL);

	bonobo_ui_util_set_ui (window->details->shell_ui,
			       NAUTILUS_DATADIR,
			       "nautilus-shell-ui.xml",
			       "nautilus");
	if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		nautilus_bonobo_set_hidden (window->details->shell_ui,
					    LOCATION_BAR_PATH, TRUE);
		nautilus_bonobo_set_hidden (window->details->shell_ui,
					    TOOL_BAR_PATH, TRUE);
		nautilus_bonobo_set_hidden (window->details->shell_ui,
					    STATUS_BAR_PATH, TRUE);
		nautilus_bonobo_set_hidden (window->details->shell_ui,
					    MENU_BAR_PATH, TRUE);
	}

	/* Wrap the location bar in a control and set it up. */
	location_bar_wrapper = bonobo_control_new (location_bar_box);
	bonobo_ui_component_object_set (window->details->shell_ui,
					"/Location Bar/Wrapper",
					bonobo_object_corba_objref (BONOBO_OBJECT (location_bar_wrapper)),
					NULL);
	bonobo_ui_component_thaw (window->details->shell_ui, NULL);
	bonobo_object_unref (BONOBO_OBJECT (location_bar_wrapper));

	/* initalize the menus and tool bars */
	nautilus_window_initialize_menus (window);
	nautilus_window_initialize_toolbars (window);

	/* watch for throbber locatoin changes, too */
	gtk_signal_connect (GTK_OBJECT (window->throbber), "location_changed",
		goto_uri_callback, window);
	
	/* Set initial sensitivity of some buttons & menu items 
	 * now that they're all created.
	 */
	nautilus_window_allow_back (window, FALSE);
	nautilus_window_allow_forward (window, FALSE);
	nautilus_window_allow_stop (window, FALSE);

	/* Set up undo manager */
	nautilus_undo_manager_attach (window->application->undo_manager, GTK_OBJECT (window));	

	/* Set up the sidebar panels. */
	update_sidebar_panels_from_preferences (window);
}

static void
nautilus_window_set_arg (GtkObject *object,
			 GtkArg *arg,
			 guint arg_id)
{
	char *old_name;
	NautilusWindow *window = (NautilusWindow *) object;
	
	switch(arg_id) {
	case ARG_APP_ID:
		if (GTK_VALUE_STRING (*arg) == NULL) {
			return;
		}
		old_name = bonobo_win_get_name (BONOBO_WIN (object));
		bonobo_win_set_name (BONOBO_WIN (object), GTK_VALUE_STRING (*arg));
		/* This hack of using the time when the name first
		 * goes non-NULL to be window-constructed time is
		 * completely lame. But it works, so for now we leave
		 * it alone.
		 */
		if (old_name == NULL) {
			nautilus_window_constructed (NAUTILUS_WINDOW (object));
		}
		g_free (old_name);
		break;
	case ARG_APP:
		window->application = NAUTILUS_APPLICATION (GTK_VALUE_OBJECT (*arg));
		break;
	}
}

static void
nautilus_window_get_arg (GtkObject *object,
			 GtkArg *arg,
			 guint arg_id)
{
	switch(arg_id) {
	case ARG_APP_ID:
		GTK_VALUE_STRING (*arg) = bonobo_win_get_name (BONOBO_WIN (object));
		break;
	case ARG_APP:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT (NAUTILUS_WINDOW (object)->application);
		break;
	}
}

static void
view_disconnect (gpointer data, gpointer user_data)
{
	g_assert (NAUTILUS_IS_WINDOW (user_data));
	g_assert (NAUTILUS_IS_VIEW_FRAME (data));

	nautilus_window_disconnect_view (NAUTILUS_WINDOW (user_data),
					 NAUTILUS_VIEW_FRAME (data));
}

static void 
nautilus_window_destroy (GtkObject *object)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (object);

	/* Let go of the file for the current location */
	nautilus_file_unref (window->details->viewed_file);

	g_free (window->details->dead_view_name);

	/* Dont keep track of sidebar panel changes no more */
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SIDEBAR_PANELS_NAMESPACE,
					      sidebar_panels_changed_callback,
					      NULL);

	nautilus_window_remove_bookmarks_menu_callback (window);
	nautilus_window_remove_bookmarks_menu_items (window);
	nautilus_window_remove_go_menu_callback (window);
	nautilus_window_remove_go_menu_items (window);
	nautilus_window_toolbar_remove_theme_callback (window);

	/* Disconnect view signals here so they don't trigger when
	 * views are destroyed normally.
	 */
	g_list_foreach (window->sidebar_panels, view_disconnect, window);
	g_list_free (window->sidebar_panels);

	nautilus_window_disconnect_view (window, window->content_view);

	nautilus_view_identifier_free (window->content_view_id);
	
	g_free (window->location);
	nautilus_g_list_free_deep (window->selection);
	nautilus_g_list_free_deep (window->pending_selection);

	nautilus_window_clear_back_list (window);
	nautilus_window_clear_forward_list (window);

	if (window->current_location_bookmark != NULL) {
		gtk_object_unref (GTK_OBJECT (window->current_location_bookmark));
	}
	if (window->last_location_bookmark != NULL) {
		gtk_object_unref (GTK_OBJECT (window->last_location_bookmark));
	}
	
	if (window->status_bar_clear_id != 0) {
		g_source_remove (window->status_bar_clear_id);
	}

	if (window->details->ui_container != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (window->details->ui_container));
	}

	g_free (window->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (GTK_OBJECT (window)));
}

static void
nautilus_window_save_geometry (NautilusWindow *window)
{
	NautilusDirectory *directory;
	char *geometry_string;

        g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	g_return_if_fail (GTK_WIDGET_VISIBLE (window));

        directory = nautilus_directory_get (window->location);
        geometry_string = gnome_geometry_string (GTK_WIDGET (window)->window);
	nautilus_directory_set_metadata (directory,
					 NAUTILUS_METADATA_KEY_WINDOW_GEOMETRY,
					 NULL,
					 geometry_string);
	g_free (geometry_string);
	nautilus_directory_unref (directory);
}

void
nautilus_window_close (NautilusWindow *window)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	/* Save the window position in the directory's metadata only if
	 * we're in every-location-in-its-own-window mode. Otherwise it
	 * would be too apparently random when the stored positions change.
	 */
	if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
					      FALSE)) {
	        nautilus_window_save_geometry (window);
	}

	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
nautilus_window_realize (GtkWidget *widget)
{
        char *filename;
	GdkPixbuf *pixbuf;
        GdkPixmap *pixmap;
        GdkBitmap *mask;
        
        /* Create our GdkWindow */
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));
        
        /* Set the mini icon */
        filename = nautilus_pixmap_file ("nautilus-mini-logo.png");
        if (filename != NULL) {
                pixbuf = gdk_pixbuf_new_from_file(filename);
                if (pixbuf != NULL) {
                        gdk_pixbuf_render_pixmap_and_mask
				(pixbuf, &pixmap, &mask, NAUTILUS_STANDARD_ALPHA_THRESHHOLD);				   
			gdk_pixbuf_unref (pixbuf);
			nautilus_set_mini_icon
				(widget->window, pixmap, mask);
			/* FIXME bugzilla.eazel.com 610: It seems we are
			 * leaking the pixmap and mask here, but if we unref
			 * them here, the task bar crashes.
			 */
		}
        	g_free (filename);
	}
}

static void
nautilus_window_size_request (GtkWidget		*widget,
			      GtkRequisition	*requisition)
{
	guint max_width;
	guint max_height;

	g_return_if_fail (NAUTILUS_IS_WINDOW (widget));
	g_return_if_fail (requisition != NULL);

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_request, (widget, requisition));

	/* Limit the requisition to be within 90% of the available screen 
	 * real state.
	 *
	 * This way the user will have a fighting chance of getting
	 * control of their window back if for whatever reason one of the
	 * window's descendants decide they want to be 4000 pixels wide.
	 *
	 * Note that the user can still make the window really huge by hand.
	 *
	 * Bugs in components or other widgets that cause such huge geometries
	 * to be requested, should still be fixed.  This code is here only to 
	 * prevent the extremely frustrating consequence of such bugs.
	 */
	max_width = get_max_forced_width ();
	max_height = get_max_forced_height ();

	if (requisition->width > max_width) {
		requisition->width = max_width;
	}
	
	if (requisition->height > max_height) {
		requisition->height = max_height;
	}
}

/*
 * Main API
 */

static void
view_menu_switch_views_callback (GtkWidget *widget, gpointer data)
{
        NautilusWindow *window;
        NautilusViewIdentifier *identifier;
        
        g_return_if_fail (GTK_IS_MENU_ITEM (widget));
        g_return_if_fail (NAUTILUS_IS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window")));
        
        window = NAUTILUS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window"));
        identifier = (NautilusViewIdentifier *) gtk_object_get_data (GTK_OBJECT (widget), "identifier");
        
        nautilus_window_set_content_view (window, identifier);
}

/* Note: The identifier parameter ownership is handed off to the menu item. */
static GtkWidget *
create_content_view_menu_item (NautilusWindow *window, NautilusViewIdentifier *identifier)
{
	GtkWidget *menu_item;
        char *menu_label;

	menu_label = g_strdup_printf (_("View as %s"), identifier->name);
	menu_item = gtk_menu_item_new_with_label (menu_label);
	g_free (menu_label);

	gtk_signal_connect (GTK_OBJECT (menu_item),
			    "activate",
			    view_menu_switch_views_callback, 
			    NULL);

	/* Store copy of iid in item; free when item destroyed. */
	gtk_object_set_data_full (GTK_OBJECT (menu_item),
				  "identifier",
				  identifier,
				  (GtkDestroyNotify) nautilus_view_identifier_free);

	/* Store reference to window in item; no need to free this. */
	gtk_object_set_data (GTK_OBJECT (menu_item), "window", window);
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

/* Make a special first item in the "View as" option menu that represents
 * the current content view. This should only be called if the current
 * content view isn't already in the "View as" option menu.
 */
static void
replace_special_current_view_in_content_view_menu (NautilusWindow *window)
{
	GtkWidget *menu;
	GtkWidget *first_menu_item;
	GtkWidget *new_menu_item;
	
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (window->view_as_option_menu));

	/* Remove menu before changing contents so it is resized properly
	 * when reattached later in this function.
	 */
	gtk_widget_ref (menu);
	gtk_option_menu_remove_menu (GTK_OPTION_MENU (window->view_as_option_menu));

	first_menu_item = nautilus_gtk_container_get_first_child (GTK_CONTAINER (menu));
	g_assert (first_menu_item != NULL);

	if (GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (first_menu_item), "current content view"))) {
		gtk_container_remove (GTK_CONTAINER (menu), first_menu_item);
	} else {
		/* Prepend separator. */
		gtk_menu_prepend (GTK_MENU (menu), new_gtk_separator ());
	}

	new_menu_item = create_content_view_menu_item (window, nautilus_view_identifier_copy (window->content_view_id));
	gtk_object_set_data (GTK_OBJECT (new_menu_item), "current content view", GINT_TO_POINTER (TRUE));
	gtk_menu_prepend (GTK_MENU (menu), new_menu_item);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (window->view_as_option_menu), menu);
	gtk_widget_unref (menu);
}

/**
 * nautilus_window_synch_content_view_menu:
 * 
 * Set the visible item of the "View as" option menu to
 * match the current content view.
 * 
 * @window: The NautilusWindow whose "View as" option menu should be synched.
 */
void
nautilus_window_synch_content_view_menu (NautilusWindow *window)
{
	GList *children, *child;
	GtkWidget *menu;
	NautilusViewIdentifier *item_id;
	int index, matching_index;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (window->view_as_option_menu));
	if (menu == NULL) {
		return;
	}
	
	children = gtk_container_children (GTK_CONTAINER (menu));
	matching_index = -1;

	for (child = children, index = 0; child != NULL; child = child->next, ++index) {
		item_id = (NautilusViewIdentifier *)(gtk_object_get_data (GTK_OBJECT (child->data), "identifier"));
		if (item_id != NULL && strcmp (window->content_view->iid, item_id->iid) == 0) {
			matching_index = index;
			break;
		}
	}

	if (matching_index == -1) {
		replace_special_current_view_in_content_view_menu (window);
		matching_index = 0;
	}

	gtk_option_menu_set_history (GTK_OPTION_MENU (window->view_as_option_menu), 
				     matching_index);

	g_list_free (children);
}

static void
chose_component_callback (NautilusViewIdentifier *identifier, gpointer callback_data)
{
	/* Can't assume callback_data is a valid NautilusWindow, because
	 * it might be garbage if the program is exiting when this dialog
	 * is up.
	 */

	if (identifier != NULL) {
		g_return_if_fail (NAUTILUS_IS_WINDOW (callback_data));
		nautilus_window_set_content_view (NAUTILUS_WINDOW (callback_data), identifier);
	}

	/* FIXME bugzilla.eazel.com 1334: 
	 * There should be some global way to signal that the
	 * file type associations have changed, so that the places that
	 * display these lists can react. For now, hardwire this case, which
	 * is the most obvious one by far.
	 */
	if (NAUTILUS_IS_WINDOW (callback_data)) {
		nautilus_window_load_content_view_menu (NAUTILUS_WINDOW (callback_data));
	}
}

static void
view_menu_choose_view_callback (GtkWidget *widget, gpointer data)
{
        NautilusWindow *window;
        NautilusFile *file;
        
        g_return_if_fail (GTK_IS_MENU_ITEM (widget));
        g_return_if_fail (NAUTILUS_IS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window")));
        
        window = NAUTILUS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window"));

	/* Set the option menu back to its previous setting (Don't leave it
	 * on this dialog-producing "View as Other..." setting). If the menu choice 
	 * causes a content view change, this will be updated again later, 
	 * in nautilus_window_load_content_view_menu. Do this right away so 
	 * the user never sees the option menu set to "View as Other...".
	 */
	nautilus_window_synch_content_view_menu (window);

	/* FIXME bugzilla.eazel.com 866: Can't expect to put this
	 * window up instantly. We might need to read the metafile
	 * first.
	 */
        file = nautilus_file_get (window->location);
        g_return_if_fail (NAUTILUS_IS_FILE (file));

	nautilus_choose_component_for_file (file, 
					    GTK_WINDOW (window), 
					    chose_component_callback, 
					    window);

	nautilus_file_unref (file);
}

static void
view_menu_vfs_method_callback (GtkWidget *widget, gpointer data)
{
        NautilusWindow *window;
	char *new_location;
	char *method;
        
        g_return_if_fail (GTK_IS_MENU_ITEM (widget));
        g_return_if_fail (NAUTILUS_IS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window")));
        
        window = NAUTILUS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window"));
        method = (char *)(gtk_object_get_data (GTK_OBJECT (widget), "method"));
	g_return_if_fail (method);

	new_location = g_strdup_printf("%s#%s:/",window->location,method);
	nautilus_window_goto_uri(window, new_location);
	g_free(new_location);

}

void
nautilus_window_load_content_view_menu (NautilusWindow *window)
{	
	GList *components;
	char *method;
        GList *p;
        GtkWidget *new_menu;
        GtkWidget *menu_item;
	char *label;
	NautilusDirectory *directory;
	NautilusFile *file;

        g_return_if_fail (NAUTILUS_IS_WINDOW (window));
        g_return_if_fail (GTK_IS_OPTION_MENU (window->view_as_option_menu));
        
        new_menu = gtk_menu_new ();
        
	file = nautilus_file_get (window->location);
	directory = nautilus_directory_get (window->location);
        /* Add a menu item for each view in the preferred list for this location. */
        components = nautilus_mime_get_short_list_components_for_uri (directory, file);
        for (p = components; p != NULL; p = p->next) {
                menu_item = create_content_view_menu_item 
                	(window, nautilus_view_identifier_new_from_content_view (p->data));
                gtk_menu_append (GTK_MENU (new_menu), menu_item);
        }
	gnome_vfs_mime_component_list_free (components);

	/* Add a menu item for each special GNOME-VFS method for this
	 * URI. This is a questionable user interface, since it's a
	 * one way trip if you choose one of these view menu items, but
	 * it's better than nothing.
	 */
	method = nautilus_mime_get_short_list_methods_for_uri (directory, file);
	/* FIXME bugzilla.eazel.com 2466: Name of the function is plural, but it returns only
	 * one item. That must be fixed.
	 */
	if (method != NULL) {
		label = g_strdup_printf (_("View as %s..."), method);
		menu_item = gtk_menu_item_new_with_label (label);
		g_free (label);

        	gtk_object_set_data (GTK_OBJECT (menu_item), "window", window);
        	gtk_object_set_data_full (GTK_OBJECT (menu_item), "method",
					  g_strdup (method), g_free);
        	gtk_signal_connect (GTK_OBJECT (menu_item),
				    "activate",
				    view_menu_vfs_method_callback,
				    NULL);
       		gtk_widget_show (menu_item);
       		gtk_menu_append (GTK_MENU (new_menu), menu_item);
		g_free (method);
	}

        /* Add separator before "Other" if there are any other viewers in menu. */
        if (components != NULL || method != NULL) {
	        gtk_menu_append (GTK_MENU (new_menu), new_gtk_separator ());
        }

	/* Add "View as Other..." extra bonus choice. */
       	menu_item = gtk_menu_item_new_with_label (_("View as Other..."));
        gtk_object_set_data (GTK_OBJECT (menu_item), "window", window);
        gtk_signal_connect (GTK_OBJECT (menu_item),
        		    "activate",
        		    view_menu_choose_view_callback,
        		    NULL);
       	gtk_widget_show (menu_item);
       	gtk_menu_append (GTK_MENU (new_menu), menu_item);

        /* We create and attach a new menu here because adding/removing
         * items from existing menu screws up the size of the option menu.
         */
        gtk_option_menu_set_menu (GTK_OPTION_MENU (window->view_as_option_menu),
                                  new_menu);


	nautilus_directory_unref (directory);
	nautilus_file_unref (file);

	nautilus_window_synch_content_view_menu (window);
}

void
nautilus_window_add_sidebar_panel (NautilusWindow *window,
				   NautilusViewFrame *sidebar_panel)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (sidebar_panel));
	g_return_if_fail (g_list_find (window->sidebar_panels, sidebar_panel) == NULL);
	
	nautilus_sidebar_add_panel (window->sidebar, sidebar_panel);

	gtk_widget_ref (GTK_WIDGET (sidebar_panel));
	window->sidebar_panels = g_list_prepend (window->sidebar_panels, sidebar_panel);
}

void
nautilus_window_remove_sidebar_panel (NautilusWindow *window, NautilusViewFrame *sidebar_panel)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (sidebar_panel));

	if (g_list_find (window->sidebar_panels, sidebar_panel) == NULL) {
		return;
	}
	
	nautilus_sidebar_remove_panel (window->sidebar, sidebar_panel);
	window->sidebar_panels = g_list_remove (window->sidebar_panels, sidebar_panel);
	gtk_widget_unref (GTK_WIDGET (sidebar_panel));
}

void
nautilus_window_back_or_forward (NautilusWindow *window, gboolean back, guint distance)
{
	GList *list;
	char *uri;
	
	list = back ? window->back_list : window->forward_list;
	g_assert (g_list_length (list) > distance);

	uri = nautilus_bookmark_get_uri (g_list_nth_data (list, distance));
	nautilus_window_begin_location_change
		(window,
		 uri,
		 back ? NAUTILUS_LOCATION_CHANGE_BACK : NAUTILUS_LOCATION_CHANGE_FORWARD,
		 distance);

	g_free (uri);
}

void
nautilus_window_go_back (NautilusWindow *window)
{
	nautilus_window_back_or_forward (window, TRUE, 0);
}

void
nautilus_window_go_forward (NautilusWindow *window)
{
	nautilus_window_back_or_forward (window, FALSE, 0);
}

void
nautilus_window_go_up (NautilusWindow *window)
{
	GnomeVFSURI *current_uri;
	GnomeVFSURI *parent_uri;
	char *parent_uri_string;
	
	if (window->location == NULL)
		return;
	
	current_uri = gnome_vfs_uri_new (window->location);
	parent_uri = gnome_vfs_uri_get_parent (current_uri);
	gnome_vfs_uri_unref (current_uri);

	if (parent_uri == NULL) {
		g_warning ("Can't go Up from here. The UI should have prevented us from getting this far.");
		return;
	}
	
	parent_uri_string = gnome_vfs_uri_to_string (parent_uri, GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (parent_uri);  
	
	nautilus_window_goto_uri (window, parent_uri_string);
	
	g_free (parent_uri_string);
}

void
nautilus_window_set_search_mode (NautilusWindow *window,
				 gboolean search_mode)
{
	nautilus_switchable_navigation_bar_set_mode
		(NAUTILUS_SWITCHABLE_NAVIGATION_BAR (window->navigation_bar),
		 search_mode
		 ? NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH
		 : NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION);

	
}

gboolean
nautilus_window_get_search_mode (NautilusWindow *window)
{
	return nautilus_switchable_navigation_bar_get_mode 
		(NAUTILUS_SWITCHABLE_NAVIGATION_BAR (window->navigation_bar)) 
	== NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH;
}

void
nautilus_window_go_web_search (NautilusWindow *window)
{
	char *search_web_uri;

	nautilus_window_set_search_mode (window, FALSE);

	search_web_uri = nautilus_preferences_get (NAUTILUS_PREFERENCES_SEARCH_WEB_URI, DEFAULT_SEARCH_WEB_URI);
	g_assert (search_web_uri != NULL);
	
	nautilus_window_goto_uri (window, search_web_uri);
	g_free (search_web_uri);
}

void
nautilus_window_go_home (NautilusWindow *window)
{
	char *default_home_uri, *home_uri;

	nautilus_window_set_search_mode (window, FALSE);

	default_home_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
	home_uri = nautilus_preferences_get (NAUTILUS_PREFERENCES_HOME_URI, default_home_uri);
	g_free (default_home_uri);
	
	g_assert (home_uri != NULL);
	nautilus_window_goto_uri (window, home_uri);
	g_free (home_uri);
}

void
nautilus_window_allow_back (NautilusWindow *window, gboolean allow)
{
	/* Because of verbs, we set the sensitivity of the menu to
	 * control both the menu and toolbar.
	 */
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_BACK, allow);
}

void
nautilus_window_allow_forward (NautilusWindow *window, gboolean allow)
{
	/* Because of verbs, we set the sensitivity of the menu to
	 * control both the menu and toolbar.
	 */
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_FORWARD, allow);
}

void
nautilus_window_allow_up (NautilusWindow *window, gboolean allow)
{
	/* Because of verbs, we set the sensitivity of the menu to
	 * control both the menu and toolbar.
	 */
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_UP, allow);
}

void
nautilus_window_allow_reload (NautilusWindow *window, gboolean allow)
{
	/* Because of verbs, we set the sensitivity of the menu to
	 * control both the menu and toolbar.
	 */
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_RELOAD, allow);
}

void
nautilus_window_allow_stop (NautilusWindow *window, gboolean allow)
{
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_STOP, allow);
	if (window->throbber != NULL) {
		if (allow) {
			nautilus_throbber_start (NAUTILUS_THROBBER (window->throbber));
		} else {
			nautilus_throbber_stop (NAUTILUS_THROBBER (window->throbber));
		}
	}
}

void
nautilus_send_history_list_changed (void)
{
	gtk_signal_emit_by_name (GTK_OBJECT (nautilus_signaller_get_current ()),
			 	 "history_list_changed");
}

static void
free_history_list (void)
{
	nautilus_gtk_object_list_free (history_list);
	history_list = NULL;
}

void
nautilus_add_to_history_list (NautilusBookmark *bookmark)
{
	/* Note that the history is shared amongst all windows so
	 * this is not a NautilusWindow function. Perhaps it belongs
	 * in its own file.
	 */
	static gboolean free_history_list_is_set_up;
	GList *found_link;

	g_return_if_fail (NAUTILUS_IS_BOOKMARK (bookmark));

	if (!free_history_list_is_set_up) {
		g_atexit (free_history_list);
		free_history_list_is_set_up = TRUE;
	}

	gtk_object_ref (GTK_OBJECT (bookmark));

	/* Compare only the uris here. Comparing the names also is not necessary
	 * and can cause problems due to the asynchronous nature of when the title
	 * of the window is set. 
	 */
	found_link = g_list_find_custom (history_list, 
					 bookmark,
					 nautilus_bookmark_compare_uris);
	
	/* Remove any older entry for this same item. There can be at most 1. */
	if (found_link != NULL) {
		history_list = g_list_remove_link (history_list, found_link);
		gtk_object_unref (found_link->data);
		g_list_free_1 (found_link);
	}

	/* New item goes first. */
	history_list = g_list_prepend (history_list, bookmark);

	/* Tell world that history list has changed. At least all the
	 * NautilusWindows (not just this one) are listening.
	 */
	nautilus_send_history_list_changed ();
}

void
nautilus_window_clear_forward_list (NautilusWindow *window)
{
	nautilus_gtk_object_list_free (window->forward_list);
	window->forward_list = NULL;
}

void
nautilus_window_clear_back_list (NautilusWindow *window)
{
	nautilus_gtk_object_list_free (window->back_list);
	window->back_list = NULL;
}

void
nautilus_forget_history (void) 
{
	GSList *window_node;
	NautilusWindow *window;

	/* Clear out each window's back & forward lists. Also, remove 
	 * each window's current location bookmark from history list 
	 * so it doesn't get clobbered.
	 */
	for (window_node = nautilus_application_windows ();
	     window_node != NULL;
	     window_node = window_node->next) {

		window = NAUTILUS_WINDOW (window_node->data);

		nautilus_window_clear_back_list (window);
		nautilus_window_clear_forward_list (window);

		nautilus_window_allow_back (window, FALSE);
		nautilus_window_allow_forward (window, FALSE);

		history_list = g_list_remove (history_list, window->current_location_bookmark);
	}

	/* Clobber history list. */
	free_history_list ();

	/* Re-add each window's current location to history list. */
	for (window_node = nautilus_application_windows ();
	     window_node != NULL;
	     window_node = window_node->next) {

		window = NAUTILUS_WINDOW (window_node->data);
		nautilus_add_to_history_list (window->current_location_bookmark);
	}
}

GList *
nautilus_get_history_list (void)
{
	return history_list;
}

static void
nautilus_window_open_location_callback (NautilusViewFrame *view,
					const char *location,
					NautilusWindow *window)
{
	nautilus_window_open_location (window, location);
}

static void
nautilus_window_open_location_in_new_window_callback (NautilusViewFrame *view,
						      const char *location,
						      GList *selection,
						      NautilusWindow *window)
{
	nautilus_window_open_location_in_new_window (window, location, selection);
}

static void
nautilus_window_report_selection_change_callback (NautilusViewFrame *view,
						  GList *selection,
						  NautilusWindow *window)
{
	nautilus_window_report_selection_change (window, selection, view);
}

static void
nautilus_window_report_status_callback (NautilusViewFrame *view,
					const char *status,
					NautilusWindow *window)
{
	nautilus_window_report_status (window, status, view);
}

static void
nautilus_window_report_load_underway_callback (NautilusViewFrame *view,
					       NautilusWindow *window)
{
	nautilus_window_report_load_underway (window, view);
}

static void
nautilus_window_report_load_complete_callback (NautilusViewFrame *view,
					       NautilusWindow *window)
{
	nautilus_window_report_load_complete (window, view);
}

static void
nautilus_window_report_load_failed_callback (NautilusViewFrame *view,
					     NautilusWindow *window)
{
	nautilus_window_report_load_failed (window, view);
}

static void
nautilus_window_title_changed_callback (NautilusViewFrame *view, 
					NautilusWindow *window)
{
	nautilus_window_title_changed (window, view);
}

static void
nautilus_window_zoom_level_changed_callback (NautilusViewFrame *view,
                                    	     double zoom_level,
                                    	     NautilusWindow *window)
{
	nautilus_zoom_control_set_zoom_level (NAUTILUS_ZOOM_CONTROL (window->zoom_control), zoom_level);

	/* We rely on the initial zoom_level_change signal to inform us that the
	* view-frame is showing a new zoomable.
	*/
	if (!GTK_WIDGET_VISIBLE (window->zoom_control)) {
		nautilus_zoom_control_set_min_zoom_level
			(NAUTILUS_ZOOM_CONTROL (window->zoom_control),
			 nautilus_view_frame_get_min_zoom_level (view));
		nautilus_zoom_control_set_max_zoom_level
			(NAUTILUS_ZOOM_CONTROL (window->zoom_control),
			 nautilus_view_frame_get_max_zoom_level (view));
		nautilus_zoom_control_set_preferred_zoom_levels
			(NAUTILUS_ZOOM_CONTROL (window->zoom_control),
			 nautilus_view_frame_get_preferred_zoom_levels (view));
			 
		gtk_widget_show (window->zoom_control);

	}

	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_ZOOM_IN,
				       zoom_level < nautilus_view_frame_get_max_zoom_level (view));
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_ZOOM_OUT,
				       zoom_level > nautilus_view_frame_get_min_zoom_level (view));
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_ZOOM_NORMAL,
				       TRUE);
	/* FIXME bugzilla.eazel.com 3442: Desensitize "Zoom Normal"? */
}

static Nautilus_HistoryList *
nautilus_window_get_history_list_callback (NautilusViewFrame *view,
					   NautilusWindow *window)
{
	Nautilus_HistoryList *list;
	NautilusBookmark *bookmark;
	int length, i;
	GList *p;
	char *name, *location;

	/* Get total number of history items */
	length = g_list_length (history_list);

	list = Nautilus_HistoryList__alloc ();

	list->_length = length;
	list->_maximum = length;
	list->_buffer = CORBA_sequence_Nautilus_HistoryItem_allocbuf (length);
	CORBA_sequence_set_release (list, CORBA_TRUE);
	
	/* Iterate through list and copy item data */
	for (i = 0, p = history_list; i < length; i++, p = p->next) {
		bookmark = p->data;

		name = nautilus_bookmark_get_name (bookmark);
		location = nautilus_bookmark_get_uri (bookmark);
		
		list->_buffer[i].title = CORBA_string_dup (name);
		list->_buffer[i].location = CORBA_string_dup (location);
		
		g_free (name);
		g_free (location);
	}

	return list;
}

void
nautilus_window_connect_view (NautilusWindow *window, NautilusViewFrame *view)
{
	GtkObject *view_object;
	
	view_object = GTK_OBJECT (view);

	#define CONNECT(signal) gtk_signal_connect (view_object, #signal, GTK_SIGNAL_FUNC (nautilus_window_##signal##_callback), window)

	CONNECT (open_location);
	CONNECT (open_location_in_new_window);
	CONNECT (report_selection_change);
	CONNECT (report_status);
	CONNECT (report_load_underway);
	CONNECT (report_load_complete);
	CONNECT (report_load_failed);
	CONNECT (title_changed);
	CONNECT (zoom_level_changed);
	CONNECT (get_history_list);

	#undef CONNECT

	/* Can't use connect_object_while_alive here, because
	 * elsewhere disconnect_by_function is used to disconnect the
	 * switched-out content view's signal, and disconnect_by_function
	 * doesn't completely clean up after connect_object_while_alive,
	 * leading to assertion failures later on.
	 */
	gtk_signal_connect_object
		(view_object,
		 "destroy",
		 nautilus_window_view_failed,
		 GTK_OBJECT (window));
	gtk_signal_connect_object
		(view_object,
		 "client_gone",
		 nautilus_window_view_failed,
		 GTK_OBJECT (window));
}

void
nautilus_window_disconnect_view (NautilusWindow *window, NautilusViewFrame *view)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	if (view == NULL) {
		return;
	}

	g_assert (NAUTILUS_IS_VIEW_FRAME (view));

	gtk_signal_disconnect_by_func (GTK_OBJECT(view), 
				       nautilus_window_view_failed, 
				       window);
}

void
nautilus_window_display_error(NautilusWindow *window, const char *error_msg)
{
	GtkWidget *dialog;
	
	dialog = gnome_message_box_new (error_msg, GNOME_MESSAGE_BOX_ERROR, _("Close"), NULL);
	gnome_dialog_set_close (GNOME_DIALOG(dialog), TRUE);
	
	gnome_dialog_set_default (GNOME_DIALOG(dialog), 0);
	
	gtk_widget_show (dialog);
}

void
nautilus_window_set_content_view_widget (NautilusWindow *window,
					 NautilusViewFrame *new_view)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	g_return_if_fail (new_view == NULL || NAUTILUS_IS_VIEW_FRAME (new_view));
	
	if (new_view == window->content_view) {
		return;
	}
	
	if (window->content_view != NULL) {
		gtk_container_remove (GTK_CONTAINER (window->content_hbox),
				      GTK_WIDGET (window->content_view));      
		window->content_view = NULL;
	}

	/* Here's an explicit check for a problem that happens all too often. */
#ifdef UIH
	if (bonobo_ui_handler_menu_path_exists (window->ui_handler, "/File/Open")) {
		g_warning ("There's a lingering Open menu item. This usually means a new Bonobo bug.");
	}
#endif
	
	if (new_view != NULL) {			
		gtk_widget_show (GTK_WIDGET (new_view));
		
		nautilus_view_frame_activate (new_view); 

		/* FIXME bugzilla.eazel.com 1243: 
		 * We should use inheritance instead of these special cases
		 * for the desktop window.
		 */
		if (!E_IS_PANED (window->content_hbox)) {
			gtk_container_add (GTK_CONTAINER (window->content_hbox),
					   GTK_WIDGET (new_view));
		} else {
			e_paned_pack2 (E_PANED (window->content_hbox),
				       GTK_WIDGET (new_view),
				       TRUE, FALSE);
		}
	}

	window->content_view = new_view;
}

/* reload the contents of the window */
void
nautilus_window_reload (NautilusWindow *window)
{
	nautilus_window_begin_location_change
		(window, window->location,
		 NAUTILUS_LOCATION_CHANGE_RELOAD, 0);
}

/**
 * update_sidebar_panels_from_preferences:
 * @window:	A NautilusWindow
 *
 * Update the current list of sidebar panels from preferences.   
 *
 * Disabled panels are removed if they are already in the list.
 *
 * Enabled panels are added if they are not already in the list.
 *
 */
static void
update_sidebar_panels_from_preferences (NautilusWindow *window)
{
	GList *identifier_list;

	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Obtain list of enabled view identifiers */
	identifier_list = nautilus_global_preferences_get_enabled_sidebar_panel_view_identifiers ();
	nautilus_window_set_sidebar_panels (window, identifier_list);
	nautilus_view_identifier_list_free (identifier_list);
}

/**
 * sidebar_panels_changed_callback:
 * @user_data:	Callback data
 *
 * Called when enabled/disabled preferences change for any
 * sidebar panel.
 */
static void
sidebar_panels_changed_callback (gpointer user_data)
{
	update_sidebar_panels_from_preferences (NAUTILUS_WINDOW (user_data));
}

static void 
show_dock_item (NautilusWindow *window, const char *dock_item_path)
{
	if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		return;
	}
	nautilus_bonobo_set_hidden (window->details->shell_ui,
				    dock_item_path,
				    FALSE);
	nautilus_window_update_show_hide_menu_items (window);
}

static void 
hide_dock_item (NautilusWindow *window, const char *dock_item_path)
{
	
	nautilus_bonobo_set_hidden (window->details->shell_ui,
				    dock_item_path,
				    TRUE);
	nautilus_window_update_show_hide_menu_items (window);
}

static gboolean
dock_item_showing (NautilusWindow *window, const char *dock_item_path)
{
	return !nautilus_bonobo_get_hidden (window->details->shell_ui,
					    dock_item_path);
}

void 
nautilus_window_hide_location_bar (NautilusWindow *window)
{
	hide_dock_item (window, LOCATION_BAR_PATH);
}

void 
nautilus_window_show_location_bar (NautilusWindow *window)
{
	show_dock_item (window, LOCATION_BAR_PATH);
}

gboolean
nautilus_window_location_bar_showing (NautilusWindow *window)
{
	return dock_item_showing (window, LOCATION_BAR_PATH);
}

void 
nautilus_window_hide_tool_bar (NautilusWindow *window)
{
	hide_dock_item (window, TOOL_BAR_PATH);
}

void 
nautilus_window_show_tool_bar (NautilusWindow *window)
{
	show_dock_item (window, TOOL_BAR_PATH);
}

gboolean
nautilus_window_tool_bar_showing (NautilusWindow *window)
{
	return dock_item_showing (window, TOOL_BAR_PATH);
}

void 
nautilus_window_hide_sidebar (NautilusWindow *window)
{
	gtk_widget_hide (GTK_WIDGET (window->sidebar));
	if (window->content_hbox != NULL && !NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		e_paned_set_position (E_PANED (window->content_hbox), 0);
	}
	nautilus_window_update_show_hide_menu_items (window);
}

void 
nautilus_window_show_sidebar (NautilusWindow *window)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (window->sidebar);
	gtk_widget_show (widget);
	if (window->content_hbox != NULL && !NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		e_paned_set_position (E_PANED (window->content_hbox), widget->allocation.width);
		/* Make sure sidebar is not in collapsed form also */
		nautilus_horizontal_splitter_expand (NAUTILUS_HORIZONTAL_SPLITTER (window->content_hbox));
	}
	nautilus_window_update_show_hide_menu_items (window);
}

gboolean
nautilus_window_sidebar_showing (NautilusWindow *window)
{
	return GTK_WIDGET_VISIBLE (window->sidebar);
}

void 
nautilus_window_hide_status_bar (NautilusWindow *window)
{
	hide_dock_item (window, STATUS_BAR_PATH);

	nautilus_window_update_show_hide_menu_items (window);
}

void 
nautilus_window_show_status_bar (NautilusWindow *window)
{
	show_dock_item (window, STATUS_BAR_PATH);

	nautilus_window_update_show_hide_menu_items (window);
}

gboolean
nautilus_window_status_bar_showing (NautilusWindow *window)
{
	return dock_item_showing (window, STATUS_BAR_PATH);
}

/**
 * nautilus_window_get_base_page_index:
 * @window:	Window to get index from
 *
 * Returns the index of the base page in the history list.
 * Base page is not the currently displayed page, but the page
 * that acts as the base from which the back and forward commands
 * navigate from.
 */
gint 
nautilus_window_get_base_page_index (NautilusWindow *window)
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
 * nautilus_window_show:
 * @widget:	GtkWidget
 *
 * Call parent and then show/hide window items
 * base on user prefs.
 */
static void
nautilus_window_show (GtkWidget *widget)
{	
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (widget);

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, show, (widget));
	
	/* Initially show or hide views based on preferences; once the window is displayed
	 * these can be controlled on a per-window basis from View menu items. 
	 */
	if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_TOOL_BAR, TRUE)) {
		nautilus_window_show_tool_bar (window);
	} else {
		nautilus_window_hide_tool_bar (window);
	}

	if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR, TRUE)) {
		nautilus_window_show_location_bar (window);
	} else {
		nautilus_window_hide_location_bar (window);
	}

	if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR, TRUE)) {
		nautilus_window_show_status_bar (window);
	} else {
		nautilus_window_hide_status_bar (window);
	}

	if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_SIDEBAR, TRUE)) {
		nautilus_window_show_sidebar (window);
	} else {
		nautilus_window_hide_sidebar (window);
	}	
}
