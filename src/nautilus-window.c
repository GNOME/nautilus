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
#include "nautilus-information-panel.h"
#include "nautilus-main.h"
#include "nautilus-signaller.h"
#include "nautilus-switchable-navigation-bar.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-zoom-control.h"
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-property-bag-client.h>
#include <bonobo/bonobo-ui-util.h>
#include <eel/eel-debug.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-generous-bin.h>
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

#define MAX_HISTORY_ITEMS 50

/* FIXME bugzilla.gnome.org 41245: hardwired sizes */
#define SIDE_PANE_MINIMUM_WIDTH 1
#define SIDE_PANE_MINIMUM_HEIGHT 400

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
#define NAUTILUS_MENU_PATH_AFTER_SHORT_LIST_SEPARATOR   "/menu/View/View Choices/After Short List"

enum {
	ARG_0,
	ARG_APP_ID,
	ARG_APP
};

static void cancel_view_as_callback             (NautilusWindow *window);

static GList *history_list;

GNOME_CLASS_BOILERPLATE (NautilusWindow, nautilus_window,
			 BonoboWindow, BONOBO_TYPE_WINDOW)

static void
set_up_default_icon_list (void)
{
	GList *icon_list;
	guint i;
	GdkPixbuf *pixbuf;
	char *path;
	const char *icon_filenames[] = { "nautilus-mini-logo.png", "nautilus-launch-icon.png" };

	icon_list = NULL;
	for (i = 0; i < G_N_ELEMENTS (icon_filenames); i++) {
		path = nautilus_pixmap_file (icon_filenames[i]);

		if (path == NULL) {
			continue;
		}
		
		pixbuf = gdk_pixbuf_new_from_file (path, NULL);
		g_free (path);
		
		if (pixbuf != NULL) {
			icon_list = g_list_prepend (icon_list, pixbuf);
		}
	}

	gtk_window_set_default_icon_list (icon_list);

	eel_g_list_free_deep_custom (icon_list, (GFunc) g_object_unref, NULL);
}

static void
icons_changed_callback (GObject *factory, NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_update_icon (window);
}

static void
nautilus_window_instance_init (NautilusWindow *window)
{
	window->details = g_new0 (NautilusWindowDetails, 1);

	/* CORBA and Bonobo setup, which must be done before the location bar setup */
	window->details->ui_container = bonobo_window_get_ui_container (BONOBO_WINDOW (window));
	bonobo_object_ref (window->details->ui_container);

	/* Set initial window title */
	gtk_window_set_title (GTK_WINDOW (window), _("Nautilus"));

	window->details->shell_ui = bonobo_ui_component_new ("Nautilus Shell");
	bonobo_ui_component_set_container
		(window->details->shell_ui,
		 nautilus_window_get_ui_container (window),
		 NULL);

	/* Register IconFactory callback to update the window border icon
	 * when the icon-theme is changed.
	 */
	g_signal_connect_object (nautilus_icon_factory_get (), "icons_changed",
				 G_CALLBACK (icons_changed_callback), window,
				 0);

	/* Create a separate component so when we remove the status
	 * we don't loose the status bar
	 */
      	window->details->status_ui = bonobo_ui_component_new ("Status Component");  
	bonobo_ui_component_set_container
		(window->details->status_ui,
		 nautilus_window_get_ui_container (window),
		 NULL);

	gtk_quit_add_destroy (1, GTK_OBJECT (window));

	/* Keep the main event loop alive as long as the window exists */
	nautilus_main_event_loop_register (GTK_OBJECT (window));
}

