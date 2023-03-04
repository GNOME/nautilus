/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-list-base.h"
#include "nautilus-view-model.h"
#include "nautilus-view-cell.h"

/*
 * Private header to be included only by subclasses.
 */

G_BEGIN_DECLS

/* Methods */
NautilusViewModel *nautilus_list_base_get_model     (NautilusListBase *self);
void               nautilus_list_base_set_icon_size (NautilusListBase *self,
                                                            gint                    icon_size);
void               nautilus_list_base_setup_gestures (NautilusListBase *self);
void               nautilus_list_base_reset_sort     (NautilusListBase *self);

/* Shareable helpers */
void                          set_directory_sort_metadata       (NautilusFile *file,
                                                                 const gchar  *metadata_name,
                                                                 gboolean      reversed);
const NautilusFileSortType    get_sorts_type_from_metadata_text (const char   *metadata_name);
void                          setup_cell_common                 (GtkListItem      *listitem,
                                                                 NautilusViewCell *cell);

G_END_DECLS
