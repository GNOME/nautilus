/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 2001 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Darin Adler <darin@bentspoon.com>
 *
 */

#include <config.h>
#include "nautilus-idle-queue.h"

#include <gtk/gtkmain.h>

struct NautilusIdleQueue {
	GList *functions;
	guint idle_id;
	gboolean in_idle;
	gboolean destroy;
};

typedef struct {
	GFunc callback;
	gpointer data;
	gpointer callback_data;
	GFreeFunc free_callback_data;
} QueuedFunction;

static gboolean
execute_queued_functions (gpointer callback_data)
{
	NautilusIdleQueue *queue;
	GList *functions, *node;
	QueuedFunction *function;

	queue = callback_data;

	/* We could receive more incoming functions while dispatching
	 * these, so keep going until the queue is empty.
	 */
	queue->in_idle = TRUE;
	while (queue->functions != NULL) {
		functions = g_list_reverse (queue->functions);
		queue->functions = NULL;

		for (node = functions; node != NULL; node = node->next) {
			function = node->data;

			if (!queue->destroy) {
				(* function->callback) (function->data, function->callback_data);
			}
			if (function->free_callback_data != NULL) {
				(* function->free_callback_data) (function->callback_data);
			}

			g_free (function);
		}

		g_list_free (functions);
	}
	queue->in_idle = FALSE;

	queue->idle_id = 0;

	if (queue->destroy) {
		nautilus_idle_queue_destroy (queue);
	}

	return FALSE;
}

NautilusIdleQueue *
nautilus_idle_queue_new (void)
{
	return g_new0 (NautilusIdleQueue, 1);
}

void
nautilus_idle_queue_add (NautilusIdleQueue *queue,
			 GFunc callback,
			 gpointer data,
			 gpointer callback_data,
			 GFreeFunc free_callback_data)
{
	QueuedFunction *function;

	function = g_new (QueuedFunction, 1);
	function->callback = callback;
	function->data = data;
	function->callback_data = callback_data;
	function->free_callback_data = free_callback_data;

	queue->functions = g_list_prepend (queue->functions, function);

	if (queue->idle_id == 0) {
		queue->idle_id = g_idle_add (execute_queued_functions, queue);
	}
}

void
nautilus_idle_queue_destroy (NautilusIdleQueue *queue)
{
	GList *node;
	QueuedFunction *function;

	if (queue->in_idle) {
		queue->destroy = TRUE;
		return;
	}

	for (node = queue->functions; node != NULL; node = node->next) {
		function = node->data;
		
		if (function->free_callback_data != NULL) {
			(* function->free_callback_data) (function->callback_data);
		}

		g_free (function);
	}
	
	g_list_free (queue->functions);

	if (queue->idle_id != 0) {
		g_source_remove (queue->idle_id);
	}

	g_free (queue);
}
