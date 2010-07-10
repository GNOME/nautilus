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
#include "nautilus-window-slot.h"
#include "nautilus-navigation-window-slot.h"
#include "nautilus-zoom-control.h"
#include "nautilus-search-bar.h"
#include "nautilus-navigation-window-pane.h"
#include <eel/eel-debug.h>
#include <eel/eel-marshal.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-horizontal-splitter.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-marshal.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-view-factory.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-undo.h>
#include <libnautilus-private/nautilus-search-directory.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <math.h>
#include <sys/time.h>

#define MAX_HISTORY_ITEMS 50

#define EXTRA_VIEW_WIDGETS_BACKGROUND "#a7c6e1"

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
	ZOOM_CHANGED,
	VIEW_AS_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct  {
	NautilusWindow *window;
	char *id;
} ActivateViewData;

static void cancel_view_as_callback         (NautilusWindowSlot      *slot);
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

	window->details->panes = NULL;
	window->details->active_pane = NULL;

	window->details->show_hidden_files_mode = NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_DEFAULT;
	
	/* Remove Top border on GtkStatusBar */
	gtk_rc_parse_string (
		"style \"statusbar-no-border\"\n"
		"{\n"
		"   GtkStatusbar::shadow_type = GTK_SHADOW_NONE\n"
		"}\n"
		"widget \"*.statusbar-noborder\" style \"statusbar-no-border\"");

	/* Set initial window title */
	gtk_window_set_title (GTK_WINDOW (window), _("Nautilus"));

	table = gtk_table_new (1, 6, FALSE);
	window->details->table = table;
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (window), table);

	statusbar = gtk_statusbar_new ();
	gtk_widget_set_name (statusbar, "statusbar-noborder");
	window->details->statusbar = statusbar;
	window->details->help_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (statusbar), "help_message");
	/* Statusbar is packed in the subclasses */

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
}

/* Unconditionally synchronize the GtkUIManager of WINDOW. */
static void
nautilus_window_ui_update (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	gtk_ui_manager_ensure_update (window->details->ui_manager);
}

static void
nautilus_window_push_status (NautilusWindow *window,
			     const char *text)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	/* clear any previous message, underflow is allowed */
	gtk_statusbar_pop (GTK_STATUSBAR (window->details->statusbar), 0);

	if (text != NULL && text[0] != '\0') {
		gtk_statusbar_push (GTK_STATUSBAR (window->details->statusbar), 0, text);
	}
}

void
nautilus_window_sync_status (NautilusWindow *window)
{
	NautilusWindowSlot *slot;

	slot = window->details->active_pane->active_slot;
	nautilus_window_push_status (window, slot->status_text);
}

void
nautilus_window_go_to (NautilusWindow *window, GFile *location)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_slot_go_to (window->details->active_pane->active_slot, location, FALSE);
}

void
nautilus_window_go_to_with_selection (NautilusWindow *window, GFile *location, GList *new_selection)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_slot_go_to_with_selection (window->details->active_pane->active_slot, location, new_selection);
}

static gboolean
nautilus_window_go_up_signal (NautilusWindow *window, gboolean close_behind)
{
	nautilus_window_go_up (window, close_behind, FALSE);
	return TRUE;
}

void
nautilus_window_go_up (NautilusWindow *window, gboolean close_behind, gboolean new_tab)
{
	NautilusWindowSlot *slot;
	GFile *parent;
	GList *selection;
	NautilusWindowOpenFlags flags;

	g_assert (NAUTILUS_IS_WINDOW (window));

	slot = window->details->active_pane->active_slot;

	if (slot->location == NULL) {
		return;
	}
	
	parent = g_file_get_parent (slot->location);

	if (parent == NULL) {
		return;
	}
	
	selection = g_list_prepend (NULL, g_object_ref (slot->location));
	
	flags = 0;
	if (close_behind) {
		flags |= NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND;
	}
	if (new_tab) {
		flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
	}

	nautilus_window_slot_open_location_full (slot, parent, 
						 NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE,
						 flags,
						 selection);
	
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
	NautilusWindowSlot *slot;
	GdkCursor *cursor;

	slot = window->details->active_pane->active_slot;

	if (slot->allow_stop) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), cursor);
		gdk_cursor_unref (cursor);
	} else {
		gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);
	}
}

