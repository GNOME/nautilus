
/*
 * Nautilus
 *
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#pragma once

#include <gtk/gtk.h>
#include <libhandy-1/handy.h>

#include "nautilus-ui-utilities.h"
#include "nautilus-window-slot.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TOOLBAR nautilus_toolbar_get_type()

G_DECLARE_FINAL_TYPE (NautilusToolbar, nautilus_toolbar, NAUTILUS, TOOLBAR, HdyHeaderBar)

GtkWidget *nautilus_toolbar_new (void);

GtkWidget *nautilus_toolbar_get_path_bar (NautilusToolbar *self);
GtkWidget *nautilus_toolbar_get_location_entry (NautilusToolbar *self);

void       nautilus_toolbar_set_show_location_entry (NautilusToolbar *self,
                                                     gboolean show_location_entry);

void       nautilus_toolbar_set_active_slot    (NautilusToolbar    *toolbar,
                                                NautilusWindowSlot *slot);

gboolean   nautilus_toolbar_is_menu_visible    (NautilusToolbar *toolbar);

gboolean   nautilus_toolbar_is_operations_button_active (NautilusToolbar *toolbar);

void       nautilus_toolbar_on_window_constructed       (NautilusToolbar *toolbar);

void       nautilus_toolbar_set_adaptive_mode (NautilusToolbar *self,
                                               NautilusAdaptiveMode adaptive_mode);

void nautilus_toolbar_set_window_slot (NautilusToolbar    *self,
                                       NautilusWindowSlot *window_slot);
G_END_DECLS
