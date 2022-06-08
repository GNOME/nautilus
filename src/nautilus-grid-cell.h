/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-view-cell.h"

G_BEGIN_DECLS

enum
{
    NAUTILUS_GRID_CELL_FIRST_CAPTION,
    NAUTILUS_GRID_CELL_SECOND_CAPTION,
    NAUTILUS_GRID_CELL_THIRD_CAPTION,
    NAUTILUS_GRID_CELL_N_CAPTIONS
};

#define NAUTILUS_TYPE_GRID_CELL (nautilus_grid_cell_get_type())

G_DECLARE_FINAL_TYPE (NautilusGridCell, nautilus_grid_cell, NAUTILUS, GRID_CELL, NautilusViewCell)

NautilusGridCell * nautilus_grid_cell_new (NautilusListBase *view);
void nautilus_grid_cell_set_caption_attributes (NautilusGridCell *self,
                                                GQuark           *attrs);

G_END_DECLS
