/*
 * Copyright (C) 2024 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-list-base.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_NARROW_VIEW (nautilus_narrow_view_get_type())

G_DECLARE_FINAL_TYPE (NautilusNarrowView, nautilus_narrow_view, NAUTILUS, NARROW_VIEW, NautilusListBase)

NautilusNarrowView *nautilus_narrow_view_new (void);

G_END_DECLS
