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
/* nautilus-window.h: Interface of the main window object */

#ifndef NAUTILUS_WINDOW_H
#define NAUTILUS_WINDOW_H

#include <bonobo/bonobo-win.h>
#include <eel/eel-glib-extensions.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-view-identifier.h>
#include "nautilus-applicable-views.h"
#include "nautilus-view-frame.h"
#include "nautilus-sidebar.h"
#include "nautilus-application.h"

#define NAUTILUS_TYPE_WINDOW              (nautilus_window_get_type())
#define NAUTILUS_WINDOW(obj)	          (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_WINDOW, NautilusWindow))
#define NAUTILUS_WINDOW_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_WINDOW, NautilusWindowClass))
#define NAUTILUS_IS_WINDOW(obj)	          (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_WINDOW))
#define NAUTILUS_IS_WINDOW_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_WINDOW))

#ifndef NAUTILUS_WINDOW_DEFINED
#define NAUTILUS_WINDOW_DEFINED
typedef struct NautilusWindow NautilusWindow;
#endif

typedef struct {
        BonoboWindowClass parent_spot;

	/* Function pointers for overriding, without corresponding signals */

	/* add_current_location_to_history_list is a function pointer that
	 * subclasses may override if they wish to add something other than
	 * NautilusWindow's idea of the "current location" to the history
	 * list, or nothing at all.
	 */
        void (* add_current_location_to_history_list) (NautilusWindow *window);
} NautilusWindowClass;

typedef struct NautilusWindowDetails NautilusWindowDetails;

struct NautilusWindow {
        BonoboWindow parent_object;
        
        NautilusWindowDetails *details;
        
        /** UI stuff **/
        NautilusSidebar *sidebar;
        GtkWidget *content_hbox;
        GtkWidget *view_as_option_menu;
        GtkWidget *navigation_bar;
        
        guint status_bar_clear_id;
        
        /** CORBA-related elements **/
        NautilusApplication *application;
        
        /** State information **/
        
        /* Information about current location/selection */
        
        /* Back/Forward chain, and history list. 
         * The data in these lists are NautilusBookmark pointers. 
         */
        GList *back_list, *forward_list;
        
        NautilusBookmark *current_location_bookmark; 
        NautilusBookmark *last_location_bookmark;
        
        /* Current views stuff */
        NautilusViewFrame *content_view;
        GList *sidebar_panels;
        
        /* Widgets to keep track of (for state changes, etc) */      
        GtkWidget *zoom_control;
        Bonobo_Unknown throbber;
        
        /* Pending changes */
        NautilusViewFrame *new_content_view;
};

GtkType          nautilus_window_get_type             (void);
void		 nautilus_window_ui_freeze	      (NautilusWindow	 *window);
void		 nautilus_window_ui_thaw	      (NautilusWindow	 *window);
void             nautilus_window_close                (NautilusWindow    *window);
char *           nautilus_window_get_location         (NautilusWindow    *window);
void             nautilus_window_go_to                (NautilusWindow    *window,
                                                       const char        *location);
gboolean         nautilus_window_get_search_mode      (NautilusWindow    *window);
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
void		 nautilus_window_clear_back_list      (NautilusWindow    *window);
void		 nautilus_window_clear_forward_list   (NautilusWindow    *window);
void		 nautilus_forget_history	      (void);
void             nautilus_bookmarks_exiting           (void);
void		 nautilus_window_reload		      (NautilusWindow	 *window);
gint 		 nautilus_window_get_base_page_index  (NautilusWindow 	 *window);
void 		 nautilus_window_hide_location_bar    (NautilusWindow 	 *window);
void 		 nautilus_window_show_location_bar    (NautilusWindow 	 *window);
gboolean	 nautilus_window_location_bar_showing (NautilusWindow    *window);
void 		 nautilus_window_hide_toolbar         (NautilusWindow 	 *window);
void 		 nautilus_window_show_toolbar         (NautilusWindow 	 *window);
gboolean	 nautilus_window_toolbar_showing      (NautilusWindow    *window);
void 		 nautilus_window_hide_sidebar         (NautilusWindow 	 *window);
void 		 nautilus_window_show_sidebar         (NautilusWindow 	 *window);
gboolean	 nautilus_window_sidebar_showing      (NautilusWindow    *window);
void 		 nautilus_window_hide_status_bar      (NautilusWindow 	 *window);
void 		 nautilus_window_show_status_bar      (NautilusWindow 	 *window);
gboolean	 nautilus_window_status_bar_showing   (NautilusWindow    *window);

#endif
