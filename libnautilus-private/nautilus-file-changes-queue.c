/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory.h: Nautilus directory model.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
*/

#include <config.h>
#include "nautilus-file-changes-queue.h"

#include "nautilus-glib-extensions.h"
#include "nautilus-directory-private.h"

#ifdef G_THREADS_ENABLED
#define MUTEX_LOCK(a)	if ((a) != NULL) g_mutex_lock (a)
#define MUTEX_UNLOCK(a)	if ((a) != NULL) g_mutex_unlock (a)
#else
#define MUTEX_LOCK(a)
#define MUTEX_UNLOCK(a)
#endif

typedef enum {
	CHANGE_FILE_INITIAL,
	CHANGE_FILE_ADDED,
	CHANGE_FILE_REMOVED,
	CHANGE_FILE_MOVED
} NautilusFileChangeKind;

typedef struct {
	NautilusFileChangeKind kind;
	char *from_uri;
	char *to_uri;
} NautilusFileChange;

typedef struct {
	GList *head;
	GList *tail;
#ifdef G_THREADS_ENABLED
	GMutex *mutex;
#endif
} NautilusFileChangesQueue;

static NautilusFileChangesQueue *
nautilus_file_changes_queue_new (void)
{
	NautilusFileChangesQueue *result;

	result = g_new0 (NautilusFileChangesQueue, 1);
	
#ifdef G_THREADS_ENABLED
	result->mutex = g_mutex_new ();
#endif
	return result;
}

static NautilusFileChangesQueue *
nautilus_file_changes_queue_get (void)
{
	static NautilusFileChangesQueue *file_changes_queue;

	if (file_changes_queue == NULL) {
		file_changes_queue = nautilus_file_changes_queue_new ();
	}

	return file_changes_queue;
}

#if 0 /* no public free call yet */

static void
nautilus_file_change_free (NautilusFileChange *change)
{
	g_free (change->from_uri);
	g_free (change->to_uri);
}


void
nautilus_file_changes_queue_free (NautilusFileChangesQueue *queue)
{
	GList *p;
	if (queue == NULL) {
		return;
	}
	
#ifdef G_THREADS_ENABLED
	/* if lock on a defunct mutex were defined (returning a failure)
	 * we would lock here 
	 */
#endif

	for (p = queue->head; p != NULL; p = p->next) {
		nautilus_file_change_free (p->data);
	}
	g_list_free (queue->head);

#ifdef G_THREADS_ENABLED
	g_mutex_free (queue->mutex);
#endif
	g_free (queue);
}

#endif /* no public free call yet */

static void
nautilus_file_changes_queue_add_common (NautilusFileChangesQueue *queue, 
	NautilusFileChange *new_item)
{
	/* enqueue the new queue item while locking down the list */
	MUTEX_LOCK (queue->mutex);

	queue->head = g_list_prepend (queue->head, new_item);
	if (queue->tail == NULL)
		queue->tail = queue->head;

	MUTEX_UNLOCK (queue->mutex);
}

void
nautilus_file_changes_queue_file_added (const char *uri)
{
	NautilusFileChange *new_item;
	NautilusFileChangesQueue *queue;

	queue = nautilus_file_changes_queue_get();

	new_item = g_new0 (NautilusFileChange, 1);
	new_item->kind = CHANGE_FILE_ADDED;
	new_item->from_uri = g_strdup (uri);
	nautilus_file_changes_queue_add_common (queue, new_item);
}

void
nautilus_file_changes_queue_file_removed (const char *uri)
{
	NautilusFileChange *new_item;
	NautilusFileChangesQueue *queue;

	queue = nautilus_file_changes_queue_get();

	new_item = g_new0 (NautilusFileChange, 1);
	new_item->kind = CHANGE_FILE_REMOVED;
	new_item->from_uri = g_strdup (uri);
	nautilus_file_changes_queue_add_common (queue, new_item);
}

