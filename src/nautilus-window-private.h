/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */

#ifndef NAUTILUS_WINDOW_PRIVATE_H
#define NAUTILUS_WINDOW_PRIVATE_H

#include "nautilus-window.h"
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-container.h>
#include <bonobo/bonobo-ui-toolbar-button-item.h>
#include <libnautilus-private/nautilus-directory.h>

typedef enum {
        NAUTILUS_LOCATION_CHANGE_STANDARD,
        NAUTILUS_LOCATION_CHANGE_BACK,
        NAUTILUS_LOCATION_CHANGE_FORWARD,
        NAUTILUS_LOCATION_CHANGE_RELOAD,
        NAUTILUS_LOCATION_CHANGE_REDIRECT
} NautilusLocationChangeType;

/* FIXME bugzilla.gnome.org 42575: Migrate more fields into here. */
struct NautilusWindowDetails
{
        /* Bonobo. */
        BonoboUIContainer *ui_container;
        BonoboUIComponent *shell_ui;
        gboolean updating_bonobo_state;

	int ui_change_depth;
	guint ui_idle_id;
	gboolean ui_is_frozen;
	gboolean ui_pending_initialize_menus_part_2;

        /* Menus. */
	guint refresh_bookmarks_menu_idle_id;
	guint refresh_go_menu_idle_id;

	/* Toolbar. */
	BonoboUIToolbarButtonItem *back_button_item;
	BonoboUIToolbarButtonItem *forward_button_item;

        /* Current location. */
        char *location;
        GList *selection;
	char *title;
	NautilusFile *viewed_file;
        gboolean viewed_file_seen;

        /* New location. */
        NautilusLocationChangeType location_change_type;
        guint location_change_distance;
        char *pending_location;
        GList *pending_selection;
        NautilusDetermineViewHandle *determine_view_handle;

        /* View As choices */
        GList *short_list_viewers;
        NautilusViewIdentifier *extra_viewer;

        /* Throbber. */
	Bonobo_EventSource_ListenerId throbber_location_change_request_listener_id;

        /* Deferred location change. */
        char *location_to_change_to_at_idle;
        guint location_change_at_idle_id;

        /* Location bar */
        gboolean temporary_navigation_bar;
};

#define NAUTILUS_MENU_PATH_BACK_ITEM			"/menu/Go/Back"
#define NAUTILUS_MENU_PATH_FORWARD_ITEM			"/menu/Go/Forward"
#define NAUTILUS_MENU_PATH_UP_ITEM			"/menu/Go/Up"

#define NAUTILUS_MENU_PATH_RELOAD_ITEM			"/menu/View/Reload"
#define NAUTILUS_MENU_PATH_ZOOM_IN_ITEM			"/menu/View/Zoom Items Placeholder/Zoom In"
#define NAUTILUS_MENU_PATH_ZOOM_OUT_ITEM		"/menu/View/Zoom Items Placeholder/Zoom Out"
#define NAUTILUS_MENU_PATH_ZOOM_NORMAL_ITEM		"/menu/View/Zoom Items Placeholder/Zoom Normal"

#define NAUTILUS_COMMAND_BACK				"/commands/Back"
#define NAUTILUS_COMMAND_FORWARD			"/commands/Forward"
#define NAUTILUS_COMMAND_UP				"/commands/Up"

#define NAUTILUS_COMMAND_RELOAD				"/commands/Reload"
#define NAUTILUS_COMMAND_STOP				"/commands/Stop"
#define NAUTILUS_COMMAND_ZOOM_IN			"/commands/Zoom In"
#define NAUTILUS_COMMAND_ZOOM_OUT			"/commands/Zoom Out"
#define NAUTILUS_COMMAND_ZOOM_NORMAL			"/commands/Zoom Normal"

/* window geometry */
/* These are very small, and a Nautilus window at this tiny size is *almost*
 * completely unusable. However, if all the extra bits (sidebar, location bar, etc)
 * are turned off, you can see an icon or two at this size. See bug 5946.
 */
#define NAUTILUS_WINDOW_MIN_WIDTH			200
#define NAUTILUS_WINDOW_MIN_HEIGHT			200

#define NAUTILUS_WINDOW_DEFAULT_WIDTH			800
#define NAUTILUS_WINDOW_DEFAULT_HEIGHT			550

void               nautilus_window_set_status                            (NautilusWindow    *window,
                                                                          const char        *status);
void               nautilus_window_load_view_as_menus                    (NautilusWindow    *window);
void               nautilus_window_synch_view_as_menus                   (NautilusWindow    *window);
void               nautilus_window_initialize_menus_part_1               (NautilusWindow    *window);
void               nautilus_window_initialize_menus_part_2               (NautilusWindow    *window);
void               nautilus_window_initialize_toolbars                   (NautilusWindow    *window);
void		   nautilus_window_handle_ui_event_callback		 (BonoboUIComponent *ui,
									  const char	    *id,
									  Bonobo_UIComponent_EventType type,
									  const char	    *state,
									  NautilusWindow    *window);
void               nautilus_window_go_back                               (NautilusWindow    *window);
void               nautilus_window_go_forward                            (NautilusWindow    *window);
void               nautilus_window_go_up                                 (NautilusWindow    *window);
void               nautilus_window_update_find_menu_item                 (NautilusWindow    *window);
void               nautilus_window_toolbar_remove_theme_callback         (NautilusWindow    *window);
void               nautilus_window_remove_bookmarks_menu_callback        (NautilusWindow    *window);
void               nautilus_window_remove_go_menu_callback               (NautilusWindow    *window);
void               nautilus_window_remove_bookmarks_menu_items           (NautilusWindow    *window);
void               nautilus_window_remove_go_menu_items                  (NautilusWindow    *window);
void               nautilus_window_update_show_hide_menu_items           (NautilusWindow    *window);
void               nautilus_window_zoom_in                               (NautilusWindow    *window);
void               nautilus_window_zoom_out                              (NautilusWindow    *window);
void               nautilus_window_zoom_to_level                         (NautilusWindow    *window,
                                                                          double             level);
void               nautilus_window_zoom_to_fit                           (NautilusWindow    *window);
void		   nautilus_window_show_view_as_dialog			 (NautilusWindow    *window);
void               nautilus_window_set_content_view_widget               (NautilusWindow    *window,
                                                                          NautilusViewFrame *content_view);
void               nautilus_window_add_sidebar_panel                     (NautilusWindow    *window,
                                                                          NautilusViewFrame *sidebar_panel);
void               nautilus_window_remove_sidebar_panel                  (NautilusWindow    *window,
                                                                          NautilusViewFrame *sidebar_panel);
Bonobo_UIContainer nautilus_window_get_ui_container                      (NautilusWindow    *window);
void               nautilus_window_set_viewed_file                       (NautilusWindow    *window,
                                                                          NautilusFile      *file);
void               nautilus_send_history_list_changed                    (void);
void               nautilus_window_add_current_location_to_history_list  (NautilusWindow    *window);
void               nautilus_remove_from_history_list_no_notify           (const char        *location);
GList *            nautilus_get_history_list                             (void);
void               nautilus_window_bookmarks_preference_changed_callback (gpointer           user_data);

#endif /* NAUTILUS_WINDOW_PRIVATE_H */
