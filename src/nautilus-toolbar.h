
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

#include <adwaita.h>
#include <gtk/gtk.h>

#include "nautilus-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TOOLBAR nautilus_toolbar_get_type()

G_DECLARE_FINAL_TYPE (NautilusToolbar, nautilus_toolbar, NAUTILUS, TOOLBAR, AdwBin)

GtkWidget *nautilus_toolbar_new (void);

void       nautilus_toolbar_show_current_location_menu (NautilusToolbar *self);

void       nautilus_toolbar_set_active_slot    (NautilusToolbar    *toolbar,
                                                NautilusWindowSlot *slot);

void nautilus_toolbar_set_window_slot (NautilusToolbar    *self,
                                       NautilusWindowSlot *window_slot);
G_END_DECLS
