/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
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
#include "nautilus-sidebar.h"
#include "nautilus-signaller.h"
#include "nautilus-switchable-navigation-bar.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-zoom-control.h"
#include <X11/Xatom.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-ui-util.h>
#include <ctype.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-generous-bin.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
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
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-geometry.h>
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

#define STATUS_BAR_CLEAR_TIMEOUT 10000	/* milliseconds */
#define MAX_HISTORY_ITEMS 50

/* dock items */
#define LOCATION_BAR_PATH	"/Location Bar"
#define TOOLBAR_PATH            "/Toolbar"
#define STATUS_BAR_PATH         "/status"
#define MENU_BAR_PATH           "/menu"

#define COMMAND_PREFIX                          "/commands/"
#define NAUTILUS_COMMAND_TOGGLE_FIND_MODE	"/commands/Toggle Find Mode"
#define NAUTILUS_COMMAND_VIEW_AS		"/commands/View as"

#define NAUTILUS_MENU_PATH_EXTRA_VIEWER_PLACEHOLDER	"/menu/View/View Choices/Extra Viewer"
#define NAUTILUS_MENU_PATH_BEFORE_SHORT_LIST_SEPARATOR  "/menu/View/View Choices/Before Short List"
#define NAUTILUS_MENU_PATH_SHORT_LIST_PLACEHOLDER  	"/menu/View/View Choices/Short List"
#define NAUTILUS_MENU_PATH_AFTER_SHORT_LIST_SEPARATOR   "/menu/View/View Choices/Before Short List"

enum {
	ARG_0,
	ARG_APP_ID,
	ARG_APP
};

static GList *history_list = NULL;

static GdkPixmap *mini_icon_pixmap;
static GdkBitmap *mini_icon_mask;

static void nautilus_window_initialize_class          (NautilusWindowClass *klass);
static void nautilus_window_initialize                (NautilusWindow      *window);
static void nautilus_window_destroy                   (GtkObject           *object);
static void nautilus_window_set_arg                   (GtkObject           *object,
						       GtkArg              *arg,
						       guint                arg_id);
static void nautilus_window_get_arg                   (GtkObject           *object,
						       GtkArg              *arg,
						       guint                arg_id);
static void nautilus_window_size_request              (GtkWidget           *widget,
						       GtkRequisition      *requisition);
static void nautilus_window_realize                   (GtkWidget           *widget);
static void update_sidebar_panels_from_preferences    (NautilusWindow      *window);
static void sidebar_panels_changed_callback           (gpointer             user_data);
static void nautilus_window_show                      (GtkWidget           *widget);
static void nautilus_window_unrealize                 (GtkWidget           *widget);
static void cancel_view_as_callback                   (NautilusWindow      *window);
static void real_add_current_location_to_history_list (NautilusWindow      *window);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusWindow,
			      nautilus_window,
			      BONOBO_TYPE_WINDOW)

static void
unref_mini_icon (void)
{
	gdk_pixmap_unref (mini_icon_pixmap);
	if (mini_icon_mask != NULL) {
		gdk_bitmap_unref (mini_icon_mask);
	}
}

