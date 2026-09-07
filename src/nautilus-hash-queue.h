/*
 * SPDX-FileCopyrightText: 2001 Maciej Stachowiak
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Maciej Stachowiak <mjs@noisehavoc.org>
 */

#pragma once

#include "nautilus-types.h"

#include <glib.h>

/** Function to create a hashable key from an item. */
typedef gpointer (* KeyCreateFunc) (gpointer item);

typedef struct NautilusHashQueue NautilusHashQueue;

NautilusHashQueue * nautilus_hash_queue_new     (GHashFunc      hash_func,
                                                 GEqualFunc     equal_func,
                                                 GDestroyNotify key_destroy_func,
                                                 GDestroyNotify value_destroy_func);
void               nautilus_hash_queue_destroy  (NautilusHashQueue *queue);

gboolean           nautilus_hash_queue_enqueue  (NautilusHashQueue *queue,
                                                 gpointer           key,
                                                 gpointer           value);
gboolean           nautilus_hash_queue_reenqueue             (NautilusHashQueue *queue,
                                                              gpointer           key,
                                                              gpointer           value);
void               nautilus_hash_queue_remove   (NautilusHashQueue *queue,
                                                 gconstpointer      key);
gpointer           nautilus_hash_queue_find_item             (NautilusHashQueue *queue,
                                                              gconstpointer      key);
void               nautilus_hash_queue_move_existing_to_head (NautilusHashQueue *queue,
                                                              gconstpointer      key);
void               nautilus_hash_queue_move_existing_to_tail (NautilusHashQueue *queue,
                                                              gconstpointer      key);
void               nautilus_hash_queue_remove_head           (NautilusHashQueue *queue);

/* Get the file at the head of the queue without removing or unrefing it. */
#define nautilus_hash_queue_peek_head(queue) (g_queue_peek_head ((GQueue *) (queue)))

#define nautilus_hash_queue_is_empty(queue) (g_queue_is_empty ((GQueue *) (queue)))

#define nautilus_hash_queue_get_length(queue) (g_queue_get_length ((GQueue *) (queue)))
