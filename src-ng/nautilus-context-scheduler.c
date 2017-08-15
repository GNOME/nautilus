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

#include "nautilus-context-scheduler.h"

#include "nautilus-scheduler-private.h"

struct _NautilusContextScheduler
{
    NautilusScheduler parent_instance;

    GMainContext *context;
    guint source_id;

    GQueue *queue;
};

G_DEFINE_TYPE (NautilusContextScheduler, nautilus_context_scheduler,
               NAUTILUS_TYPE_SCHEDULER)

typedef struct
{
    GSource parent_instance;

    NautilusContextScheduler *scheduler;
} NautilusSource;

G_LOCK_DEFINE_STATIC (table_mutex);
static GHashTable *scheduler_table = NULL;

static void
queue (NautilusScheduler *scheduler,
       NautilusCallback   func,
       gpointer           func_data)
{
    NautilusContextScheduler *context_scheduler;
    NautilusThreadWork *work;

    context_scheduler = NAUTILUS_CONTEXT_SCHEDULER (scheduler);
    work = nautilus_thread_work_new (func, func_data);

    g_queue_push_tail (context_scheduler->queue, work);
}

static void
nautilus_context_scheduler_class_init (NautilusContextSchedulerClass *klass)
{
    NautilusSchedulerClass *scheduler_class;

    scheduler_class = NAUTILUS_SCHEDULER_CLASS (klass);

    scheduler_class->queue = queue;
}

static gboolean
source_prepare (GSource *source,
                gint    *timeout)
{
    NautilusSource *nautilus_source;

    nautilus_source = (NautilusSource *) source;

    *timeout = -1;

    return !g_queue_is_empty (nautilus_source->scheduler->queue);
}

static gboolean
source_check (GSource *source)
{
    NautilusSource *nautilus_source;

    nautilus_source = (NautilusSource *) source;

    return !g_queue_is_empty (nautilus_source->scheduler->queue);
}

static gboolean
source_dispatch (GSource     *source,
                 GSourceFunc  callback,
                 gpointer     user_data)
{
    return callback (user_data);
}

static GSourceFuncs source_funcs =
{
    source_prepare,
    source_check,
    source_dispatch,
    NULL
};

static gboolean
source_callback (gpointer data)
{
    NautilusContextScheduler *scheduler;
    NautilusThreadWork *work;

    scheduler = data;

    while ((work = g_queue_pop_head (scheduler->queue)) != NULL)
    {
        nautilus_thread_work_run (work);
    }

    return G_SOURCE_CONTINUE;
}

static void
nautilus_context_scheduler_init (NautilusContextScheduler *self)
{
    g_autoptr (GSource) source = NULL;

    source = g_source_new (&source_funcs, sizeof (NautilusSource));

    g_source_set_priority (source, G_PRIORITY_DEFAULT_IDLE);
    g_source_set_callback (source, source_callback, self, NULL);

    self->source_id = g_source_attach (source, self->context);
}

NautilusScheduler *
nautilus_context_scheduler_get_for_context (GMainContext *context)
{
    NautilusContextScheduler *scheduler;

    g_return_val_if_fail (context != NULL, NULL);

    G_LOCK (table_mutex);

    if (scheduler_table == NULL)
    {
        scheduler_table = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                 NULL, g_object_unref);
    }

    scheduler = g_hash_table_lookup (scheduler_table, context);
    if (scheduler == NULL)
    {
        scheduler = g_object_new (NAUTILUS_TYPE_CONTEXT_SCHEDULER, NULL);

        scheduler->context = context;

        g_hash_table_insert (scheduler_table, context, scheduler);
    }

    G_UNLOCK (table_mutex);

    return NAUTILUS_SCHEDULER (scheduler);
}
