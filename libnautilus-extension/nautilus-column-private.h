/*
 * SPDX-FileCopyrightText: 2026 GNOME Files Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Peter Eisenmann <p3732@getgoogleoff.me>
 */

#pragma once

#include "nautilus-column.h"

G_BEGIN_DECLS

gboolean
nautilus_column_get_visible (NautilusColumn *self);
const char *
nautilus_column_peek_name (NautilusColumn *self);
const char *
nautilus_column_peek_label (NautilusColumn *self);

G_END_DECLS
