/*
 * nautilus-freedesktop-dbus: Implementation for the org.freedesktop DBus file-management interfaces
 *
 * SPDX-FileCopyrightText: 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Akshay Gupta <kitallis@gmail.com>
 *          Federico Mena Quintero <federico@gnome.org>
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

#define NAUTILUS_FDO_DBUS_IFACE "org.freedesktop.FileManager1"
#define NAUTILUS_FDO_DBUS_NAME  "org.freedesktop.FileManager1"
#define NAUTILUS_FDO_DBUS_PATH  "/org/freedesktop/FileManager1"

#define NAUTILUS_TYPE_FREEDESKTOP_DBUS nautilus_freedesktop_dbus_get_type()

G_DECLARE_FINAL_TYPE (NautilusFreedesktopDBus, nautilus_freedesktop_dbus, NAUTILUS, FREEDESKTOP_DBUS, GObject);

NautilusFreedesktopDBus * nautilus_freedesktop_dbus_new (void);

gboolean nautilus_freedesktop_dbus_register (NautilusFreedesktopDBus *fdb, GDBusConnection *connection, GError **error);
void nautilus_freedesktop_dbus_unregister (NautilusFreedesktopDBus *fdb);

void nautilus_freedesktop_dbus_set_open_locations (NautilusFreedesktopDBus *fdb, const gchar **locations);

void nautilus_freedesktop_dbus_set_open_windows_with_locations (NautilusFreedesktopDBus *fdb, GVariant *locations);
