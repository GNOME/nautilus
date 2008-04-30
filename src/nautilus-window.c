/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000, 2004 Red Hat, Inc.
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
 *           Alexander Larsson <alexl@redhat.com>
 */

/* nautilus-window.c: Implementation of the main window object */

#include <config.h>
#include "nautilus-window-private.h"

#include "nautilus-actions.h"
#include "nautilus-application.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-information-panel.h"
#include "nautilus-main.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-bookmarks.h"
#include "nautilus-zoom-control.h"
#include "nautilus-search-bar.h"
#include <eel/eel-debug.h>
#include <eel/eel-marshal.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>
#include <glib/gi18n.h>
#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-horizontal-splitter.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-view-factory.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-undo.h>
#include <libnautilus-private/nautilus-search-directory.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <math.h>
#include <sys/time.h>

/* FIXME bugzilla.gnome.org 41243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

#define MAX_HISTORY_ITEMS 50

#define EXTRA_VIEW_WIDGETS_BACKGROUND "#a7c6e1"

/* FIXME bugzilla.gnome.org 41245: hardwired sizes */
#define SIDE_PANE_MINIMUM_WIDTH 1
#define SIDE_PANE_MINIMUM_HEIGHT 400

/* dock items */

#define NAUTILUS_MENU_PATH_EXTRA_VIEWER_PLACEHOLDER	"/MenuBar/View/View Choices/Extra Viewer"
#define NAUTILUS_MENU_PATH_SHORT_LIST_PLACEHOLDER  	"/MenuBar/View/View Choices/Short List"
#define NAUTILUS_MENU_PATH_AFTER_SHORT_LIST_SEPARATOR   "/MenuBar/View/View Choices/After Short List"

enum {
	ARG_0,
	ARG_APP
};

enum {
	GO_UP,
	RELOAD,
	PROMPT_FOR_LOCATION,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct  {
	NautilusWindow *window;
	char *id;
} ActivateViewData;

static void cancel_view_as_callback         (NautilusWindow          *window);
static void nautilus_window_info_iface_init (NautilusWindowInfoIface *iface);
static void action_view_as_callback         (GtkAction               *action,
					     ActivateViewData        *data);

static GList *history_list;

G_DEFINE_TYPE_WITH_CODE (NautilusWindow, nautilus_window, GTK_TYPE_WINDOW,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_WINDOW_INFO,
						nautilus_window_info_iface_init));

static const struct {
	unsigned int keyval;
	const char *action;
} extra_window_keybindings [] = {
#ifdef HAVE_X11_XF86KEYSYM_H
	{ XF86XK_AddFavorite,	NAUTILUS_ACTION_ADD_BOOKMARK },
	{ XF86XK_Favorites,	NAUTILUS_ACTION_EDIT_BOOKMARKS },
	{ XF86XK_Go,		NAUTILUS_ACTION_GO_TO_LOCATION },
/* TODO?{ XF86XK_History,	NAUTILUS_ACTION_HISTORY }, */
	{ XF86XK_HomePage,      NAUTILUS_ACTION_GO_HOME },
	{ XF86XK_OpenURL,	NAUTILUS_ACTION_GO_TO_LOCATION },
	{ XF86XK_Refresh,	NAUTILUS_ACTION_RELOAD },
	{ XF86XK_Reload,	NAUTILUS_ACTION_RELOAD },
	{ XF86XK_Search,	NAUTILUS_ACTION_SEARCH },
	{ XF86XK_Start,		NAUTILUS_ACTION_GO_HOME },
	{ XF86XK_Stop,		NAUTILUS_ACTION_STOP },
	{ XF86XK_ZoomIn,	NAUTILUS_ACTION_ZOOM_IN },
	{ XF86XK_ZoomOut,	NAUTILUS_ACTION_ZOOM_OUT }
#endif
};

static void
nautilus_window_init (NautilusWindow *window)
{
	GtkWidget *table;
	GtkWidget *menu;
	GtkWidget *statusbar;

	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NAUTILUS_TYPE_WINDOW, NautilusWindowDetails);

	window->details->show_hidden_files_mode = NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_DEFAULT;
	
	/* Set initial window title */
	gtk_window_set_title (GTK_WINDOW (window), _("Nautilus"));

	table = gtk_table_new (1, 6, FALSE);
	window->details->table = table;
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (window), table);

	statusbar = gtk_statusbar_new ();
	window->details->statusbar = statusbar;
	gtk_table_attach (GTK_TABLE (table),
			  statusbar,
			  /* X direction */                   /* Y direction */
			  0, 1,                               5, 6,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0,
			  0,                                  0);
	window->details->help_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (statusbar), "help_message");
	gtk_widget_show (statusbar);

	nautilus_window_initialize_menus (window);
	
	menu = gtk_ui_manager_get_widget (window->details->ui_manager, "/MenuBar");
	window->details->menubar = menu;
	gtk_widget_show (menu);
	gtk_table_attach (GTK_TABLE (table),
			  menu, 
			  /* X direction */                   /* Y direction */
			  0, 1,                               0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0,
			  0,                                  0);

	/* Register to menu provider extension signal managing menu updates */
	g_signal_connect_object (nautilus_signaller_get_current (), "popup_menu_changed",
			 G_CALLBACK (nautilus_window_load_extension_menus), window, G_CONNECT_SWAPPED);

	gtk_quit_add_destroy (1, GTK_OBJECT (window));

	/* Keep the main event loop alive as long as the window exists */
	nautilus_main_event_loop_register (GTK_OBJECT (window));

	nautilus_window_allow_stop (window, FALSE);
}

