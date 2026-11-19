/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-types.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_CELL (nautilus_view_cell_get_type())

G_DECLARE_DERIVABLE_TYPE (NautilusViewCell, nautilus_view_cell, NAUTILUS, VIEW_CELL, GtkWidget)

struct _NautilusViewCellClass
{
    GtkWidgetClass parent_class;
};

NautilusListBase *nautilus_view_cell_get_view (NautilusViewCell *self);
NautilusViewItem *nautilus_view_cell_get_item (NautilusViewCell *self);
void nautilus_view_cell_set_item (NautilusViewCell *self,
                                  NautilusViewItem *item);
void nautilus_view_cell_set_list_item (NautilusViewCell *self,
                                       GtkListItem      *list_item);
guint nautilus_view_cell_get_position (NautilusViewCell *self);

void nautilus_view_cell_connect_item_signals (NautilusViewCell *self,
                                             GCallback         file_changed_callback,
                                             ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS
