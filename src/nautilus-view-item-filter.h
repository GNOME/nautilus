/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_ITEM_FILTER (nautilus_view_item_filter_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewItemFilter, nautilus_view_item_filter, NAUTILUS, VIEW_ITEM_FILTER, GtkFilter)

NautilusViewItemFilter *
nautilus_view_item_filter_new (void);

void
nautilus_view_item_filter_set_file_filter (NautilusViewItemFilter *self,
                                           GtkFileFilter          *file_filter);

G_END_DECLS