static void
nautilus_window_initialize_class (NautilusWindowClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	char *filename;
	GdkPixbuf *pixbuf;
	
	object_class = (GtkObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	object_class->destroy = nautilus_window_destroy;
	object_class->get_arg = nautilus_window_get_arg;
	object_class->set_arg = nautilus_window_set_arg;
	
	widget_class->show = nautilus_window_show;
	widget_class->unrealize = nautilus_window_unrealize;
	
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

	klass->add_current_location_to_history_list
		= real_add_current_location_to_history_list;

        filename = nautilus_pixmap_file ("nautilus-mini-logo.png");
        if (filename != NULL) {
                pixbuf = gdk_pixbuf_new_from_file (filename);
                if (pixbuf != NULL) {
                        gdk_pixbuf_render_pixmap_and_mask
				(pixbuf, &mini_icon_pixmap, &mini_icon_mask,
				 EEL_STANDARD_ALPHA_THRESHHOLD);
			gdk_pixbuf_unref (pixbuf);
			g_atexit (unref_mini_icon);
		}
        	g_free (filename);
	}
}

static void
add_sidebar_panel_callback (const char *name,
			    const char *iid,
			    const char *preference_key,
			    gpointer callback_data) 
{
	g_return_if_fail (name != NULL);
	g_return_if_fail (iid != NULL);
	g_return_if_fail (preference_key != NULL);
	g_return_if_fail (NAUTILUS_IS_WINDOW (callback_data));
	
	eel_preferences_add_callback_while_alive (preference_key,
						  sidebar_panels_changed_callback,
						  callback_data,
						  GTK_OBJECT (callback_data));
}

static void
nautilus_window_initialize (NautilusWindow *window)
{
	window->details = g_new0 (NautilusWindowDetails, 1);

	gtk_quit_add_destroy (1, GTK_OBJECT (window));

	/* Keep track of changes in enabled state of sidebar panels */
	nautilus_sidebar_for_each_panel (add_sidebar_panel_callback, window);

	/* Keep the main event loop alive as long as the window exists */
	nautilus_main_event_loop_register (GTK_OBJECT (window));
}

static gint
ui_idle_handler (gpointer data)
{
	NautilusWindow *window;
	gboolean old_updating_bonobo_state;

	window = data;

	g_assert (window->details->ui_change_depth == 0);

	/* Simulate an extra freeze/thaw so that calling window_ui_freeze
	 * and thaw from within the idle handler doesn't try to remove it
	 * (the already running idle handler)
	 */
	window->details->ui_change_depth++;
	old_updating_bonobo_state = window->details->updating_bonobo_state;

	if (window->details->ui_pending_initialize_menus_part_2) {
		nautilus_window_initialize_menus_part_2 (window);
		window->details->ui_pending_initialize_menus_part_2 = FALSE;
	}

	if (window->details->ui_is_frozen) {
		window->details->updating_bonobo_state = TRUE;
		bonobo_ui_engine_thaw (bonobo_ui_container_get_engine (window->details->ui_container));
		window->details->ui_is_frozen = FALSE;
		window->details->updating_bonobo_state = old_updating_bonobo_state;
	}

	window->details->ui_change_depth--;

	window->details->ui_idle_id = 0;
	return FALSE;
}

static inline void
ui_install_idle_handler (NautilusWindow *window)
{
	if (window->details->ui_idle_id == 0) {
		window->details->ui_idle_id = gtk_idle_add_priority (GTK_PRIORITY_LOW, ui_idle_handler, window);
	}
}

static inline void
ui_remove_idle_handler (NautilusWindow *window)
{
	if (window->details->ui_idle_id != 0) {
		gtk_idle_remove (window->details->ui_idle_id);
		window->details->ui_idle_id = 0;
	}
}

/* Register that BonoboUI changes are going to be made to WINDOW. The UI
 * won't actually be synchronised until some arbitrary date in the future.
 */
void
nautilus_window_ui_freeze (NautilusWindow *window)
{
	if (window->details->ui_change_depth == 0) {
		ui_remove_idle_handler (window);
	}

	if (!window->details->ui_is_frozen) {
		bonobo_ui_engine_freeze (bonobo_ui_container_get_engine (window->details->ui_container));
		window->details->ui_is_frozen = TRUE;
	}

	window->details->ui_change_depth++;
}

/* Register that the BonoboUI changes for WINDOW have finished. There _must_
 * be one and only one call to this function for every call to
 * starting_ui_change ()
 */
void
nautilus_window_ui_thaw (NautilusWindow *window)
{
	window->details->ui_change_depth--;

	g_assert (window->details->ui_change_depth >= 0);

	if (window->details->ui_change_depth == 0
	    && (window->details->ui_is_frozen
		|| window->details->ui_pending_initialize_menus_part_2)) {
		ui_install_idle_handler (window);
	}
}

/* Unconditionally synchronize the BonoboUI of WINDOW. */
static void
nautilus_window_ui_update (NautilusWindow *window)
{
	BonoboUIEngine *engine;
	gboolean old_updating_bonobo_state;

	engine = bonobo_ui_container_get_engine (window->details->ui_container);
	old_updating_bonobo_state = window->details->updating_bonobo_state;

	window->details->updating_bonobo_state = TRUE;
	if (window->details->ui_is_frozen) {
		bonobo_ui_engine_thaw (engine);
		if (window->details->ui_change_depth == 0) {
			window->details->ui_is_frozen = FALSE;
			if (!window->details->ui_pending_initialize_menus_part_2) {
				ui_remove_idle_handler (window);
			}
		} else {
			bonobo_ui_engine_freeze (engine);
		}
	} else {
		bonobo_ui_engine_update (engine);
	}
	window->details->updating_bonobo_state = old_updating_bonobo_state;
}

static gboolean
nautilus_window_clear_status (gpointer callback_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (callback_data);

	nautilus_window_ui_freeze (window);

	/* FIXME bugzilla.gnome.org 43597:
	 * Should pass "" or NULL here. This didn't work, then did, now doesn't again.
	 * When this is fixed in Bonobo we should change this line.
	 */
	bonobo_ui_component_set_status (window->details->shell_ui, " ", NULL);
	window->status_bar_clear_id = 0;

	nautilus_window_ui_thaw (window);
	return FALSE;
}

void
nautilus_window_set_status (NautilusWindow *window, const char *text)
{
	if (window->status_bar_clear_id != 0) {
		g_source_remove (window->status_bar_clear_id);
	}
	
	nautilus_window_ui_freeze (window);

	if (text != NULL && text[0] != '\0') {
		bonobo_ui_component_set_status (window->details->shell_ui, text, NULL);
		window->status_bar_clear_id = g_timeout_add
			(STATUS_BAR_CLEAR_TIMEOUT, nautilus_window_clear_status, window);
	} else {
		nautilus_window_clear_status (window);
		window->status_bar_clear_id = 0;
	}

	nautilus_window_ui_thaw (window);
}

void
nautilus_window_go_to (NautilusWindow *window, const char *uri)
{
	nautilus_window_open_location (window, uri);
}

char *
nautilus_window_get_location (NautilusWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

	return g_strdup (window->details->location);
}

static void
go_to_callback (GtkWidget *widget,
		const char *uri,
		NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	nautilus_window_go_to (window, uri);
}

static void
navigation_bar_location_changed_callback (GtkWidget *widget,
					  const char *uri,
					  NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	if (window->details->temporary_navigation_bar) {
		if (nautilus_window_location_bar_showing (window)) {
			nautilus_window_hide_location_bar (window);
		}
		window->details->temporary_navigation_bar = FALSE;
	}

	nautilus_window_go_to (window, uri);
}

static void
navigation_bar_mode_changed_callback (GtkWidget *widget,
				      NautilusSwitchableNavigationBarMode mode,
				      NautilusWindow *window)
{
	window->details->updating_bonobo_state = TRUE;

	g_assert (mode == NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION 
		  || mode == NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH);

	nautilus_window_ui_freeze (window);

	nautilus_bonobo_set_toggle_state (window->details->shell_ui,
					  NAUTILUS_COMMAND_TOGGLE_FIND_MODE,
					  mode == NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH);
	
	window->details->updating_bonobo_state = FALSE;

	nautilus_window_ui_thaw (window);
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
menu_bar_no_resize_hack_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	/* do nothing */
}

static void
menu_bar_no_resize_hack_menu_bar_finder (GtkWidget *child, gpointer data)
{
	if (GTK_IS_MENU_BAR (child)) {
		* (GtkObject **) data = GTK_OBJECT (child);
	}
}

/* Since there's only one desktop at a time, we can keep track
 * of our faux-class with a single static.
 */
static GtkObjectClass *menu_bar_no_resize_hack_class;

static void
menu_bar_no_resize_hack_class_free (void)
{
	g_free (menu_bar_no_resize_hack_class);
}

/* This call is used to keep the desktop menu bar from resizing.
 * It patches out its class with one where the size_allocate
 * method has been replaced with a no-op.
 */
static void
menu_bar_no_resize_hack (NautilusWindow *window)
{
	GtkObject *menu_bar;
	GtkTypeQuery *type_query;
	
	menu_bar = NULL;
	
	eel_gtk_container_foreach_deep (GTK_CONTAINER (window),
					     menu_bar_no_resize_hack_menu_bar_finder,
					     &menu_bar);

	g_return_if_fail (menu_bar != NULL);

	if (menu_bar_no_resize_hack_class == NULL) {
		g_atexit (menu_bar_no_resize_hack_class_free);
	}
	
	type_query = gtk_type_query (menu_bar->klass->type);
	g_free (menu_bar_no_resize_hack_class);
	menu_bar_no_resize_hack_class = g_memdup (menu_bar->klass, type_query->class_size);
	g_free (type_query);
	
	((GtkWidgetClass *) menu_bar_no_resize_hack_class)->size_allocate
		= menu_bar_no_resize_hack_size_allocate;

	menu_bar->klass = menu_bar_no_resize_hack_class;
}

static gboolean
location_change_at_idle_callback (gpointer callback_data)
{
	NautilusWindow *window;
	char *location;

	window = NAUTILUS_WINDOW (callback_data);

	location = window->details->location_to_change_to_at_idle;
	window->details->location_to_change_to_at_idle = NULL;
	window->details->location_change_at_idle_id = 0;

	nautilus_window_go_to (window, location);
	g_free (location);

	return FALSE;
}

/* handle bonobo events from the throbber -- since they can come in at
   any time right in the middle of things, defer until idle */
static void 
throbber_location_change_request_callback (BonoboListener *listener,
					   char *event_name, 
					   CORBA_any *arg,
					   CORBA_Environment *ev,
					   gpointer callback_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (callback_data);

	g_free (window->details->location_to_change_to_at_idle);
	window->details->location_to_change_to_at_idle = g_strdup (BONOBO_ARG_GET_STRING (arg));

	if (window->details->location_change_at_idle_id == 0) {
		window->details->location_change_at_idle_id =
			gtk_idle_add (location_change_at_idle_callback, window);
	}
}

/* Add a dummy menu with a "View as ..." item when we first create the
 * view_as_option_menu -- without this the menu draws empty and shrunk,
 * once we populate it it grows and forces the toolbar and all the other
 * views to re-layout.
 */
static void
set_dummy_initial_view_as_menu (NautilusWindow *window)
{
	GtkWidget *new_menu;
	GtkWidget *menu_item;

	new_menu = gtk_menu_new ();
	menu_item = gtk_menu_item_new_with_label (_("View as..."));
       	gtk_widget_show (menu_item);
       	gtk_menu_append (GTK_MENU (new_menu), menu_item);

        gtk_option_menu_set_menu (GTK_OPTION_MENU (window->view_as_option_menu),
                                  new_menu);
}

static void
nautilus_window_constructed (NautilusWindow *window)
{
	GtkWidget *location_bar_box;
	GtkWidget *view_as_menu_vbox;
  	int sidebar_width;
	BonoboControl *location_bar_wrapper;
	EPaned *panel;
	CORBA_Environment ev;
	Bonobo_PropertyBag property_bag;
	
	/* CORBA and Bonobo setup, which must be done before the location bar setup */
	window->details->ui_container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (window->details->ui_container,
				     BONOBO_WINDOW (window));

	/* Load the user interface from the XML file. */
	window->details->shell_ui = bonobo_ui_component_new ("Nautilus Shell");
	bonobo_ui_component_set_container
		(window->details->shell_ui,
		 nautilus_window_get_ui_container (window));

	nautilus_window_ui_freeze (window);

	bonobo_ui_component_freeze (window->details->shell_ui, NULL);
	bonobo_ui_util_set_ui (window->details->shell_ui,
			       DATADIR,
			       "nautilus-shell-ui.xml",
			       "nautilus");
	bonobo_ui_component_thaw (window->details->shell_ui, NULL);
	
	/* set up location bar */
	location_bar_box = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (location_bar_box), GNOME_PAD_SMALL);
	
	window->navigation_bar = nautilus_switchable_navigation_bar_new (window);
	gtk_widget_show (GTK_WIDGET (window->navigation_bar));

	gtk_signal_connect (GTK_OBJECT (window->navigation_bar), "location_changed",
			    navigation_bar_location_changed_callback, window);

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
	
	window->view_as_option_menu = gtk_option_menu_new ();
	gtk_box_pack_end (GTK_BOX (view_as_menu_vbox), window->view_as_option_menu, TRUE, FALSE, 0);
	gtk_widget_show (window->view_as_option_menu);
 	/* Make sure there is something in the option menu to prevent it from
 	 * growing later.
 	 */
	set_dummy_initial_view_as_menu (window);
	
	/* Allocate the zoom control and place on the right next to the menu.
	 * It gets shown later, if the view-frame contains something zoomable.
	 */
	window->zoom_control = nautilus_zoom_control_new ();
	gtk_signal_connect_object (GTK_OBJECT (window->zoom_control), "zoom_in",
				   nautilus_window_zoom_in, GTK_OBJECT (window));
	gtk_signal_connect_object (GTK_OBJECT (window->zoom_control), "zoom_out",
				   nautilus_window_zoom_out, GTK_OBJECT (window));
	gtk_signal_connect_object (GTK_OBJECT (window->zoom_control), "zoom_to_level",
				   nautilus_window_zoom_to_level, GTK_OBJECT (window));
	gtk_signal_connect_object (GTK_OBJECT (window->zoom_control), "zoom_to_fit",
				   nautilus_window_zoom_to_fit, GTK_OBJECT (window));
	gtk_box_pack_end (GTK_BOX (location_bar_box), window->zoom_control, FALSE, FALSE, 0);
	
	gtk_widget_show (location_bar_box);
	
	/* FIXME bugzilla.gnome.org 41243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
        if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		window->content_hbox = gtk_widget_new (EEL_TYPE_GENEROUS_BIN, NULL);
	} else {
		set_initial_window_geometry (window);
	
		window->content_hbox = nautilus_horizontal_splitter_new ();
		panel = E_PANED (window->content_hbox);
		
		/* FIXME bugzilla.gnome.org 41245: Saved in pixels instead of in %? */
		/* FIXME bugzilla.gnome.org 41245: No reality check on the value? */
		sidebar_width = eel_preferences_get_integer (NAUTILUS_PREFERENCES_SIDEBAR_WIDTH);
		e_paned_set_position (E_PANED (window->content_hbox), sidebar_width);
	}
	gtk_widget_show (window->content_hbox);
	bonobo_window_set_contents (BONOBO_WINDOW (window), window->content_hbox);
	
	/* FIXME bugzilla.gnome.org 41243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
        if (!NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		/* set up the sidebar */
		window->sidebar = nautilus_sidebar_new ();
		gtk_widget_show (GTK_WIDGET (window->sidebar));
		gtk_signal_connect (GTK_OBJECT (window->sidebar), "location_changed",
				    go_to_callback, window);
		e_paned_pack1 (E_PANED (window->content_hbox),
			       GTK_WIDGET (window->sidebar),
			       FALSE, TRUE);