void
nautilus_window_sync_allow_stop (NautilusWindow *window,
				 NautilusWindowSlot *slot)
{
	GtkAction *action;
	gboolean allow_stop;

	g_assert (NAUTILUS_IS_WINDOW (window));

	action = gtk_action_group_get_action (window->details->main_action_group,
					      NAUTILUS_ACTION_STOP);
	allow_stop = gtk_action_get_sensitive (action);

	if (slot != window->details->active_pane->active_slot ||
	    allow_stop != slot->allow_stop) {
		if (slot == window->details->active_pane->active_slot) {
			gtk_action_set_sensitive (action, slot->allow_stop);
		}

		if (gtk_widget_get_realized (GTK_WIDGET (window))) {
			update_cursor (window);
		}

		EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
				 sync_allow_stop, (window, slot));
	}
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
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_slot_go_home (window->details->active_pane->active_slot, FALSE);
}

void
nautilus_window_prompt_for_location (NautilusWindow *window,
				     const char     *initial)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	
	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
                         prompt_for_location, (window, initial));
}

static char *
nautilus_window_get_location_uri (NautilusWindow *window)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));

	slot = window->details->active_pane->active_slot;

	if (slot->location) {
		return g_file_get_uri (slot->location);
	}
	return NULL;
}

void
nautilus_window_zoom_in (NautilusWindow *window)
{
	g_assert (window != NULL);

	nautilus_window_pane_zoom_in (window->details->active_pane);
}

void
nautilus_window_zoom_to_level (NautilusWindow *window,
			       NautilusZoomLevel level)
{
	g_assert (window != NULL);

	nautilus_window_pane_zoom_to_level (window->details->active_pane, level);
}

void
nautilus_window_zoom_out (NautilusWindow *window)
{
	g_assert (window != NULL);

	nautilus_window_pane_zoom_out (window->details->active_pane);
}

void
nautilus_window_zoom_to_default (NautilusWindow *window)
{
	g_assert (window != NULL);

	nautilus_window_pane_zoom_to_default (window->details->active_pane);
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

	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			 get_min_size, (window, &min_width, &min_height));

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

static void
nautilus_window_constructed (GObject *self)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (self);

	nautilus_window_initialize_bookmarks_menu (window);
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
	GList *panes_copy;

	window = NAUTILUS_WINDOW (object);

	/* close all panes safely */
	panes_copy = g_list_copy (window->details->panes);
	g_list_foreach (panes_copy, (GFunc) nautilus_window_close_pane, NULL);
	g_list_free (panes_copy);

	/* the panes list should now be empty */
	g_assert (window->details->panes == NULL);
	g_assert (window->details->active_pane == NULL);

	GTK_OBJECT_CLASS (nautilus_window_parent_class)->destroy (object);
}

static void
nautilus_window_finalize (GObject *object)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (object);

	nautilus_window_remove_trash_monitor_callback (window);
	free_stored_viewers (window);

	if (window->details->bookmark_list != NULL) {
		g_object_unref (window->details->bookmark_list);
	}

	/* nautilus_window_close() should have run */
	g_assert (window->details->panes == NULL);

	g_object_unref (window->details->ui_manager);

	G_OBJECT_CLASS (nautilus_window_parent_class)->finalize (object);
}

static GObject *
nautilus_window_constructor (GType type,
			     guint n_construct_properties,
			     GObjectConstructParam *construct_params)
{
	GObject *object;
	NautilusWindow *window;
	NautilusWindowSlot *slot;

	object = (* G_OBJECT_CLASS (nautilus_window_parent_class)->constructor) (type,
										 n_construct_properties,
										 construct_params);

	window = NAUTILUS_WINDOW (object);

	slot = nautilus_window_open_slot (window->details->active_pane, 0);
	nautilus_window_set_active_slot (window, slot);

	return object;
}