static gint
ui_idle_handler (gpointer data)
{
	NautilusWindow *window;
	gboolean old_updating_bonobo_state;

	window = data;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_object_ref (data);

	g_assert (window->details->ui_change_depth == 0);

	/* Simulate an extra freeze/thaw so that calling window_ui_freeze
	 * and thaw from within the idle handler doesn't try to remove it
	 * (the already running idle handler)
	 */
	window->details->ui_change_depth++;
	old_updating_bonobo_state = window->details->updating_bonobo_state;

	if (window->details->ui_pending_initialize_menus_part_2) {
#if !NEW_UI_COMPLETE
		if (NAUTILUS_IS_NAVIGATION_WINDOW (window)) {
			nautilus_navigation_window_initialize_menus_part_2 (NAUTILUS_NAVIGATION_WINDOW (window));
		}
#endif
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

	g_object_unref (data);

	return FALSE;
}

static inline void
ui_install_idle_handler (NautilusWindow *window)
{
	if (window->details->ui_idle_id == 0) {
		window->details->ui_idle_id = g_idle_add_full (G_PRIORITY_LOW, ui_idle_handler, window, NULL);
	}
}

static inline void
ui_remove_idle_handler (NautilusWindow *window)
{
	if (window->details->ui_idle_id != 0) {
		g_source_remove (window->details->ui_idle_id);
		window->details->ui_idle_id = 0;
	}
}

/* Register that BonoboUI changes are going to be made to WINDOW. The UI
 * won't actually be synchronised until some arbitrary date in the future.
 */
void
nautilus_window_ui_freeze (NautilusWindow *window)
{
	g_object_ref (window);

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

	g_object_unref (window);
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

	bonobo_ui_component_set_status (window->details->status_ui, NULL, NULL);

	return FALSE;
}

void
nautilus_window_set_status (NautilusWindow *window, const char *text)
{
	if (text != NULL && text[0] != '\0') {
		bonobo_ui_component_set_status (window->details->status_ui, text, NULL);
	} else {
		nautilus_window_clear_status (window);
	}
}

void
nautilus_window_go_to (NautilusWindow *window, const char *uri)
{
	nautilus_window_open_location (window, uri);
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
nautilus_window_allow_up (NautilusWindow *window, gboolean allow)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_ui_freeze (window);

	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_UP, allow);

	nautilus_window_ui_thaw (window);
}

void
nautilus_window_allow_stop (NautilusWindow *window, gboolean allow)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_ui_freeze (window);

	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_STOP, allow);

	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			 set_throbber_active, (window, allow));

	nautilus_window_ui_thaw (window);
}

void
nautilus_window_allow_reload (NautilusWindow *window, gboolean allow)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_ui_freeze (window);

        nautilus_bonobo_set_sensitive (window->details->shell_ui,
                                       NAUTILUS_COMMAND_RELOAD, allow);

	nautilus_window_ui_thaw (window);
}

void
nautilus_window_allow_burn_cd (NautilusWindow *window, gboolean allow)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_ui_freeze (window);

        nautilus_bonobo_set_hidden (window->details->shell_ui,
				    NAUTILUS_COMMAND_BURN_CD, !allow);

	nautilus_window_ui_thaw (window);
}

void
nautilus_window_go_home (NautilusWindow *window)
{
	char *home_uri;

#if !NEW_UI_COMPLETE
	/* Hrm, this probably belongs in any location switch, not 
	 * just when going home. */
	if (NAUTILUS_IS_NAVIGATION_WINDOW (window)) {
		nautilus_navigation_window_set_search_mode (NAUTILUS_NAVIGATION_WINDOW (window), FALSE);
	}
#endif

#ifdef WEB_NAVIGATION_ENABLED
	home_uri = eel_preferences_get (NAUTILUS_PREFERENCES_HOME_URI);
#else
	home_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
#endif
	
	g_assert (home_uri != NULL);
	nautilus_window_go_to (window, home_uri);
	g_free (home_uri);
}

void
nautilus_window_launch_cd_burner (NautilusWindow *window)	
{
	GError *error;
	char *argv[] = { "nautilus-cd-burner", NULL};
	char *text;

	error = NULL;
	if (!g_spawn_async (NULL,
			    argv, NULL,
			    G_SPAWN_SEARCH_PATH,
			    NULL, NULL,
			    NULL,
			    &error)) {
		text = g_strdup_printf (_("Unable to launch the cd burner application:\n%s"), error->message);
		eel_show_error_dialog (text,
				       _("Can't launch cd burner"),
				       GTK_WINDOW (window));
		g_free (text);
		g_error_free (error);
	}
}

void
nautilus_window_prompt_for_location (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));
	
	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
                         prompt_for_location, (window));
}

char *
nautilus_window_get_location (NautilusWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

	return g_strdup (window->details->location);
}

void
nautilus_window_zoom_in (NautilusWindow *window)
{
	if (window->content_view != NULL) {
		nautilus_view_frame_zoom_in (window->content_view);
	}
}

void
nautilus_window_zoom_to_level (NautilusWindow *window, float level)
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
get_max_forced_height (GdkScreen *screen)
{
	return (gdk_screen_get_height (screen) * 90) / 100;
}

/* Code should never force the window wider than this size.
 * (The user can still stretch the window wider if desired).
 */
static guint
get_max_forced_width (GdkScreen *screen)
{
	return (gdk_screen_get_width (screen) * 90) / 100;
}