#if 0
		bonobo_ui_engine_add_sync (bonobo_window_get_ui_engine (BONOBO_WINDOW (window)),
					   sidebar_sync);
#endif
	}
	
	bonobo_ui_component_freeze (window->details->shell_ui, NULL);

	/* FIXME bugzilla.gnome.org 41243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
	if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		nautilus_bonobo_set_hidden (window->details->shell_ui,
					    LOCATION_BAR_PATH, TRUE);
		nautilus_bonobo_set_hidden (window->details->shell_ui,
					    TOOLBAR_PATH, TRUE);
		nautilus_bonobo_set_hidden (window->details->shell_ui,
					    STATUS_BAR_PATH, TRUE);
		nautilus_bonobo_set_hidden (window->details->shell_ui,
					    MENU_BAR_PATH, TRUE);

		/* FIXME bugzilla.gnome.org 44752:
		 * If we ever get the unsigned math errors in
		 * gtk_menu_item_size_allocate fixed this can be removed.
		 */
		menu_bar_no_resize_hack (window);
	}

	/* Wrap the location bar in a control and set it up. */
	location_bar_wrapper = bonobo_control_new (location_bar_box);
	bonobo_ui_component_object_set (window->details->shell_ui,
					"/Location Bar/Wrapper",
					bonobo_object_corba_objref (BONOBO_OBJECT (location_bar_wrapper)),
					NULL);
	bonobo_ui_component_thaw (window->details->shell_ui, NULL);
	bonobo_object_unref (BONOBO_OBJECT (location_bar_wrapper));

	/* initalize the menus and toolbars */
	nautilus_window_initialize_menus_part_1 (window);
	nautilus_window_initialize_toolbars (window);

	/* We'll do the second part later (bookmarks and go menus) */
	window->details->ui_pending_initialize_menus_part_2 = TRUE;

	/* watch for throbber location changes, too */
	if (window->throbber != CORBA_OBJECT_NIL) {
		CORBA_exception_init (&ev);
		property_bag = Bonobo_Control_getProperties (window->throbber, &ev);
		if (!BONOBO_EX (&ev) && property_bag != CORBA_OBJECT_NIL) {
			window->details->throbber_location_change_request_listener_id =
				bonobo_event_source_client_add_listener
				(property_bag, throbber_location_change_request_callback, 
				 "Bonobo/Property:change:location", NULL, window); 
			bonobo_object_release_unref (property_bag, NULL);	
		}
		CORBA_exception_free (&ev);
	}
	
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

	/* Register that things may be dragged from this window */
	nautilus_drag_window_register (GTK_WINDOW (window));

	nautilus_window_ui_thaw (window);
}

