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

#include "nautilus-freedesktop-dbus.h"

/* We share the same debug domain as nautilus-dbus-manager */
#define DEBUG_FLAG NAUTILUS_DEBUG_DBUS
#include "nautilus-debug.h"

#include "nautilus-application.h"
#include "nautilus-file.h"
#include "nautilus-freedesktop-generated.h"
#include "nautilus-properties-window.h"

#include <gio/gio.h>

struct _NautilusFreedesktopDBus
{
    GObject parent;

    /* Id from g_dbus_own_name() */
    guint owner_id;

    /* Our DBus implementation skeleton */
    NautilusFreedesktopFileManager1 *skeleton;

    GStrv pending_open_locations;
    GVariant *pending_open_windows_with_locations;

    gboolean name_lost;
};

G_DEFINE_TYPE (NautilusFreedesktopDBus, nautilus_freedesktop_dbus, G_TYPE_OBJECT);

static gboolean
skeleton_handle_show_items_cb (NautilusFreedesktopFileManager1 *object,
                               GDBusMethodInvocation           *invocation,
                               const gchar * const             *uris,
                               const gchar                     *startup_id,
                               gpointer                         data)
{
    NautilusApplication *application;
    int i;

    application = NAUTILUS_APPLICATION (g_application_get_default ());

    for (i = 0; uris[i] != NULL; i++)
    {
        GFile *file;
        GFile *parent;

        file = g_file_new_for_uri (uris[i]);
        parent = g_file_get_parent (file);

        if (parent != NULL)
        {
            nautilus_application_open_location (application, parent, file, startup_id);
            g_object_unref (parent);
        }
        else
        {
            nautilus_application_open_location (application, file, NULL, startup_id);
        }

        g_object_unref (file);
    }

    nautilus_freedesktop_file_manager1_complete_show_items (object, invocation);
    return TRUE;
}

static gboolean
skeleton_handle_show_folders_cb (NautilusFreedesktopFileManager1 *object,
                                 GDBusMethodInvocation           *invocation,
                                 const gchar * const             *uris,
                                 const gchar                     *startup_id,
                                 gpointer                         data)
{
    NautilusApplication *application;
    int i;

    application = NAUTILUS_APPLICATION (g_application_get_default ());

    for (i = 0; uris[i] != NULL; i++)
    {
        GFile *file;

        file = g_file_new_for_uri (uris[i]);

        nautilus_application_open_location (application, file, NULL, startup_id);

        g_object_unref (file);
    }

    nautilus_freedesktop_file_manager1_complete_show_folders (object, invocation);
    return TRUE;
}

static void
properties_window_on_finished (gpointer user_data)
{
    g_application_release (g_application_get_default ());
}

static gboolean
skeleton_handle_show_item_properties_cb (NautilusFreedesktopFileManager1 *object,
                                         GDBusMethodInvocation           *invocation,
                                         const gchar * const             *uris,
                                         const gchar                     *startup_id,
                                         gpointer                         data)
{
    GList *files;
    int i;

    files = NULL;

    for (i = 0; uris[i] != NULL; i++)
    {
        files = g_list_prepend (files, nautilus_file_get_by_uri (uris[i]));
    }

    files = g_list_reverse (files);

    g_application_hold (g_application_get_default ());
    nautilus_properties_window_present (files, NULL, startup_id,
                                        properties_window_on_finished, NULL);

    nautilus_file_list_free (files);

    nautilus_freedesktop_file_manager1_complete_show_item_properties (object, invocation);
    return TRUE;
}

static void
bus_acquired_cb (GDBusConnection *conn,
                 const gchar     *name,
                 gpointer         user_data)
{
    NautilusFreedesktopDBus *fdb = user_data;

    DEBUG ("Bus acquired at %s", name);

    fdb->skeleton = nautilus_freedesktop_file_manager1_skeleton_new ();

    g_signal_connect (fdb->skeleton, "handle-show-items",
                      G_CALLBACK (skeleton_handle_show_items_cb), fdb);
    g_signal_connect (fdb->skeleton, "handle-show-folders",
                      G_CALLBACK (skeleton_handle_show_folders_cb), fdb);
    g_signal_connect (fdb->skeleton, "handle-show-item-properties",
                      G_CALLBACK (skeleton_handle_show_item_properties_cb), fdb);

    g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (fdb->skeleton), conn, NAUTILUS_FDO_DBUS_PATH, NULL);

    if (G_UNLIKELY (fdb->pending_open_locations != NULL))
    {
        g_auto (GStrv) locations = NULL;

        locations = g_steal_pointer (&fdb->pending_open_locations);

        nautilus_freedesktop_dbus_set_open_locations (fdb, (const gchar **) locations);
    }

    if (G_UNLIKELY (fdb->pending_open_windows_with_locations != NULL))
    {
        g_autoptr (GVariant) locations = NULL;

        locations = g_steal_pointer (&fdb->pending_open_windows_with_locations);

        nautilus_freedesktop_dbus_set_open_windows_with_locations (fdb, locations);
    }
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
    NautilusFreedesktopDBus *fdb;

    DEBUG ("Lost (or failed to acquire) the name %s on the session message bus\n", name);

    fdb = NAUTILUS_FREEDESKTOP_DBUS (user_data);

    fdb->name_lost = TRUE;
}

