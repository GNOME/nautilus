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

#include "nautilus-properties-window.h"

#include <gio/gio.h>


/* Parent application */
static NautilusApplication *application;

/* Id from g_dbus_own_name() */
static guint owner_id;

/* DBus paraphernalia */
static GDBusConnection *connection;
static GDBusObjectManagerServer *object_manager;

/* Our DBus implementation skeleton */
static NautilusFreedesktopFileManager1 *skeleton;


static gboolean
skeleton_handle_show_items_cb (NautilusFreedesktopFileManager1 *object,
			       GDBusMethodInvocation *invocation,
			       const gchar *const *uris,
			       const gchar *startup_id)
{
	int i;

	for (i = 0; uris[i] != NULL; i++) {
		GFile *file;
		GFile *files[1];

		file = g_file_new_for_uri (uris[i]);
		files[0] = file;

		/* FIXME: we are not using the startup_id.  This is not
		 * what g_application_open() expects, and neither does
		 * NautilusApplication internally.
		 */
		g_application_open (G_APPLICATION (application), files, 1, "");
		g_object_unref (file);
	}

	nautilus_freedesktop_file_manager1_complete_show_items (object, invocation);
	return TRUE;
}

static gboolean
skeleton_handle_show_folders_cb (NautilusFreedesktopFileManager1 *object,
				 GDBusMethodInvocation *invocation,
				 const gchar *const *uris,
				 const gchar *startup_id)
{
	/* FIXME: NautilusApplication makes no distinction between showing
	 * files vs. folders.  For now we will just use the same
	 * implementation.
	 */

	int i;

	for (i = 0; uris[i] != NULL; i++) {
		GFile *file;
		GFile *files[1];

		file = g_file_new_for_uri (uris[i]);
		files[0] = file;

		/* FIXME: we are not using the startup_id.  This is not
		 * what g_application_open() expects, and neither does
		 * NautilusApplication internally.
		 */
		g_application_open (G_APPLICATION (application), files, 1, "");
		g_object_unref (file);
	}

	nautilus_freedesktop_file_manager1_complete_show_folders (object, invocation);
	return TRUE;
}

static gboolean
skeleton_handle_show_item_properties_cb (NautilusFreedesktopFileManager1 *object,
					 GDBusMethodInvocation *invocation,
					 const gchar *const *uris,
					 const gchar *startup_id)
{
	GList *files;
	int i;

	files = NULL;

	for (i = 0; uris[i] != NULL; i++)
		files = g_list_prepend (files, nautilus_file_get_by_uri (uris[i]));

	files = g_list_reverse (files);

	/* FIXME: we are not using the startup_id */
	nautilus_properties_window_present (files, NULL);

	nautilus_file_list_free (files);

	nautilus_freedesktop_file_manager1_complete_show_item_properties (object, invocation);
	return TRUE;
}

static void
bus_acquired_cb (GDBusConnection *conn,
		 const gchar     *name,
		 gpointer         user_data)
{
	DEBUG ("Bus acquired at %s", name);

	connection = g_object_ref (conn);
	object_manager = g_dbus_object_manager_server_new ("/org/freedesktop/FileManager1");

	skeleton = nautilus_freedesktop_file_manager1_skeleton_new ();

	g_signal_connect (skeleton, "handle-show-items",
			  G_CALLBACK (skeleton_handle_show_items_cb), NULL);
	g_signal_connect (skeleton, "handle-show-folders",
			  G_CALLBACK (skeleton_handle_show_folders_cb), NULL);
	g_signal_connect (skeleton, "handle-show-item-properties",
			  G_CALLBACK (skeleton_handle_show_item_properties_cb), NULL);

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
nautilus_freedesktop_dbus_start (NautilusApplication *app)
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

	application = app;
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
	application = NULL;
}
