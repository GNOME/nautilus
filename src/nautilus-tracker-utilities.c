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
#include "nautilus-global-preferences.h"

#define TRACKER_KEY_RECURSIVE_DIRECTORIES "index-recursive-directories"
#define TRACKER_KEY_SINGLE_DIRECTORIES "index-single-directories"

/* Shared global connection to Tracker Miner FS */
static const gchar *tracker_miner_fs_busname = NULL;
static TrackerSparqlConnection *tracker_miner_fs_connection = NULL;
static GError *tracker_miner_fs_error = NULL;

static gboolean
get_host_tracker_miner_fs (GError **error)
{
    const gchar *busname = "org.freedesktop.Tracker3.Miner.Files";

    g_message ("Connecting to %s", busname);
    tracker_miner_fs_connection = tracker_sparql_connection_bus_new (busname, NULL, NULL, error);
    if (*error)
    {
        g_warning ("Unable to create connection for session-wide Tracker indexer: %s", (*error)->message);
        return FALSE;
    }

    tracker_miner_fs_busname = busname;
    return TRUE;
}

static gboolean
start_local_tracker_miner_fs (GError **error)
{
    const gchar *busname = APPLICATION_ID ".Tracker3.Miner.Files";

    g_message ("Starting %s", busname);
    tracker_miner_fs_connection = tracker_sparql_connection_bus_new (busname, NULL, NULL, error);
    if (*error)
    {
        g_critical ("Could not start local Tracker indexer at %s: %s", busname, (*error)->message);
        return FALSE;
    }

    tracker_miner_fs_busname = busname;
    return TRUE;
}

static gboolean
inside_flatpak (void)
{
    return g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
}

static void
setup_tracker_miner_fs_connection (void)
{
    static gsize tried_tracker_init = FALSE;

    if (g_once_init_enter (&tried_tracker_init))
    {
        gboolean success;

        success = get_host_tracker_miner_fs (&tracker_miner_fs_error);

        if (!success && inside_flatpak ())
        {
            g_clear_error (&tracker_miner_fs_error);
            success = start_local_tracker_miner_fs (&tracker_miner_fs_error);
        }

        g_once_init_leave (&tried_tracker_init, TRUE);
    }
}

/**
 * nautilus_tracker_get_miner_fs_connection:
 * @error: return location for a #GError
 *
 * This function returns a global singleton #TrackerSparqlConnection, or %NULL
 * if we couldn't connect to Tracker Miner FS.
 *
 * The first time you call it, this function will block while trying to connect.
 * This may take some time if starting Tracker Miners from a Flatpak bundle.
 *
 * The returned object is a globally shared singleton which should NOT be
 * unreffed.
 *
 * Returns: a #TrackerSparqlConnection, or %NULL
 */
TrackerSparqlConnection *
nautilus_tracker_get_miner_fs_connection (GError **error)
{
    setup_tracker_miner_fs_connection ();

    if (tracker_miner_fs_error && error)
    {
        *error = g_error_copy (tracker_miner_fs_error);
    }

    return tracker_miner_fs_connection;
}

/**
 * nautilus_tracker_get_miner_fs_busname:
 * @error: return location for a #GError
 *
 * This function returns a DBus name that can be used to talk to
 * tracker-miner-fs, or %NULL if there is no Tracker Miner FS available.
 *
 * The first time you call it, this function will block while trying to connect.
 * This may take some time if starting Tracker Miners from a Flatpak bundle.
 *
 * Returns: a string
 */
const gchar *
nautilus_tracker_get_miner_fs_busname (GError **error)
{
    setup_tracker_miner_fs_connection ();

    if (tracker_miner_fs_error && error)
    {
        *error = g_error_copy (tracker_miner_fs_error);
    }

    return tracker_miner_fs_busname;
}
