
/*
 * SPDX-FileCopyrightText: 2011, Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
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

void nautilus_toolbar_set_window_slot (NautilusToolbar    *self,
                                       NautilusWindowSlot *window_slot);
G_END_DECLS
