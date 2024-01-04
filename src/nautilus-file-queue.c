/*
 *  Copyright (C) 2001 Maciej Stachowiak
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author: Maciej Stachowiak <mjs@noisehavoc.org>
 */

#include <config.h>
#include "nautilus-file-queue.h"

#include <glib.h>

/**
 * `NautilusFileQueue` is a `GQueue` with 2 special features:
 * 1) It can find and remove items in constant time
 * 2) It doesn't allow duplicates.
 */
struct NautilusFileQueue
{
    GQueue parent;
    GHashTable *item_to_link_map;
};

NautilusFileQueue *
nautilus_file_queue_new (void)
{
    NautilusFileQueue *queue;

    queue = g_new0 (NautilusFileQueue, 1);
    g_queue_init ((GQueue *) queue);
    queue->item_to_link_map = g_hash_table_new (g_direct_hash, g_direct_equal);

    return queue;
}

void
nautilus_file_queue_destroy (NautilusFileQueue *queue)
{
    g_hash_table_destroy (queue->item_to_link_map);
    g_queue_clear_full ((GQueue *) queue, g_object_unref);
    g_free (queue);
}

void
nautilus_file_queue_enqueue (NautilusFileQueue *queue,
                             NautilusFile      *file)
{
    if (g_hash_table_lookup (queue->item_to_link_map, file) != NULL)
    {
        /* It's already on the queue. */
        return;
    }

    g_queue_push_tail ((GQueue *) queue, file);
    g_hash_table_insert (queue->item_to_link_map, g_object_ref (file), queue->parent.tail);
}

void
nautilus_file_queue_remove (NautilusFileQueue *queue,
                            NautilusFile      *file)
{
    GList *link;

    link = g_hash_table_lookup (queue->item_to_link_map, file);

    if (link == NULL)
    {
        /* It's not on the queue */
        return;
    }

    g_queue_delete_link ((GQueue *) queue, link);
    g_hash_table_remove (queue->item_to_link_map, file);

    nautilus_file_unref (file);
}
