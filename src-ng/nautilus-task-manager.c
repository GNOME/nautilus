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

#include "nautilus-task-manager.h"

struct _NautilusTaskManager
{
    GObject parent_instance;

    GThreadPool *thread_pool;
};

G_DEFINE_TYPE (NautilusTaskManager, nautilus_task_manager, G_TYPE_OBJECT)

static NautilusTaskManager *instance = NULL;

static GObject *
constructor (GType                  type,
             guint                  n_construct_properties,
             GObjectConstructParam *construct_properties)
{
    static GMutex mutex;
    GObjectClass *parent_class;

    g_mutex_lock (&mutex);

    if (instance != NULL)
    {
        g_mutex_unlock (&mutex);
        return g_object_ref (instance);
    }

    parent_class = G_OBJECT_CLASS (nautilus_task_manager_parent_class);
    instance = NAUTILUS_TASK_MANAGER (parent_class->constructor (type,
                                                                 n_construct_properties,
                                                                 construct_properties));

    g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *) &instance);

    g_mutex_unlock (&mutex);

    return G_OBJECT (instance);
}

static void
finalize (GObject *object)
{
    NautilusTaskManager *self;

    self = NAUTILUS_TASK_MANAGER (object);

    g_thread_pool_free (self->thread_pool, TRUE, TRUE);

    G_OBJECT_CLASS (nautilus_task_manager_parent_class)->finalize (object);
}

static void
nautilus_task_manager_class_init (NautilusTaskManagerClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->constructor = constructor;
    object_class->finalize = finalize;
}

static void
execute_task (gpointer data,
              gpointer user_data)
{
    g_autoptr (NautilusTask) task = NULL;

    task = NAUTILUS_TASK (data);

    nautilus_task_execute (task);
}

static void
nautilus_task_manager_init (NautilusTaskManager *self)
{
    self->thread_pool = g_thread_pool_new (execute_task, self,
                                           16, FALSE,
                                           NULL);
}

void
nautilus_task_manager_queue_task (NautilusTaskManager  *self,
                                  NautilusTask         *task)
{
    g_return_if_fail (NAUTILUS_IS_TASK_MANAGER (self));
    g_return_if_fail (NAUTILUS_IS_TASK (task));

    g_thread_pool_push (self->thread_pool, g_object_ref (task), NULL);
}

NautilusTaskManager *
nautilus_task_manager_dup_singleton (void)
{
    return g_object_new (NAUTILUS_TYPE_TASK_MANAGER, NULL);
}

