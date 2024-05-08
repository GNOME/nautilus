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
#include "nautilus-hash-queue.h"

#include <glib.h>

/**
 * `NautilusHashQueue` is a `GQueue` with 2 special features:
 * 1) It can find and remove items in constant time
 * 2) It doesn't allow duplicates.
 */
struct NautilusHashQueue
{
    GQueue parent;
    GHashTable *item_to_link_map;
    KeyCreateFunc key_create_func;
    GDestroyNotify key_destroy_func;
};

/**
 * nautilus_hash_queue_new:
 * @hash_func: a function to create a hash value from a key
 * @key_equal_func: a function to check two keys for equality
 * @key_create_func: a function to create a hashable key from an enqueued value
 * @key_destroy_func: (nullable): a function to free the memory allocated for
 *     the key used when removing the entry from the #GHashTable, or `NULL` if
 *     you don't want to supply such a function.
 *
 * Creates a new #NautilusHashQueue.
 *
 * Returns: (transfer full): a new #NautilusHashQueue
 */
NautilusHashQueue *
nautilus_hash_queue_new (GHashFunc      hash_func,
                         GEqualFunc     equal_func,
                         KeyCreateFunc  key_create_func,
                         GDestroyNotify key_destroy_func)
{
    NautilusHashQueue *queue;

    queue = g_new0 (NautilusHashQueue, 1);
    g_queue_init ((GQueue *) queue);
    queue->item_to_link_map = g_hash_table_new_full (hash_func, equal_func, key_destroy_func, NULL);
    queue->key_create_func = key_create_func;
    queue->key_destroy_func = key_destroy_func;

    return queue;
}

void
nautilus_hash_queue_destroy (NautilusHashQueue *queue)
{
    g_hash_table_destroy (queue->item_to_link_map);
    /* Items in queue already freed by hash table */
    g_free (queue);
}

/** Add an item to the tail of the queue, unless it's already in the queue. */
void
nautilus_hash_queue_enqueue (NautilusHashQueue *queue,
                             gpointer           item)
{
    gpointer key = queue->key_create_func (item);

    if (g_hash_table_lookup (queue->item_to_link_map, key) != NULL)
    {
        /* It's already on the queue. */
        queue->key_destroy_func (key);
        return;
    }

    g_queue_push_tail ((GQueue *) queue, item);
    g_hash_table_insert (queue->item_to_link_map, key, queue->parent.tail);
}

/**
 * Remove the item responding to the provided key from the queue in constant time.
 */
void
nautilus_hash_queue_remove (NautilusHashQueue *queue,
                            gconstpointer      key)
{
    GList *link = g_hash_table_lookup (queue->item_to_link_map, key);

    if (link == NULL)
    {
        /* It's not on the queue */
        return;
    }

    g_queue_delete_link ((GQueue *) queue, link);
    g_hash_table_remove (queue->item_to_link_map, key);
}
