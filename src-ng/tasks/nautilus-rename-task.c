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

#include "nautilus-rename-task.h"

#include "nautilus-file-changes.h"
#include "nautilus-task-private.h"

#include <glib.h>

struct _NautilusRenameTask
{
    NautilusTask parent_instance;

    GHashTable *targets;
};

G_DEFINE_TYPE (NautilusRenameTask, nautilus_rename_task, NAUTILUS_TYPE_TASK)

enum
{
    FINISHED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
finalize (GObject *object)
{
    NautilusRenameTask *self;

    self = NAUTILUS_RENAME_TASK (object);

    g_hash_table_destroy (self->targets);

    G_OBJECT_CLASS (nautilus_rename_task_parent_class)->finalize (object);
}

static void
execute (NautilusTask *task)
{
    NautilusRenameTask *self;
    GHashTableIter iter;
    gpointer key;
    gpointer value;
    g_autoptr (GCancellable) cancellable = NULL;
    GError *error = NULL;

    self = NAUTILUS_RENAME_TASK (task);
    cancellable = nautilus_task_get_cancellable (task);

    g_hash_table_iter_init (&iter, self->targets);

    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        g_autoptr (GFile) location_from = NULL;
        g_autoptr (GFile) location_to = NULL;

        location_from = G_FILE (key);
        g_message ("Renaming GFile %p",
                   (gpointer) location_from);
        location_to = g_file_set_display_name (location_from,
                                               (const gchar *) value,
                                               cancellable, &error);

        if (location_to != NULL)
        {
            g_message ("GFile %p renamed to %p",
                       (gpointer) location_from,
                       (gpointer) location_to);
            nautilus_notify_file_renamed (location_from, location_to);
        }
        else
        {
            g_message ("Renaming GFile %p failed: %s",
                       (gpointer) location_from,
                       error->message);
        }
    }

    /* This will typically be handled before the file and its parent
     * are notified of the changes.
     */
    nautilus_task_emit_signal_in_main_context (task, signals[FINISHED], 0);
}

static void
nautilus_rename_task_class_init (NautilusRenameTaskClass *klass)
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
                                      g_cclosure_marshal_VOID__VOID,
                                      G_TYPE_NONE,
                                      0);
}

static void
nautilus_rename_task_init (NautilusRenameTask *self)
{
    self->targets = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                           g_object_unref, g_free);
}

void
nautilus_rename_task_add_target (NautilusRenameTask *task,
                                 GFile              *file,
                                 const gchar        *name)
{
    g_return_if_fail (NAUTILUS_IS_RENAME_TASK (task));
    g_return_if_fail (!g_hash_table_contains (task->targets, file));
    g_return_if_fail (G_IS_FILE (file));
    g_return_if_fail (name != NULL);

    (void) g_hash_table_insert (task->targets,
                                g_object_ref (file), g_strdup (name));
}

NautilusTask *
nautilus_rename_task_new (void)
{
    return g_object_new (NAUTILUS_TYPE_RENAME_TASK, NULL);
}