void
nautilus_window_show_window (NautilusWindow    *window)
{
	NautilusWindowSlot *slot;
	NautilusWindowPane *pane;
	GList *l, *walk;

	for (walk = window->details->panes; walk; walk = walk->next) {
		pane = walk->data;
		for (l = pane->slots; l != NULL; l = l->next) {
			slot = l->data;

			nautilus_window_slot_update_title (slot);
			nautilus_window_slot_update_icon (slot);
		}
	}

	gtk_widget_show (GTK_WIDGET (window));

	slot = window->details->active_pane->active_slot;

	if (slot->viewed_file) {
		if (NAUTILUS_IS_SPATIAL_WINDOW (window)) {
			nautilus_file_set_has_open_window (slot->viewed_file, TRUE);
		}
	}
}

static void
nautilus_window_view_visible (NautilusWindow *window,
			      NautilusView *view)
{
	NautilusWindowSlot *slot;
	NautilusWindowPane *pane;
	GList *l, *walk;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	slot = nautilus_window_get_slot_for_view (window, view);

	/* Ensure we got the right active state for newly added panes */
	nautilus_window_slot_is_in_active_pane (slot, slot->pane->is_active);

	if (slot->visible) {
		return;
	}

	slot->visible = TRUE;

	pane = slot->pane;

	if (pane->visible) {
		return;
	}

	/* Look for other non-visible slots */
	for (l = pane->slots; l != NULL; l = l->next) {
		slot = l->data;

		if (!slot->visible) {
			return;
		}
	}

	/* None, this pane is visible */
	nautilus_window_pane_show (pane);

	/* Look for other non-visible panes */
	for (walk = window->details->panes; walk; walk = walk->next) {
		pane = walk->data;

		if (!pane->visible) {
			return;
		}
	}

	nautilus_window_pane_grab_focus (window->details->active_pane);

	/* All slots and panes visible, show window */
	nautilus_window_show_window (window);
}

void
nautilus_window_close (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			 close, (window));
	
	gtk_widget_destroy (GTK_WIDGET (window));
}

NautilusWindowSlot *
nautilus_window_open_slot (NautilusWindowPane *pane,
			   NautilusWindowOpenSlotFlags flags)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW_PANE (pane));
	g_assert (NAUTILUS_IS_WINDOW (pane->window));

	slot = EEL_CALL_METHOD_WITH_RETURN_VALUE (NAUTILUS_WINDOW_CLASS, pane->window,
						  open_slot, (pane, flags));

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));
	g_assert (pane->window == slot->pane->window);

	pane->slots = g_list_append (pane->slots, slot);

	return slot;
}

void
nautilus_window_close_pane (NautilusWindowPane *pane)
{
	NautilusWindow *window;

	g_assert (NAUTILUS_IS_WINDOW_PANE (pane));
	g_assert (NAUTILUS_IS_WINDOW (pane->window));
	g_assert (g_list_find (pane->window->details->panes, pane) != NULL);

	while (pane->slots != NULL) {
		NautilusWindowSlot *slot = pane->slots->data;

		nautilus_window_close_slot (slot);
	}

	window = pane->window;

	/* If the pane was active, set it to NULL. The caller is responsible
	 * for setting a new active pane with nautilus_window_pane_switch_to()
	 * if it wants to continue using the window. */
	if (window->details->active_pane == pane) {
		window->details->active_pane = NULL;
	}

	window->details->panes = g_list_remove (window->details->panes, pane);

	g_object_unref (pane);
}

static void
real_close_slot (NautilusWindowPane *pane,
		 NautilusWindowSlot *slot)
{
	nautilus_window_manage_views_close_slot (pane, slot);
	cancel_view_as_callback (slot);
}

void
nautilus_window_close_slot (NautilusWindowSlot *slot)
{
	NautilusWindowPane *pane;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));
	g_assert (NAUTILUS_IS_WINDOW_PANE(slot->pane));
	g_assert (g_list_find (slot->pane->slots, slot) != NULL);

	/* save pane because slot is not valid anymore after this call */
	pane = slot->pane;

	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, slot->pane->window,
			 close_slot, (slot->pane, slot));

	g_object_run_dispose (G_OBJECT (slot));
	slot->pane = NULL;
	g_object_unref (slot);
	pane->slots = g_list_remove (pane->slots, slot);
	pane->active_slots = g_list_remove (pane->active_slots, slot);

}

NautilusWindowPane*
nautilus_window_get_active_pane (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));
	return window->details->active_pane;
}

