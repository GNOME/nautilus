
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
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
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
#include "nautilus-bookmark.h"
#include "nautilus-search-directory.h"

G_BEGIN_DECLS

typedef enum {
        NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND = 1 << 0,
        NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW = 1 << 1,
        NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB = 1 << 2,
        NAUTILUS_WINDOW_OPEN_SLOT_APPEND = 1 << 3,
        NAUTILUS_WINDOW_OPEN_FLAG_DONT_MAKE_ACTIVE = 1 << 4
} NautilusWindowOpenFlags;

#define NAUTILUS_TYPE_WINDOW (nautilus_window_get_type ())
G_DECLARE_DERIVABLE_TYPE (NautilusWindow, nautilus_window, NAUTILUS, WINDOW, GtkApplicationWindow);

typedef gboolean (* NautilusWindowGoToCallback) (NautilusWindow *window,
                                                 GFile *location,
                                                 GError *error,
                                                 gpointer user_data);

#include "nautilus-files-view.h"
#include "nautilus-window-slot.h"

#define NAUTILUS_WINDOW_SIDEBAR_PLACES "places"
#define NAUTILUS_WINDOW_SIDEBAR_TREE "tree"

/* window geometry */
/* Min values are very small, and a Nautilus window at this tiny size is *almost*
 * completely unusable. However, if all the extra bits (sidebar, location bar, etc)
 * are turned off, you can see an icon or two at this size. See bug 5946.
 */

#define NAUTILUS_WINDOW_MIN_WIDTH		200
#define NAUTILUS_WINDOW_MIN_HEIGHT		200
#define NAUTILUS_WINDOW_DEFAULT_WIDTH		890
#define NAUTILUS_WINDOW_DEFAULT_HEIGHT		550


struct _NautilusWindowClass
{
        GtkApplicationWindowClass parent_spot;

	/* Function pointers for overriding, without corresponding signals */
        void   (* sync_title) (NautilusWindow *window,
			       NautilusWindowSlot *slot);
        void   (* close) (NautilusWindow *window);
        /* Use this in case your window has a special slot. Also is expected that
         * the slot is initialized with nautilus_window_initialize_slot.
         */
        NautilusWindowSlot * (* create_slot) (NautilusWindow *window,
                                              GFile          *location);
};

NautilusWindow * nautilus_window_new                  (GdkScreen         *screen);
void             nautilus_window_close                (NautilusWindow    *window);

void nautilus_window_open_location_full               (NautilusWindow          *window,
                                                       GFile                   *location,
                                                       NautilusWindowOpenFlags  flags,
                                                       GList                   *selection,
                                                       NautilusWindowSlot      *target_slot);

void             nautilus_window_new_tab              (NautilusWindow    *window);
NautilusWindowSlot * nautilus_window_get_active_slot       (NautilusWindow *window);
void                 nautilus_window_set_active_slot       (NautilusWindow    *window,
                                                            NautilusWindowSlot *slot);
GList *              nautilus_window_get_slots             (NautilusWindow *window);
void                 nautilus_window_slot_close            (NautilusWindow *window,
                                                            NautilusWindowSlot *slot);

void                 nautilus_window_sync_location_widgets (NautilusWindow *window);

void     nautilus_window_hide_sidebar         (NautilusWindow *window);
void     nautilus_window_show_sidebar         (NautilusWindow *window);
void     nautilus_window_back_or_forward      (NautilusWindow *window,
                                               gboolean        back,
                                               guint           distance,
                                               NautilusWindowOpenFlags flags);
void nautilus_window_hide_view_menu (NautilusWindow *window);
void nautilus_window_reset_menus (NautilusWindow *window);

GtkWidget *         nautilus_window_get_notebook (NautilusWindow *window);

NautilusWindowOpenFlags nautilus_event_get_window_open_flags   (void);
void     nautilus_window_show_about_dialog    (NautilusWindow *window);

GtkWidget *nautilus_window_get_toolbar (NautilusWindow *window);

/* sync window GUI with current slot. Used when changing slots,
 * and when updating the slot state.
 */
void nautilus_window_sync_allow_stop       (NautilusWindow *window,
					    NautilusWindowSlot *slot);
void nautilus_window_sync_title            (NautilusWindow *window,
					    NautilusWindowSlot *slot);

void nautilus_window_show_operation_notification (NautilusWindow *window,
                                                  gchar          *main_label,
                                                  GFile          *folder_to_open);
void nautilus_window_start_dnd (NautilusWindow *window,
                                GdkDragContext *context);
void nautilus_window_end_dnd (NautilusWindow *window,
                              GdkDragContext *context);

void nautilus_window_search (NautilusWindow *window,
                             const gchar    *text);

void nautilus_window_initialize_slot (NautilusWindow          *window,
                                      NautilusWindowSlot      *slot,
                                      NautilusWindowOpenFlags  flags);

G_END_DECLS

#endif
