/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>

#include "nautilus-window-slot.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_CONTROLS (nautilus_view_controls_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewControls, nautilus_view_controls, NAUTILUS, VIEW_CONTROLS, AdwBin)

G_END_DECLS
