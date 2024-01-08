/*
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
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
 *  Author: Pavel Cisler <pavel@eazel.com>
 */

#include <config.h>
#include "nautilus-file-changes-queue.h"

#include "nautilus-directory-notify.h"
#include "nautilus-tag-manager.h"

typedef enum
{
    CHANGE_FILE_INITIAL,
    CHANGE_FILE_ADDED,
    CHANGE_FILE_CHANGED,
    CHANGE_FILE_UNMOUNTED,
    CHANGE_FILE_REMOVED,
    CHANGE_FILE_MOVED,
} NautilusFileChangeKind;

typedef struct
{
    NautilusFileChangeKind kind;
    GFile *from;
    GFile *to;
} NautilusFileChange;

static GAsyncQueue *
nautilus_file_changes_queue_get (void)
{
    static GAsyncQueue *file_changes_queue;
    static gsize init_value = 0;

    if (g_once_init_enter (&init_value))
    {
        file_changes_queue = g_async_queue_new ();
        g_once_init_leave (&init_value, 1);
    }

    return file_changes_queue;
}

void
nautilus_file_changes_queue_file_added (GFile *location)
{
    NautilusFileChange *new_item;
    GAsyncQueue *queue;

    queue = nautilus_file_changes_queue_get ();

    new_item = g_new0 (NautilusFileChange, 1);
    new_item->kind = CHANGE_FILE_ADDED;
    new_item->from = g_object_ref (location);
    g_async_queue_push (queue, new_item);
}

void
nautilus_file_changes_queue_file_changed (GFile *location)
{
    NautilusFileChange *new_item;
    GAsyncQueue *queue;

    queue = nautilus_file_changes_queue_get ();

    new_item = g_new0 (NautilusFileChange, 1);
    new_item->kind = CHANGE_FILE_CHANGED;
    new_item->from = g_object_ref (location);
    g_async_queue_push (queue, new_item);
}

/* A specialized variant of nautilus_file_changes_queue_file_removed(). */
void
nautilus_file_changes_queue_file_unmounted (GFile *location)
{
    NautilusFileChange *new_item;
    GAsyncQueue *queue;

    queue = nautilus_file_changes_queue_get ();

    new_item = g_new0 (NautilusFileChange, 1);
    new_item->kind = CHANGE_FILE_UNMOUNTED;
    new_item->from = g_object_ref (location);
    g_async_queue_push (queue, new_item);
}

void
nautilus_file_changes_queue_file_removed (GFile *location)
{
    NautilusFileChange *new_item;
    GAsyncQueue *queue;

    queue = nautilus_file_changes_queue_get ();

    new_item = g_new0 (NautilusFileChange, 1);
    new_item->kind = CHANGE_FILE_REMOVED;
    new_item->from = g_object_ref (location);
    g_async_queue_push (queue, new_item);
}

void
nautilus_file_changes_queue_file_moved (GFile *from,
                                        GFile *to)
{
    NautilusFileChange *new_item;
    GAsyncQueue *queue;

    queue = nautilus_file_changes_queue_get ();

    new_item = g_new (NautilusFileChange, 1);
    new_item->kind = CHANGE_FILE_MOVED;
    new_item->from = g_object_ref (from);
    new_item->to = g_object_ref (to);
    g_async_queue_push (queue, new_item);
}

static void
pairs_list_free (GList *pairs)
{
    GList *p;
    GFilePair *pair;

    /* deep delete the list of pairs */

    for (p = pairs; p != NULL; p = p->next)
    {
        /* delete the strings in each pair */
        pair = p->data;
        g_object_unref (pair->from);
        g_object_unref (pair->to);
    }

    /* delete the list and the now empty pair structs */
    g_list_free_full (pairs, g_free);
}

/* go through changes in the change queue, send ones with the same kind
 * in a list to the different nautilus_directory_notify calls
 */
