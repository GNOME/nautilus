/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
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
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

#ifndef NAUTILUS_WINDOW_PRIVATE_H
#define NAUTILUS_WINDOW_PRIVATE_H

#include "nautilus-window.h"
#include <libnautilus/libnautilus.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <gtk/gtk.h>

typedef enum {
  CV_PROGRESS_INITIAL = 1,
  CV_PROGRESS_DONE,
  CV_PROGRESS_ERROR,
  VIEW_ERROR,
  RESET_TO_IDLE, /* Not a real item - a command */
  NAVINFO_RECEIVED,
  NEW_CONTENT_VIEW_ACTIVATED,
} NautilusWindowStateItem;

/* FIXME bugzilla.eazel.com 2575: Need to migrate window fields into here. */
struct NautilusWindowDetails
{
	guint refresh_dynamic_bookmarks_idle_id;
	guint refresh_go_menu_idle_id;
};

#define NAUTILUS_MENU_PATH_BACK_ITEM			"/Go/Back"
#define NAUTILUS_MENU_PATH_FORWARD_ITEM			"/Go/Forward"
#define NAUTILUS_MENU_PATH_UP_ITEM			"/Go/Up"

#define NAUTILUS_MENU_PATH_RELOAD_ITEM			"/View/Reload"
#define NAUTILUS_MENU_PATH_ZOOM_IN_ITEM			"/View/Zoom In"
#define NAUTILUS_MENU_PATH_ZOOM_OUT_ITEM		"/View/Zoom Out"
#define NAUTILUS_MENU_PATH_ZOOM_NORMAL_ITEM		"/View/Zoom Normal"


void                 nautilus_window_set_state_info                    (NautilusWindow             *window,
									... /* things to set, plus optional parameters */);
void                 nautilus_window_set_status                        (NautilusWindow             *window,
									const char                 *status);
void                 nautilus_window_back_or_forward                   (NautilusWindow             *window,
									gboolean                    back,
									guint                       distance);
void                 nautilus_window_load_content_view_menu            (NautilusWindow             *window);
void                 nautilus_window_synch_content_view_menu           (NautilusWindow             *window);
void                 nautilus_window_connect_view                      (NautilusWindow             *window,
									NautilusViewFrame          *view);
void		     nautilus_window_disconnect_view		       (NautilusWindow		   *window,
									NautilusViewFrame	   *view);
void                 nautilus_window_view_failed                       (NautilusWindow             *window,
									NautilusViewFrame          *view);
void                 nautilus_send_history_list_changed                (void);
void                 nautilus_add_to_history_list                      (NautilusBookmark           *bookmark);
GList *              nautilus_get_history_list                         (void);
void                 nautilus_window_add_bookmark_for_current_location (NautilusWindow             *window);
void                 nautilus_window_initialize_menus                  (NautilusWindow             *window);
void                 nautilus_window_initialize_toolbars               (NautilusWindow             *window);
void                 nautilus_window_go_back                           (NautilusWindow             *window);
void                 nautilus_window_go_forward                        (NautilusWindow             *window);
void                 nautilus_window_go_up                             (NautilusWindow             *window);
void		     nautilus_window_update_find_menu_item	       (NautilusWindow		   *window);
void                 nautilus_window_toolbar_remove_theme_callback     (NautilusWindow             *window);
NautilusUndoManager *nautilus_window_get_undo_manager                  (NautilusWindow             *window);
void                 nautilus_window_begin_location_change             (NautilusWindow             *window,
									const char                 *location,
									NautilusViewFrame          *requesting_view,
									NautilusLocationChangeType  type,
									guint                       distance);
void		     nautilus_window_remove_bookmarks_menu_callback    (NautilusWindow 		   *window);
void		     nautilus_window_remove_go_menu_callback 	       (NautilusWindow 		   *window);
void		     nautilus_window_remove_bookmarks_menu_items       (NautilusWindow 		   *window);
void		     nautilus_window_remove_go_menu_items 	       (NautilusWindow 		   *window);
void		     nautilus_window_update_show_hide_menu_items       (NautilusWindow		   *window);
void		     nautilus_window_zoom_in 			       (NautilusWindow 		   *window);
void		     nautilus_window_zoom_out 			       (NautilusWindow 		   *window);
void		     nautilus_window_zoom_to_level	       	       (NautilusWindow 		   *window,
									double			    level);
void		     nautilus_window_zoom_to_fit 		       (NautilusWindow 		   *window);

#endif /* NAUTILUS_WINDOW_PRIVATE_H */
