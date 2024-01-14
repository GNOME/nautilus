/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-file.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_ITEM (nautilus_view_item_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewItem, nautilus_view_item, NAUTILUS, VIEW_ITEM, GObject)

NautilusViewItem * nautilus_view_item_new           (NautilusFile *file);

void               nautilus_view_item_set_cut       (NautilusViewItem *self,
                                                     gboolean          is_cut);
void               nautilus_view_item_set_drag_accept (NautilusViewItem *self,
                                                       gboolean          drag_accept);
gboolean           nautilus_view_item_get_loading   (NautilusViewItem *self);
void               nautilus_view_item_set_loading   (NautilusViewItem *self,
                                                     gboolean          is_loading);

NautilusFile *     nautilus_view_item_get_file      (NautilusViewItem *self);

void               nautilus_view_item_set_item_ui   (NautilusViewItem *self,
                                                     GtkWidget        *item_ui);

GtkWidget *        nautilus_view_item_get_item_ui   (NautilusViewItem *self);
void               nautilus_view_item_file_changed  (NautilusViewItem *self);

G_END_DECLS
