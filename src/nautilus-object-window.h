/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* nautilus-window.h: Interface of the main window object */

#ifndef NAUTILUS_WINDOW_H
#define NAUTILUS_WINDOW_H

#include <libgnomeui/gnome-app.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-bookmark.h>
#include <libnautilus-extensions/nautilus-view-identifier.h>
#include "nautilus-applicable-views.h"
#include "nautilus-view-frame.h"
#include "nautilus-sidebar.h"
#include "nautilus-application.h"
#include <bonobo/bonobo-ui-handler.h>

#define NAUTILUS_TYPE_WINDOW (nautilus_window_get_type())
#define NAUTILUS_WINDOW(obj)	        (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_WINDOW, NautilusWindow))
#define NAUTILUS_WINDOW_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_WINDOW, NautilusWindowClass))
#define NAUTILUS_IS_WINDOW(obj)	        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_WINDOW))
#define NAUTILUS_IS_WINDOW_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_WINDOW))

#ifndef NAUTILUS_WINDOW_DEFINED
#define NAUTILUS_WINDOW_DEFINED
typedef struct NautilusWindow NautilusWindow;
#endif

typedef struct {
  GnomeAppClass parent_spot;
} NautilusWindowClass;

typedef struct NautilusWindowStateInfo NautilusWindowStateInfo;

typedef enum {
  NAUTILUS_LOCATION_CHANGE_STANDARD,
  NAUTILUS_LOCATION_CHANGE_BACK,
  NAUTILUS_LOCATION_CHANGE_FORWARD,
  NAUTILUS_LOCATION_CHANGE_RELOAD
} NautilusLocationChangeType;

typedef struct NautilusWindowDetails NautilusWindowDetails;

struct NautilusWindow {
  GnomeApp parent_object;

  NautilusWindowDetails *details;

  /** UI stuff **/
  NautilusSidebar *sidebar;
  GtkWidget *content_hbox;
  GtkWidget *view_as_option_menu;
  GtkWidget *navigation_bar;

  guint statusbar_ctx, statusbar_clear_id;

  /** CORBA-related elements **/
  BonoboUIHandler *ui_handler;
  NautilusApplication *application;
  
  /* FIXME bugzilla.eazel.com 916: Workaround for Bonobo bug. */
  gboolean updating_bonobo_radio_menu_item;

  /** State information **/

  /* Information about current location/selection */
  char *location;
  GList *selection;
  char *requested_title;
  char *default_title;
  
  /* Back/Forward chain, and history list. 
   * The data in these lists are NautilusBookmark pointers. 
   */
  GSList *back_list, *forward_list;

  NautilusBookmark *current_location_bookmark; 
  NautilusBookmark *last_location_bookmark;

  /* Current views stuff */
  NautilusViewFrame *content_view;
  NautilusViewIdentifier *content_view_id;
  GList *sidebar_panels;

  /* Widgets to keep track of (for state changes, etc) */
  GtkWidget *back_button;
  GtkWidget *forward_button;
  GtkWidget *up_button;
  GtkWidget *reload_button;
  GtkWidget *search_local_button;
  GtkWidget *search_web_button;
  GtkWidget *stop_button;
  GtkWidget *home_button;
  
  GtkWidget *zoom_control;

  /* Pending changes */
  NautilusNavigationInfo *pending_ni;
  NautilusViewFrame *new_content_view, *new_requesting_view;
  GList *new_sidebar_panels;
  GList *error_views;

  enum { NW_LOADING_INFO, NW_LOADING_VIEWS, NW_IDLE } state;

  NautilusNavigationInfo *cancel_tag;
  gboolean location_change_end_reached;

  guint action_tag;
  guint16 made_changes, making_changes;

  NautilusLocationChangeType location_change_type;
  guint location_change_distance;
  
  nautilus_boolean_bit changes_pending : 1;
  nautilus_boolean_bit views_shown : 1;
  nautilus_boolean_bit view_bombed_out : 1;
  nautilus_boolean_bit view_activation_complete : 1;
  nautilus_boolean_bit sent_update_view : 1;
  nautilus_boolean_bit cv_progress_initial : 1;
  nautilus_boolean_bit cv_progress_done : 1;
  nautilus_boolean_bit cv_progress_error : 1;
  nautilus_boolean_bit reset_to_idle : 1;
};

GtkType          nautilus_window_get_type             (void);
void             nautilus_window_close                (NautilusWindow    *window);
void             nautilus_window_set_content_view     (NautilusWindow    *window,
                                                       NautilusViewFrame *content_view);
void             nautilus_window_add_sidebar_panel    (NautilusWindow    *window,
                                                       NautilusViewFrame *sidebar_panel);
void             nautilus_window_remove_sidebar_panel (NautilusWindow    *window,
                                                       NautilusViewFrame *sidebar_panel);
void             nautilus_window_goto_uri             (NautilusWindow    *window,
                                                       const char        *uri);
void             nautilus_window_set_search_mode      (NautilusWindow    *window,
                                                       gboolean           search_mode);
void             nautilus_window_go_home              (NautilusWindow    *window);
void		 nautilus_window_go_web_search	      (NautilusWindow    *window);
void             nautilus_window_display_error        (NautilusWindow    *window,
                                                       const char        *error_msg);
void             nautilus_window_allow_back           (NautilusWindow    *window,
                                                       gboolean           allow);
void             nautilus_window_allow_forward        (NautilusWindow    *window,
                                                       gboolean           allow);
void             nautilus_window_allow_up             (NautilusWindow    *window,
                                                       gboolean           allow);
void             nautilus_window_allow_reload         (NautilusWindow    *window,
                                                       gboolean           allow);
void             nautilus_window_allow_stop           (NautilusWindow    *window,
                                                       gboolean           allow);
void             nautilus_bookmarks_exiting           (void);
void		 nautilus_window_reload		      (NautilusWindow	 *window);
gint 		 nautilus_window_get_base_page_index  (NautilusWindow 	 *window);
void 		 nautilus_window_hide_locationbar     (NautilusWindow 	 *window);
void 		 nautilus_window_show_locationbar     (NautilusWindow 	 *window);
void 		 nautilus_window_hide_toolbar         (NautilusWindow 	 *window);
void 		 nautilus_window_show_toolbar         (NautilusWindow 	 *window);
void 		 nautilus_window_hide_sidebar         (NautilusWindow 	 *window);
void 		 nautilus_window_show_sidebar         (NautilusWindow 	 *window);
void 		 nautilus_window_hide_statusbar       (NautilusWindow 	 *window);
void 		 nautilus_window_show_statusbar       (NautilusWindow 	 *window);

#endif
