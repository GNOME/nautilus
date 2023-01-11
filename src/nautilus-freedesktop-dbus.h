/*
 * nautilus-freedesktop-dbus: Implementation for the org.freedesktop DBus file-management interfaces
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

NautilusFreedesktopDBus * nautilus_freedesktop_dbus_new (GDBusConnection *connection);

gboolean nautilus_freedesktop_dbus_register (NautilusFreedesktopDBus *fdb, GError **error);
void nautilus_freedesktop_dbus_unregister (NautilusFreedesktopDBus *fdb);

void nautilus_freedesktop_dbus_set_open_locations (NautilusFreedesktopDBus *fdb, const gchar **locations);

void nautilus_freedesktop_dbus_set_open_windows_with_locations (NautilusFreedesktopDBus *fdb, GVariant *locations);
