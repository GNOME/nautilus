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

#include "nautilus-directory.h"

#include "nautilus-cache.h"
#include "nautilus-task.h"
#include "nautilus-tasks.h"

enum
{
    CHILDREN,
    N_ITEMS
};

typedef struct
{
    NautilusCache *cache;
    gssize cache_items[N_ITEMS];
} NautilusDirectoryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NautilusDirectory, nautilus_directory,
                            NAUTILUS_TYPE_FILE)

enum
{
    CHILDREN_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
children_changed (NautilusDirectory *directory)
{
    g_message ("Children changed in NautilusDirectory %p",
               (gpointer) directory);
}

static void
nautilus_directory_class_init (NautilusDirectoryClass *klass)
{
    klass->children_changed = children_changed;

    signals[CHILDREN_CHANGED] = g_signal_new ("children-changed",
                                              G_TYPE_FROM_CLASS (klass),
                                              G_SIGNAL_RUN_FIRST,
                                              G_STRUCT_OFFSET (NautilusDirectoryClass, children_changed),
                                              NULL, NULL,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE,
                                              0);
}

static void
file_list_free (gpointer data)
{
    g_autoptr (GList) list = NULL;

    list = data;

    for (GList *i = list; i != NULL; i = i->next)
    {
        g_object_unref (i->data);
    }
}

static void
nautilus_directory_init (NautilusDirectory *self)
{
    NautilusDirectoryPrivate *priv;

    priv = nautilus_directory_get_instance_private (self);

    priv->cache = nautilus_cache_new ();
    priv->cache_items[CHILDREN] =
        nautilus_cache_install_item (priv->cache,
                                     (GDestroyNotify) file_list_free);
}

typedef struct
{
    NautilusDirectory *directory;

    NautilusEnumerateChildrenCallback callback;
    gpointer callback_data;
} EnumerateChildrenDetails;

/*static void
create_file_list (gpointer key,
                  gpointer value,
                  gpointer user_data)
{
    GList **list;

    list = user_data;

    *list = g_list_prepend (*list,
                            nautilus_file_new_with_info (G_FILE (key),
                                                         G_FILE_INFO (value)));
}*/

/*static void
on_enumerate_children_finished (NautilusEnumerateChildrenTask *task,
                                GFile      *file,
                                GHashTable *files,
                                GError     *error,
                                gpointer    user_data)
{
    EnumerateChildrenDetails *details;
    NautilusDirectoryPrivate *priv;
    NautilusCacheState cache_state;
    GList *children = NULL;

    details = user_data;
    priv = nautilus_directory_get_instance_private (details->directory);
    cache_state = nautilus_cache_item_get_state (priv->cache,
                                                 priv->cache_items[CHILDREN]);

    if (cache_state == NAUTILUS_CACHE_INVALID)
    {*/
        /* TODO: restart */
        /*return;
    }

    g_hash_table_foreach (files, create_file_list, &children);

    nautilus_cache_item_set_value (priv->cache, priv->cache_items[CHILDREN],
                                   children);

    details->callback (details->directory, g_list_copy (children), error,
                       details->callback_data);

    g_free (details);
}*/

void
nautilus_directory_enumerate_children (NautilusDirectory                 *directory,
                                       GCancellable                      *cancellable,
                                       NautilusEnumerateChildrenCallback  callback,
                                       gpointer                           user_data)
{
    NautilusDirectoryPrivate *priv;
    NautilusCacheState cache_state;
    GFile *location;
    g_autoptr (NautilusTask) task = NULL;

    g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

    priv = nautilus_directory_get_instance_private (directory);
    cache_state = nautilus_cache_item_get_state (priv->cache,
                                                 priv->cache_items[CHILDREN]);

    if (cache_state == NAUTILUS_CACHE_PENDING ||
        cache_state == NAUTILUS_CACHE_VALID)
    {
        callback (directory,
                  nautilus_cache_item_get_value (priv->cache,
                                                 priv->cache_items[CHILDREN],
                                                 (NautilusCopyFunc) g_list_copy),
                  NULL, user_data);

        return;
    }

    nautilus_cache_item_set_pending (priv->cache,
                                     priv->cache_items[CHILDREN]);

    location = nautilus_file_get_location (NAUTILUS_FILE (directory));
    task = nautilus_task_new_with_func (nautilus_enumerate_children_task_func, location,
                                        g_object_unref, cancellable);

    nautilus_task_run (task);
}

NautilusFile *
nautilus_directory_new (GFile *location)
{
    gpointer instance;

    g_return_val_if_fail (G_IS_FILE (location), NULL);

    instance = g_object_new (NAUTILUS_TYPE_DIRECTORY,
                             "location", location,
                             NULL);

    g_assert (NAUTILUS_IS_DIRECTORY (instance));

    return instance;
}
