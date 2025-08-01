/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

/* Needed for NautilusColumn (typedef only). */
#include "nautilus-types.h"

#include "nautilus-view-cell.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_LABEL_CELL (nautilus_label_cell_get_type())

G_DECLARE_FINAL_TYPE (NautilusLabelCell, nautilus_label_cell, NAUTILUS, LABEL_CELL, NautilusViewCell)

NautilusViewCell * nautilus_label_cell_new (NautilusColumn   *column);
void nautilus_label_cell_recycle (NautilusLabelCell **self);
void nautilus_label_cell_clear_cache (void);

G_END_DECLS
