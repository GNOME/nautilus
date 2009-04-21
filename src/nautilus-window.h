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

#include <gtk/gtk.h>
#include <eel/eel-glib-extensions.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-window-info.h>
#include <libnautilus-private/nautilus-search-directory.h>
#include "nautilus-application.h"
#include "nautilus-information-panel.h"
#include "nautilus-side-pane.h"

#define NAUTILUS_TYPE_WINDOW nautilus_window_get_type()
#define NAUTILUS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_WINDOW, NautilusWindow))
#define NAUTILUS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_WINDOW, NautilusWindowClass))
#define NAUTILUS_IS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_WINDOW))
#define NAUTILUS_IS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_WINDOW))
#define NAUTILUS_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_WINDOW, NautilusWindowClass))

#ifndef NAUTILUS_WINDOW_DEFINED
#define NAUTILUS_WINDOW_DEFINED
typedef struct NautilusWindow NautilusWindow;
#endif

#ifndef NAUTILUS_WINDOW_SLOT_DEFINED
#define NAUTILUS_WINDOW_SLOT_DEFINED
typedef struct NautilusWindowSlot NautilusWindowSlot;
#endif

typedef struct NautilusWindowSlotClass NautilusWindowSlotClass;
typedef enum NautilusWindowOpenSlotFlags NautilusWindowOpenSlotFlags;

GType          nautilus_window_slot_get_type (void);

typedef enum {
        NAUTILUS_WINDOW_NOT_SHOWN,
        NAUTILUS_WINDOW_POSITION_SET,
        NAUTILUS_WINDOW_SHOULD_SHOW
} NautilusWindowShowState;

enum NautilusWindowOpenSlotFlags {
	NAUTILUS_WINDOW_OPEN_SLOT_NONE = 0,
	NAUTILUS_WINDOW_OPEN_SLOT_APPEND = 1
};

typedef struct NautilusWindowDetails NautilusWindowDetails;

typedef struct {
        GtkWindowClass parent_spot;

        NautilusWindowType window_type;
        const char *bookmarks_placeholder;

	/* Function pointers for overriding, without corresponding signals */

        char * (* get_title) (NautilusWindow *window);
        void   (* sync_title) (NautilusWindow *window,
			       NautilusWindowSlot *slot);
        NautilusIconInfo * (* get_icon) (NautilusWindow *window,
					 NautilusWindowSlot *slot);

        void   (* load_view_as_menu) (NautilusWindow *window);

        void   (* sync_allow_stop) (NautilusWindow *window,
				    NautilusWindowSlot *slot);
	void   (* set_allow_up) (NautilusWindow *window, gboolean allow);
	void   (* reload)              (NautilusWindow *window);
        void   (* prompt_for_location) (NautilusWindow *window, const char *initial);
        void   (* sync_search_widgets) (NautilusWindow *window);
        void   (* sync_zoom_widgets) (NautilusWindow *window);
        void   (* get_default_size) (NautilusWindow *window, guint *default_width, guint *default_height);
        void   (* show_window)  (NautilusWindow *window);
        void   (* close) (NautilusWindow *window);

        NautilusWindowSlot * (* open_slot) (NautilusWindow *window,
					    NautilusWindowOpenSlotFlags flags);
        void                 (* close_slot) (NautilusWindow *window,
					     NautilusWindowSlot *slot);
        void                 (* set_active_slot) (NautilusWindow *window,
						  NautilusWindowSlot *slot);

        /* Signals used only for keybindings */
        gboolean (* go_up) (NautilusWindow *window, gboolean close);
} NautilusWindowClass;

struct NautilusWindow {
        GtkWindow parent_object;
        
        NautilusWindowDetails *details;
        
        /** CORBA-related elements **/
        NautilusApplication *application;
};

GType            nautilus_window_get_type             (void);
void             nautilus_window_show_window          (NautilusWindow    *window);
void             nautilus_window_close                (NautilusWindow    *window);

void             nautilus_window_connect_content_view (NautilusWindow    *window,
						       NautilusView      *view);
void             nautilus_window_disconnect_content_view (NautilusWindow    *window,
							  NautilusView      *view);

void             nautilus_window_go_to                (NautilusWindow    *window,
                                                       GFile             *location);
void             nautilus_window_go_to_with_selection (NautilusWindow    *window,
                                                       GFile             *location,
                                                       GList             *new_selection);
void             nautilus_window_go_home              (NautilusWindow    *window);
void             nautilus_window_go_up                (NautilusWindow    *window,
                                                       gboolean           close_behind,
						       gboolean           new_tab);
void             nautilus_window_prompt_for_location  (NautilusWindow    *window,
                                                       const char        *initial);
void		 nautilus_window_sync_search_widgets  (NautilusWindow    *window);
void             nautilus_window_launch_cd_burner     (NautilusWindow    *window);
void             nautilus_window_display_error        (NautilusWindow    *window,
                                                       const char        *error_msg);
void		 nautilus_window_reload		      (NautilusWindow	 *window);

void             nautilus_window_allow_reload         (NautilusWindow    *window,
                                                       gboolean           allow);
void             nautilus_window_allow_up             (NautilusWindow    *window, 
                                                       gboolean           allow);
void             nautilus_window_allow_stop           (NautilusWindow    *window, 
                                                       gboolean           allow);
void             nautilus_window_allow_burn_cd        (NautilusWindow    *window,
                                                       gboolean           allow);
GtkUIManager *   nautilus_window_get_ui_manager       (NautilusWindow    *window);
gboolean         nautilus_window_has_menubar_and_statusbar (NautilusWindow *window);

#endif