static void
set_initial_window_geometry (NautilusWindow *window)
{
	GdkScreen *screen;
	guint max_width_for_screen, max_height_for_screen;

	screen = gtk_window_get_screen (GTK_WINDOW (window));
	
	/* Don't let GTK determine the minimum size
	 * automatically. It will insist that the window be
	 * really wide based on some misguided notion about
	 * the content view area. Also, it might start the
	 * window wider (or taller) than the screen, which
	 * is evil. So we choose semi-arbitrary initial and
	 * minimum widths instead of letting GTK decide.
	 */
	/* FIXME - the above comment suggests that the size request
	 * of the content view area is wrong, probably because of
	 * another stupid set_usize someplace. If someone gets the
	 * content view area's size request right then we can
	 * probably remove this broken set_size_request() here.
	 * - hp@redhat.com
	 */

	max_width_for_screen = get_max_forced_width (screen);
	max_height_for_screen = get_max_forced_height (screen);

	gtk_widget_set_size_request (GTK_WIDGET (window), 
				     MIN (NAUTILUS_WINDOW_MIN_WIDTH, 
					  max_width_for_screen),
				     MIN (NAUTILUS_WINDOW_MIN_HEIGHT, 
					  max_height_for_screen));

	gtk_window_set_default_size (GTK_WINDOW (window), 
				     MIN (NAUTILUS_WINDOW_DEFAULT_WIDTH, 
				          max_width_for_screen), 
				     MIN (NAUTILUS_WINDOW_DEFAULT_HEIGHT, 
				          max_height_for_screen));
}

static void
real_merge_menus (NautilusWindow *window)
{
	/* Load the user interface from the XML file. */
	bonobo_ui_util_set_ui (window->details->shell_ui,
			       DATADIR,
			       "nautilus-shell-ui.xml",
			       "nautilus", NULL);

	
	bonobo_ui_component_freeze (window->details->shell_ui, NULL);

	bonobo_ui_component_thaw (window->details->shell_ui, NULL);

	/* initalize the menus and toolbars */
	nautilus_window_initialize_menus_part_1 (window);

	/* We'll do the second part later (bookmarks and go menus) */
	window->details->ui_pending_initialize_menus_part_2 = TRUE;
}

static void
nautilus_window_constructed (NautilusWindow *window)
{
	nautilus_window_ui_freeze (window);

	set_initial_window_geometry (window);
	
	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			 merge_menus, (window));

	nautilus_window_allow_stop (window, FALSE);
	nautilus_window_allow_burn_cd (window, FALSE);

	/* Set up undo manager */
	nautilus_undo_manager_attach (window->application->undo_manager, G_OBJECT (window));	

	/* Register that things may be dragged from this window */
	nautilus_drag_window_register (GTK_WINDOW (window));

	nautilus_window_ui_thaw (window);
}

static void
nautilus_window_set_property (GObject *object,
			      guint arg_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	char *old_name;
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (object);
	
	switch (arg_id) {
	case ARG_APP_ID:
		if (g_value_get_string (value) == NULL) {
			return;
		}
		old_name = bonobo_window_get_name (BONOBO_WINDOW (window));
		bonobo_window_set_name (BONOBO_WINDOW (window), g_value_get_string (value));
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
		window->application = NAUTILUS_APPLICATION (g_value_get_object (value));
		break;
	}
}

