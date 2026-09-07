/*
 * SPDX-FileCopyrightText: 2024–2025 Markus Göllnitz <camelcasenick@bewares.it>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "nautilus-types.h"

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_BATCH_RENAME_ITEM (nautilus_batch_rename_item_get_type())

G_DECLARE_FINAL_TYPE (NautilusBatchRenameItem, nautilus_batch_rename_item, NAUTILUS, BATCH_RENAME_ITEM, GObject);

NautilusBatchRenameItem* nautilus_batch_rename_item_new (const gchar *name_before,
                                                         const gchar *name_after,
                                                         NautilusBatchRenameDialog *dialog);

void nautilus_batch_rename_item_set_name_before (NautilusBatchRenameItem *item,
                                                 const gchar *name_before);

void nautilus_batch_rename_item_set_name_after (NautilusBatchRenameItem *item,
                                                const gchar *name_after);

void nautilus_batch_rename_item_set_has_conflict (NautilusBatchRenameItem *item,
                                                  gboolean has_conflict);

G_END_DECLS
