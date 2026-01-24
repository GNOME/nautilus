/*
 * Copyright © 2026 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_GRID_VIEW_CAPTIONS_DIALOG (nautilus_grid_view_captions_dialog_get_type())

G_DECLARE_FINAL_TYPE (NautilusGridViewCaptionsDialog, nautilus_grid_view_captions_dialog, NAUTILUS, GRID_VIEW_CAPTIONS_DIALOG, AdwDialog)

void
nautilus_grid_view_captions_dialog_present (GtkWidget *parent);

G_END_DECLS