static void
nautilus_window_get_property (GObject *object,
			      guint arg_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	switch (arg_id) {
	case ARG_APP_ID:
		g_value_set_string_take_ownership (
			value,
			bonobo_window_get_name (BONOBO_WINDOW (object)));
		break;
	case ARG_APP:
		g_value_set_object (value, NAUTILUS_WINDOW (object)->application);
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
nautilus_window_destroy (GtkObject *object)
{
	NautilusWindow *window;
	
	window = NAUTILUS_WINDOW (object);

	nautilus_window_manage_views_destroy (window);

	if (window->content_view) {
		gtk_object_destroy (GTK_OBJECT (window->content_view));
		window->content_view = NULL;
	}

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
nautilus_window_finalize (GObject *object)
{
	NautilusWindow *window;
	
	window = NAUTILUS_WINDOW (object);

	nautilus_window_manage_views_finalize (window);

	nautilus_window_set_viewed_file (window, NULL);
	nautilus_window_remove_go_menu_callback (window);

	if (window->details->ui_idle_id != 0) {
		g_source_remove (window->details->ui_idle_id);
	}

	if (window->details->shell_ui != NULL) {
		bonobo_ui_component_unset_container (window->details->shell_ui, NULL);
		bonobo_object_unref (window->details->shell_ui);
		window->details->shell_ui = NULL;
	}

	if (window->details->status_ui != NULL) {
		bonobo_ui_component_unset_container (window->details->status_ui, NULL);
		bonobo_object_unref (window->details->status_ui);
		window->details->status_ui = NULL;
	}

	nautilus_file_unref (window->details->viewed_file);

	free_stored_viewers (window);

	g_free (window->details->location);
	eel_g_list_free_deep (window->details->selection);
	eel_g_list_free_deep (window->details->pending_selection);

	if (window->current_location_bookmark != NULL) {
		g_object_unref (window->current_location_bookmark);
	}
	if (window->last_location_bookmark != NULL) {
		g_object_unref (window->last_location_bookmark);
	}

	bonobo_object_unref (window->details->ui_container);

	if (window->details->location_change_at_idle_id != 0) {
		g_source_remove (window->details->location_change_at_idle_id);
	}

	g_free (window->details->title);
	
	g_free (window->details);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
nautilus_window_close (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
nautilus_window_size_request (GtkWidget		*widget,
			      GtkRequisition	*requisition)
{
	GdkScreen *screen;
	guint max_width;
	guint max_height;

	g_assert (NAUTILUS_IS_WINDOW (widget));
	g_assert (requisition != NULL);

	GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

	screen = gtk_window_get_screen (GTK_WINDOW (widget));

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
	max_width = get_max_forced_width (screen);
	max_height = get_max_forced_height (screen);

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
		 identifier->view_as_label_with_mnemonic,
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

/* Make a special first item in the "View as" option menu that represents
 * the current content view. This should only be called if the current
 * content view isn't already in the "View as" option menu.
 */
static void
update_extra_viewer_in_view_as_menus (NautilusWindow *window,
				      const NautilusViewIdentifier *id)
{
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
static void
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
real_load_view_as_menu (NautilusWindow *window)
{
	GList *node;
	int index;

        /* Clear out the menu items created last time. For the option menu, we need do
         * nothing since we replace the entire menu. For the View menu, we have
         * to do this explicitly.
         */
	nautilus_bonobo_remove_menu_items_and_commands
		(window->details->shell_ui, NAUTILUS_MENU_PATH_SHORT_LIST_PLACEHOLDER);
	nautilus_bonobo_remove_menu_items_and_commands
		(window->details->shell_ui, NAUTILUS_MENU_PATH_EXTRA_VIEWER_PLACEHOLDER);

	refresh_stored_viewers (window);


        /* Add a menu item for each view in the preferred list for this location. */
        for (node = window->details->short_list_viewers, index = 0; 
             node != NULL; 
             node = node->next, ++index) {
		/* Menu item in View menu. */
                add_view_as_bonobo_menu_item (window, 
                			      NAUTILUS_MENU_PATH_SHORT_LIST_PLACEHOLDER, 
                			      node->data, 
                			      index);
        }

        nautilus_bonobo_set_hidden (window->details->shell_ui,
        			    NAUTILUS_MENU_PATH_AFTER_SHORT_LIST_SEPARATOR, 
        			    window->details->short_list_viewers == NULL);

	nautilus_window_synch_view_as_menus (window);
}

static void
load_view_as_menus_callback (NautilusFile *file, 
			    gpointer callback_data)
{
	NautilusWindow *window;
	
	window = NAUTILUS_WINDOW (callback_data);
	
	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
                         load_view_as_menu, (window));
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
	NautilusFileAttributes attributes;

        g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	attributes = nautilus_mime_actions_get_full_file_attributes ();

	cancel_view_as_callback (window);
	nautilus_file_call_when_ready (window->details->viewed_file,
				       attributes, 
				       load_view_as_menus_callback,
				       window);
}

void
nautilus_window_display_error (NautilusWindow *window, const char *error_msg)
{
	GtkWidget *dialog;
	
	dialog = gtk_message_dialog_new (GTK_WINDOW (window), 0, GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK, error_msg, NULL);
	gtk_widget_show (dialog);
}

static char *
compute_default_title (const char *text_uri)
{
	NautilusFile *file;
	char *title;

	if (text_uri == NULL) {
		title = g_strdup ("");
	} else if (strcmp (text_uri, "computer://") == 0 ||
		   strcmp (text_uri, "computer:///") == 0) {
		title = g_strdup (_("Computer"));
	} else if (strcmp (text_uri, "network://") == 0 ||
		   strcmp (text_uri, "network:///") == 0) {
		title = g_strdup (_("Network"));
	} else if (strcmp (text_uri, "fonts://") == 0 ||
		   strcmp (text_uri, "fonts:///") == 0) {
		title = g_strdup (_("Fonts"));
	} else {
		file = nautilus_file_get (text_uri);
		title = nautilus_file_get_display_name (file);
		nautilus_file_unref (file);
	}

	return title;
}

static char *
real_get_title (NautilusWindow *window)
{
	char *title;

	title = NULL;
	
	if (window->new_content_view != NULL) {
                title = nautilus_view_frame_get_title (window->new_content_view);
        } else if (window->content_view != NULL) {
                title = nautilus_view_frame_get_title (window->content_view);
        }
        
	if (title == NULL) {
                title = compute_default_title (window->details->location);
        }

	return title;
}

static char *
nautilus_window_get_title (NautilusWindow *window)
{
	return EEL_CALL_METHOD_WITH_RETURN_VALUE (NAUTILUS_WINDOW_CLASS, window,
						  get_title, (window));
}

static void
real_set_title (NautilusWindow *window,
		const char *title)
{
	g_free (window->details->title);
        window->details->title = g_strdup (title);

        if (window->details->title [0] != '\0' && window->current_location_bookmark &&
            nautilus_bookmark_set_name (window->current_location_bookmark, window->details->title)) {
                /* Name of item in history list changed, tell listeners. */
                nautilus_send_history_list_changed ();
        }
	
	/* warn all views and sidebar panels of the potential title change */
        if (window->content_view != NULL) {
                nautilus_view_frame_title_changed (window->content_view, title);
        }
}

static void
nautilus_window_set_title (NautilusWindow *window, 
			   const char *title)
{
	if (window->details->title != NULL
	    && strcmp (title, window->details->title) == 0) {
		return;
	}
	
	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
                         set_title, (window, title));
}

void
nautilus_window_update_title (NautilusWindow *window)
{
	char *title;
	
	title = nautilus_window_get_title (window);
	nautilus_window_set_title (window, title);
	
	g_free (title);
}

static void
real_set_content_view_widget (NautilusWindow *window,
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
	}

	window->content_view = new_view;

        /* Update displayed view in menu. Only do this if we're not switching
         * locations though, because if we are switching locations we'll
         * install a whole new set of views in the menu later (the current
         * views in the menu are for the old location).
         */
	if (window->details->pending_location == NULL) {
		nautilus_window_synch_view_as_menus (window);
	}
}

void
nautilus_window_set_content_view_widget (NautilusWindow *window,
					 NautilusViewFrame *frame)
{
	g_assert (NAUTILUS_IS_WINDOW (window));
	
	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
                         set_content_view_widget, (window, frame));
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
nautilus_window_hide_status_bar (NautilusWindow *window)
{
	hide_dock_item (window, STATUS_BAR_PATH);

	nautilus_window_update_show_hide_menu_items (window);
	if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR) &&
	    eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR, FALSE);
	}
}

void 
nautilus_window_show_status_bar (NautilusWindow *window)
{
	show_dock_item (window, STATUS_BAR_PATH);

	nautilus_window_update_show_hide_menu_items (window);
	if (eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR) &&
	    !eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR)) {
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR, TRUE);
	}
}

