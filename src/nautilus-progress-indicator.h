/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

#include "nautilus-window-slot.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROGRESS_INDICATOR (nautilus_progress_indicator_get_type())

G_DECLARE_FINAL_TYPE (NautilusProgressIndicator, nautilus_progress_indicator, NAUTILUS, PROGRESS_INDICATOR, AdwBin)

GtkWidget * nautilus_progress_indicator_get_operations_popover (NautilusProgressIndicator *self);

G_END_DECLS
