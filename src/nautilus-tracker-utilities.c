/* nautilus-tracker-utilities.c
 *
 * Copyright 2019 Carlos Soriano <csoriano@redhat.com>
 * Copyright 2020 Sam Thursfield <sam@afuera.me.uk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"
#include "nautilus-application.h"
#include "nautilus-global-preferences.h"
#include "nautilus-tracker-utilities.h"

#define TRACKER_KEY_RECURSIVE_DIRECTORIES "index-recursive-directories"
#define TRACKER_KEY_SINGLE_DIRECTORIES "index-single-directories"

/* Shared global connection to Tracker Miner FS */
static const gchar *tracker_miner_fs_busname = NULL;
static TrackerSparqlConnection *tracker_miner_fs_connection = NULL;
static GError *tracker_miner_fs_error = NULL;

static void
local_tracker_miner_fs_ready (GObject      *source,
                              GAsyncResult *res,
                              gpointer      user_data)
{
    tracker_miner_fs_connection = tracker_sparql_connection_new_finish (res, &tracker_miner_fs_error);
    if (tracker_miner_fs_error != NULL)
    {
        g_critical ("Could not start local Tracker indexer at %s: %s", tracker_miner_fs_busname, tracker_miner_fs_error->message);
    }
}

static void
start_local_tracker_miner_fs (void)
{
    const gchar *busname = APPLICATION_ID ".Tracker3.Miner.Files";

    g_message ("Starting %s", busname);
    tracker_sparql_connection_bus_new_async (busname, NULL, NULL, NULL, local_tracker_miner_fs_ready, NULL);

    tracker_miner_fs_busname = busname;
}

static void
host_tracker_miner_fs_ready (GObject      *source,
                             GAsyncResult *res,
                             gpointer      user_data)
{
    tracker_miner_fs_connection = tracker_sparql_connection_bus_new_finish (res, &tracker_miner_fs_error);
    if (tracker_miner_fs_error)
    {
        g_warning ("Unable to create connection for session-wide Tracker indexer: %s", (tracker_miner_fs_error)->message);
        if (nautilus_application_is_sandboxed ())
        {
            g_clear_error (&tracker_miner_fs_error);
            start_local_tracker_miner_fs ();
        }
    }
}

void
nautilus_tracker_setup_miner_fs_connection (void)
{
    static gsize tried_tracker_init = FALSE;

    if (tracker_miner_fs_connection != NULL)
    {
        /* The connection was already established */
        return;
    }

    if (g_once_init_enter (&tried_tracker_init))
    {
        const gchar *busname = "org.freedesktop.Tracker3.Miner.Files";

        g_message ("Connecting to %s", busname);
        tracker_sparql_connection_bus_new_async (busname, NULL, NULL, NULL, host_tracker_miner_fs_ready, NULL);

        tracker_miner_fs_busname = busname;

        g_once_init_leave (&tried_tracker_init, TRUE);
    }
}

/**
 * nautilus_tracker_setup_host_miner_fs_connection_sync:
 *
 * This function is only meant to be used within tests.
 * This version of this setup function intentionally blocks to help with tests.
 *
 */
void
nautilus_tracker_setup_host_miner_fs_connection_sync (void)
{
    g_autoptr (GError) error = NULL;
    const gchar *busname = "org.freedesktop.Tracker3.Miner.Files";

    g_message ("Starting %s", busname);
    tracker_miner_fs_connection = tracker_sparql_connection_bus_new (busname, NULL, NULL, &error);
    if (error != NULL)
    {
        g_critical ("Could not start local Tracker indexer at %s: %s", busname, error->message);
        return;
    }

    tracker_miner_fs_busname = busname;
}

/**
 * nautilus_tracker_get_miner_fs_connection:
 * @error: return location for a #GError
 *
 * This function returns a global singleton #TrackerSparqlConnection, or %NULL
 * if either we couldn't connect to Tracker Miner FS or the connection is still
 * pending.
 *
 * The returned object is a globally shared singleton which should NOT be
 * unreffed.
 *
 * Returns: a #TrackerSparqlConnection, or %NULL
 */
