/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "nautilus-types.h"
#include "nautilus-view-item.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_CELL (nautilus_view_cell_get_type())

G_DECLARE_DERIVABLE_TYPE (NautilusViewCell, nautilus_view_cell, NAUTILUS, VIEW_CELL, AdwBin)

struct _NautilusViewCellClass
{
    AdwBinClass parent_class;
};

NautilusListBase *nautilus_view_cell_get_view (NautilusViewCell *self);
void nautilus_view_cell_set_item (NautilusViewCell      *self,
                                  NautilusViewItem *item);
NautilusViewItem *nautilus_view_cell_get_item (NautilusViewCell *self);
gboolean nautilus_view_cell_once (NautilusViewCell *self);

G_END_DECLS