static void
real_set_active_pane (NautilusWindow *window, NautilusWindowPane *new_pane)
{
	/* make old pane inactive, and new one active.
	 * Currently active pane may be NULL (after init). */
	if (window->details->active_pane &&
	    window->details->active_pane != new_pane) {
		nautilus_window_pane_set_active (new_pane->window->details->active_pane, FALSE);
	}
	nautilus_window_pane_set_active (new_pane, TRUE);

	window->details->active_pane = new_pane;
}

/* Make the given pane the active pane of its associated window. This
 * always implies making the containing active slot the active slot of
 * the window. */
void
nautilus_window_set_active_pane (NautilusWindow *window,
				 NautilusWindowPane *new_pane)
{
	g_assert (NAUTILUS_IS_WINDOW_PANE (new_pane));
	if (new_pane->active_slot) {
		nautilus_window_set_active_slot (window, new_pane->active_slot);
	} else if (new_pane != window->details->active_pane) {
		real_set_active_pane (window, new_pane);
	}
}

/* Make both, the given slot the active slot and its corresponding
 * pane the active pane of the associated window.
 * new_slot may be NULL. */
void
nautilus_window_set_active_slot (NautilusWindow *window, NautilusWindowSlot *new_slot)
{
	NautilusWindowSlot *old_slot;

	g_assert (NAUTILUS_IS_WINDOW (window));

	if (new_slot) {
		g_assert (NAUTILUS_IS_WINDOW_SLOT (new_slot));
		g_assert (NAUTILUS_IS_WINDOW_PANE (new_slot->pane));
		g_assert (window == new_slot->pane->window);
		g_assert (g_list_find (new_slot->pane->slots, new_slot) != NULL);
	}

	if (window->details->active_pane != NULL) {
		old_slot = window->details->active_pane->active_slot;
	} else {
		old_slot = NULL;
	}

	if (old_slot == new_slot) {
		return;
	}

	/* make old slot inactive if it exists (may be NULL after init, for example) */
	if (old_slot != NULL) {
		/* inform window */
		if (old_slot->content_view != NULL) {
			nautilus_window_slot_disconnect_content_view (old_slot, old_slot->content_view);
		}

		/* inform slot & view */
		g_signal_emit_by_name (old_slot, "inactive");
	}

	/* deal with panes */
	if (new_slot &&
	    new_slot->pane != window->details->active_pane) {
		real_set_active_pane (window, new_slot->pane);
	}

	window->details->active_pane->active_slot = new_slot;

	/* make new slot active, if it exists */
	if (new_slot) {
		window->details->active_pane->active_slots =
			g_list_remove (window->details->active_pane->active_slots, new_slot);
		window->details->active_pane->active_slots =
			g_list_prepend (window->details->active_pane->active_slots, new_slot);

		/* inform sidebar panels */
                nautilus_window_report_location_change (window);
		/* TODO decide whether "selection-changed" should be emitted */

		if (new_slot->content_view != NULL) {
                        /* inform window */
                        nautilus_window_slot_connect_content_view (new_slot, new_slot->content_view);
                }

		/* inform slot & view */
                g_signal_emit_by_name (new_slot, "active");
	}
}

void
nautilus_window_slot_close (NautilusWindowSlot *slot)
{
    nautilus_window_pane_slot_close (slot->pane, slot);
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

static gboolean
nautilus_window_key_press_event (GtkWidget *widget,
				 GdkEventKey *event)
{
	NautilusWindow *window;
	int i;

	window = NAUTILUS_WINDOW (widget);

	for (i = 0; i < G_N_ELEMENTS (extra_window_keybindings); i++) {
		if (extra_window_keybindings[i].keyval == event->keyval) {
			const GList *action_groups;
			GtkAction *action;

			action = NULL;

			action_groups = gtk_ui_manager_get_action_groups (window->details->ui_manager);
			while (action_groups != NULL && action == NULL) {
				action = gtk_action_group_get_action (action_groups->data, extra_window_keybindings[i].action);
				action_groups = action_groups->next;
			}

			g_assert (action != NULL);
			if (gtk_action_is_sensitive (action)) {
				gtk_action_activate (action);
				return TRUE;
			}

			break;
		}
	}

	return GTK_WIDGET_CLASS (nautilus_window_parent_class)->key_press_event (widget, event);
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
	NautilusWindow *window;
	NautilusWindowSlot *slot;

	window = data->window;

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		slot = window->details->active_pane->active_slot;
		nautilus_window_slot_set_content_view (slot,
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
	NautilusWindowSlot *slot;
	const char *id;

	slot = window->details->active_pane->active_slot;

	id = nautilus_window_slot_get_content_view_id (slot);
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
	NautilusWindowSlot *slot;
	int index;
	char action_name[32];
	GList *node;
	GtkAction *action;

	g_assert (NAUTILUS_IS_WINDOW (window));

	slot = window->details->active_pane->active_slot;

	if (slot->content_view == NULL) {
		return;
	}
	for (node = window->details->short_list_viewers, index = 1;
	     node != NULL;
	     node = node->next, ++index) {
		if (nautilus_window_slot_content_view_matches_iid (slot, (char *)node->data)) {
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
	NautilusWindowSlot *slot;
	GList *viewers;
	char *uri, *mimetype;

	slot = window->details->active_pane->active_slot;

	uri = nautilus_file_get_uri (slot->viewed_file);
	mimetype = nautilus_file_get_mime_type (slot->viewed_file);
	viewers = nautilus_view_factory_get_views_for_uri (uri,
							   nautilus_file_get_file_type (slot->viewed_file),
							   mimetype);
	g_free (uri);
	g_free (mimetype);

        free_stored_viewers (window);
	window->details->short_list_viewers = viewers;
}

static void
load_view_as_menu (NautilusWindow *window)
{
	NautilusWindowSlot *slot;
	GList *node;
	int index;
	guint merge_id;

	slot = window->details->active_pane->active_slot;
	
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

	g_signal_emit (window, signals[VIEW_AS_CHANGED], 0);

}

static void
load_view_as_menus_callback (NautilusFile *file, 
			    gpointer callback_data)
{
	NautilusWindow *window;
	NautilusWindowSlot *slot;

	slot = callback_data;
	window = NAUTILUS_WINDOW (slot->pane->window);

	if (slot == window->details->active_pane->active_slot) {
		load_view_as_menu (window);
	}
}

static void
cancel_view_as_callback (NautilusWindowSlot *slot)
{
	nautilus_file_cancel_call_when_ready (slot->viewed_file, 
					      load_view_as_menus_callback,
					      slot);
}

void
nautilus_window_load_view_as_menus (NautilusWindow *window)
{
	NautilusWindowSlot *slot;
	NautilusFileAttributes attributes;

        g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	attributes = nautilus_mime_actions_get_required_file_attributes ();

	slot = window->details->active_pane->active_slot;

	cancel_view_as_callback (slot);
	nautilus_file_call_when_ready (slot->viewed_file,
				       attributes, 
				       load_view_as_menus_callback,
				       slot);
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
	g_assert (NAUTILUS_IS_WINDOW (window));

	return nautilus_window_slot_get_title (window->details->active_pane->active_slot);
}

static void
real_sync_title (NautilusWindow *window,
		 NautilusWindowSlot *slot)
{
	char *copy;

	if (slot == window->details->active_pane->active_slot) {
		copy = g_strdup (slot->title);
		g_signal_emit_by_name (window, "title_changed",
				       slot->title);
		g_free (copy);
	}
}

void
nautilus_window_sync_title (NautilusWindow *window,
			    NautilusWindowSlot *slot)
{
	EEL_CALL_METHOD (NAUTILUS_WINDOW_CLASS, window,
			 sync_title, (window, slot));
}

void
nautilus_window_sync_zoom_widgets (NautilusWindow *window)
{
	NautilusWindowSlot *slot;
	NautilusView *view;
	GtkAction *action;
	gboolean supports_zooming;
	gboolean can_zoom, can_zoom_in, can_zoom_out;
	NautilusZoomLevel zoom_level;

	slot = window->details->active_pane->active_slot;
	view = slot->content_view;

	if (view != NULL) {
		supports_zooming = nautilus_view_supports_zooming (view);
		zoom_level = nautilus_view_get_zoom_level (view);
		can_zoom = supports_zooming &&
			   zoom_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			   zoom_level <= NAUTILUS_ZOOM_LEVEL_LARGEST;
		can_zoom_in = can_zoom && nautilus_view_can_zoom_in (view);
		can_zoom_out = can_zoom && nautilus_view_can_zoom_out (view);
	} else {
		zoom_level = NAUTILUS_ZOOM_LEVEL_STANDARD;
		supports_zooming = FALSE;
		can_zoom = FALSE;
		can_zoom_in = FALSE;
		can_zoom_out = FALSE;
	}

	action = gtk_action_group_get_action (window->details->main_action_group,
					      NAUTILUS_ACTION_ZOOM_IN);
	gtk_action_set_visible (action, supports_zooming);
	gtk_action_set_sensitive (action, can_zoom_in);
	
	action = gtk_action_group_get_action (window->details->main_action_group,
					      NAUTILUS_ACTION_ZOOM_OUT);
	gtk_action_set_visible (action, supports_zooming);
	gtk_action_set_sensitive (action, can_zoom_out);

	action = gtk_action_group_get_action (window->details->main_action_group,
					      NAUTILUS_ACTION_ZOOM_NORMAL);
	gtk_action_set_visible (action, supports_zooming);
	gtk_action_set_sensitive (action, can_zoom);

	g_signal_emit (window, signals[ZOOM_CHANGED], 0,
		       zoom_level, supports_zooming, can_zoom,
		       can_zoom_in, can_zoom_out);
}

static void
zoom_level_changed_callback (NautilusView *view,
                             NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	/* This is called each time the component in
	 * the active slot successfully completed
	 * a zooming operation.
	 */
	nautilus_window_sync_zoom_widgets (window);
}


/* These are called
 *   A) when switching the view within the active slot
 *   B) when switching the active slot
 *   C) when closing the active slot (disconnect)
*/
void
nautilus_window_connect_content_view (NautilusWindow *window,
				      NautilusView *view)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_VIEW (view));

	slot = nautilus_window_get_slot_for_view (window, view);
	g_assert (slot == nautilus_window_get_active_slot (window));

	g_signal_connect (view, "zoom-level-changed",
			  G_CALLBACK (zoom_level_changed_callback),
			  window);

      /* Update displayed view in menu. Only do this if we're not switching
       * locations though, because if we are switching locations we'll
       * install a whole new set of views in the menu later (the current
       * views in the menu are for the old location).
       */
	if (slot->pending_location == NULL) {
		nautilus_window_load_view_as_menus (window);
	}

	nautilus_view_grab_focus (view);
}

void
nautilus_window_disconnect_content_view (NautilusWindow *window,
					 NautilusView *view)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_VIEW (view));

	slot = nautilus_window_get_slot_for_view (window, view);
	g_assert (slot == nautilus_window_get_active_slot (window));

	g_signal_handlers_disconnect_by_func (view, G_CALLBACK (zoom_level_changed_callback), window);
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

NautilusWindowPane *
nautilus_window_get_next_pane (NautilusWindow *window)
{
       NautilusWindowPane *next_pane;
       GList *node;

       /* return NULL if there is only one pane */
       if (!window->details->panes || !window->details->panes->next) {
	       return NULL;
       }

       /* get next pane in the (wrapped around) list */
       node = g_list_find (window->details->panes, window->details->active_pane);
       g_return_val_if_fail (node, NULL);
       if (node->next) {
	       next_pane = node->next->data;
       } else {
	       next_pane =  window->details->panes->data;
       }

       return next_pane;
}


void
nautilus_window_slot_set_viewed_file (NautilusWindowSlot *slot,
				      NautilusFile *file)
{
	NautilusWindow *window;
	NautilusFileAttributes attributes;

	if (slot->viewed_file == file) {
		return;
	}

	nautilus_file_ref (file);

	cancel_view_as_callback (slot);

	if (slot->viewed_file != NULL) {
		window = slot->pane->window;

		if (NAUTILUS_IS_SPATIAL_WINDOW (window)) {
			nautilus_file_set_has_open_window (slot->viewed_file,
							   FALSE);
		}
		nautilus_file_monitor_remove (slot->viewed_file,
					      slot);
	}

	if (file != NULL) {
		attributes =
			NAUTILUS_FILE_ATTRIBUTE_INFO |
			NAUTILUS_FILE_ATTRIBUTE_LINK_INFO;
		nautilus_file_monitor_add (file, slot, attributes);
	}

	nautilus_file_unref (slot->viewed_file);
	slot->viewed_file = file;
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

gboolean
nautilus_add_bookmark_to_history_list (NautilusBookmark *bookmark)
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

	bookmark = nautilus_bookmark_new (location, "", FALSE, NULL);
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

	bookmark = nautilus_bookmark_new (location, name, has_custom_name, icon);
	ret = nautilus_add_bookmark_to_history_list (bookmark);
	g_object_unref (bookmark);

	return ret;
}

NautilusWindowSlot *
nautilus_window_get_slot_for_view (NautilusWindow *window,
				   NautilusView *view)
{
	NautilusWindowSlot *slot;
	GList *l, *walk;

	for (walk = window->details->panes; walk; walk = walk->next) {
		NautilusWindowPane *pane = walk->data;

		for (l = pane->slots; l != NULL; l = l->next) {
			slot = l->data;
			if (slot->content_view == view ||
			    slot->new_content_view == view) {
				return slot;
			}
		}
	}

	return NULL;
}

void
nautilus_forget_history (void) 
{
	NautilusWindowSlot *slot;
	NautilusNavigationWindowSlot *navigation_slot;
	GList *window_node, *l, *walk;

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

			for (walk = NAUTILUS_WINDOW (window_node->data)->details->panes; walk; walk = walk->next) {
				NautilusWindowPane *pane = walk->data;
				for (l = pane->slots; l != NULL; l = l->next) {
					navigation_slot = l->data;

					nautilus_navigation_window_slot_clear_back_list (navigation_slot);
					nautilus_navigation_window_slot_clear_forward_list (navigation_slot);
				}
			}

			nautilus_navigation_window_allow_back (window, FALSE);
			nautilus_navigation_window_allow_forward (window, FALSE);
		}

		for (walk = NAUTILUS_WINDOW (window_node->data)->details->panes; walk; walk = walk->next) {
			NautilusWindowPane *pane = walk->data;
			for (l = pane->slots; l != NULL; l = l->next) {
				slot = l->data;
				history_list = g_list_remove (history_list,
							      slot->current_location_bookmark);
			}
		}
	}

	/* Clobber history list. */
	free_history_list ();

	/* Re-add each window's current location to history list. */
	for (window_node = nautilus_application_get_window_list ();
	     window_node != NULL;
	     window_node = window_node->next) {
		NautilusWindow *window;
		NautilusWindowSlot *slot;
		GList *l;

		window = NAUTILUS_WINDOW (window_node->data);
		for (walk = window->details->panes; walk; walk = walk->next) {
			NautilusWindowPane *pane = walk->data;
			for (l = pane->slots; l != NULL; l = l->next) {
				slot = NAUTILUS_WINDOW_SLOT (l->data);
				nautilus_window_slot_add_current_location_to_history_list (slot);
			}
		}
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
	NautilusWindowSlot *slot;
 
	g_assert (NAUTILUS_IS_WINDOW (window));
 
	slot = window->details->active_pane->active_slot;
 
	if (slot->content_view != NULL) {
		return nautilus_view_get_selection_count (slot->content_view);
	}

	return 0;
}

