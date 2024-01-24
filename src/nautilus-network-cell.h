/*
 * Copyright (C) 2024 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-view-cell.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_NETWORK_CELL (nautilus_network_cell_get_type())

G_DECLARE_FINAL_TYPE (NautilusNetworkCell, nautilus_network_cell, NAUTILUS, NETWORK_CELL, NautilusViewCell)

NautilusViewCell * nautilus_network_cell_new (NautilusListBase *view);

G_END_DECLS
