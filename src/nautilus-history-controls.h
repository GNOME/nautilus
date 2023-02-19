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

#define NAUTILUS_TYPE_HISTORY_CONTROLS nautilus_history_controls_get_type()

G_DECLARE_FINAL_TYPE (NautilusHistoryControls, nautilus_history_controls, NAUTILUS, HISTORY_CONTROLS, AdwBin)

G_END_DECLS
