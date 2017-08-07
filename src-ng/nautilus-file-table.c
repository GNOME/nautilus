/* Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "nautilus-file-table.h"

static GMutex      mutex;

static gpointer
create_hash_table (gpointer data)
{
    (void) data;

    return g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                  g_object_unref, NULL);
}

static GHashTable *
get_hash_table (void)
{
    static GOnce once = G_ONCE_INIT;

    g_once (&once, create_hash_table, NULL);

    g_assert (once.retval != NULL);

    return once.retval;
}

gboolean
nautilus_file_table_insert (GFile        *location,
                            NautilusFile *instance)
{
    gboolean success;

    g_mutex_lock (&mutex);

    success = g_hash_table_insert (get_hash_table (), location, instance);

    g_mutex_unlock (&mutex);

    return success;
}

gboolean
nautilus_file_table_remove (GFile *location)
{
    gboolean success;

    g_mutex_lock (&mutex);

    success = g_hash_table_remove (get_hash_table (), location);

    g_mutex_unlock (&mutex);

    return success;
}

NautilusFile *
nautilus_file_table_lookup (GFile *location)
{
    gpointer instance;

    g_mutex_lock (&mutex);

    instance = g_hash_table_lookup (get_hash_table (), location);

    g_mutex_unlock (&mutex);

    return instance;
}
