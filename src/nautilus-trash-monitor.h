
/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Pavel Cisler <pavel@eazel.com>
 */

#pragma once

#include <gio/gio.h>

#define NAUTILUS_TYPE_TRASH_MONITOR (nautilus_trash_monitor_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTrashMonitor, nautilus_trash_monitor,
                      NAUTILUS, TRASH_MONITOR,
                      GObject)

NautilusTrashMonitor   *nautilus_trash_monitor_get      (void);
gboolean                nautilus_trash_monitor_is_empty (void);
GIcon                  *nautilus_trash_monitor_get_symbolic_icon (void);
void                    nautilus_trash_monitor_clear (void);
