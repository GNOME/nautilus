/* eel-glib-extensions.c - implementation of new functions that conceptually
 *                               belong in glib. Perhaps some of these will be
 *                               actually rolled into glib someday.
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: John Sullivan <sullivan@eazel.com>
 */

#include <config.h>
#include "eel-glib-extensions.h"

#include "eel-debug.h"
#include "eel-lib-self-check-functions.h"
#include "eel-string.h"
#include <glib-object.h>
#include <math.h>
#include <stdlib.h>

gboolean
eel_g_strv_equal (char **a,
                  char **b)
{
    int i;

    if (g_strv_length (a) != g_strv_length (b))
    {
        return FALSE;
    }

    for (i = 0; a[i] != NULL; i++)
    {
        if (strcmp (a[i], b[i]) != 0)
        {
            return FALSE;
        }
    }
    return TRUE;
}

static int
compare_pointers (gconstpointer pointer_1,
                  gconstpointer pointer_2)
{
    if ((const char *) pointer_1 < (const char *) pointer_2)
    {
        return -1;
    }
    if ((const char *) pointer_1 > (const char *) pointer_2)
    {
        return +1;
    }
    return 0;
}

gboolean
eel_g_lists_sort_and_check_for_intersection (GList **list_1,
                                             GList **list_2)
{
    GList *node_1, *node_2;
    int compare_result;

    *list_1 = g_list_sort (*list_1, compare_pointers);
    *list_2 = g_list_sort (*list_2, compare_pointers);

    node_1 = *list_1;
    node_2 = *list_2;

    while (node_1 != NULL && node_2 != NULL)
    {
        compare_result = compare_pointers (node_1->data, node_2->data);
        if (compare_result == 0)
        {
            return TRUE;
        }
        if (compare_result <= 0)
        {
            node_1 = node_1->next;
        }
        if (compare_result >= 0)
        {
            node_2 = node_2->next;
        }
    }

    return FALSE;
}

typedef struct
{
    GList *keys;
    GList *values;
} FlattenedHashTable;

static void
flatten_hash_table_element (gpointer key,
                            gpointer value,
                            gpointer callback_data)
{
    FlattenedHashTable *flattened_table;

    flattened_table = callback_data;
    flattened_table->keys = g_list_prepend
                                (flattened_table->keys, key);
    flattened_table->values = g_list_prepend
                                  (flattened_table->values, value);
}

void
eel_g_hash_table_safe_for_each (GHashTable *hash_table,
                                GHFunc      callback,
                                gpointer    callback_data)
{
    FlattenedHashTable flattened;
    GList *p, *q;

    flattened.keys = NULL;
    flattened.values = NULL;

    g_hash_table_foreach (hash_table,
                          flatten_hash_table_element,
                          &flattened);

    for (p = flattened.keys, q = flattened.values;
         p != NULL;
         p = p->next, q = q->next)
    {
        (*callback)(p->data, q->data, callback_data);
    }

    g_list_free (flattened.keys);
    g_list_free (flattened.values);
}

#if !defined (EEL_OMIT_SELF_CHECK)

#endif /* !EEL_OMIT_SELF_CHECK */
