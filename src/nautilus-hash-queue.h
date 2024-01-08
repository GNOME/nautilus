/*
   Copyright (C) 2001 Maciej Stachowiak

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.

   Author: Maciej Stachowiak <mjs@noisehavoc.org>
*/

#pragma once

#include "nautilus-file.h"

/** Function to create a hashable key from an item. */
typedef gpointer (* CreateKeyFunc) (gpointer item);

typedef struct NautilusHashQueue NautilusHashQueue;

NautilusHashQueue *nautilus_hash_queue_new      (GHashFunc      hash_func,
                                                 GEqualFunc     equal_func,
                                                 GDestroyNotify key_destroy_func,
                                                 CreateKeyFunc  create_key_func);
void               nautilus_hash_queue_destroy  (NautilusHashQueue *queue);

gboolean           nautilus_hash_queue_enqueue  (NautilusHashQueue *queue,
                                                 gpointer           item);
gpointer           nautilus_hash_queue_dequeue  (NautilusHashQueue *queue);
void               nautilus_hash_queue_remove   (NautilusHashQueue *queue,
                                                 gconstpointer      key);

gpointer           nautilus_hash_queue_find_item             (NautilusHashQueue *queue,
                                                              gconstpointer      key);

void               nautilus_hash_queue_move_existing_to_head (NautilusHashQueue *queue,
                                                              gconstpointer      key);
void               nautilus_hash_queue_move_existing_to_tail (NautilusHashQueue *queue,
                                                              gconstpointer      key);
