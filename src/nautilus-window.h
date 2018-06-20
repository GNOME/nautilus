
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

#pragma once

#include <gtk/gtk.h>
#include <libhandy-1/handy.h>

#include "nautilus-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_WINDOW (nautilus_window_get_type ())
G_DECLARE_FINAL_TYPE (NautilusWindow, nautilus_window, NAUTILUS, WINDOW, HdyApplicationWindow);

typedef gboolean (* NautilusWindowGoToCallback) (NautilusWindow *window,
                                                 GFile *location,
                                                 GError *error,
                                                 gpointer user_data);

typedef void (* NautilusWindowHandleExported) (NautilusWindow *window,
                                               const char *handle,
                                               guint xid,
                                               gpointer user_data);

/* window geometry */
/* Min values are very small, and a Nautilus window at this tiny size is *almost*
 * completely unusable. However, if all the extra bits (sidebar, location bar, etc)
 * are turned off, you can see an icon or two at this size. See bug 5946.
 */

#define NAUTILUS_WINDOW_MIN_WIDTH		200
#define NAUTILUS_WINDOW_MIN_HEIGHT		200
#define NAUTILUS_WINDOW_DEFAULT_WIDTH		890
#define NAUTILUS_WINDOW_DEFAULT_HEIGHT		550

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
                                               guint           distance);
void nautilus_window_reset_menus (NautilusWindow *window);

GtkWidget *         nautilus_window_get_notebook (NautilusWindow *window);

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
                             NautilusQuery  *query);

void nautilus_window_initialize_slot (NautilusWindow          *window,
                                      NautilusWindowSlot      *slot,
                                      NautilusWindowOpenFlags  flags);

gboolean nautilus_window_export_handle (NautilusWindow *window,
                                        NautilusWindowHandleExported callback,
                                        gpointer user_data);
void nautilus_window_unexport_handle (NautilusWindow *window);

G_END_DECLS