gboolean
nautilus_window_status_bar_showing (NautilusWindow *window)
{
	return dock_item_showing (window, STATUS_BAR_PATH);
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

	GTK_WIDGET_CLASS (parent_class)->show (widget);

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR)) {
		nautilus_window_show_status_bar (window);
	} else {
		nautilus_window_hide_status_bar (window);
	}

	nautilus_window_ui_update (window);
}

Bonobo_UIContainer 
nautilus_window_get_ui_container (NautilusWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), CORBA_OBJECT_NIL);

	return BONOBO_OBJREF (window->details->ui_container);
}

void
nautilus_window_set_viewed_file (NautilusWindow *window,
				 NautilusFile *file)
{
	NautilusFileAttributes attributes;

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
		attributes = NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME;
		nautilus_file_monitor_add (file, window, attributes);
	}

	nautilus_file_unref (window->details->viewed_file);
	window->details->viewed_file = file;
}

void
nautilus_send_history_list_changed (void)
{
	g_signal_emit_by_name (nautilus_signaller_get_current (),
			       "history_list_changed");
}

static void
free_history_list (void)
{
	eel_g_object_list_free (history_list);
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
		g_object_unref (node->data);
		g_list_free_1 (node);
	}
}

static void
add_to_history_list (NautilusBookmark *bookmark)
{
	/* Note that the history is shared amongst all windows so
	 * this is not a NautilusNavigationWindow function. Perhaps it belongs
	 * in its own file.
	 */
	int i;
	GList *l, *next;
	static gboolean free_history_list_is_set_up;

	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));

	if (!free_history_list_is_set_up) {
		eel_debug_call_at_shutdown (free_history_list);
		free_history_list_is_set_up = TRUE;
	}