static void
nautilus_window_set_arg (GtkObject *object,
			 GtkArg *arg,
			 guint arg_id)
{
	char *old_name;
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (object);
	
	switch (arg_id) {
	case ARG_APP_ID:
		if (GTK_VALUE_STRING (*arg) == NULL) {
			return;
		}
		old_name = bonobo_window_get_name (BONOBO_WINDOW (window));
		bonobo_window_set_name (BONOBO_WINDOW (window), GTK_VALUE_STRING (*arg));
		/* This hack of using the time when the name first
		 * goes non-NULL to be window-constructed time is
		 * completely lame. But it works, so for now we leave
		 * it alone.
		 */
		if (old_name == NULL) {
			nautilus_window_constructed (window);
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
	switch (arg_id) {
	case ARG_APP_ID:
		GTK_VALUE_STRING (*arg) = bonobo_window_get_name (BONOBO_WINDOW (object));
		break;
	case ARG_APP:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT (NAUTILUS_WINDOW (object)->application);
		break;
	}
}

static void
free_stored_viewers (NautilusWindow *window)
{
	eel_g_list_free_deep_custom (window->details->short_list_viewers, 
				     (GFunc) nautilus_view_identifier_free, 
				     NULL);
	window->details->short_list_viewers = NULL;
	nautilus_view_identifier_free (window->details->extra_viewer);
	window->details->extra_viewer = NULL;
}

static void
nautilus_window_unrealize (GtkWidget *widget)
{
	NautilusWindow *window;
	CORBA_Environment ev;
	Bonobo_PropertyBag property_bag;
	
	window = NAUTILUS_WINDOW (widget);

	/* Get rid of the throbber explicitly before it self-destructs
	 * (which it will do when the control frame goes away.
	 */
	if (window->throbber != CORBA_OBJECT_NIL) {
		CORBA_exception_init (&ev);
		property_bag = Bonobo_Control_getProperties (window->throbber, &ev);
		if (!BONOBO_EX (&ev) && property_bag != CORBA_OBJECT_NIL) {	
			bonobo_event_source_client_remove_listener
				(property_bag,
				 window->details->throbber_location_change_request_listener_id,
				 &ev);
			bonobo_object_release_unref (property_bag, NULL);	
		}
		CORBA_exception_free (&ev);

		bonobo_object_release_unref (window->throbber, NULL);

		window->throbber = CORBA_OBJECT_NIL;
	}

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, unrealize, (widget));
}

static void 
nautilus_window_destroy (GtkObject *object)
{
	NautilusWindow *window;
	
	window = NAUTILUS_WINDOW (object);

	/* Handle the part of destroy that's private to the view
	 * management.
	 */
	nautilus_window_manage_views_destroy (window);

	/* Get rid of all callbacks. */
	nautilus_window_set_viewed_file (window, NULL);
	nautilus_window_remove_bookmarks_menu_callback (window);
	nautilus_window_remove_go_menu_callback (window);
	nautilus_window_toolbar_remove_theme_callback (window);

	if (window->details->ui_idle_id != 0) {
		gtk_idle_remove (window->details->ui_idle_id);
	}

	/* Get rid of all owned objects. */

	if (window->details->shell_ui != NULL) {
		bonobo_ui_component_unset_container (window->details->shell_ui);
		bonobo_object_unref (BONOBO_OBJECT (window->details->shell_ui));
	}

	nautilus_file_unref (window->details->viewed_file);

	g_list_free (window->sidebar_panels);

	free_stored_viewers (window);
	
	g_free (window->details->location);
	eel_g_list_free_deep (window->details->selection);
	eel_g_list_free_deep (window->details->pending_selection);

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

	if (window->details->location_change_at_idle_id != 0) {
		gtk_idle_remove (window->details->location_change_at_idle_id);
	}

	g_free (window->details->title);
	
	g_free (window->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_window_save_geometry (NautilusWindow *window)
{
	char *geometry_string;

        g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	g_return_if_fail (GTK_WIDGET_VISIBLE (window));

        geometry_string = gnome_geometry_string (GTK_WIDGET (window)->window);
	nautilus_file_set_metadata (window->details->viewed_file,
				    NAUTILUS_METADATA_KEY_WINDOW_GEOMETRY,
				    NULL,
				    geometry_string);
	g_free (geometry_string);
}

void
nautilus_window_close (NautilusWindow *window)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	/* Save the window position in the directory's metadata only if
	 * we're in every-location-in-its-own-window mode. Otherwise it
	 * would be too apparently random when the stored positions change.
	 */
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW)) {
	        nautilus_window_save_geometry (window);
	}

	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
nautilus_window_update_launcher (GdkWindow *window)
{
	struct timeval tmp;
	
	gettimeofday (&tmp, NULL);

	/* Set a property on the root window to the time of day in seconds.
	 * The launcher will monitor the root window for this property change
	 * to update its launching state */
	gdk_property_change (GDK_ROOT_PARENT (),
			     gdk_atom_intern ("_NAUTILUS_LAST_WINDOW_REALIZE_TIME", FALSE),
			     XA_CARDINAL,
			     32,
			     PropModeReplace,
			     (guchar *) &tmp.tv_sec,
			     1);
}

static void
nautilus_window_realize (GtkWidget *widget)
{
	char *filename;

        /* Create our GdkWindow */
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, realize, (widget));

        /* Set the mini icon */
	if (mini_icon_pixmap != NULL) {
		eel_set_mini_icon (widget->window, mini_icon_pixmap, mini_icon_mask);
	}

	/* Set the maxi icon */
	filename = gnome_pixmap_file ("nautilus-launch-icon.png");
	if (filename != NULL) {
		gnome_window_icon_set_from_file (GTK_WINDOW (widget),
						 filename);
		g_free (filename);
	}

	/* Notify the launcher that our window has been realized */
	nautilus_window_update_launcher (widget->window);
}

