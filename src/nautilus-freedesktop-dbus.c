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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Authors: Akshay Gupta <kitallis@gmail.com>
 *          Federico Mena Quintero <federico@gnome.org>
 */

#include <config.h>

#include "nautilus-freedesktop-dbus.h"
#include "nautilus-freedesktop-generated.h"

/* We share the same debug domain as nautilus-dbus-manager */
#define DEBUG_FLAG NAUTILUS_DEBUG_DBUS
#include "nautilus-debug.h"

#include "gio/gio.h"


/* Id from g_dbus_own_name() */
static guint owner_id;

/* DBus paraphernalia */
static GDBusConnection *connection;
static GDBusObjectManagerServer *object_manager;

/* Our DBus implementation skeleton */
static NautilusFreedesktopFileManager1 *skeleton;

static void
bus_acquired_cb (GDBusConnection *conn,
		 const gchar     *name,
		 gpointer         user_data)
{
	DEBUG ("Bus acquired at %s", name);

	connection = g_object_ref (conn);
	object_manager = g_dbus_object_manager_server_new ("/org/freedesktop/FileManager1");

	skeleton = nautilus_freedesktop_file_manager1_skeleton_new ();

	g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (skeleton), connection, "/org/freedesktop/FileManager1", NULL);

	g_dbus_object_manager_server_set_connection (object_manager, connection);
}

static void
name_acquired_cb (GDBusConnection *connection,
		  const gchar     *name,
		  gpointer         user_data)
{
	DEBUG ("Acquired the name %s on the session message bus\n", name);
}

static void
name_lost_cb (GDBusConnection *connection,
	      const gchar     *name,
	      gpointer         user_data)
{
	DEBUG ("Lost (or failed to acquire) the name %s on the session message bus\n", name);
}

/* Tries to own the org.freedesktop.FileManager1 service name */
void
nautilus_freedesktop_dbus_start (void)
{
	if (owner_id != 0)
		return;

	owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
				   "org.freedesktop.FileManager1",
				   G_BUS_NAME_OWNER_FLAGS_NONE,
				   bus_acquired_cb,
				   name_acquired_cb,
				   name_lost_cb,
				   NULL,
				   NULL);
}

/* Releases the org.freedesktop.FileManager1 service name */
void
nautilus_freedesktop_dbus_stop (void)
{
	if (owner_id != 0) {
		g_bus_unown_name (owner_id);
		owner_id = 0;
	}

	if (skeleton != NULL) {
		g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (skeleton));
		g_object_unref (skeleton);
		skeleton = NULL;
	}

	if (object_manager != NULL) {
		g_object_unref (object_manager);
		object_manager = NULL;
	}

	g_clear_object (&connection);
}
