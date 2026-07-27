/* SPDX-Copyright-Text: 2026 The Files contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */

#include "nautilus-recursive-directory-iterator.h"

#include <gio/gio.h>
#include <glib.h>

#define NAUTILUS_TYPE_RECURSIVE_DIRECTORY_ITERATOR (nautilus_recursive_directory_iterator_get_type ())


typedef struct
{
    GQueue *pending_directories;
    GHashTable *visited;
    char *attributes;
    guint max_depth;
    gboolean local_only;

    GCancellable *cancellable;
    IterationFileCallback file_callback;
    gpointer user_data;
} NautilusRecursiveDirectoryIterator;

static NautilusRecursiveDirectoryIterator *
nautilus_recursive_directory_iterator_new (guint                  max_depth,
                                           gboolean               local_only,
                                           const char            *attributes,
                                           GCancellable          *cancellable,
                                           IterationFileCallback  file_callback,
                                           gpointer               user_data)
{
    NautilusRecursiveDirectoryIterator *self = g_new0 (NautilusRecursiveDirectoryIterator, 1);

    self->pending_directories = g_queue_new ();
    self->visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    self->attributes = g_strdup (attributes != NULL
                                 ? attributes : G_FILE_ATTRIBUTE_STANDARD_NAME);
    self->max_depth = max_depth;
    self->local_only = local_only;
    self->cancellable = (cancellable != NULL) ? g_object_ref (cancellable) : NULL;
    self->file_callback = file_callback;
    self->user_data = user_data;

    return self;
}

static void
nautilus_recursive_directory_iterator_free (NautilusRecursiveDirectoryIterator *self)
{
    g_queue_free_full (self->pending_directories, g_object_unref);
    g_hash_table_unref (self->visited);
    g_free (self->attributes);
    g_clear_object (&self->cancellable);
    g_free (self);
}

static gboolean
is_remote (GFileInfo *info)
{
    return g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE);
}

static void
recursive_directory_iterator_thread (GTask        *task,
                                     GObject      *source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
    NautilusRecursiveDirectoryIterator *self = task_data;
    GFileInfo *info = NULL;
    GFile *location = NULL;

    for (guint depth = 0; depth <= self->max_depth; depth++)
    {
        /* Breadth-first iteration, all stored directories are for next depth level */
        g_autoptr (GQueue) directories_at_depth = g_steal_pointer (&self->pending_directories);

        self->pending_directories = g_queue_new ();

        while (!g_queue_is_empty (directories_at_depth))
        {
            g_autoptr (GFile) directory = g_queue_pop_head (directories_at_depth);
            g_autoptr (GError) error = NULL;
            g_autoptr (GFileEnumerator) enumerator =
                g_file_enumerate_children (directory,
                                           self->attributes,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           self->cancellable,
                                           &error);

            if (error != NULL)
            {
                g_warning ("Error enumerating directory '%s': %s",
                           g_file_get_path (directory), error->message);
                continue;
            }

            while (g_file_enumerator_iterate (enumerator, &info, &location, self->cancellable, &error) && info != NULL)
            {
                const char *id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE);

                if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY &&
                    (!self->local_only || !is_remote (info)))
                {
                    if (id != NULL)
                    {
                        if (g_hash_table_contains (self->visited, id))
                        {
                            /* Already visited, don't call callback a second time */
                            continue;
                        }
                        else
                        {
                            g_hash_table_add (self->visited, g_strdup (id));
                        }
                    }
                    g_queue_push_tail (self->pending_directories, g_object_ref (location));
                }

                self->file_callback (info, location, self->user_data);
            }

            if (g_cancellable_is_cancelled (self->cancellable))
            {
                g_task_return_boolean (task, FALSE);
                return;
            }

            if (error != NULL)
            {
                g_warning ("Error iterating directory '%s': %s",
                           g_file_get_path (directory), error->message);
            }
        }
    }

    g_task_return_boolean (task, TRUE);
}

/**
 * Set G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE attribute when using `local_only`.
 */
gboolean
nautilus_iterate_directory_recursive (GFile                 *directory,
                                      guint                  max_depth,
                                      const char            *attributes,
                                      gboolean               local_only,
                                      GCancellable          *cancellable,
                                      IterationFileCallback  file_callback,
                                      GAsyncReadyCallback    done_callback,
                                      gpointer               user_data)
{
    g_return_val_if_fail (G_IS_FILE (directory), FALSE);
    g_return_val_if_fail (file_callback != NULL, FALSE);

    g_autoptr (GError) error = NULL;
    NautilusRecursiveDirectoryIterator *self =
        nautilus_recursive_directory_iterator_new (max_depth,
                                                   local_only,
                                                   attributes,
                                                   cancellable,
                                                   file_callback,
                                                   user_data);
    g_autoptr (GTask) task = g_task_new (NULL, cancellable, done_callback, user_data);

    g_queue_push_tail (self->pending_directories, g_object_ref (directory));

    g_task_set_task_data (task, self, (GDestroyNotify) nautilus_recursive_directory_iterator_free);
    g_task_run_in_thread (task, (GTaskThreadFunc) recursive_directory_iterator_thread);

    return TRUE;
}