/* Unconditionally synchronize the GtkUIManager of WINDOW. */
static void
nautilus_window_ui_update (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	gtk_ui_manager_ensure_update (window->details->ui_manager);
}

static gboolean
nautilus_window_clear_status (gpointer callback_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (callback_data);

	gtk_statusbar_pop (GTK_STATUSBAR (window->details->statusbar), 0); /* clear any previous message, underflow is allowed */

	return FALSE;
}

void
nautilus_window_set_status (NautilusWindow *window, const char *text)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	if (text != NULL && text[0] != '\0') {
		gtk_statusbar_push (GTK_STATUSBAR (window->details->statusbar), 0, text);
	} else {
		nautilus_window_clear_status (window);
	}
}

void
nautilus_window_go_to (NautilusWindow *window, GFile *location)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_open_location (window, location, FALSE);
}


void
nautilus_window_go_to_with_selection (NautilusWindow *window, GFile *location, GList *new_selection)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_open_location_with_selection (window, location, new_selection, FALSE);
}

static gboolean
nautilus_window_go_up_signal (NautilusWindow *window, gboolean close_behind)
{
	nautilus_window_go_up (window, close_behind);
	return TRUE;
}

void
nautilus_window_go_up (NautilusWindow *window, gboolean close_behind)
{
	GFile *parent;
	GList *selection;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	if (window->details->location == NULL) {
		return;
	}
	
	parent = g_file_get_parent (window->details->location);

	if (parent == NULL) {
		return;
	}
	
	selection = g_list_prepend (NULL, g_object_ref (window->details->location));
	
	nautilus_window_open_location_with_selection (window, parent, selection, close_behind);
	
	g_object_unref (parent);
	
	eel_g_object_list_free (selection);
}

static void
real_set_allow_up (NautilusWindow *window,
		   gboolean        allow)
{
	GtkAction *action;
	
        g_assert (NAUTILUS_IS_WINDOW (window));

	action = gtk_action_group_get_action (window->details->main_action_group,
					      NAUTILUS_ACTION_UP);
	gtk_action_set_sensitive (action, allow);
	action = gtk_action_group_get_action (window->details->main_action_group,
					      NAUTILUS_ACTION_UP_ACCEL);
	gtk_action_set_sensitive (action, allow);
}

void
nautilus_window_allow_up (NautilusWindow *window, gboolean allow)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			 set_allow_up, (window, allow));
}

static void
update_cursor (NautilusWindow *window)
{
	GdkCursor *cursor;

	if (window->details->allow_stop) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (GTK_WIDGET (window)->window, cursor);
		gdk_cursor_unref (cursor);
	} else {
		gdk_window_set_cursor (GTK_WIDGET (window)->window, NULL);
	}
}

void
nautilus_window_allow_stop (NautilusWindow *window, gboolean allow)
{
	GtkAction *action;
	
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	action = gtk_action_group_get_action (window->details->main_action_group,
					      NAUTILUS_ACTION_STOP);
	gtk_action_set_sensitive (action, allow);

	if (window->details->allow_stop != allow) {
		window->details->allow_stop = allow;

		if (GTK_WIDGET_REALIZED (GTK_WIDGET (window))) {
			update_cursor (window);
		}
	}
	
	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			 set_throbber_active, (window, allow));
}

void
nautilus_window_allow_reload (NautilusWindow *window, gboolean allow)
{
	GtkAction *action;
	
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	action = gtk_action_group_get_action (window->details->main_action_group,
					      NAUTILUS_ACTION_RELOAD);
	gtk_action_set_sensitive (action, allow);
}

void
nautilus_window_go_home (NautilusWindow *window)
{
	GFile *home;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	home = g_file_new_for_path (g_get_home_dir ());
	nautilus_window_open_location (window, home, FALSE);
	g_object_unref (home);
}

void
nautilus_window_prompt_for_location (NautilusWindow *window,
				     const char     *initial)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	
	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
                         prompt_for_location, (window, initial));
}