static GList *
nautilus_window_get_selection (NautilusWindow *window)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));

	slot = window->details->active_pane->active_slot;

	if (slot->content_view != NULL) {
		return nautilus_view_get_selection (slot->content_view);
	}
	return NULL;
}

static NautilusWindowShowHiddenFilesMode
nautilus_window_get_hidden_files_mode (NautilusWindowInfo *window)
{
	return window->details->show_hidden_files_mode;
}

static void
nautilus_window_set_hidden_files_mode (NautilusWindowInfo *window,
				       NautilusWindowShowHiddenFilesMode  mode)
{
	window->details->show_hidden_files_mode = mode;

	g_signal_emit_by_name (window, "hidden_files_mode_changed");
}

static gboolean
nautilus_window_get_initiated_unmount (NautilusWindowInfo *window)
{
	return window->details->initiated_unmount;
}

static void
nautilus_window_set_initiated_unmount (NautilusWindowInfo *window,
				       gboolean initiated_unmount)
{
	window->details->initiated_unmount = initiated_unmount;
}

static char *
nautilus_window_get_cached_title (NautilusWindow *window)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));

	slot = window->details->active_pane->active_slot;

	return g_strdup (slot->title);
}

NautilusWindowSlot *
nautilus_window_get_active_slot (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	return window->details->active_pane->active_slot;
}

