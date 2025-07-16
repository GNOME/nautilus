/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
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
void nautilus_view_cell_set_item (NautilusViewCell      *self,
                                  NautilusViewItem *item);
NautilusViewItem *nautilus_view_cell_get_item (NautilusViewCell *self);
guint nautilus_view_cell_get_position (NautilusViewCell *self);
gboolean nautilus_view_cell_once (NautilusViewCell *self);
gboolean nautilus_view_cell_setup (NautilusViewCell *self,
                                   GCallback         on_item_click_pressed,
                                   GCallback         on_item_click_stopped,
                                   GCallback         on_item_click_released,
                                   GCallback         on_item_longpress_pressed,
                                   GCallback         on_item_drag_prepare,
                                   GCallback         on_item_drag_enter,
                                   GCallback         on_item_drag_value_notify,
                                   GCallback         on_item_drag_leave,
                                   GCallback         on_item_drag_motion,
                                   GCallback         on_item_drop,
                                   GCallback         on_item_drag_hover_enter,
                                   GCallback         on_item_drag_hover_leave,
                                   GCallback         on_item_drag_hover_motion);

G_END_DECLS