char *
nautilus_window_get_location_uri (NautilusWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

	if (window->details->location) {
		return g_file_get_uri (window->details->location);
	}
	return NULL;
}

GFile *
nautilus_window_get_location (NautilusWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

	if (window->details->location != NULL) {
		return g_object_ref (window->details->location);
	}
	return NULL;
}

void
nautilus_window_set_search_mode (NautilusWindow *window,
				 gboolean search_mode,
				 NautilusSearchDirectory *search_directory)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	window->details->search_mode = search_mode;

	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			 set_search_mode, (window, search_mode, search_directory));
}


void
nautilus_window_zoom_in (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	if (window->content_view != NULL) {
		nautilus_view_bump_zoom_level (window->content_view, 1);
	}
}

void
nautilus_window_zoom_to_level (NautilusWindow *window,
			       NautilusZoomLevel level)
{
	if (window->content_view != NULL) {
		nautilus_view_zoom_to_level (window->content_view, level);
	}
}

void
nautilus_window_zoom_out (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	if (window->content_view != NULL) {
		nautilus_view_bump_zoom_level (window->content_view, -1);
	}
}

void
nautilus_window_zoom_to_default (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	if (window->content_view != NULL) {
		nautilus_view_restore_default_zoom_level (window->content_view);
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

/* This must be called when construction of NautilusWindow is finished,
 * since it depends on the type of the argument, which isn't decided at
 * construction time.
 */
static void
nautilus_window_set_initial_window_geometry (NautilusWindow *window)
{
	GdkScreen *screen;
	guint max_width_for_screen, max_height_for_screen, min_width, min_height;
	guint default_width, default_height;
	
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

	if (NAUTILUS_IS_SPATIAL_WINDOW (window)) {
		min_width = NAUTILUS_SPATIAL_WINDOW_MIN_WIDTH;
		min_height = NAUTILUS_SPATIAL_WINDOW_MIN_HEIGHT;
	} else {
		min_width = NAUTILUS_WINDOW_MIN_WIDTH;
		min_height = NAUTILUS_WINDOW_MIN_HEIGHT;
	}
		
	gtk_widget_set_size_request (GTK_WIDGET (window), 
				     MIN (min_width, 
					  max_width_for_screen),
				     MIN (min_height, 
					  max_height_for_screen));

	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			 get_default_size, (window, &default_width, &default_height));

	gtk_window_set_default_size (GTK_WINDOW (window), 
				     MIN (default_width, 
				          max_width_for_screen), 
				     MIN (default_height, 
				          max_height_for_screen));
}

void
nautilus_window_constructed (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_set_initial_window_geometry (window);
	nautilus_undo_manager_attach (window->application->undo_manager, G_OBJECT (window));
}

static void
nautilus_window_set_property (GObject *object,
			      guint arg_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (object);
	
	switch (arg_id) {
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
	case ARG_APP:
		g_value_set_object (value, NAUTILUS_WINDOW (object)->application);
		break;
	}
}

static void
free_stored_viewers (NautilusWindow *window)
{
	eel_g_list_free_deep_custom (window->details->short_list_viewers, 
				     (GFunc) g_free, 
				     NULL);
	window->details->short_list_viewers = NULL;
	g_free (window->details->extra_viewer);
	window->details->extra_viewer = NULL;
}

static void
nautilus_window_destroy (GtkObject *object)
{
	NautilusWindow *window;
	GtkWidget *widget;

	window = NAUTILUS_WINDOW (object);

	cancel_view_as_callback (window);
	
	nautilus_window_manage_views_destroy (window);

	if (window->content_view) {
		g_object_unref (window->content_view);
		window->content_view = NULL;
	}

	if (window->new_content_view) {
		widget = nautilus_view_get_widget (window->new_content_view);
		gtk_widget_destroy (widget);
		g_object_unref (window->new_content_view);
		window->new_content_view = NULL;
	}

	GTK_OBJECT_CLASS (nautilus_window_parent_class)->destroy (object);
}

static void
nautilus_window_finalize (GObject *object)
{
	NautilusWindow *window;
	
	window = NAUTILUS_WINDOW (object);

	nautilus_window_remove_bookmarks_menu_callback (window);
	
	nautilus_window_manage_views_finalize (window);

	nautilus_window_set_viewed_file (window, NULL);

	nautilus_file_unref (window->details->viewed_file);

	free_stored_viewers (window);

	if (window->details->location) {
		g_object_ref (window->details->location);
	}
	eel_g_list_free_deep (window->details->pending_selection);

	if (window->current_location_bookmark != NULL) {
		g_object_unref (window->current_location_bookmark);
	}
	if (window->last_location_bookmark != NULL) {
		g_object_unref (window->last_location_bookmark);
	}

	g_object_unref (window->details->ui_manager);

	if (window->details->location_change_at_idle_id != 0) {
		g_source_remove (window->details->location_change_at_idle_id);
	}

	g_free (window->details->title);

	if (window->details->find_mount_cancellable != NULL) {
		g_cancellable_cancel (window->details->find_mount_cancellable);
		window->details->find_mount_cancellable = NULL;
	}

	G_OBJECT_CLASS (nautilus_window_parent_class)->finalize (object);
}

static GObject *
nautilus_window_constructor (GType type,
			     guint n_construct_properties,
			     GObjectConstructParam *construct_params)
{
	GObject *object;
	NautilusWindow *window;

	object = (* G_OBJECT_CLASS (nautilus_window_parent_class)->constructor) (type,
										 n_construct_properties,
										 construct_params);

	window = NAUTILUS_WINDOW (object);

	nautilus_window_initialize_menus_constructed (window);

	return object;
}

void
nautilus_window_show_window (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			 show_window, (window));

	nautilus_window_update_title (window);
	nautilus_window_update_icon (window);

	gtk_widget_show (GTK_WIDGET (window));

	if (window->details->viewed_file) {
		if (NAUTILUS_IS_SPATIAL_WINDOW (window)) {
			nautilus_file_set_has_open_window (window->details->viewed_file, TRUE);
		}
	}
}

void
nautilus_window_close (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			 close, (window));
	
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

	GTK_WIDGET_CLASS (nautilus_window_parent_class)->size_request (widget, requisition);

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

static void
nautilus_window_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (nautilus_window_parent_class)->realize (widget);
	update_cursor (NAUTILUS_WINDOW (widget));
}

/* Here we use an approach similar to the GEdit one. We override
 * GtkWindow's handler to reverse the order in which keybindings are
 * processed, and then we chain up to the grand parent handler.
 */
static gboolean
nautilus_window_key_press_event (GtkWidget *widget,
				 GdkEventKey *event)
{
	static gpointer grand_parent_class = NULL;
	NautilusWindow *window;
	gboolean handled;
	int i;

	window = NAUTILUS_WINDOW (widget);
	handled = FALSE;
	if (!grand_parent_class) {
		grand_parent_class = g_type_class_peek_parent (nautilus_window_parent_class);
	}

	/* handle currently focused widget */
	if (!handled) {
		handled = gtk_window_propagate_key_event (GTK_WINDOW (window), event);
	}
	
	/* handle extra window keybindings */
	if (!handled) {
		for (i = 0; i < G_N_ELEMENTS (extra_window_keybindings); i++) {
			if (extra_window_keybindings[i].keyval == event->keyval) {
				const GList *action_groups;
				GtkAction *action;

				action = NULL;

				action_groups = gtk_ui_manager_get_action_groups (window->details->ui_manager);
				while (action_groups != NULL && action == NULL) {
					action = gtk_action_group_get_action (action_groups->data,
									      extra_window_keybindings[i].action);
					action_groups = action_groups->next;
				}

				g_assert (action != NULL);
				if (gtk_action_is_sensitive (action)) {
					gtk_action_activate (action);
					handled = TRUE;
				}

				break;
			}
		}
	}
	
	/* handle mnemonics and accelerators */
	if (!handled) {
		handled = gtk_window_activate_key (GTK_WINDOW (window), event);
	}
	
	/* chain up to the grand parent */
	if (!handled) {
		handled = GTK_WIDGET_CLASS (grand_parent_class)->key_press_event (widget, event);
	}
	
	return handled;
}

/*
 * Main API
 */

static void
free_activate_view_data (gpointer data)
{
	ActivateViewData *activate_data;

	activate_data = data;

	g_free (activate_data->id);

	g_slice_free (ActivateViewData, activate_data);
}

static void
action_view_as_callback (GtkAction *action,
			 ActivateViewData *data)
{
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		nautilus_window_set_content_view (data->window, 
						  data->id);
	}
}

