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

#define HOVER_TIMEOUT 500

/* Drag & Drop target names. */
#define NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE	"x-special/gnome-icon-list"
#define NAUTILUS_ICON_DND_URI_LIST_TYPE		"text/uri-list"
#define NAUTILUS_ICON_DND_NETSCAPE_URL_TYPE	"_NETSCAPE_URL"
#define NAUTILUS_ICON_DND_ROOTWINDOW_DROP_TYPE	"application/x-rootwindow-drop"
#define NAUTILUS_ICON_DND_XDNDDIRECTSAVE_TYPE	"XdndDirectSave0" /* XDS Protocol Type */
#define NAUTILUS_ICON_DND_RAW_TYPE	"application/octet-stream"

GdkDragAction      nautilus_drag_drop_action_ask                 (GtkWidget        *widget,
                                                                  GdkDragAction     possible_actions);

GdkDragAction      nautilus_dnd_get_preferred_action              (NautilusFile     *target_file,
                                                                   GFile            *dropped);
GdkPaintable *     get_paintable_for_drag_selection              (GList            *selection,
                                                                  int               scale);
