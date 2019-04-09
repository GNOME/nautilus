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

static const gchar *
path_from_tracker_dir (const gchar *value)
{
    const gchar *path;

    if (g_strcmp0 (value, "&DESKTOP") == 0)
    {
        path = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
    }
    else if (g_strcmp0 (value, "&DOCUMENTS") == 0)
    {
        path = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
    }
    else if (g_strcmp0 (value, "&DOWNLOAD") == 0)
    {
        path = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
    }
    else if (g_strcmp0 (value, "&MUSIC") == 0)
    {
        path = g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);
    }
    else if (g_strcmp0 (value, "&PICTURES") == 0)
    {
        path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
    }
    else if (g_strcmp0 (value, "&PUBLIC_SHARE") == 0)
    {
        path = g_get_user_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE);
    }
    else if (g_strcmp0 (value, "&TEMPLATES") == 0)
    {
        path = g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES);
    }
    else if (g_strcmp0 (value, "&VIDEOS") == 0)
    {
        path = g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS);
    }
    else if (g_strcmp0 (value, "$HOME") == 0)
    {
        path = g_get_home_dir ();
    }
    else
    {
        path = value;
    }

    return path;
}

static GList *
get_tracker_locations (void)
{
    g_auto (GStrv) locations = NULL;
    GList *list = NULL;
    gint idx;
    GFile *location;
    const gchar *path;

    locations = g_settings_get_strv (tracker_preferences, TRACKER_KEY_RECURSIVE_DIRECTORIES);

    for (idx = 0; locations[idx] != NULL; idx++)
    {
        path = path_from_tracker_dir (locations[idx]);
        location = g_file_new_for_commandline_arg (path);
        list = g_list_prepend (list, location);
    }

    return list;
}

gboolean
nautilus_tracker_directory_is_tracked (GFile *directory)
{
    g_autolist (GFile) locations = NULL;
    GList *l;

    locations = get_tracker_locations ();
    for (l = locations; l != NULL; l = l->next)
    {
        if (g_file_equal (directory, G_FILE (l->data)) ||
            g_file_has_prefix (directory, G_FILE (l->data)))
        {
            return TRUE;
        }
    }

    return FALSE;
}
