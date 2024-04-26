/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SHORTCUT_MANAGER (nautilus_shortcut_manager_get_type())

G_DECLARE_FINAL_TYPE (NautilusShortcutManager, nautilus_shortcut_manager, NAUTILUS, SHORTCUT_MANAGER, AdwBin)

NautilusShortcutManager * nautilus_shortcut_manager_new (void);

G_END_DECLS