static void
nautilus_freedesktop_dbus_dispose (GObject *object)
{
    NautilusFreedesktopDBus *fdb = (NautilusFreedesktopDBus *) object;

    if (fdb->owner_id != 0)
    {
        g_bus_unown_name (fdb->owner_id);
        fdb->owner_id = 0;
    }

    if (fdb->skeleton != NULL)
    {
        g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (fdb->skeleton));
        g_object_unref (fdb->skeleton);
        fdb->skeleton = NULL;
    }

    G_OBJECT_CLASS (nautilus_freedesktop_dbus_parent_class)->dispose (object);
}

static void
nautilus_freedesktop_dbus_finalize (GObject *object)
{
    NautilusFreedesktopDBus *fdb;

    fdb = NAUTILUS_FREEDESKTOP_DBUS (object);

    g_clear_pointer (&fdb->pending_open_locations, g_strfreev);
    g_clear_pointer (&fdb->pending_open_windows_with_locations, g_variant_unref);
}

static void
nautilus_freedesktop_dbus_class_init (NautilusFreedesktopDBusClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = nautilus_freedesktop_dbus_dispose;
    object_class->finalize = nautilus_freedesktop_dbus_finalize;
}

static void
nautilus_freedesktop_dbus_init (NautilusFreedesktopDBus *fdb)
{
    fdb->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                    NAUTILUS_FDO_DBUS_NAME,
                                    G_BUS_NAME_OWNER_FLAGS_NONE,
                                    bus_acquired_cb,
                                    name_acquired_cb,
                                    name_lost_cb,
                                    fdb,
                                    NULL);
    fdb->skeleton = NULL;
    fdb->pending_open_locations = NULL;
    fdb->pending_open_windows_with_locations = NULL;
    fdb->name_lost = FALSE;
}

void
nautilus_freedesktop_dbus_set_open_locations (NautilusFreedesktopDBus  *fdb,
                                              const gchar             **locations)
{
    g_return_if_fail (NAUTILUS_IS_FREEDESKTOP_DBUS (fdb));

    if (G_UNLIKELY (fdb->skeleton == NULL))
    {
        if (G_LIKELY (fdb->name_lost))
        {
            return;
        }

        g_clear_pointer (&fdb->pending_open_locations, g_strfreev);

        fdb->pending_open_locations = g_strdupv ((gchar **) locations);
    }
    else
    {
        nautilus_freedesktop_file_manager1_set_open_locations (fdb->skeleton, locations);
    }
}

/**
 * nautilus_freedesktop_dbus_set_open_windows_with_locations:
 * fdb: The skeleton for the dbus interface
 * locations: Mapping of windows to locations open in each window
 *
 * This allows the application to publish the locations that are open in each window.
 * It is used by shell extensions like dash-to-dock/ubuntu-dock to match special dock
 * icons to the windows where the icon's location is open. For example, the Trash or
 * a removable device.
 */
void
nautilus_freedesktop_dbus_set_open_windows_with_locations (NautilusFreedesktopDBus *fdb,
                                                           GVariant                *locations)
{
    g_return_if_fail (NAUTILUS_IS_FREEDESKTOP_DBUS (fdb));

    if (G_UNLIKELY (fdb->skeleton == NULL))
    {
        if (G_LIKELY (fdb->name_lost))
        {
            return;
        }

        g_clear_pointer (&fdb->pending_open_windows_with_locations, g_variant_unref);

        fdb->pending_open_windows_with_locations = g_variant_ref (locations);
    }
    else
    {
        nautilus_freedesktop_file_manager1_set_open_windows_with_locations (fdb->skeleton,
                                                                            locations);
    }
}

/* Tries to own the org.freedesktop.FileManager1 service name */
NautilusFreedesktopDBus *
nautilus_freedesktop_dbus_new (void)
{
    return g_object_new (NAUTILUS_TYPE_FREEDESKTOP_DBUS, NULL);
}
