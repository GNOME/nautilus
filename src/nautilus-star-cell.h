/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-view-cell.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_STAR_CELL (nautilus_star_cell_get_type())

G_DECLARE_FINAL_TYPE (NautilusStarCell, nautilus_star_cell, NAUTILUS, STAR_CELL, NautilusViewCell)

NautilusViewCell * nautilus_star_cell_new (void);
void nautilus_star_cell_recycle (NautilusStarCell **self);
void nautilus_star_cell_clear_cache (void);

G_END_DECLS
