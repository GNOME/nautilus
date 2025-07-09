/* nautilus-batch-rename-item.h
 *
 * Copyright (C) 2024–2025 Markus Göllnitz <camelcasenick@bewares.it>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
