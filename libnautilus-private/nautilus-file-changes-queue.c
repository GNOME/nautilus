/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
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

   Author: Pavel Cisler <pavel@eazel.com>
*/

#include <config.h>
#include "nautilus-file-changes-queue.h"

#include "nautilus-directory-notify.h"
#include <eel/eel-glib-extensions.h>

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
	CHANGE_FILE_CHANGED,
	CHANGE_FILE_REMOVED,
	CHANGE_FILE_MOVED,
	CHANGE_METADATA_COPIED,
	CHANGE_METADATA_MOVED,
	CHANGE_METADATA_REMOVED,
	CHANGE_POSITION_SET,
	CHANGE_POSITION_REMOVE
} NautilusFileChangeKind;

typedef struct {
	NautilusFileChangeKind kind;
	char *from_uri;
	char *to_uri;
	GdkPoint point;
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
nautilus_file_changes_queue_file_changed (const char *uri)
{
	NautilusFileChange *new_item;
	NautilusFileChangesQueue *queue;

	queue = nautilus_file_changes_queue_get();

	new_item = g_new0 (NautilusFileChange, 1);
	new_item->kind = CHANGE_FILE_CHANGED;
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

void
nautilus_file_changes_queue_schedule_metadata_copy (const char *from_uri,
	const char *to_uri)
{
	NautilusFileChange *new_item;
	NautilusFileChangesQueue *queue;

	queue = nautilus_file_changes_queue_get ();

	new_item = g_new (NautilusFileChange, 1);
	new_item->kind = CHANGE_METADATA_COPIED;
	new_item->from_uri = g_strdup (from_uri);
	new_item->to_uri = g_strdup (to_uri);
	nautilus_file_changes_queue_add_common (queue, new_item);
}

void
nautilus_file_changes_queue_schedule_metadata_move (const char *from_uri,
	const char *to_uri)
{
	NautilusFileChange *new_item;
	NautilusFileChangesQueue *queue;

	queue = nautilus_file_changes_queue_get ();

	new_item = g_new (NautilusFileChange, 1);
	new_item->kind = CHANGE_METADATA_MOVED;
	new_item->from_uri = g_strdup (from_uri);
	new_item->to_uri = g_strdup (to_uri);
	nautilus_file_changes_queue_add_common (queue, new_item);
}

void
nautilus_file_changes_queue_schedule_metadata_remove (const char *uri)
{
	NautilusFileChange *new_item;
	NautilusFileChangesQueue *queue;

	queue = nautilus_file_changes_queue_get ();

	new_item = g_new (NautilusFileChange, 1);
	new_item->kind = CHANGE_METADATA_REMOVED;
	new_item->from_uri = g_strdup (uri);
	nautilus_file_changes_queue_add_common (queue, new_item);
}

void
nautilus_file_changes_queue_schedule_position_set (const char *uri, 
						   GdkPoint point)
{
	NautilusFileChange *new_item;
	NautilusFileChangesQueue *queue;

	queue = nautilus_file_changes_queue_get ();

	new_item = g_new (NautilusFileChange, 1);
	new_item->kind = CHANGE_POSITION_SET;
	new_item->from_uri = g_strdup (uri);
	new_item->point = point;
	nautilus_file_changes_queue_add_common (queue, new_item);
}

void
nautilus_file_changes_queue_schedule_position_remove (const char *uri)
{
	NautilusFileChange *new_item;
	NautilusFileChangesQueue *queue;

	queue = nautilus_file_changes_queue_get ();

	new_item = g_new (NautilusFileChange, 1);
	new_item->kind = CHANGE_POSITION_REMOVE;
	new_item->from_uri = g_strdup (uri);
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
		g_list_free_1 (queue->tail);
		queue->tail = new_tail;
	}

	MUTEX_UNLOCK (queue->mutex);

	return result;
}

enum {
	CONSUME_CHANGES_MAX_CHUNK = 20
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
	eel_g_list_free_deep (pairs);
}

static void
position_set_list_free (GList *list)
{
	GList *p;
	NautilusFileChangesQueuePosition *item;

	for (p = list; p != NULL; p = p->next) {
		item = p->data;
		g_free (item->uri);
	}
	/* delete the list and the now empty structs */
	eel_g_list_free_deep (list);
}

/* go through changes in the change queue, send ones with the same kind
 * in a list to the different nautilus_directory_notify calls
 */ 
void
nautilus_file_changes_consume_changes (gboolean consume_all)
{
	NautilusFileChange *change;
	GList *additions, *changes, *deletions, *moves;
	GList *metadata_copy_requests, *metadata_move_requests, *metadata_remove_requests;
	GList *position_set_requests;
	URIPair *pair;
	NautilusFileChangesQueuePosition *position_set;
	guint chunk_count;
	NautilusFileChangesQueue *queue;
	gboolean flush_needed;
	

	additions = NULL;
	changes = NULL;
	deletions = NULL;
	moves = NULL;
	metadata_copy_requests = NULL;
	metadata_move_requests = NULL;
	metadata_remove_requests = NULL;
	position_set_requests = NULL;

	queue = nautilus_file_changes_queue_get();
		
	/* Consume changes from the queue, stuffing them into one of three lists,
	 * keep doing it while the changes are of the same kind, then send them off.
	 * This is to ensure that the changes get sent off in the same order that they 
	 * arrived.
	 */
	for (chunk_count = 0; ; chunk_count++) {
		change = nautilus_file_changes_queue_get_change (queue);

		/* figure out if we need to flush the pending changes that we collected sofar */

		if (change == NULL) {
			flush_needed = TRUE;
			/* no changes left, flush everything */
		} else {
			flush_needed = additions != NULL
				&& change->kind != CHANGE_FILE_ADDED
				&& change->kind != CHANGE_METADATA_COPIED
				&& change->kind != CHANGE_POSITION_SET
				&& change->kind != CHANGE_POSITION_REMOVE;
			
			flush_needed |= changes != NULL
				&& change->kind != CHANGE_FILE_CHANGED;
			
			flush_needed |= moves != NULL
				&& change->kind != CHANGE_FILE_MOVED
				&& change->kind != CHANGE_METADATA_MOVED
				&& change->kind != CHANGE_POSITION_SET
				&& change->kind != CHANGE_POSITION_REMOVE;
			
			flush_needed |= deletions != NULL
				&& change->kind != CHANGE_FILE_REMOVED
				&& change->kind != CHANGE_METADATA_REMOVED;
			
			flush_needed |= metadata_copy_requests != NULL
				&& change->kind != CHANGE_FILE_ADDED
				&& change->kind != CHANGE_METADATA_COPIED
				&& change->kind != CHANGE_POSITION_SET
				&& change->kind != CHANGE_POSITION_REMOVE;
			
			flush_needed |= metadata_move_requests != NULL
				&& change->kind != CHANGE_FILE_MOVED
				&& change->kind != CHANGE_METADATA_MOVED
				&& change->kind != CHANGE_POSITION_SET
				&& change->kind != CHANGE_POSITION_REMOVE;
			
			flush_needed |= metadata_remove_requests != NULL
				&& change->kind != CHANGE_FILE_REMOVED
				&& change->kind != CHANGE_METADATA_REMOVED;
	
			flush_needed |= position_set_requests != NULL
				&& change->kind != CHANGE_POSITION_SET
				&& change->kind != CHANGE_POSITION_REMOVE
				&& change->kind != CHANGE_FILE_ADDED
				&& change->kind != CHANGE_FILE_MOVED
				&& change->kind != CHANGE_METADATA_COPIED
				&& change->kind != CHANGE_METADATA_MOVED;
			
			flush_needed |= !consume_all && chunk_count >= CONSUME_CHANGES_MAX_CHUNK;
				/* we have reached the chunk maximum */
		}
		
		if (flush_needed) {
			/* Send changes we collected off. 
			 * At one time we may only have one of the lists
			 * contain changes.
			 */
			
			if (deletions != NULL) {
				deletions = g_list_reverse (deletions);
				nautilus_directory_notify_files_removed (deletions);
				eel_g_list_free_deep (deletions);
				deletions = NULL;
			}
			if (moves != NULL) {
				moves = g_list_reverse (moves);
				nautilus_directory_notify_files_moved (moves);
				pairs_list_free (moves);
				moves = NULL;
			}
			if (additions != NULL) {
				additions = g_list_reverse (additions);
				nautilus_directory_notify_files_added (additions);
				eel_g_list_free_deep (additions);
				additions = NULL;
			}
			if (changes != NULL) {
				changes = g_list_reverse (changes);
				nautilus_directory_notify_files_changed (changes);
				eel_g_list_free_deep (changes);
				changes = NULL;
			}
			if (metadata_copy_requests != NULL) {
				metadata_copy_requests = g_list_reverse (metadata_copy_requests);
				nautilus_directory_schedule_metadata_copy (metadata_copy_requests);
				pairs_list_free (metadata_copy_requests);
				metadata_copy_requests = NULL;
			}
			if (metadata_move_requests != NULL) {
				metadata_move_requests = g_list_reverse (metadata_move_requests);
				nautilus_directory_schedule_metadata_move (metadata_move_requests);
				pairs_list_free (metadata_move_requests);
				metadata_move_requests = NULL;
			}
			if (metadata_remove_requests != NULL) {
				metadata_remove_requests = g_list_reverse (metadata_remove_requests);
				nautilus_directory_schedule_metadata_remove (metadata_remove_requests);
				eel_g_list_free_deep (metadata_remove_requests);
				metadata_remove_requests = NULL;
			}
			if (position_set_requests != NULL) {
				position_set_requests = g_list_reverse (position_set_requests);
				nautilus_directory_schedule_position_set (position_set_requests);
				position_set_list_free (position_set_requests);
				position_set_requests = NULL;
			}
		}

		if (change == NULL) {
			/* we are done */
			return;
		}
		
		/* add the new change to the list */
		switch (change->kind) {
		case CHANGE_FILE_ADDED:
			additions = g_list_prepend (additions, change->from_uri);
			break;

		case CHANGE_FILE_CHANGED:
			changes = g_list_prepend (changes, change->from_uri);
			break;

		case CHANGE_FILE_REMOVED:
			deletions = g_list_prepend (deletions, change->from_uri);
			break;

		case CHANGE_FILE_MOVED:
			pair = g_new (URIPair, 1);
			pair->from_uri = change->from_uri;
			pair->to_uri = change->to_uri;
			moves = g_list_prepend (moves, pair);
			break;

		case CHANGE_METADATA_COPIED:
			pair = g_new (URIPair, 1);
			pair->from_uri = change->from_uri;
			pair->to_uri = change->to_uri;
			metadata_copy_requests = g_list_prepend (metadata_copy_requests, pair);
			break;

		case CHANGE_METADATA_MOVED:
			pair = g_new (URIPair, 1);
			pair->from_uri = change->from_uri;
			pair->to_uri = change->to_uri;
			metadata_move_requests = g_list_prepend (metadata_move_requests, pair);
			break;

		case CHANGE_METADATA_REMOVED:
			metadata_remove_requests = g_list_prepend (metadata_remove_requests, 
				change->from_uri);
			break;

		case CHANGE_POSITION_SET:
			position_set = g_new (NautilusFileChangesQueuePosition, 1);
			position_set->uri = change->from_uri;
			position_set->set = TRUE;
			position_set->point = change->point;
			position_set_requests = g_list_prepend (position_set_requests,
								position_set);
			break;

		case CHANGE_POSITION_REMOVE:
			position_set = g_new (NautilusFileChangesQueuePosition, 1);
			position_set->uri = change->from_uri;
			position_set->set = FALSE;
			position_set_requests = g_list_prepend (position_set_requests,
								position_set);
			break;

		default:
			g_assert_not_reached ();
			break;
		}

		g_free (change);
	}	
}
