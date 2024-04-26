/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "nautilus-shortcut-manager.h"


/**
 * NautilusShortcutManager:
 *
 * A bin container to limit the scope of GTK_SHORTCUT_SCOPE_MANAGED shortcuts.
 *
 * The primary use case is to prevent keyboard shortcuts from being triggered
 * while `AdwDialog`s are presented. This assumes an implementation detail: that
 * `AdwDialog`s are internally children of `AdwWindow`/`AdwApplicationWindow`,
 * but not children of `AdwWindow:child`/`AdwApplicationWindow:child`.
 *
 * This is simply an AdwBin augmented with the GtkShortcutManager interface. The
 * default implementation of the interface is sufficient for the purpose.
 */
struct _NautilusShortcutManager
{
    AdwBin parent_instance;
};

static void
nautilus_shortcut_manager_interface_init (GtkShortcutManagerInterface *iface)
{
}

G_DEFINE_TYPE_WITH_CODE (NautilusShortcutManager, nautilus_shortcut_manager, ADW_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SHORTCUT_MANAGER,
                                                nautilus_shortcut_manager_interface_init))

static void
nautilus_shortcut_manager_class_init (NautilusShortcutManagerClass *klass)
{
}

static void
nautilus_shortcut_manager_init (NautilusShortcutManager *self)
{
}

NautilusShortcutManager *
nautilus_shortcut_manager_new (void)
{
    return g_object_new (NAUTILUS_TYPE_SHORTCUT_MANAGER, NULL);
}
