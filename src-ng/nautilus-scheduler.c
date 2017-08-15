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

#include "nautilus-scheduler-private.h"

typedef struct
{
    GThreadPool *thread_pool;
} NautilusSchedulerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NautilusScheduler, nautilus_scheduler, G_TYPE_OBJECT)

struct NautilusThreadWork
{
    NautilusCallback func;
    gpointer func_data;
};

static void
finalize (GObject *object)
{
    NautilusScheduler *scheduler;
    NautilusSchedulerPrivate *priv;

    scheduler = NAUTILUS_SCHEDULER (object);
    priv = nautilus_scheduler_get_instance_private (scheduler);

    g_thread_pool_free (priv->thread_pool, TRUE, TRUE);

    G_OBJECT_CLASS (nautilus_scheduler_parent_class)->finalize (object);
}

static void
queue (NautilusScheduler *scheduler,
       NautilusCallback   func,
       gpointer           func_data)
{
    NautilusSchedulerPrivate *priv;
    NautilusThreadWork *work;

    priv = nautilus_scheduler_get_instance_private (scheduler);
    work = nautilus_thread_work_new (func, func_data);

    (void) g_thread_pool_push (priv->thread_pool, work, NULL);
}

static void
nautilus_scheduler_class_init (NautilusSchedulerClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = finalize;

    klass->queue = queue;
}

static void
nautilus_scheduler_init (NautilusScheduler *self)
{
}

void
nautilus_scheduler_queue (NautilusScheduler *scheduler,
                          NautilusCallback   func,
                          gpointer           func_data)
{
    g_return_if_fail (NAUTILUS_IS_SCHEDULER (scheduler));
    g_return_if_fail (func != NULL);

    NAUTILUS_SCHEDULER_GET_CLASS (scheduler)->queue (scheduler, func, func_data);
}

static void
thread_pool_func (gpointer data,
                  gpointer user_data)
{
    g_autoptr (NautilusThreadWork) work = NULL;

    work = data;

    nautilus_thread_work_run (work);
}

static NautilusScheduler *
nautilus_scheduler_new (gint max_threads)
{
    NautilusScheduler *scheduler;
    NautilusSchedulerPrivate *priv;

    scheduler = g_object_new (NAUTILUS_TYPE_SCHEDULER, NULL);
    priv = nautilus_scheduler_get_instance_private (scheduler);

    priv->thread_pool = g_thread_pool_new (thread_pool_func, NULL, max_threads, FALSE, NULL);

    return scheduler;
}

static gpointer
create_default_instance (gpointer data)
{
    (void) data;

    return nautilus_scheduler_new (16);
}

NautilusScheduler *
nautilus_scheduler_get_default (void)
{
    static GOnce once = G_ONCE_INIT;

    g_once (&once, create_default_instance, NULL);

    return g_object_ref (once.retval);
}


void
nautilus_thread_work_run (NautilusThreadWork *work)
{
    work->func (work->func_data);
}

void
nautilus_thread_work_free (NautilusThreadWork *work)
{
    g_free (work);
}

NautilusThreadWork *
nautilus_thread_work_new (NautilusCallback callback,
                          gpointer         data)
{
    NautilusThreadWork *work;

    work = g_new0 (NautilusThreadWork, 1);

    work->func = callback;
    work->func_data = data;

    return work;
}
