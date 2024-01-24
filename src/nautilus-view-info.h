/*
 * Copyright (C) 2024 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

/* Keep values in sync with the org.gnome.nautilus.FolderView schema enums: */
#define NAUTILUS_VIEW_LIST_ID            1
#define NAUTILUS_VIEW_GRID_ID            2
/* Special ids, not used by GSettings schemas: */
#define NAUTILUS_VIEW_NETWORK_ID         3
#define NAUTILUS_VIEW_INVALID_ID         0

typedef struct
{
    unsigned int view_id;
    int zoom_level_min;
    int zoom_level_max;
    int zoom_level_standard;
} NautilusViewInfo;