static void
nautilus_window_size_request (GtkWidget		*widget,
			      GtkRequisition	*requisition)
{
	guint max_width;
	guint max_height;

	g_return_if_fail (NAUTILUS_IS_WINDOW (widget));
	g_return_if_fail (requisition != NULL);

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_request, (widget, requisition));

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

	if (requisition->width > (int) max_width) {
		requisition->width = max_width;
	}
	
	if (requisition->height > (int) max_height) {
		requisition->height = max_height;
	}
}


/*
 * Main API
 */

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
handle_view_as_item_from_bonobo_menu (NautilusWindow *window, const char *id)
{
	char *container_path;

	container_path = nautilus_bonobo_get_numbered_menu_item_container_path_from_command (id);

	if (eel_strcmp (container_path, NAUTILUS_MENU_PATH_SHORT_LIST_PLACEHOLDER) == 0) {
		activate_nth_short_list_item 
			(window,
			 nautilus_bonobo_get_numbered_menu_item_index_from_command (id));
	} else if (eel_strcmp (container_path, NAUTILUS_MENU_PATH_EXTRA_VIEWER_PLACEHOLDER) == 0) {
		g_return_if_fail 
			(nautilus_bonobo_get_numbered_menu_item_index_from_command (id) == 0);
		activate_extra_viewer (window);
	}

	g_free (container_path);
}

void
nautilus_window_handle_ui_event_callback (BonoboUIComponent *ui,
		 		 	  const char *id,
		 		 	  Bonobo_UIComponent_EventType type,
		 		 	  const char *state,
		 		 	  NautilusWindow *window)
{
	if (!window->details->updating_bonobo_state
	    && type == Bonobo_UIComponent_STATE_CHANGED
	    && strcmp (state, "1") == 0) {
	    	handle_view_as_item_from_bonobo_menu (window, id);
	}
}		 

static void
view_as_menu_switch_views_callback (GtkWidget *widget, gpointer data)
{
        NautilusWindow *window;
        int viewer_index;
        
        g_return_if_fail (GTK_IS_MENU_ITEM (widget));
        g_return_if_fail (NAUTILUS_IS_WINDOW (data));

        window = NAUTILUS_WINDOW (data);

        if (GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (widget), "extra viewer")) == TRUE) {
        	activate_extra_viewer (window);
        } else {
		viewer_index = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (widget), "viewer index"));
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
	menu_item = gtk_menu_item_new_with_label (menu_label);
	g_free (menu_label);

	gtk_signal_connect (GTK_OBJECT (menu_item),
			    "activate",
			    view_as_menu_switch_views_callback, 
			    window);

	gtk_object_set_data (GTK_OBJECT (menu_item),
			     "viewer index",
			     GINT_TO_POINTER (index));

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
add_view_as_bonobo_menu_item (NautilusWindow *window,
			      const char *placeholder_path,
			      NautilusViewIdentifier *identifier,
			      int index)
{
	char *tip;
	char *item_path;

	nautilus_bonobo_add_numbered_radio_menu_item
		(window->details->shell_ui,
		 placeholder_path,
		 index,
		 identifier->view_as_label,
		 "viewers group");

	tip = g_strdup_printf (_("Display this location with \"%s\""),
			       identifier->viewer_label);
	item_path = nautilus_bonobo_get_numbered_menu_item_path
		(window->details->shell_ui, 
		 placeholder_path, 
		 index);
	nautilus_bonobo_set_tip (window->details->shell_ui, item_path, tip);
	g_free (item_path);
	g_free (tip);
}

static void
remove_first_child (GtkContainer *container)
{
	gtk_container_remove (container,
			      eel_gtk_container_get_first_child (container));
}

/* Make a special first item in the "View as" option menu that represents
 * the current content view. This should only be called if the current
 * content view isn't already in the "View as" option menu.
 */
static void
update_extra_viewer_in_view_as_menus (NautilusWindow *window,
				      const NautilusViewIdentifier *id)
{
	GtkWidget *menu;
	GtkWidget *new_menu_item;
	gboolean had_extra_viewer;

	had_extra_viewer = window->details->extra_viewer != NULL;

	if (id == NULL) {
		if (!had_extra_viewer) {
			return;
		}
	} else {
		if (had_extra_viewer
		    && nautilus_view_identifier_compare (window->details->extra_viewer, id) == 0) {
			return;
		}
	}
	nautilus_view_identifier_free (window->details->extra_viewer);
	window->details->extra_viewer = nautilus_view_identifier_copy (id);

	/* Update the View As option menu */	
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (window->view_as_option_menu));

	/* Remove menu before changing contents so it is resized properly
	 * when reattached later in this function.
	 */
	gtk_widget_ref (menu);
	gtk_option_menu_remove_menu (GTK_OPTION_MENU (window->view_as_option_menu));

	/* Remove old menu item, and either remove or add separator. */
	if (had_extra_viewer) {
		remove_first_child (GTK_CONTAINER (menu));
		if (id == NULL) {
			remove_first_child (GTK_CONTAINER (menu));
		}
	} else {
		if (id != NULL) {
			gtk_menu_prepend (GTK_MENU (menu), new_gtk_separator ());
		}
	}

	/* Add new menu item. */
	if (id != NULL) {
		new_menu_item = create_view_as_menu_item (window, window->details->extra_viewer, 0);
		gtk_object_set_data (GTK_OBJECT (new_menu_item), "extra viewer", GINT_TO_POINTER (TRUE));
		gtk_menu_prepend (GTK_MENU (menu), new_menu_item);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (window->view_as_option_menu), menu);
	gtk_widget_unref (menu);

	/* Also update the Bonobo View menu item */
	if (id == NULL) {
			nautilus_bonobo_remove_menu_items_and_commands
				(window->details->shell_ui, NAUTILUS_MENU_PATH_EXTRA_VIEWER_PLACEHOLDER);
	} else {
		add_view_as_bonobo_menu_item (window, 
					      NAUTILUS_MENU_PATH_EXTRA_VIEWER_PLACEHOLDER, 
					      window->details->extra_viewer, 
					      0);
	}
}

static void
remove_extra_viewer_in_view_as_menus (NautilusWindow *window)
{
	update_extra_viewer_in_view_as_menus (window, NULL);
}

static void
replace_extra_viewer_in_view_as_menus (NautilusWindow *window)
{
	NautilusViewIdentifier *id;

	id = nautilus_window_get_content_view_id (window);
	update_extra_viewer_in_view_as_menus (window, id);
	nautilus_view_identifier_free (id);
}

