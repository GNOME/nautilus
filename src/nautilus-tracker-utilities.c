/* nautilus-tracker-utilities.c
 *
 * Copyright 2019 Carlos Soriano <csoriano@redhat.com>
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

#include "nautilus-tracker-utilities.h"
#include "nautilus-global-preferences.h"

#define TRACKER_KEY_RECURSIVE_DIRECTORIES "index-recursive-directories"
#define TRACKER_KEY_SINGLE_DIRECTORIES "index-single-directories"

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

    locations = g_settings_get_strv (tracker_preferences, key);

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