void
nautilus_file_changes_consume_changes (void)
{
    NautilusFileChange *change;
    GList *additions, *changes, *deletions, *moves;
    GList *unmounts = NULL;
    GFilePair *pair;
    GAsyncQueue *queue;
    gboolean flush_needed;


    additions = NULL;
    changes = NULL;
    deletions = NULL;
    moves = NULL;

    queue = nautilus_file_changes_queue_get ();

    /* Consume changes from the queue, stuffing them into one of three lists,
     * keep doing it while the changes are of the same kind, then send them off.
     * This is to ensure that the changes get sent off in the same order that they
     * arrived.
     */
    for (;;)
    {
        change = g_async_queue_try_pop (queue);

        /* figure out if we need to flush the pending changes that we collected sofar */

        if (change == NULL)
        {
            flush_needed = TRUE;
            /* no changes left, flush everything */
        }
        else
        {
            flush_needed = additions != NULL
                           && change->kind != CHANGE_FILE_ADDED;

            flush_needed |= changes != NULL
                            && change->kind != CHANGE_FILE_CHANGED;

            flush_needed |= moves != NULL
                            && change->kind != CHANGE_FILE_MOVED;

            /* In some cases, GFileMonitor sends both DELETE and UNMOUNT events
             * for the same location, so we want to deal with both at the same
             * time. And even by itself, UNMOUNTED implies REMOVED anyway. */
            flush_needed |= deletions != NULL
                            && change->kind != CHANGE_FILE_UNMOUNTED
                            && change->kind != CHANGE_FILE_REMOVED;
        }

        if (flush_needed)
        {
            /* Send changes we collected off.
             * At one time we may only have one of the lists
             * contain changes.
             */

            if (deletions != NULL)
            {
                /* Mark unmounted files before notifying their removal, for
                 * clients to know this is why the file is gone. */
                nautilus_directory_mark_files_unmounted (unmounts);
                g_clear_list (&unmounts, g_object_unref);

                deletions = g_list_reverse (deletions);
                nautilus_directory_notify_files_removed (deletions);
                g_list_free_full (deletions, g_object_unref);
                deletions = NULL;
            }
            if (moves != NULL)
            {
                moves = g_list_reverse (moves);
                nautilus_directory_notify_files_moved (moves);
                pairs_list_free (moves);
                moves = NULL;
            }
            if (additions != NULL)
            {
                additions = g_list_reverse (additions);
                nautilus_directory_notify_files_added (additions);
                g_list_free_full (additions, g_object_unref);
                additions = NULL;
            }
            if (changes != NULL)
            {
                changes = g_list_reverse (changes);
                nautilus_directory_notify_files_changed (changes);
                g_list_free_full (changes, g_object_unref);
                changes = NULL;
            }
        }

        if (change == NULL)
        {
            /* we are done */
            return;
        }

        /* add the new change to the list */
        switch (change->kind)
        {
            case CHANGE_FILE_ADDED:
            {
                additions = g_list_prepend (additions, change->from);
            }
            break;

            case CHANGE_FILE_CHANGED:
            {
                changes = g_list_prepend (changes, change->from);
            }
            break;

            case CHANGE_FILE_UNMOUNTED:
            {
                deletions = g_list_prepend (deletions, change->from);
                unmounts = g_list_prepend (unmounts, g_object_ref (change->from));
            }
            break;

            case CHANGE_FILE_REMOVED:
            {
                deletions = g_list_prepend (deletions, change->from);
            }
            break;

            case CHANGE_FILE_MOVED:
            {
                nautilus_tag_manager_update_moved_uris (nautilus_tag_manager_get (),
                                                        change->from,
                                                        change->to);

                pair = g_new (GFilePair, 1);
                pair->from = change->from;
                pair->to = change->to;
                moves = g_list_prepend (moves, pair);
            }
            break;

            default:
            {
                g_assert_not_reached ();
            }
            break;
        }

        g_free (change);
    }
}
