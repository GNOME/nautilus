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
    CreateKeyFunc create_key_func;
};

static gpointer
identity_key_func (gpointer ptr)
{
    return ptr;
}

/**
 * nautilus_hash_queue_new:
 * @hash_func: a function to create a hash value from a key
 * @key_equal_func: a function to check two keys for equality
 * @key_destroy_func: (nullable): a function to free the memory allocated for
 *     the key used when removing the entry from the #GHashTable, or `NULL` if
 *     you don't want to supply such a function.
 * @create_key_func: (nullable): a function to create a hashable key from a
 *     value, or `NULL` to use the item itself as a key.
 *
 * Creates a new #NautilusHashQueue.
 *
 * Returns: (transfer full): a new #NautilusHashQueue
 */
NautilusHashQueue *
nautilus_hash_queue_new (GHashFunc      hash_func,
                         GEqualFunc     equal_func,
                         GDestroyNotify key_destroy_func,
                         CreateKeyFunc  create_key_func)
{
    NautilusHashQueue *queue;

    queue = g_new0 (NautilusHashQueue, 1);
    g_queue_init ((GQueue *) queue);
    queue->item_to_link_map = g_hash_table_new_full (hash_func, equal_func, key_destroy_func, NULL);
    queue->create_key_func = create_key_func != NULL ? create_key_func : identity_key_func;

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
gboolean
nautilus_hash_queue_enqueue (NautilusHashQueue *queue,
                             gpointer           item)
{
    if (g_hash_table_lookup (queue->item_to_link_map, item) != NULL)
    {
        /* It's already on the queue. */
        return FALSE;
    }

    gpointer key = queue->create_key_func (item);

    g_queue_push_tail ((GQueue *) queue, item);
    g_hash_table_insert (queue->item_to_link_map, key, queue->parent.tail);

    return TRUE;
}

/** Remove the item at the head of the queue and return it. */
gpointer
nautilus_hash_queue_dequeue (NautilusHashQueue *queue)
{
    gpointer item = g_queue_pop_head ((GQueue *) queue);
    gpointer key = queue->create_key_func (item);

    g_hash_table_remove (queue->item_to_link_map, key);

    return item;
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

gpointer
nautilus_hash_queue_find_item (NautilusHashQueue *queue,
                               gconstpointer      key)
{
    GList *link = g_hash_table_lookup (queue->item_to_link_map, key);

    if (link == NULL)
    {
        return NULL;
    }

    return link->data;
}

void
nautilus_hash_queue_move_existing_to_head (NautilusHashQueue *queue,
                                           gconstpointer      key)
{
    GList *link = g_hash_table_lookup (queue->item_to_link_map, key);

    if (link == NULL)
    {
        return;
    }

    g_queue_unlink ((GQueue *) queue, link);
    g_queue_push_head_link ((GQueue *) queue, link);
}

void
nautilus_hash_queue_move_existing_to_tail (NautilusHashQueue *queue,
                                           gconstpointer      key)
{
    GList *link = g_hash_table_lookup (queue->item_to_link_map, key);

    if (link == NULL)
    {
        return;
    }

    g_queue_unlink ((GQueue *) queue, link);
    g_queue_push_tail_link ((GQueue *) queue, link);
}
