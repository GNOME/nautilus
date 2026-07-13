/*
 * SPDX-FileCopyrightText: 2026 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>

#include "nautilus-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SCRIPTS_MONITOR (nautilus_scripts_monitor_get_type ())

G_DECLARE_FINAL_TYPE (NautilusScriptsMonitor, nautilus_scripts_monitor, NAUTILUS, SCRIPTS_MONITOR, GObject)

NautilusScriptsMonitor *nautilus_scripts_monitor_get (void);

typedef void (*AddScriptClosure) (NautilusFile *, GMenu *, GHashTable *, gpointer);
GMenu *nautilus_scripts_monitor_get_menu (NautilusScriptsMonitor *self,
                                          AddScriptClosure        closure,
                                          gpointer                data);

G_END_DECLS
