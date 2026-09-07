/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 * SPDX-FileCopyrightText: 2001, 2002 Anders Carlsson <andersca@gnu.org>
 * SPDX-FileCopyrightText: 2022 GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-list-base.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_LIST_VIEW (nautilus_list_view_get_type())

G_DECLARE_FINAL_TYPE (NautilusListView, nautilus_list_view, NAUTILUS, LIST_VIEW, NautilusListBase)

NautilusListView *nautilus_list_view_new (void);

void nautilus_list_view_present_column_editor (NautilusListView *self);

G_END_DECLS
