/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 * 
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Rebecca Schulman <rebecka@eazel.com>
 *  
 */

/* nautilus-queue.c -- A simple FIFO structure */

#include <config.h>
#include <glib.h>
#include "nautilus-queue.h"
#include "nautilus-lib-self-check-functions.h"


struct _NautilusQueue {
        GSList *elements;
        GSList *tail;
}; 


/**
 * nautilus_queue_new:
 * 
 * Return value: returns a newly allocated %NautilusQueue.
 */
NautilusQueue *    
nautilus_queue_new (void)
{
        NautilusQueue *queue;

        queue = g_new0 (NautilusQueue, 1);
        queue->elements = NULL;
        queue->tail = NULL;
        
        return queue;
}

/**
 * nautilus_queue_add:
 * @queue: queue to add stuff into.
 * @element: data to add into the queue.
 *
 * adds @element to @queue.
 */
void             
nautilus_queue_add (NautilusQueue *queue,
                    gpointer element)
{
        g_return_if_fail (queue != NULL);
  
        if (queue->tail == NULL) {
                g_assert (queue->elements == NULL);
                queue->tail = g_slist_append (queue->tail,
                                              element);
                queue->elements = queue->tail;
        } else {
                queue->tail = g_slist_append (queue->tail,
                                              element);
                queue->tail = queue->tail->next;
        }
}

/**
 * nautilus_queue_remove:
 * @queue: The queue.
 *
 * remove the first element entered into the queue.
 *
 * Return value: the data of the element removed.
 */
gpointer         
nautilus_queue_remove (NautilusQueue *queue)
{
        gpointer result;
        GSList *old_cell;

        g_return_val_if_fail (queue != NULL, NULL);

        if (queue->elements == NULL) {
                return NULL;
        }

        result = queue->elements->data;

        old_cell = queue->elements;
        queue->elements = queue->elements->next;
        if (queue->elements == NULL) {
                g_assert (queue->tail == old_cell);
                queue->tail = NULL;
        }
        g_slist_free_1 (old_cell);
        return result;
}

/**
 * nautilus_queue_free_deep:
 * @queue: the queue to free.
 *
 * frees @queue and calls g_free on the data added to the queue.
 */
void
nautilus_queue_free_deep (NautilusQueue *queue)
{
        gpointer element;

        g_return_if_fail (queue != NULL);

        while (!nautilus_queue_is_empty (queue)) {
                element = nautilus_queue_remove (queue);
                g_free (element);
        }
        nautilus_queue_free (queue);
}


/**
 * nautilus_queue_is_empty:
 * @queue: the queue.
 *
 * Return value: TRUE is @queue is empty and FALSE otherwise.
 */
gboolean 
nautilus_queue_is_empty (NautilusQueue *queue)
{
        g_return_val_if_fail (queue != NULL, TRUE);

        return queue->elements == NULL;
}

/**
 * nautilus_queue_free:
 * @queue: the queue.
 *
 * frees @queue.
 */
void
nautilus_queue_free (NautilusQueue *queue)
{
        g_return_if_fail (queue != NULL);

        /* As with glist, we can't be responsible for 
           freeing the queue elements */
        g_slist_free (queue->elements);
        /* No need to free the tail, it is a 
           pointer within the elements list */
        g_free (queue);
}



#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void nautilus_self_check_queue (void)
{
        char *some_elements[] = { "one", "two", "three" };
        char *removed_element;
        NautilusQueue *queue;

        queue = nautilus_queue_new ();
        NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_queue_is_empty (queue), TRUE);

        nautilus_queue_add (queue, some_elements[0]);
        NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_queue_is_empty (queue), FALSE);

        removed_element = nautilus_queue_remove (queue);
        NAUTILUS_CHECK_BOOLEAN_RESULT (removed_element == some_elements[0], TRUE);
        NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_queue_is_empty (queue), TRUE);

        nautilus_queue_add (queue, some_elements[0]);
        nautilus_queue_add (queue, some_elements[1]);
        nautilus_queue_add (queue, some_elements[2]);
        NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_queue_remove (queue) == some_elements[0], TRUE);
        NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_queue_remove (queue) == some_elements[1], TRUE);
        NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_queue_remove (queue) == some_elements[2], TRUE);
        NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_queue_is_empty (queue), TRUE);

        nautilus_queue_free (queue);
  
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */

