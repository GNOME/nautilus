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

/* nautilus-tree-change-queue.c - Class to help pool changes and defer
   them to an idle handler. */

#include <config.h>
#include "nautilus-tree-change-queue.h"

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>

struct NautilusTreeChangeQueueDetails {
	GSList *head;
	GSList *tail;
};


static void nautilus_tree_change_queue_destroy          (GtkObject   *object);
static void nautilus_tree_change_queue_initialize       (gpointer     object,
							 gpointer     klass);
static void nautilus_tree_change_queue_initialize_class (gpointer     klass);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusTreeChangeQueue, nautilus_tree_change_queue, GTK_TYPE_OBJECT)


static NautilusTreeChange *nautilus_tree_change_new     (NautilusTreeChangeType  change_type,
							 NautilusTreeNode       *node);


/* type system infrastructure code */

static void
nautilus_tree_change_queue_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_change_queue_destroy;

}

static void
nautilus_tree_change_queue_initialize (gpointer object, 
				       gpointer klass)
{
	NautilusTreeChangeQueue *change_queue;
	
	change_queue = NAUTILUS_TREE_CHANGE_QUEUE (object);

	change_queue->details = g_new0 (NautilusTreeChangeQueueDetails, 1);
}

static void       
nautilus_tree_change_queue_destroy (GtkObject *object)
{
	NautilusTreeChangeQueue *queue;
	
	queue = (NautilusTreeChangeQueue *) object;
        eel_g_slist_free_deep_custom (queue->details->head,
					   (GFunc) nautilus_tree_change_free,
					   NULL);
	g_free (queue->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}


NautilusTreeChangeQueue *
nautilus_tree_change_queue_new (void)
{
	NautilusTreeChangeQueue *change_queue;

	change_queue = NAUTILUS_TREE_CHANGE_QUEUE (gtk_object_new (NAUTILUS_TYPE_TREE_CHANGE_QUEUE, NULL));
	gtk_object_ref (GTK_OBJECT (change_queue));
	gtk_object_sink (GTK_OBJECT (change_queue));
	return change_queue;
}

void
nautilus_tree_change_queue_enqueue (NautilusTreeChangeQueue *queue,
				    NautilusTreeChangeType   change_type,
				    NautilusTreeNode        *node)
{
	NautilusTreeChange *change;

	change = nautilus_tree_change_new (change_type, node);

	if (queue->details->head == NULL) {
		queue->details->head = g_slist_prepend (NULL, change);
		queue->details->tail = queue->details->head;
	} else {
		g_slist_append (queue->details->tail, change);
		queue->details->tail = queue->details->tail->next;
	}
}

NautilusTreeChange *
nautilus_tree_change_queue_dequeue (NautilusTreeChangeQueue *queue)
{
	NautilusTreeChange *change;
	GSList *tmp_slist;

	if (queue->details->head == NULL) {
		return NULL;
	}

	change = (NautilusTreeChange *) queue->details->head->data;
	
	/* Remove head from list */
	tmp_slist = queue->details->head;
	queue->details->head = queue->details->head->next;
	tmp_slist->next = NULL;
	g_slist_free (tmp_slist);
	
	if (queue->details->head == NULL) {
		queue->details->tail = NULL;
	}

	return change;
}

static NautilusTreeChange *
nautilus_tree_change_new (NautilusTreeChangeType  change_type,
			  NautilusTreeNode       *node)
{
	NautilusTreeChange *change;

	change = g_new0 (NautilusTreeChange, 1);
	change->change_type = change_type;
	gtk_object_ref (GTK_OBJECT (node));
	change->node = node;

	return change;
}

void
nautilus_tree_change_free (NautilusTreeChange *change)
{
	gtk_object_unref (GTK_OBJECT (change->node));
	g_free (change);
}