NautilusWindowSlot *
nautilus_window_get_extra_slot (NautilusWindow *window)
{
	NautilusWindowPane *extra_pane;
	GList *node;

	g_assert (NAUTILUS_IS_WINDOW (window));


	/* return NULL if there is only one pane */
	if (window->details->panes == NULL ||
	    window->details->panes->next == NULL) {
		return NULL;
	}

	/* get next pane in the (wrapped around) list */
	node = g_list_find (window->details->panes,
			    window->details->active_pane);
	g_return_val_if_fail (node, FALSE);

	if (node->next) {
		extra_pane = node->next->data;
	}
	else {
		extra_pane =  window->details->panes->data;
	}

	return extra_pane->active_slot;
}

GList *
nautilus_window_get_slots (NautilusWindow *window)
{
	GList *walk,*list;

	g_assert (NAUTILUS_IS_WINDOW (window));

	list = NULL;
	for (walk = window->details->panes; walk; walk = walk->next) {
		NautilusWindowPane *pane = walk->data;
		list  = g_list_concat (list, g_list_copy(pane->slots));
	}
	return list;
}

static void
nautilus_window_info_iface_init (NautilusWindowInfoIface *iface)
{
	iface->report_load_underway = nautilus_window_report_load_underway;
	iface->report_load_complete = nautilus_window_report_load_complete;
	iface->report_selection_changed = nautilus_window_report_selection_changed;
	iface->report_view_failed = nautilus_window_report_view_failed;
	iface->view_visible = nautilus_window_view_visible;
	iface->close_window = nautilus_window_close;
	iface->push_status = nautilus_window_push_status;
	iface->get_window_type = nautilus_window_get_window_type;
	iface->get_title = nautilus_window_get_cached_title;
	iface->get_history = nautilus_window_get_history;
	iface->get_current_location = nautilus_window_get_location_uri;
	iface->get_ui_manager = nautilus_window_get_ui_manager;
	iface->get_selection_count = nautilus_window_get_selection_count;
	iface->get_selection = nautilus_window_get_selection;
	iface->get_hidden_files_mode = nautilus_window_get_hidden_files_mode;
	iface->set_hidden_files_mode = nautilus_window_set_hidden_files_mode;
	iface->get_active_slot = nautilus_window_get_active_slot;
	iface->get_extra_slot = nautilus_window_get_extra_slot;
	iface->get_initiated_unmount = nautilus_window_get_initiated_unmount;
	iface->set_initiated_unmount = nautilus_window_set_initiated_unmount;
}