static GtkRadioAction *
add_view_as_menu_item (NautilusWindow *window,
		       const char *placeholder_path,
		       const char *identifier,
		       int index, /* extra_viewer is always index 0 */
		       guint merge_id)
{
	const NautilusViewInfo *info;
	GtkRadioAction *action;
	char action_name[32];
	ActivateViewData *data;

	char accel[32];
	char accel_path[48];
	unsigned int accel_keyval;

	info = nautilus_view_factory_lookup (identifier);
	
	g_snprintf (action_name, sizeof (action_name), "view_as_%d", index);
	action = gtk_radio_action_new (action_name,
				       _(info->view_menu_label_with_mnemonic),
				       _(info->display_location_label),
				       NULL,
				       0);

	if (index >= 1 && index <= 9) {
		g_snprintf (accel, sizeof (accel), "%d", index);
		g_snprintf (accel_path, sizeof (accel_path), "<Nautilus-Window>/%s", action_name);

		accel_keyval = gdk_keyval_from_name (accel);
		g_assert (accel_keyval != GDK_VoidSymbol);

		gtk_accel_map_add_entry (accel_path, accel_keyval, GDK_CONTROL_MASK);
		gtk_action_set_accel_path (GTK_ACTION (action), accel_path);
	}

	if (window->details->view_as_radio_action != NULL) {
		gtk_radio_action_set_group (action,
					    gtk_radio_action_get_group (window->details->view_as_radio_action));
	} else if (index != 0) {
		/* Index 0 is the extra view, and we don't want to use that here,
		   as it can get deleted/changed later */
		window->details->view_as_radio_action = action;
	}

	data = g_slice_new (ActivateViewData);
	data->window = window;
	data->id = g_strdup (identifier);
	g_signal_connect_data (action, "activate",
			       G_CALLBACK (action_view_as_callback),
			       data, (GClosureNotify) free_activate_view_data, 0);
	
	gtk_action_group_add_action (window->details->view_as_action_group,
				     GTK_ACTION (action));
	g_object_unref (action);

	gtk_ui_manager_add_ui (window->details->ui_manager,
			       merge_id,
			       placeholder_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	return action; /* return value owned by group */
}

/* Make a special first item in the "View as" option menu that represents
 * the current content view. This should only be called if the current
 * content view isn't already in the "View as" option menu.
 */
static void
update_extra_viewer_in_view_as_menus (NautilusWindow *window,
				      const char *id)
{
	gboolean had_extra_viewer;

	had_extra_viewer = window->details->extra_viewer != NULL;

	if (id == NULL) {
		if (!had_extra_viewer) {
			return;
		}
	} else {
		if (had_extra_viewer
		    && strcmp (window->details->extra_viewer, id) == 0) {
			return;
		}
	}
	g_free (window->details->extra_viewer);
	window->details->extra_viewer = g_strdup (id);

	if (window->details->extra_viewer_merge_id != 0) {
		gtk_ui_manager_remove_ui (window->details->ui_manager,
					  window->details->extra_viewer_merge_id);
		window->details->extra_viewer_merge_id = 0;
	}
	
	if (window->details->extra_viewer_radio_action != NULL) {
		gtk_action_group_remove_action (window->details->view_as_action_group,
						GTK_ACTION (window->details->extra_viewer_radio_action));
		window->details->extra_viewer_radio_action = NULL;
	}
	
	if (id != NULL) {
		window->details->extra_viewer_merge_id = gtk_ui_manager_new_merge_id (window->details->ui_manager);
                window->details->extra_viewer_radio_action =
			add_view_as_menu_item (window, 
					       NAUTILUS_MENU_PATH_EXTRA_VIEWER_PLACEHOLDER, 
					       window->details->extra_viewer, 
					       0,
					       window->details->extra_viewer_merge_id);
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
	const char *id;

	id = nautilus_window_get_content_view_id (window);
	update_extra_viewer_in_view_as_menus (window, id);
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
	char action_name[32];
	GList *node;
	GtkAction *action;

	g_assert (NAUTILUS_IS_WINDOW (window));

	if (window->content_view == NULL) {
		return;
	}
	for (node = window->details->short_list_viewers, index = 1;
	     node != NULL;
	     node = node->next, ++index) {
		if (nautilus_window_content_view_matches_iid (window, (char *)node->data)) {
			break;
		}
	}
	if (node == NULL) {
		replace_extra_viewer_in_view_as_menus (window);
		index = 0;
	} else {
		remove_extra_viewer_in_view_as_menus (window);
	}

	g_snprintf (action_name, sizeof (action_name), "view_as_%d", index);
	action = gtk_action_group_get_action (window->details->view_as_action_group,
					      action_name);

	/* Don't trigger the action callback when we're synchronizing */
	g_signal_handlers_block_matched (action,
					 G_SIGNAL_MATCH_FUNC,
					 0, 0,
					 NULL,
					 action_view_as_callback,
					 NULL);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	g_signal_handlers_unblock_matched (action,
					   G_SIGNAL_MATCH_FUNC,
					   0, 0,
					   NULL,
					   action_view_as_callback,
					   NULL);
}

static void
refresh_stored_viewers (NautilusWindow *window)
{
	GList *viewers;
	char *uri, *mimetype;

	uri = nautilus_file_get_uri (window->details->viewed_file);
	mimetype = nautilus_file_get_mime_type (window->details->viewed_file);
	viewers = nautilus_view_factory_get_views_for_uri (uri,
							   nautilus_file_get_file_type (window->details->viewed_file),
							   mimetype);
	g_free (uri);
	g_free (mimetype);

        free_stored_viewers (window);
	window->details->short_list_viewers = viewers;
}

static void
real_load_view_as_menu (NautilusWindow *window)
{
	GList *node;
	int index;
	guint merge_id;
	
	if (window->details->short_list_merge_id != 0) {
		gtk_ui_manager_remove_ui (window->details->ui_manager,
					  window->details->short_list_merge_id);
		window->details->short_list_merge_id = 0;
	}
	if (window->details->extra_viewer_merge_id != 0) {
		gtk_ui_manager_remove_ui (window->details->ui_manager,
					  window->details->extra_viewer_merge_id);
		window->details->extra_viewer_merge_id = 0;
		window->details->extra_viewer_radio_action = NULL;
	}
	if (window->details->view_as_action_group != NULL) {
		gtk_ui_manager_remove_action_group (window->details->ui_manager,
						    window->details->view_as_action_group);
		window->details->view_as_action_group = NULL;
	}

	g_free (window->details->extra_viewer);
	window->details->extra_viewer = NULL;
	
	refresh_stored_viewers (window);

	merge_id = gtk_ui_manager_new_merge_id (window->details->ui_manager);
	window->details->short_list_merge_id = merge_id;
	window->details->view_as_action_group = gtk_action_group_new ("ViewAsGroup");
	gtk_action_group_set_translation_domain (window->details->view_as_action_group, GETTEXT_PACKAGE);
	window->details->view_as_radio_action = NULL;
	
        /* Add a menu item for each view in the preferred list for this location. */
	/* Start on 1, because extra_viewer gets index 0 */
        for (node = window->details->short_list_viewers, index = 1; 
             node != NULL; 
             node = node->next, ++index) {
		/* Menu item in View menu. */
                add_view_as_menu_item (window, 
				       NAUTILUS_MENU_PATH_SHORT_LIST_PLACEHOLDER, 
				       node->data, 
				       index,
				       merge_id);
        }
	gtk_ui_manager_insert_action_group (window->details->ui_manager,
					    window->details->view_as_action_group,
					    -1);
	g_object_unref (window->details->view_as_action_group); /* owned by ui_manager */

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

	attributes = nautilus_mime_actions_get_required_file_attributes ();

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

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	dialog = gtk_message_dialog_new (GTK_WINDOW (window), 0, GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK, error_msg, NULL);
	gtk_widget_show (dialog);
}

static char *
real_get_title (NautilusWindow *window)
{
	char *title;

	title = NULL;
	
	if (window->new_content_view != NULL) {
                title = nautilus_view_get_title (window->new_content_view);
        } else if (window->content_view != NULL) {
                title = nautilus_view_get_title (window->content_view);
        }
        
	if (title == NULL) {
                title = nautilus_compute_title_for_location (window->details->location);
        }

	return title;
}

static char *
nautilus_window_get_cached_title (NautilusWindow *window)
{
	return g_strdup (window->details->title);
}

static char *
nautilus_window_get_title (NautilusWindow *window)
{
	return EEL_CALL_METHOD_WITH_RETURN_VALUE (NAUTILUS_WINDOW_CLASS, window,
						  get_title, (window));
}

static gboolean
real_set_title (NautilusWindow *window,
		const char *title)
{
	gboolean changed;
	char *copy;

	changed = FALSE;

	if (eel_strcmp (title, window->details->title) != 0) {
		changed = TRUE;

		g_free (window->details->title);
		window->details->title = g_strdup (title);
	}

        if (eel_strlen (window->details->title) > 0 && window->current_location_bookmark &&
            nautilus_bookmark_set_name (window->current_location_bookmark, window->details->title)) {
		changed = TRUE;

                /* Name of item in history list changed, tell listeners. */
                nautilus_send_history_list_changed ();
        }

	if (changed) {
		copy = g_strdup (window->details->title);
		g_signal_emit_by_name (window, "title_changed",
				       copy);
		g_free (copy);
	}

	return changed;
}

/* Sets window->details->title, and the actual GtkWindow title which
 * might look a bit different (e.g. with "file browser:" added) */
static void
nautilus_window_set_title (NautilusWindow *window, 
			   const char *title)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
                         set_title, (window, title));

}

