/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-tree-change-queue.h - Class to help pool changes and defer
   them to an idle handler. */

#ifndef NAUTILUS_TREE_CHANGE_QUEUE_H
#define NAUTILUS_TREE_CHANGE_QUEUE_H

#include <gtk/gtkobject.h>
#include "nautilus-tree-node.h"

typedef struct NautilusTreeChangeQueue NautilusTreeChangeQueue;
typedef struct NautilusTreeChangeQueueClass NautilusTreeChangeQueueClass;

#define NAUTILUS_TYPE_TREE_CHANGE_QUEUE	    (nautilus_tree_change_queue_get_type ())
#define NAUTILUS_TREE_CHANGE_QUEUE(obj)	    (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_TREE_CHANGE_QUEUE, NautilusTreeChangeQueue))
#define NAUTILUS_TREE_CHANGE_QUEUE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TREE_CHANGE_QUEUE, NautilusTreeChangeQueueClass))
#define NAUTILUS_IS_TREE_CHANGE_QUEUE(obj)	   (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_TREE_CHANGE_QUEUE))
#define NAUTILUS_IS_TREE_CHANGE_QUEUE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TREE_CHANGE_QUEUE))


typedef enum {
	NAUTILUS_TREE_CHANGE_TYPE_CHANGED,
	NAUTILUS_TREE_CHANGE_TYPE_REMOVED,
	NAUTILUS_TREE_CHANGE_TYPE_DONE_LOADING
} NautilusTreeChangeType;


typedef struct {
	NautilusTreeChangeType change_type;
	NautilusTreeNode      *node;
} NautilusTreeChange;


typedef struct NautilusTreeChangeQueueDetails NautilusTreeChangeQueueDetails;

struct NautilusTreeChangeQueue {
	GtkObject                       parent;
	NautilusTreeChangeQueueDetails *details;
};

struct NautilusTreeChangeQueueClass {
	GtkObjectClass parent_class;
};



GtkType                   nautilus_tree_change_queue_get_type  (void);
NautilusTreeChangeQueue * nautilus_tree_change_queue_new       (void);

void                      nautilus_tree_change_queue_enqueue   (NautilusTreeChangeQueue *queue,
								NautilusTreeChangeType   change_type,
								NautilusTreeNode        *node);
NautilusTreeChange *      nautilus_tree_change_queue_dequeue   (NautilusTreeChangeQueue *queue);

void                      nautilus_tree_change_free            (NautilusTreeChange      *change);

#endif /* NAUTILUS_TREE_CHANGE_QUEUE_H */