/**
 * nautilus_window_synch_view_as_menus:
 * 
 * Set the visible item of the "View as" option menu and
 * the marked "View as" item in the View menu to
 * match the current content view.
 * 
 * @window: The NautilusWindow whose "View as" option menu should be synched.
 */
void
nautilus_window_synch_view_as_menus (NautilusWindow *window)
{
	int index;
	char *verb_name, *command_path;
	GList *node;
	const char *numbered_menu_item_container_path;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	if (window->content_view == NULL) {
		return;
	}

	for (node = window->details->short_list_viewers, index = 0;
	     node != NULL;
	     node = node->next, ++index) {
		if (nautilus_window_content_view_matches_iid (window, ((NautilusViewIdentifier *)node->data)->iid)) {
			break;
		}
	}
	if (node == NULL) {
		replace_extra_viewer_in_view_as_menus (window);
		index = 0;
		numbered_menu_item_container_path = NAUTILUS_MENU_PATH_EXTRA_VIEWER_PLACEHOLDER;
	} else {
		remove_extra_viewer_in_view_as_menus (window);
		numbered_menu_item_container_path = NAUTILUS_MENU_PATH_SHORT_LIST_PLACEHOLDER;
	}

	g_assert (numbered_menu_item_container_path != NULL);

	/* Make option menu show the right item */
	gtk_option_menu_set_history (GTK_OPTION_MENU (window->view_as_option_menu), index);

	/* Make View menu in menu bar mark the right item */
	verb_name = nautilus_bonobo_get_numbered_menu_item_command
		(window->details->shell_ui, 
		 numbered_menu_item_container_path, index);
	command_path = g_strconcat (COMMAND_PREFIX, verb_name, NULL);
	nautilus_bonobo_set_toggle_state (window->details->shell_ui, command_path, TRUE);
	g_free (command_path);
	g_free (verb_name);
}

static void
chose_component_callback (NautilusViewIdentifier *identifier, gpointer callback_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (callback_data);
	if (identifier != NULL) {
		nautilus_window_set_content_view (window, identifier);
	}
	
	/* FIXME bugzilla.gnome.org 41334: There should be some global
	 * way to signal that the file type associations have changed,
	 * so that the places that display these lists can react. For
	 * now, hardwire this case, which is the most obvious one by
	 * far.
	 */
	nautilus_window_load_view_as_menus (window);
}

static void
cancel_chose_component_callback (NautilusWindow *window)
{
	if (window->details->viewed_file != NULL) {
		nautilus_cancel_choose_component_for_file (window->details->viewed_file,
							   chose_component_callback, 
							   window);
	}
}

void
nautilus_window_show_view_as_dialog (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	/* Call back when the user chose the component. */
	cancel_chose_component_callback (window);
	nautilus_choose_component_for_file (window->details->viewed_file,
					    GTK_WINDOW (window), 
					    chose_component_callback, 
					    window);
}

static void
view_as_menu_choose_view_callback (GtkWidget *widget, gpointer data)
{
        NautilusWindow *window;
        
        g_return_if_fail (GTK_IS_MENU_ITEM (widget));
        g_return_if_fail (NAUTILUS_IS_WINDOW (data));
        
        window = NAUTILUS_WINDOW (data);

	/* Set the option menu back to its previous setting (Don't
	 * leave it on this dialog-producing "View as..."
	 * setting). If the menu choice causes a content view change,
	 * this will be updated again later, in
	 * nautilus_window_load_view_as_menus. Do this right away so
	 * the user never sees the option menu set to "View as
	 * Other...".
	 */
	nautilus_window_synch_view_as_menus (window);

	nautilus_window_show_view_as_dialog (window);
}

static void
refresh_stored_viewers (NautilusWindow *window)
{
	GList *components, *node, *viewers;
	NautilusViewIdentifier *identifier;

        components = nautilus_mime_get_short_list_components_for_file (window->details->viewed_file);
	viewers = NULL;
        for (node = components; node != NULL; node = node->next) {
        	identifier = nautilus_view_identifier_new_from_content_view (node->data);
        	viewers = g_list_prepend (viewers, identifier);
        }
	gnome_vfs_mime_component_list_free (components);

        free_stored_viewers (window);
	window->details->short_list_viewers = g_list_reverse (viewers);
}

static void
load_view_as_menus_callback (NautilusFile *file, 
			    gpointer callback_data)
{	
        GtkWidget *new_menu;
        GtkWidget *menu_item;
        GList *node;
	NautilusWindow *window;
	int index;

	window = NAUTILUS_WINDOW (callback_data);

        g_return_if_fail (GTK_IS_OPTION_MENU (window->view_as_option_menu));

        /* Clear out the menu items created last time. For the option menu, we need do
         * nothing since we replace the entire menu. For the View menu, we have
         * to do this explicitly.
         */
	nautilus_bonobo_remove_menu_items_and_commands
		(window->details->shell_ui, NAUTILUS_MENU_PATH_SHORT_LIST_PLACEHOLDER);
	nautilus_bonobo_remove_menu_items_and_commands
		(window->details->shell_ui, NAUTILUS_MENU_PATH_EXTRA_VIEWER_PLACEHOLDER);

	refresh_stored_viewers (window);

        new_menu = gtk_menu_new ();
	
        /* Add a menu item for each view in the preferred list for this location. */
        for (node = window->details->short_list_viewers, index = 0; 
             node != NULL; 
             node = node->next, ++index) {
        	/* Menu item in option menu. This doesn't use Bonobo, for various
        	 * historical and technical reasons.
        	 */
                menu_item = create_view_as_menu_item (window, node->data, index);
                gtk_menu_append (GTK_MENU (new_menu), menu_item);

		/* Menu item in View menu. */
                add_view_as_bonobo_menu_item (window, 
                			      NAUTILUS_MENU_PATH_SHORT_LIST_PLACEHOLDER, 
                			      node->data, 
                			      index);
        }

        /* Add/Show separator before "View as..." if there are any other viewers in menu. */
        if (window->details->short_list_viewers != NULL) {
	        gtk_menu_append (GTK_MENU (new_menu), new_gtk_separator ());
        }
        nautilus_bonobo_set_hidden (window->details->shell_ui,
        			    NAUTILUS_MENU_PATH_AFTER_SHORT_LIST_SEPARATOR, 
        			    window->details->short_list_viewers == NULL);

	/* Add "View as..." extra bonus choice. */
       	menu_item = gtk_menu_item_new_with_label (_("View as..."));
        gtk_signal_connect (GTK_OBJECT (menu_item),
        		    "activate",
        		    view_as_menu_choose_view_callback,
        		    window);
       	gtk_widget_show (menu_item);
       	gtk_menu_append (GTK_MENU (new_menu), menu_item);

        /* We create and attach a new menu here because adding/removing
         * items from existing menu screws up the size of the option menu.
         */
        gtk_option_menu_set_menu (GTK_OPTION_MENU (window->view_as_option_menu),
                                  new_menu);

	nautilus_window_synch_view_as_menus (window);
}

