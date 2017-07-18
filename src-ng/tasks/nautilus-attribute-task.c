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

#include "nautilus-attribute-task.h"

#include "nautilus-marshallers.h"
#include "nautilus-task-private.h"

struct _NautilusAttributeTask
{
    NautilusTask parent_instance;

    GFile *file;
    const char *attributes;
    GFileQueryInfoFlags flags;
} NautilusAttributeTaskPrivate;

G_DEFINE_TYPE (NautilusAttributeTask, nautilus_attribute_task,
               NAUTILUS_TYPE_TASK)

enum
{
    FINISHED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
finalize (GObject *object)
{
    NautilusAttributeTask *self;

    self = NAUTILUS_ATTRIBUTE_TASK (object);

    g_clear_object (&self->file);
    g_clear_pointer (&self->attributes, g_free);

    G_OBJECT_CLASS (nautilus_attribute_task_parent_class)->finalize (object);
}

static void
execute (NautilusTask *task)
{
    NautilusAttributeTask *self;
    g_autoptr (GCancellable) cancellable = NULL;
    GError *error = NULL;
    GFileInfo *info;

    self = NAUTILUS_ATTRIBUTE_TASK (task);
    cancellable = nautilus_task_get_cancellable (task);
    info = g_file_query_info (self->file,
                              self->attributes,
                              self->flags,
                              cancellable,
                              &error);

    nautilus_task_emit_signal_in_main_context (task, signals[FINISHED], 0,
                                               self->file, info, error);
}

static void
nautilus_attribute_task_class_init (NautilusAttributeTaskClass *klass)
{
    GObjectClass *object_class;
    NautilusTaskClass *task_class;

    object_class = G_OBJECT_CLASS (klass);
    task_class = NAUTILUS_TASK_CLASS (klass);

    object_class->finalize = finalize;

    task_class->execute = execute;

    signals[FINISHED] = g_signal_new ("finished",
                                      G_TYPE_FROM_CLASS (klass),
                                      G_SIGNAL_RUN_LAST,
                                      0, NULL, NULL,
                                      nautilus_cclosure_marshal_VOID__OBJECT_OBJECT_BOXED,
                                      G_TYPE_NONE,
                                      3,
                                      G_TYPE_FILE, G_TYPE_FILE_INFO, G_TYPE_ERROR);

}

static void
nautilus_attribute_task_init (NautilusAttributeTask *self)
{
}

NautilusTask *
nautilus_attribute_task_new (GFile               *file,
                             const char          *attributes,
                             GFileQueryInfoFlags  flags,
                             GCancellable        *cancellable)
{
    NautilusAttributeTask *instance;

    g_return_val_if_fail (G_IS_FILE (file), NULL);
    g_return_val_if_fail (attributes != NULL, NULL);

    instance = g_object_new (NAUTILUS_TYPE_ATTRIBUTE_TASK,
                             "cancellable", cancellable,
                             NULL);

    instance->file = g_object_ref (file);
    instance->attributes = g_strdup (attributes);
    instance->flags = flags;

    return NAUTILUS_TASK (instance);
}
