/* nautilus-dnd.h - Common Drag & drop handling code
 *
 * Authors: Pavel Cisler <pavel@eazel.com>,
 *          Ettore Perazzoli <ettore@gnu.org>
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include "nautilus-file.h"
#include "nautilus-files-view.h"

#define HOVER_TIMEOUT 500

#define NAUTILUS_DRAG_SURFACE_ICON_SIZE 64

GdkDragAction      nautilus_drag_drop_action_ask                 (GtkWidget        *widget,
                                                                  GdkDragAction     possible_actions);

GdkDragAction      nautilus_dnd_get_preferred_action             (NautilusFile     *target_file,
                                                                  GFile            *dropped);
GdkPaintable *     get_paintable_for_drag_selection              (GList            *selection,
                                                                  int               scale);

void               nautilus_dnd_perform_drop                     (NautilusFilesView *view,
                                                                  const GValue      *value,
                                                                  GdkDragAction      action,
                                                                  GFile             *target_location);
