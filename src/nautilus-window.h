
/*
 * SPDX-FileCopyrightText: 1999, 2000 Red Hat, Inc.
 * SPDX-FileCopyrightText: 1999, 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Elliot Lee <sopwith@redhat.com>
 *          Darin Adler <darin@bentspoon.com>
 */
/* nautilus-window.h: Interface of the main window object */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

#include "nautilus-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_WINDOW (nautilus_window_get_type ())
G_DECLARE_FINAL_TYPE (NautilusWindow, nautilus_window, NAUTILUS, WINDOW, AdwApplicationWindow);

typedef gboolean (* NautilusWindowGoToCallback) (NautilusWindow *window,
                                                 GFile *location,
                                                 GError *error,
                                                 gpointer user_data);

#define NAUTILUS_NAVIGATION_DIRECTION_BACK -1
#define NAUTILUS_NAVIGATION_DIRECTION_FORWARD 1

NautilusWindow * nautilus_window_new                  (void);
void             nautilus_window_close                (NautilusWindow    *window);

void nautilus_window_open_location_full               (NautilusWindow     *window,
                                                       GFile              *location,
                                                       NautilusOpenFlags   flags,
                                                       NautilusFileList   *selection);

void             nautilus_window_new_tab              (NautilusWindow    *window);

gboolean
nautilus_window_has_open_location (NautilusWindow *self,
                                   GFile          *location);
GFile *
nautilus_window_get_active_location (NautilusWindow *self);
GList *
nautilus_window_get_locations (NautilusWindow *self);

void     nautilus_window_show_about_dialog    (NautilusWindow *window);

void nautilus_window_show_operation_notification (NautilusWindow *window,
                                                  gchar          *main_label,
                                                  GFile          *folder_to_open,
                                                  gboolean        was_quick);

void nautilus_window_search (NautilusWindow *window,
                             NautilusQuery  *query);

void nautilus_window_back_or_forward_in_new_tab (NautilusWindow *window,
                                                 int             distance);

G_END_DECLS