void
nautilus_file_changes_queue_file_moved (const char *from, const char *to)
{
	NautilusFileChange *new_item;
	NautilusFileChangesQueue *queue;

	queue = nautilus_file_changes_queue_get ();

	new_item = g_new (NautilusFileChange, 1);
	new_item->kind = CHANGE_FILE_MOVED;
	new_item->from_uri = g_strdup (from);
	new_item->to_uri = g_strdup (to);
	nautilus_file_changes_queue_add_common (queue, new_item);
}

static NautilusFileChange *
nautilus_file_changes_queue_get_change (NautilusFileChangesQueue *queue)
{
	GList *new_tail;
	NautilusFileChange *result;

	g_assert (queue != NULL);
	
	/* dequeue the tail item while locking down the list */
	MUTEX_LOCK (queue->mutex);

	if (queue->tail == NULL) {
		result = NULL;
	} else {
		new_tail = queue->tail->prev;
		result = queue->tail->data;
		queue->head = g_list_remove_link (queue->head,
						  queue->tail);
		queue->tail = new_tail;
	}

	MUTEX_UNLOCK (queue->mutex);

	return result;
}

enum {
	CONSUME_CHANGES_MAX_CHUNK = 10
};

static void
pairs_list_free (GList *pairs)
{
	GList *p;
	URIPair *pair;

	/* deep delete the list of pairs */

	for (p = pairs; p != NULL; p = p->next) {
		/* delete the strings in each pair */
		pair = p->data;
		g_free (pair->from_uri);
		g_free (pair->to_uri);
	}

	/* delete the list and the now empty pair structs */
	nautilus_g_list_free_deep (pairs);
}

/* go through changes in the change queue, send ones with the same kind
 * in a list to the different nautilus_directory_notify calls
 */ 
void
nautilus_file_changes_consume_changes (gboolean consume_all)
{
	NautilusFileChange *change;
	GList *additions;
	GList *deletions;
	GList *moves;
	URIPair *pair;
	int kind;
	int chunk_count;
	NautilusFileChangesQueue *queue;


	additions = NULL;
	deletions = NULL;
	moves = NULL;
	kind = CHANGE_FILE_INITIAL;

	queue = nautilus_file_changes_queue_get();
		
	/* Consume changes from the queue, stuffing them into one of three lists,
	 * keep doing it while the changes are of the same kind, then send them off.
	 * This is to ensure that the changes get sent off in the same order that they 
	 * arrived.
	 */
	for (chunk_count = 0; ; chunk_count++) {
		change = nautilus_file_changes_queue_get_change (queue);

		if (change == NULL
			/* no changes left */
			|| change->kind != kind
			/* all the changes we have are different that the new one */
			|| (!consume_all && chunk_count >= CONSUME_CHANGES_MAX_CHUNK)) {
			/* we have reached the chunk maximum */

			/* Send changes we collected off. 
			 * At one time we may only have one of the three lists
			 * contain changes.
			 */

			g_assert ((deletions != NULL) + (moves != NULL) 
				+ (additions != NULL) <= 1);
				
			if (deletions != NULL) {
				nautilus_directory_notify_files_removed (deletions);
				nautilus_g_list_free_deep (deletions);
				deletions = NULL;
			}
			if (moves != NULL) {
				nautilus_directory_notify_files_moved (moves);
				pairs_list_free (moves);
				moves = NULL;
			}
			if (additions != NULL) {
				nautilus_directory_notify_files_added (additions);
				nautilus_g_list_free_deep (additions);
				additions = NULL;
			}
		}

		if (change == NULL) {
			/* we are done */
			return;
		}
		
		kind = change->kind;

		/* add the new change to the list */
		switch (kind) {
		case CHANGE_FILE_ADDED:
			additions = g_list_append (additions, change->from_uri);
			break;

		case CHANGE_FILE_REMOVED:
			deletions = g_list_append (deletions, change->from_uri);
			break;

		case CHANGE_FILE_MOVED:
			pair = g_new (URIPair, 1);
			pair->from_uri = change->from_uri;
			pair->to_uri = change->to_uri;
			moves = g_list_append (moves, pair);
			break;

		default:
			g_assert_not_reached ();
			break;
		}
		change->from_uri = NULL;
		change->to_uri = NULL;
	}	

}
