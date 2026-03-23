
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

typedef enum
{
    NAUTILUS_NAVIGATION_DIRECTION_NONE,
    NAUTILUS_NAVIGATION_DIRECTION_BACK,
    NAUTILUS_NAVIGATION_DIRECTION_FORWARD
} NautilusNavigationDirection;

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

void nautilus_window_back_or_forward_in_new_tab (NautilusWindow              *window,
                                                 NautilusNavigationDirection  back);

G_END_DECLS
