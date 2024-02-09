/*
 * Copyright (C) 2024 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-view-cell.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_MINIMAL_CELL (nautilus_minimal_cell_get_type())

G_DECLARE_FINAL_TYPE (NautilusMinimalCell, nautilus_minimal_cell, NAUTILUS, MINIMAL_CELL, GObject)

NautilusMinimalCell * nautilus_minimal_cell_new (gchar        *title,
                                                 gchar        *subtitle,
                                                 GdkPaintable *paintable);

const gchar * nautilus_minimal_cell_get_subtitle (NautilusMinimalCell *self);
const gchar * nautilus_minimal_cell_get_title (NautilusMinimalCell *self);

G_END_DECLS
