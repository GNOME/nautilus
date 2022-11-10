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
#include "nautilus-tracker-utilities.h"

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

static gboolean
inside_flatpak (void)
{
    return g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
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
        if (inside_flatpak ())
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