static void
nautilus_window_class_init (NautilusWindowClass *class)
{
	GtkBindingSet *binding_set;

	G_OBJECT_CLASS (class)->finalize = nautilus_window_finalize;
	G_OBJECT_CLASS (class)->constructor = nautilus_window_constructor;
	G_OBJECT_CLASS (class)->constructed = nautilus_window_constructed;
	G_OBJECT_CLASS (class)->get_property = nautilus_window_get_property;
	G_OBJECT_CLASS (class)->set_property = nautilus_window_set_property;
	GTK_OBJECT_CLASS (class)->destroy = nautilus_window_destroy;
	GTK_WIDGET_CLASS (class)->show = nautilus_window_show;
	GTK_WIDGET_CLASS (class)->size_request = nautilus_window_size_request;
	GTK_WIDGET_CLASS (class)->realize = nautilus_window_realize;
	GTK_WIDGET_CLASS (class)->key_press_event = nautilus_window_key_press_event;
	class->get_title = real_get_title;
	class->sync_title = real_sync_title;
	class->set_allow_up = real_set_allow_up;
	class->close_slot = real_close_slot;

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
	signals[ZOOM_CHANGED] =
		g_signal_new ("zoom-changed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      nautilus_marshal_VOID__INT_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN,
			      G_TYPE_NONE, 5,
			      G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
			      G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	signals[VIEW_AS_CHANGED] =
		g_signal_new ("view-as-changed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

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
