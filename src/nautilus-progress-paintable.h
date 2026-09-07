/*
 * SPDX-FileCopyrightText: 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROGRESS_PAINTABLE (nautilus_progress_paintable_get_type())

G_DECLARE_FINAL_TYPE (NautilusProgressPaintable, nautilus_progress_paintable, NAUTILUS, PROGRESS_PAINTABLE, GObject)

GdkPaintable *nautilus_progress_paintable_new          (GtkWidget              *widget);

void          nautilus_progress_paintable_animate_done (NautilusProgressPaintable *self);

G_END_DECLS