/* update_title:
 * 
 * Re-calculate the window title.
 * Called when the location or view has changed.
 * @window: The NautilusWindow in question.
 * 
 */
void
nautilus_window_update_title (NautilusWindow *window)
{
	char *title;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	
	title = nautilus_window_get_title (window);
	nautilus_window_set_title (window, title);
	
	g_free (title);
}

/* nautilus_window_update_icon:
 * 
 * Re-calculate the window icon
 * Called when the location or view or icon set has changed.
 * @window: The NautilusWindow in question.
 * 
 */
void
nautilus_window_update_icon (NautilusWindow *window)
{
	NautilusIconInfo *info;
	const char *icon_name;
	GdkPixbuf *pixbuf;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	info = EEL_CALL_METHOD_WITH_RETURN_VALUE (NAUTILUS_WINDOW_CLASS, window,
						 get_icon, (window));

	icon_name = NULL;
	if (info) {
		icon_name = nautilus_icon_info_get_used_name (info);
		if (icon_name != NULL) {
			gtk_window_set_icon_name (GTK_WINDOW (window), icon_name);
		} else {
			pixbuf = nautilus_icon_info_get_pixbuf_nodefault (info);
			
			if (pixbuf) {
				gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
				g_object_unref (pixbuf);
			} 
		}
		
		g_object_unref (info);
	}
}


