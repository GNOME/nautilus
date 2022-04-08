/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-list-base.h"
#include "nautilus-window-slot.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_GRID_VIEW (nautilus_grid_view_get_type())

G_DECLARE_FINAL_TYPE (NautilusGridView, nautilus_grid_view, NAUTILUS, GRID_VIEW, NautilusListBase)

NautilusGridView *nautilus_grid_view_new (NautilusWindowSlot *slot);

G_END_DECLS