TrackerSparqlConnection *
nautilus_tracker_get_miner_fs_connection (GError **error)
{
    nautilus_tracker_setup_miner_fs_connection ();

    if (tracker_miner_fs_error && error)
    {
        *error = g_error_copy (tracker_miner_fs_error);
    }

    return tracker_miner_fs_connection;
}

static GFile *
location_from_tracker_dir (const gchar *value)
{
    const gchar *special_dir;
    g_autoptr (GFile) home = NULL;
    GFile *location;

    home = g_file_new_for_path (g_get_home_dir ());

    if (g_strcmp0 (value, "$HOME") == 0)
    {
        return g_steal_pointer (&home);
    }

    special_dir = NULL;
    if (g_strcmp0 (value, "&DESKTOP") == 0)
    {
        special_dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
    }
    else if (g_strcmp0 (value, "&DOCUMENTS") == 0)
    {
        special_dir = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
    }
    else if (g_strcmp0 (value, "&DOWNLOAD") == 0)
    {
        special_dir = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
    }
    else if (g_strcmp0 (value, "&MUSIC") == 0)
    {
        special_dir = g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);
    }
    else if (g_strcmp0 (value, "&PICTURES") == 0)
    {
        special_dir = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
    }
    else if (g_strcmp0 (value, "&PUBLIC_SHARE") == 0)
    {
        special_dir = g_get_user_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE);
    }
    else if (g_strcmp0 (value, "&TEMPLATES") == 0)
    {
        special_dir = g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES);
    }
    else if (g_strcmp0 (value, "&VIDEOS") == 0)
    {
        special_dir = g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS);
    }

    if (special_dir != NULL)
    {
        location = g_file_new_for_commandline_arg (special_dir);

        /* Ignore XDG directories set to $HOME, like the miner does */
        if (g_file_equal (location, home))
        {
            g_clear_object (&location);
        }
    }
    else
    {
        location = g_file_new_for_commandline_arg (value);
    }

    return location;
}
static GList *
get_tracker_locations (const gchar *key)
{
    g_auto (GStrv) locations = NULL;
    GList *list = NULL;
    gint idx;
    GFile *location;

    locations = g_settings_get_strv (localsearch_preferences, key);

    for (idx = 0; locations[idx] != NULL; idx++)
    {
        location = location_from_tracker_dir (locations[idx]);
        if (location != NULL)
        {
            list = g_list_prepend (list, location);
        }
    }

    return list;
}

/**
 * nautilus_tracker_directory_is_tracked:
 * @directory: a #GFile representing a directory
 *
 * This function reads the "index-recursive-directories" and
 * "index-single-directories" keys from the org.freedesktop.tracker.miner.files
 * schema, and assumes the listed directories (and their descendants for the
 * former key) are tracked.
 *
 * Exception: XDG user dirs set to $HOME are ignored.
 *
 * FIXME: Tracker's files miner's logic is actually a lot more complex,
 * including configurable ignore patterns, but we are overlooking that.
 *
 * Returns: $TRUE if the @directory is, in principle, tracked. $FALSE otherwise.
 */
gboolean
nautilus_tracker_directory_is_tracked (GFile *directory)
{
    g_autolist (GFile) recursive_locations = NULL;
    g_autolist (GFile) single_locations = NULL;
    GList *l;

    recursive_locations = get_tracker_locations (TRACKER_KEY_RECURSIVE_DIRECTORIES);
    for (l = recursive_locations; l != NULL; l = l->next)
    {
        if (g_file_equal (directory, G_FILE (l->data)) ||
            g_file_has_prefix (directory, G_FILE (l->data)))
        {
            return TRUE;
        }
    }

    return nautilus_tracker_directory_is_single (directory);
}

gboolean
nautilus_tracker_directory_is_single (GFile *directory)
{
    g_autolist (GFile) single_locations = NULL;
    GList *l;

    single_locations = get_tracker_locations (TRACKER_KEY_SINGLE_DIRECTORIES);
    for (l = single_locations; l != NULL; l = l->next)
    {
        if (g_file_equal (directory, G_FILE (l->data)))
        {
            return TRUE;
        }
    }

    return FALSE;
}