static void
real_set_content_view_widget (NautilusWindow *window,
			      NautilusView *new_view)
{
	GtkWidget *widget;
	
	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (new_view == NULL || NAUTILUS_IS_VIEW (new_view));
	
	if (new_view == window->content_view) {
		return;
	}
	
	if (window->content_view != NULL) {
		widget = nautilus_view_get_widget (window->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (window->content_view);
		window->content_view = NULL;
	}

	if (new_view != NULL) {
		widget = nautilus_view_get_widget (new_view);
		gtk_widget_show (widget);
	}

	window->content_view = new_view;
	g_object_ref (window->content_view);

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
					 NautilusView *frame)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	
	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
                         set_content_view_widget, (window, frame));

	nautilus_view_grab_focus (frame);
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

	GTK_WIDGET_CLASS (nautilus_window_parent_class)->show (widget);
	
	nautilus_window_ui_update (window);
}

GtkUIManager *
nautilus_window_get_ui_manager (NautilusWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

	return window->details->ui_manager;
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

	if (window->details->viewed_file != NULL) {
		if (NAUTILUS_IS_SPATIAL_WINDOW (window)) {
			nautilus_file_set_has_open_window (window->details->viewed_file,
							   FALSE);
		}
		nautilus_file_monitor_remove (window->details->viewed_file,
					      window);
	}

	if (file != NULL) {
		attributes =
			NAUTILUS_FILE_ATTRIBUTE_INFO |
			NAUTILUS_FILE_ATTRIBUTE_LINK_INFO;
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

static gboolean
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

		return TRUE;
	}

	return FALSE;
}

void
nautilus_remove_from_history_list_no_notify (GFile *location)
{
	NautilusBookmark *bookmark;

	bookmark = nautilus_bookmark_new (location, "");
	remove_from_history_list (bookmark);
	g_object_unref (bookmark);
}

gboolean
nautilus_add_to_history_list_no_notify (GFile *location,
					const char *name,
					gboolean has_custom_name,
					GIcon *icon)
{
	NautilusBookmark *bookmark;
	gboolean ret;

	bookmark = nautilus_bookmark_new_with_icon (location, name, has_custom_name, icon);
	ret = add_to_history_list (bookmark);
	g_object_unref (bookmark);

	return ret;
}

static void
real_add_current_location_to_history_list (NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));
                
	if (add_to_history_list (window->current_location_bookmark)) {
		nautilus_send_history_list_changed ();
	}
}

