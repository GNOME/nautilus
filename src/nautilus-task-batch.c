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
 * along with Nautilus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-task-batch.h"

#include "nautilus-task.h"

struct _NautilusTaskBatch
{
    GObject parent_instance;

    GList *tasks;
};

G_DEFINE_TYPE (NautilusTaskBatch, nautilus_task_batch,
               NAUTILUS_TYPE_TASK)

static void
finalize (GObject *object)
{
    NautilusTaskBatch *self;

    self = NAUTILUS_TASK_BATCH (object);

    g_list_free_full (self->tasks, (GDestroyNotify) g_object_unref);
    self->tasks = NULL;

    G_OBJECT_CLASS (nautilus_task_batch_parent_class)->finalize (object);
}

static void
execute (NautilusTask *task)
{
    NautilusTaskBatch *self;

    self = NAUTILUS_TASK_BATCH (task);

    if (self->tasks == NULL)
    {
        return;
    }

    for (GList *i = g_list_reverse (self->tasks); i != NULL; i = i->next)
    {
        nautilus_task_execute (NAUTILUS_TASK (i->data));
    }
}

static void
nautilus_task_batch_class_init (NautilusTaskBatchClass *klass)
{
    GObjectClass *object_class;
    NautilusTaskClass *task_class;

    object_class = G_OBJECT_CLASS (klass);
    task_class = NAUTILUS_TASK_CLASS (klass);

    object_class->finalize = finalize;

    task_class->execute = execute;
}

static void
nautilus_task_batch_init (NautilusTaskBatch *self)
{
    self->tasks = NULL;
}

void
nautilus_task_batch_add_task (NautilusTaskBatch *batch,
                              NautilusTask      *task)
{
    g_return_if_fail (NAUTILUS_IS_TASK_BATCH (batch));

    batch->tasks = g_list_prepend (batch->tasks, g_object_ref (task));
}

NautilusTask *
nautilus_task_batch_new (GCancellable *cancellable)
{
    return g_object_new (NAUTILUS_TYPE_TASK_BATCH,
                         "cancellable", cancellable,
                         NULL);
}
