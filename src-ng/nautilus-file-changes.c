/* Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "nautilus-file-changes.h"

#include "nautilus-file.h"
#include "nautilus-signal-utilities.h"

typedef struct
{
    NautilusFileChange type;
    GFile *location;
} Change;

typedef struct
{
    NautilusFileChange type;
    GFile *location_from;
    GFile *location_to;
} MoveChange;

static void move_change_free (MoveChange *change);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MoveChange, move_change_free)

static guint source = 0;
static GMutex source_mutex;

static void
move_change_free (MoveChange *change)
{
    g_clear_object (&change->location_to);
    g_clear_object (&change->location_from);
    g_free (change);
}

static gpointer
init_default_queue (gpointer data)
{
    return g_async_queue_new ();
}

static GAsyncQueue *
get_default_queue (void)
{
    static GOnce once = G_ONCE_INIT;

    g_once (&once, init_default_queue, NULL);

    return once.retval;
}

static gboolean
emit_signals (gpointer user_data)
{
    GAsyncQueue *queue;
    Change *change;

    queue = user_data;

    g_async_queue_lock (queue);

    while ((change = g_async_queue_try_pop_unlocked (queue)) != NULL)
    {
        g_autoptr (NautilusFile) file = NULL;
        g_autoptr (NautilusFile) parent = NULL;

        file = nautilus_file_new (change->location);
        if (file == NULL)
        {
            continue;
        }
        parent = nautilus_file_get_parent (file);

        switch (change->type)
        {
            case NAUTILUS_FILE_CHANGE_RENAMED:
            {
                g_autoptr (MoveChange) move_change = NULL;

                move_change = (MoveChange *) change;

                nautilus_emit_signal_in_main_context_by_name (file,
                                                              NULL,
                                                              "renamed",
                                                              move_change->location_to);

                if (parent == NULL)
                {
                    break;
                }

                nautilus_emit_signal_in_main_context_by_name (parent,
                                                              NULL,
                                                              "children-changed");
            }
            break;
        }
    }

    g_async_queue_unlock (queue);

    g_mutex_lock (&source_mutex);
    source = 0;
    g_mutex_unlock (&source_mutex);

    return G_SOURCE_REMOVE;
}

static void
schedule_signal_emission (void)
{
    g_mutex_lock (&source_mutex);

    if (source == 0)
    {
        source = g_timeout_add (100, emit_signals, get_default_queue ());
    }
    else
    {
        g_source_remove (source);
    }

    g_mutex_unlock (&source_mutex);
}

static void
notify_file_moved_or_renamed (GFile    *from,
                              GFile    *to,
                              gboolean  move_is_rename)
{
    MoveChange *change;
    GAsyncQueue *queue;

    change = g_new0 (MoveChange, 1);
    queue = get_default_queue ();

    change->type = move_is_rename? NAUTILUS_FILE_CHANGE_RENAMED
                                 : NAUTILUS_FILE_CHANGE_MOVED;
    change->location_from = g_object_ref (from);
    change->location_to = g_object_ref (to);

    g_async_queue_push (queue, change);

    schedule_signal_emission ();
}

void
nautilus_notify_file_renamed (GFile *location,
                              GFile *new_location)
{
    notify_file_moved_or_renamed (location, new_location, TRUE);
}
