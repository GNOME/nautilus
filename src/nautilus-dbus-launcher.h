/*
 * Copyright (C) 2022 Corey Berla <corey@berla.me>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-file.h"

#define NAUTILUS_TYPE_DBUS_LAUNCHER (nautilus_dbus_launcher_get_type())

G_DECLARE_FINAL_TYPE (NautilusDBusLauncher, nautilus_dbus_launcher, NAUTILUS, DBUS_LAUNCHER, GObject)


typedef enum {
    NAUTILUS_DBUS_APP_0,
    NAUTILUS_DBUS_LAUNCHER_SETTINGS,
    NAUTILUS_DBUS_LAUNCHER_DISKS,
    NAUTILUS_DBUS_LAUNCHER_CONSOLE,
    NAUTILUS_DBUS_LAUNCHER_N_APPS
} NautilusDBusLauncherApp;

NautilusDBusLauncher * nautilus_dbus_launcher_new (void); //to be called on `NautilusApplication::startup` only

NautilusDBusLauncher * nautilus_dbus_launcher_get (void); //to be called by consumers; doesn't change reference count.

gboolean nautilus_dbus_launcher_is_available (NautilusDBusLauncher    *self,
                                              NautilusDBusLauncherApp  app);

void nautilus_dbus_launcher_call (NautilusDBusLauncher    *self,
                                  NautilusDBusLauncherApp  app,
                                  const gchar             *method_name,
                                  GVariant                *parameters,
                                  GtkWindow               *window);

