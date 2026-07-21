/*
 * Copyright (C) 2026 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-view-cell.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_NARROW_CELL (nautilus_narrow_cell_get_type())

G_DECLARE_FINAL_TYPE (NautilusNarrowCell, nautilus_narrow_cell, NAUTILUS, NARROW_CELL, NautilusViewCell)

NautilusNarrowCell * nautilus_narrow_cell_new (NautilusListBase *view);

G_END_DECLS