static void
cancel_view_as_callback (NautilusWindow *window)
{
	nautilus_file_cancel_call_when_ready (window->details->viewed_file, 
					      load_view_as_menus_callback,
					      window);
}

void
nautilus_window_load_view_as_menus (NautilusWindow *window)
{
	GList *attributes;

        g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	attributes = nautilus_mime_actions_get_full_file_attributes ();

	cancel_view_as_callback (window);
	nautilus_file_call_when_ready (window->details->viewed_file,
				       attributes, 
				       load_view_as_menus_callback,
				       window);

	g_list_free (attributes);
}

void
nautilus_window_add_sidebar_panel (NautilusWindow *window,
				   NautilusViewFrame *sidebar_panel)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (sidebar_panel));
	g_return_if_fail (NAUTILUS_IS_SIDEBAR (window->sidebar));
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
	GList *selection;
	char *parent_uri_string;
	
	if (window->details->location == NULL) {
		return;
	}
	
	current_uri = gnome_vfs_uri_new (window->details->location);
	parent_uri = gnome_vfs_uri_get_parent (current_uri);
	gnome_vfs_uri_unref (current_uri);

	if (parent_uri == NULL) {
		g_warning ("Can't go Up from here. The UI should have prevented us from getting this far.");
		return;
	}
	
	parent_uri_string = gnome_vfs_uri_to_string (parent_uri, GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (parent_uri);

	selection = g_list_prepend (NULL, g_strdup (window->details->location));
	
	nautilus_window_open_location_with_selection (window, parent_uri_string, selection);
	
	g_free (parent_uri_string);
	eel_g_list_free_deep (selection);
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

	search_web_uri = eel_preferences_get (NAUTILUS_PREFERENCES_SEARCH_WEB_URI);
	g_assert (search_web_uri != NULL);
	
	nautilus_window_go_to (window, search_web_uri);
	g_free (search_web_uri);
}

void
nautilus_window_go_home (NautilusWindow *window)
{
	char *home_uri;

	nautilus_window_set_search_mode (window, FALSE);

	home_uri = eel_preferences_get (NAUTILUS_PREFERENCES_HOME_URI);
	
	g_assert (home_uri != NULL);
	nautilus_window_go_to (window, home_uri);
	g_free (home_uri);
}

void
nautilus_window_allow_back (NautilusWindow *window, gboolean allow)
{
	nautilus_window_ui_freeze (window);

	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_BACK, allow);
	/* Have to handle non-standard Back button explicitly (it's
	 * non-standard to support right-click menu).
	 */
	gtk_widget_set_sensitive 
		(GTK_WIDGET (window->details->back_button_item), allow);

	nautilus_window_ui_thaw (window);
}

void
nautilus_window_allow_forward (NautilusWindow *window, gboolean allow)
{
	nautilus_window_ui_freeze (window);

	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_FORWARD, allow);
	/* Have to handle non-standard Forward button explicitly (it's
	 * non-standard to support right-click menu).
	 */
	gtk_widget_set_sensitive 
		(GTK_WIDGET (window->details->forward_button_item), allow);

	nautilus_window_ui_thaw (window);
}

void
nautilus_window_allow_up (NautilusWindow *window, gboolean allow)
{
	nautilus_window_ui_freeze (window);

	/* Because of verbs, we set the sensitivity of the menu to
	 * control both the menu and toolbar.
	 */
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_UP, allow);

	nautilus_window_ui_thaw (window);
}

void
nautilus_window_allow_reload (NautilusWindow *window, gboolean allow)
{
	nautilus_window_ui_freeze (window);

	/* Because of verbs, we set the sensitivity of the menu to
	 * control both the menu and toolbar.
	 */
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_RELOAD, allow);

	nautilus_window_ui_thaw (window);
}

void
nautilus_window_allow_stop (NautilusWindow *window, gboolean allow)
{
	CORBA_Environment ev;
	Bonobo_PropertyBag property_bag;
	
	nautilus_window_ui_freeze (window);

	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_STOP, allow);

	if (window->throbber != CORBA_OBJECT_NIL) {
		CORBA_exception_init (&ev);
		property_bag = Bonobo_Control_getProperties (window->throbber, &ev);
		if (!BONOBO_EX (&ev) && property_bag != CORBA_OBJECT_NIL) {
			bonobo_property_bag_client_set_value_gboolean (property_bag, "throbbing", allow, &ev);
			bonobo_object_release_unref (property_bag, NULL);
		}
		CORBA_exception_free (&ev);
	}

	nautilus_window_ui_thaw (window);
}

void
nautilus_send_history_list_changed (void)
{
	gtk_signal_emit_by_name (nautilus_signaller_get_current (),
			 	 "history_list_changed");
}

static void
free_history_list (void)
{
	eel_gtk_object_list_free (history_list);
	history_list = NULL;
}

/* Remove the this URI from the history list.
 * Do not sent out a change notice.
 * We pass in a bookmark for convenience.
 */
static void
remove_from_history_list (NautilusBookmark *bookmark)
{
	GList *node;

	/* Compare only the uris here. Comparing the names also is not
	 * necessary and can cause problems due to the asynchronous
	 * nature of when the title of the window is set.
	 */
	node = g_list_find_custom (history_list, 
				   bookmark,
				   nautilus_bookmark_compare_uris);
	
	/* Remove any older entry for this same item. There can be at most 1. */
	if (node != NULL) {
		history_list = g_list_remove_link (history_list, node);
		gtk_object_unref (node->data);
		g_list_free_1 (node);
	}
}

static void
add_to_history_list (NautilusBookmark *bookmark)
{
	/* Note that the history is shared amongst all windows so
	 * this is not a NautilusWindow function. Perhaps it belongs
	 * in its own file.
	 */
	static gboolean free_history_list_is_set_up;
	int extra_count, index;

	g_return_if_fail (NAUTILUS_IS_BOOKMARK (bookmark));

	if (!free_history_list_is_set_up) {
		g_atexit (free_history_list);
		free_history_list_is_set_up = TRUE;
	}

	gtk_object_ref (GTK_OBJECT (bookmark));
	remove_from_history_list (bookmark);
	history_list = g_list_prepend (history_list, bookmark);

	extra_count = g_list_length (history_list) - MAX_HISTORY_ITEMS;
	if (extra_count > 0) {
		history_list = g_list_reverse (history_list);
		for (index = 0; index < extra_count; ++index) {
			gtk_object_unref (history_list->data);
			history_list = g_list_remove (history_list, history_list->data);
		}
		history_list = g_list_reverse (history_list);
	}

	nautilus_send_history_list_changed ();
}

void
nautilus_remove_from_history_list_no_notify (const char *uri)
{
	NautilusBookmark *bookmark;

	bookmark = nautilus_bookmark_new (uri, "");
	remove_from_history_list (bookmark);
	gtk_object_unref (GTK_OBJECT (bookmark));
}

