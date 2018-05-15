/* nautilus-metadata.c - metadata utils
 *
 * Copyright (C) 2009 Red Hatl, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "nautilus-metadata.h"
#include <glib.h>

static char *used_metadata_names[] =
{
    NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
    NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
    NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
    NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
    NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
    NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER,
    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
    NAUTILUS_METADATA_KEY_CUSTOM_ICON_NAME,
    NAUTILUS_METADATA_KEY_EMBLEMS,
    NULL
};

guint
nautilus_metadata_get_id (const char *metadata)
{
    static GHashTable *hash;
    int i;

    if (hash == NULL)
    {
        hash = g_hash_table_new (g_str_hash, g_str_equal);
        for (i = 0; used_metadata_names[i] != NULL; i++)
        {
            g_hash_table_insert (hash,
                                 used_metadata_names[i],
                                 GINT_TO_POINTER (i + 1));
        }
    }

    return GPOINTER_TO_INT (g_hash_table_lookup (hash, metadata));
}
