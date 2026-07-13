/*
 * Copyright (C) 2026 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-files-view.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SCRIPTS (nautilus_scripts_get_type ())

G_DECLARE_FINAL_TYPE (NautilusScripts, nautilus_scripts, NAUTILUS, SCRIPTS, GObject)

NautilusScripts *nautilus_scripts_get (void);

typedef void (*AddScriptClosure) (NautilusFilesView *, NautilusFile *, GMenu *, GHashTable *);
GMenu *nautilus_scripts_get_menu_for_view (NautilusScripts   *self,
                                           NautilusFilesView *view,
                                           AddScriptClosure   closure);

G_END_DECLS