/*	g_warning ("Add to history list '%s' '%s'",
		   nautilus_bookmark_get_name (bookmark),
		   nautilus_bookmark_get_uri (bookmark)); */

	if (!history_list ||
	    nautilus_bookmark_compare_uris (history_list->data, bookmark)) {
		g_object_ref (bookmark);
		remove_from_history_list (bookmark);
		history_list = g_list_prepend (history_list, bookmark);

		for (i = 0, l = history_list; l; l = next) {
			next = l->next;
			
			if (i++ >= MAX_HISTORY_ITEMS) {
				g_object_unref (l->data);
				history_list = g_list_delete_link (history_list, l);
			}
		}

		nautilus_send_history_list_changed ();
	}
}

void
nautilus_remove_from_history_list_no_notify (const char *uri)
{
	NautilusBookmark *bookmark;

	bookmark = nautilus_bookmark_new (uri, "");
	remove_from_history_list (bookmark);
	g_object_unref (bookmark);
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
nautilus_forget_history (void) 
{
	GList *window_node;

	/* Clear out each window's back & forward lists. Also, remove 
	 * each window's current location bookmark from history list 
	 * so it doesn't get clobbered.
	 */
	for (window_node = nautilus_application_get_window_list ();
	     window_node != NULL;
	     window_node = window_node->next) {

		if (NAUTILUS_IS_NAVIGATION_WINDOW (window_node->data)) {
			NautilusNavigationWindow *window;
			
			window = NAUTILUS_NAVIGATION_WINDOW (window_node->data);
			
			nautilus_navigation_window_clear_back_list (window);
			nautilus_navigation_window_clear_forward_list (window);
			
			nautilus_navigation_window_allow_back (window, FALSE);
			nautilus_navigation_window_allow_forward (window, FALSE);
		}
			
		history_list = g_list_remove (history_list, NAUTILUS_WINDOW (window_node->data)->current_location_bookmark);
	}

	/* Clobber history list. */
	free_history_list ();

	/* Re-add each window's current location to history list. */
	for (window_node = nautilus_application_get_window_list ();
	     window_node != NULL;
	     window_node = window_node->next) {
		NautilusWindow *window;
		
		window = NAUTILUS_WINDOW (window_node->data);
		nautilus_window_add_current_location_to_history_list (NAUTILUS_WINDOW (window));
	}
}

GList *
nautilus_get_history_list (void)
{
	return history_list;
}

static void
nautilus_window_class_init (NautilusWindowClass *class)
{
	G_OBJECT_CLASS (class)->finalize = nautilus_window_finalize;
	G_OBJECT_CLASS (class)->get_property = nautilus_window_get_property;
	G_OBJECT_CLASS (class)->set_property = nautilus_window_set_property;
	GTK_OBJECT_CLASS (class)->destroy = nautilus_window_destroy;
	GTK_WIDGET_CLASS (class)->show = nautilus_window_show;
	GTK_WIDGET_CLASS (class)->size_request = nautilus_window_size_request;
	class->add_current_location_to_history_list = real_add_current_location_to_history_list;
	class->get_title = real_get_title;
	class->set_title = real_set_title;
	class->merge_menus = real_merge_menus;
	class->set_content_view_widget = real_set_content_view_widget;
	class->load_view_as_menu = real_load_view_as_menu;
	
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 ARG_APP_ID,
					 g_param_spec_string ("app_id",
							      _("Application ID"),
							      _("The application ID of the window."),
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 ARG_APP,
					 g_param_spec_object ("app",
							      _("Application"),
							      _("The NautilusApplication associated with this window."),
							      NAUTILUS_TYPE_APPLICATION,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	
	/* Set default for all windows. This probably should be done
	 * in main or NautilusApplication rather than here in case
	 * some other window is created before the first
	 * NautilusWindow. Also, do we really want this icon for
	 * dialogs?
	 */
	set_up_default_icon_list ();
}
