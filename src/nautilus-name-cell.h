/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-view-cell.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_NAME_CELL (nautilus_name_cell_get_type())

G_DECLARE_FINAL_TYPE (NautilusNameCell, nautilus_name_cell, NAUTILUS, NAME_CELL, NautilusViewCell)

NautilusViewCell * nautilus_name_cell_new (NautilusListBase *view);
void nautilus_name_cell_show_snippet (NautilusNameCell *self);
GtkTreeExpander * nautilus_name_cell_get_expander (NautilusNameCell *self);
GtkWidget * nautilus_name_cell_get_content (NautilusNameCell *self);

G_END_DECLS
