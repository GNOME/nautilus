/*
 * SPDX-FileCopyrightText: 2009 Red Hatl, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