static void
real_add_current_location_to_history_list (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	add_to_history_list (window->current_location_bookmark);
}

void
nautilus_window_add_current_location_to_history_list (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			      add_current_location_to_history_list, (window));
}

void
nautilus_window_clear_forward_list (NautilusWindow *window)
{
	eel_gtk_object_list_free (window->forward_list);
	window->forward_list = NULL;
}

void
nautilus_window_clear_back_list (NautilusWindow *window)
{
	eel_gtk_object_list_free (window->back_list);
	window->back_list = NULL;
}

void
nautilus_forget_history (void) 
{
	GList *window_node;
	NautilusWindow *window;

	/* Clear out each window's back & forward lists. Also, remove 
	 * each window's current location bookmark from history list 
	 * so it doesn't get clobbered.
	 */
	for (window_node = nautilus_application_get_window_list ();
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
	for (window_node = nautilus_application_get_window_list ();
	     window_node != NULL;
	     window_node = window_node->next) {

		window = NAUTILUS_WINDOW (window_node->data);
		nautilus_window_add_current_location_to_history_list (window);
	}
}

GList *
nautilus_get_history_list (void)
{
	return history_list;
}

void
nautilus_window_display_error (NautilusWindow *window, const char *error_msg)
{
	GtkWidget *dialog;
	
	dialog = gnome_message_box_new (error_msg, GNOME_MESSAGE_BOX_ERROR, _("Close"), NULL);
	gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
	gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
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
		gtk_object_destroy (GTK_OBJECT (window->content_view));
		window->content_view = NULL;
	}

	if (new_view != NULL) {
		gtk_widget_show (GTK_WIDGET (new_view));

		/* When creating the desktop window the UI needs to
		 * be in sync. Otherwise I get failed assertions in
		 * bonobo while trying to reference something called
		 * `/commands/Unmount Volume Conditional'
		 */
		nautilus_window_ui_update (window);

		/* FIXME bugzilla.gnome.org 41243: 
		 * We should use inheritance instead of these special cases
		 * for the desktop window.
		 */
		if (!E_IS_PANED (window->content_hbox)) {
			gtk_container_add (GTK_CONTAINER (window->content_hbox),
					   GTK_WIDGET (new_view));
		} else {
			e_paned_pack2 (E_PANED (window->content_hbox),
				       GTK_WIDGET (new_view),
				       TRUE, TRUE);
		}
	}

	/* Display or hide zoom control */
	if (new_view != NULL && nautilus_view_frame_get_is_zoomable (new_view)) {
		gtk_widget_show (window->zoom_control);
	} else {
		gtk_widget_hide (window->zoom_control);
	}

	window->content_view = new_view;
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

	if (window->sidebar == NULL) {
		return;
	}

	/* Obtain list of enabled view identifiers */
	identifier_list = nautilus_sidebar_get_enabled_sidebar_panel_view_identifiers ();
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

	nautilus_window_ui_freeze (window);

	nautilus_bonobo_set_hidden (window->details->shell_ui,
				    dock_item_path,
				    FALSE);
	nautilus_window_update_show_hide_menu_items (window);

	nautilus_window_ui_thaw (window);
}

static void 
hide_dock_item (NautilusWindow *window, const char *dock_item_path)
{
	nautilus_window_ui_freeze (window);

	nautilus_bonobo_set_hidden (window->details->shell_ui,
				    dock_item_path,
				    TRUE);
	nautilus_window_update_show_hide_menu_items (window);

	nautilus_window_ui_thaw (window);
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
	window->details->temporary_navigation_bar = FALSE;
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
nautilus_window_hide_toolbar (NautilusWindow *window)
{
	hide_dock_item (window, TOOLBAR_PATH);
}

void 
nautilus_window_show_toolbar (NautilusWindow *window)
{
	show_dock_item (window, TOOLBAR_PATH);
}

gboolean
nautilus_window_toolbar_showing (NautilusWindow *window)
{
	return dock_item_showing (window, TOOLBAR_PATH);
}

void 
nautilus_window_hide_sidebar (NautilusWindow *window)
{
	if (window->sidebar == NULL) {
		return;
	}
	gtk_widget_hide (GTK_WIDGET (window->sidebar));
	if (window->content_hbox != NULL) {
		e_paned_set_position (E_PANED (window->content_hbox), 0);
	}
	nautilus_window_update_show_hide_menu_items (window);
}

void 
nautilus_window_show_sidebar (NautilusWindow *window)
{
	GtkWidget *widget;

	if (window->sidebar == NULL) {
		return;
	}
	widget = GTK_WIDGET (window->sidebar);
	gtk_widget_show (widget);
	if (window->content_hbox != NULL) {
		e_paned_set_position (E_PANED (window->content_hbox), widget->allocation.width);
		/* Make sure sidebar is not in collapsed form also */
		nautilus_horizontal_splitter_expand (NAUTILUS_HORIZONTAL_SPLITTER (window->content_hbox));
	}
	nautilus_window_update_show_hide_menu_items (window);
}

gboolean
nautilus_window_sidebar_showing (NautilusWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), FALSE);

	return window->sidebar != NULL && GTK_WIDGET_VISIBLE (window->sidebar);
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

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, show, (widget));
	
	/* Initially show or hide views based on preferences; once the window is displayed
	 * these can be controlled on a per-window basis from View menu items. 
	 */
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_TOOLBAR)) {
		nautilus_window_show_toolbar (window);
	} else {
		nautilus_window_hide_toolbar (window);
	}

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR)) {
		nautilus_window_show_location_bar (window);
	} else {
		nautilus_window_hide_location_bar (window);
	}

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR)) {
		nautilus_window_show_status_bar (window);
	} else {
		nautilus_window_hide_status_bar (window);
	}

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_SIDEBAR)) {
		nautilus_window_show_sidebar (window);
	} else {
		nautilus_window_hide_sidebar (window);
	}

	nautilus_window_ui_update (window);
}

Bonobo_UIContainer 
nautilus_window_get_ui_container (NautilusWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), CORBA_OBJECT_NIL);

	return bonobo_object_corba_objref (BONOBO_OBJECT (window->details->ui_container));
}

void
nautilus_window_set_viewed_file (NautilusWindow *window,
				 NautilusFile *file)
{
	GList *attributes;

	if (window->details->viewed_file == file) {
		return;
	}

	nautilus_file_ref (file);

	cancel_view_as_callback (window);
	cancel_chose_component_callback (window);

	if (window->details->viewed_file != NULL) {
		nautilus_file_monitor_remove (window->details->viewed_file,
					      window);
	}

	if (file != NULL) {
		attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME);
		nautilus_file_monitor_add (file, window, attributes);
		g_list_free (attributes);
	}

	nautilus_file_unref (window->details->viewed_file);
	window->details->viewed_file = file;
}
