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
NautilusFile      *nautilus_list_base_get_directory_as_file (NautilusListBase *self);
NautilusViewModel *nautilus_list_base_get_model     (NautilusListBase *self);
void               nautilus_list_base_setup_gestures (NautilusListBase *self);

/* Shareable helpers */
void                          setup_cell_common                 (GObject          *listitem,
                                                                 NautilusViewCell *cell);
void                          setup_cell_hover                  (NautilusViewCell *cell);
void                          setup_cell_hover_inner_target     (NautilusViewCell *cell,
                                                                 GtkWidget        *target);

G_END_DECLS