void
nautilus_window_add_current_location_to_history_list (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

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

static GList *
nautilus_window_get_history (NautilusWindow *window)
{
	return eel_g_object_list_copy (history_list);
}


static NautilusWindowType
nautilus_window_get_window_type (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	return NAUTILUS_WINDOW_GET_CLASS (window)->window_type;
}

static int
nautilus_window_get_selection_count (NautilusWindow *window)
{
	if (window->content_view != NULL) {
		return nautilus_view_get_selection_count (window->content_view);
	}
	return 0;
}

static GList *
nautilus_window_get_selection (NautilusWindow *window)
{
	if (window->content_view != NULL) {
		return nautilus_view_get_selection (window->content_view);
	}
	return NULL;
}

static NautilusWindowShowHiddenFilesMode
nautilus_window_get_hidden_files_mode (NautilusWindow *window)
{
	return window->details->show_hidden_files_mode;
}

static void
nautilus_window_set_hidden_files_mode (NautilusWindowInfo *window,
				       NautilusWindowShowHiddenFilesMode  mode)
{
	window->details->show_hidden_files_mode = mode;

	g_signal_emit_by_name (window, "hidden_files_mode_changed",
			       mode);
}

static NautilusBookmarkList *
nautilus_window_get_bookmark_list (NautilusWindow *window)
{
  return nautilus_get_bookmark_list ();
}

static void
nautilus_window_info_iface_init (NautilusWindowInfoIface *iface)
{
	iface->report_load_underway = nautilus_window_report_load_underway;
	iface->report_load_complete = nautilus_window_report_load_complete;
	iface->report_selection_changed = nautilus_window_report_selection_changed;
	iface->report_view_failed = nautilus_window_report_view_failed;
	iface->open_location = nautilus_window_open_location_full;
	iface->show_window = nautilus_window_show_window;
	iface->close_window = nautilus_window_close;
	iface->set_status = nautilus_window_set_status;
	iface->get_window_type = nautilus_window_get_window_type;
	iface->get_title = nautilus_window_get_cached_title;
	iface->get_history = nautilus_window_get_history;
	iface->get_bookmark_list = nautilus_window_get_bookmark_list;
	iface->get_current_location = nautilus_window_get_location_uri;
	iface->get_ui_manager = nautilus_window_get_ui_manager;
	iface->get_selection_count = nautilus_window_get_selection_count;
	iface->get_selection = nautilus_window_get_selection;
	iface->get_hidden_files_mode = nautilus_window_get_hidden_files_mode;
	iface->set_hidden_files_mode = nautilus_window_set_hidden_files_mode;
}

static void
nautilus_window_class_init (NautilusWindowClass *class)
{
	GtkBindingSet *binding_set;

	G_OBJECT_CLASS (class)->finalize = nautilus_window_finalize;
	G_OBJECT_CLASS (class)->constructor = nautilus_window_constructor;
	G_OBJECT_CLASS (class)->get_property = nautilus_window_get_property;
	G_OBJECT_CLASS (class)->set_property = nautilus_window_set_property;
	GTK_OBJECT_CLASS (class)->destroy = nautilus_window_destroy;
	GTK_WIDGET_CLASS (class)->show = nautilus_window_show;
	GTK_WIDGET_CLASS (class)->size_request = nautilus_window_size_request;
	GTK_WIDGET_CLASS (class)->realize = nautilus_window_realize;
	GTK_WIDGET_CLASS (class)->key_press_event = nautilus_window_key_press_event;
	class->add_current_location_to_history_list = real_add_current_location_to_history_list;
	class->get_title = real_get_title;
	class->set_title = real_set_title;
	class->set_content_view_widget = real_set_content_view_widget;
	class->load_view_as_menu = real_load_view_as_menu;
	class->set_allow_up = real_set_allow_up;

	g_object_class_install_property (G_OBJECT_CLASS (class),
					 ARG_APP,
					 g_param_spec_object ("app",
							      "Application",
							      "The NautilusApplication associated with this window.",
							      NAUTILUS_TYPE_APPLICATION,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	
	signals[GO_UP] =
		g_signal_new ("go_up",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (NautilusWindowClass, go_up),
			      g_signal_accumulator_true_handled, NULL,
			      eel_marshal_BOOLEAN__BOOLEAN,
			      G_TYPE_BOOLEAN, 1, G_TYPE_BOOLEAN);
	signals[RELOAD] =
		g_signal_new ("reload",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (NautilusWindowClass, reload),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[PROMPT_FOR_LOCATION] =
		g_signal_new ("prompt-for-location",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (NautilusWindowClass, prompt_for_location),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	
	binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, 0,
				      "go_up", 1,
				      G_TYPE_BOOLEAN, FALSE);
	gtk_binding_entry_add_signal (binding_set, GDK_F5, 0,
				      "reload", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_slash, 0,
				      "prompt-for-location", 1,
				      G_TYPE_STRING, "/");

	class->reload = nautilus_window_reload;
	class->go_up = nautilus_window_go_up_signal;

	/* Allow to set the colors of the extra view widgets */
	gtk_rc_parse_string ("\n"
			     "   style \"nautilus-extra-view-widgets-style-internal\"\n"
			     "   {\n"
			     "      bg[NORMAL] = \"" EXTRA_VIEW_WIDGETS_BACKGROUND "\"\n"
			     "   }\n"
			     "\n"
			     "    widget \"*.nautilus-extra-view-widget\" style:rc \"nautilus-extra-view-widgets-style-internal\" \n"
			     "\n");

	g_type_class_add_private (G_OBJECT_CLASS (class), sizeof (NautilusWindowDetails));
}

/**
 * nautilus_window_has_menubar_and_statusbar:
 * @window: A #NautilusWindow
 * 
 * Queries whether the window should have a menubar and statusbar, based on the
 * window_type from its class structure.
 * 
 * Return value: TRUE if the window should have a menubar and statusbar; FALSE
 * otherwise.
 **/
gboolean
nautilus_window_has_menubar_and_statusbar (NautilusWindow *window)
{
	return (nautilus_window_get_window_type (window) != NAUTILUS_WINDOW_DESKTOP);
}
