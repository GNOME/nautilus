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

#include "nautilus-enumerate-children-task.h"

#include "nautilus-marshallers.h"
#include "nautilus-task-private.h"

struct _NautilusEnumerateChildrenTask
{
    NautilusTask parent_instance;

    GFile *file;
    const char *attributes;
    GFileQueryInfoFlags flags;
};

G_DEFINE_TYPE (NautilusEnumerateChildrenTask, nautilus_enumerate_children_task,
               NAUTILUS_TYPE_TASK)

enum
{
    FINISHED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
execute (NautilusTask *task)
{
    NautilusEnumerateChildrenTask *self;
    g_autoptr (GCancellable) cancellable = NULL;
    GError *error = NULL;
    g_autoptr (GFileEnumerator) enumerator = NULL;
    GHashTable *hash_table;
    GFileInfo *info;

    self = NAUTILUS_ENUMERATE_CHILDREN_TASK (task);
    cancellable = nautilus_task_get_cancellable (NAUTILUS_TASK (self));
    enumerator = g_file_enumerate_children (self->file, self->attributes,
                                            self->flags, cancellable, &error);

    if (error != NULL)
    {
        nautilus_task_emit_signal_in_main_context (task,
                                                   signals[FINISHED], 0,
                                                   self->file, NULL, error);
    }

    hash_table = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                        g_object_unref, g_object_unref);

    do
    {
        GFile *child;

        info = g_file_enumerator_next_file (enumerator, cancellable, &error);

        if (error != NULL)
        {
            g_hash_table_destroy (hash_table);

            nautilus_task_emit_signal_in_main_context (task,
                                                       signals[FINISHED], 0,
                                                       self->file, NULL, error);

            return;
        }

        if (info != NULL)
        {
            child = g_file_enumerator_get_child (enumerator, info);

            g_assert (g_hash_table_insert (hash_table, child, info));
        }
    } while (info != NULL);

    nautilus_task_emit_signal_in_main_context (task, signals[FINISHED], 0,
                                               self->file, hash_table, error);
}

static void
nautilus_enumerate_children_task_class_init (NautilusEnumerateChildrenTaskClass *klass)
{
    NautilusTaskClass *task_class;

    task_class = NAUTILUS_TASK_CLASS (klass);

    task_class->execute = execute;

    signals[FINISHED] = g_signal_new ("finished",
                                      G_TYPE_FROM_CLASS (klass),
                                      G_SIGNAL_RUN_LAST,
                                      0, NULL, NULL,
                                      nautilus_cclosure_marshal_VOID__OBJECT_BOXED_BOXED,
                                      G_TYPE_NONE,
                                      3,
                                      G_TYPE_FILE, G_TYPE_HASH_TABLE, G_TYPE_ERROR);
}

static void
nautilus_enumerate_children_task_init (NautilusEnumerateChildrenTask *self)
{
}

NautilusTask *
nautilus_enumerate_children_task_new (GFile               *file,
                                      const char          *attributes,
                                      GFileQueryInfoFlags  flags,
                                      GCancellable        *cancellable)
{
    NautilusEnumerateChildrenTask *instance;

    g_return_val_if_fail (G_IS_FILE (file), NULL);
    g_return_val_if_fail (attributes != NULL, NULL);

    instance = g_object_new (NAUTILUS_TYPE_ENUMERATE_CHILDREN_TASK,
                         "cancellable", cancellable,
                         NULL);

    instance->file = g_object_ref (file);
    instance->attributes = g_strdup (attributes);
    instance->flags = flags;

    return NAUTILUS_TASK (instance);
}
