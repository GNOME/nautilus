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

#include "nautilus-file.h"

#include "nautilus-cache.h"
#include "nautilus-directory.h"
#include "nautilus-task-manager.h"
#include "tasks/nautilus-attribute-task.h"

enum
{
    INFO,
    N_ITEMS
};

typedef struct
{
    GFile *location;

    NautilusCache *cache;
    gssize cache_items[N_ITEMS];
} NautilusFilePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NautilusFile, nautilus_file, G_TYPE_OBJECT)

enum
{
    PROP_LOCATION = 1,
    N_PROPERTIES
};

enum
{
    RENAMED,
    LAST_SIGNAL
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };
static guint       signals[LAST_SIGNAL]     = { 0 };
static GHashTable *files                    = NULL;
static GMutex      files_mutex;

static GObject *
constructor (GType                  type,
             guint                  n_construct_properties,
             GObjectConstructParam *construct_properties)
{
    GFile *location = NULL;
    gpointer instance;

    for (guint i = 0; i < n_construct_properties; i++)
    {
        if (construct_properties[i].pspec == properties[PROP_LOCATION])
        {
            location = g_value_get_object (construct_properties[i].value);
        }
    }

    g_assert (location != NULL);

    g_mutex_lock (&files_mutex);

    if (files == NULL)
    {
        files = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                       g_object_unref, NULL);
    }


    instance = g_hash_table_lookup (files, location);
    if (instance != NULL)
    {
        instance = g_object_ref (instance);
    }
    else
    {
        GObjectClass *parent_class;

        parent_class = G_OBJECT_CLASS (nautilus_file_parent_class);
        instance = parent_class->constructor (type, n_construct_properties,
                                              construct_properties);

        g_assert (g_hash_table_insert (files, location, instance));
    }

    g_mutex_unlock (&files_mutex);

    return instance;
}

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    NautilusFilePrivate *priv;

    priv = nautilus_file_get_instance_private (NAUTILUS_FILE (object));

    switch (property_id)
    {
        case PROP_LOCATION:
        {
            priv->location = g_value_dup_object (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
finalize (GObject *object)
{
    NautilusFilePrivate *priv;

    priv = nautilus_file_get_instance_private (NAUTILUS_FILE (object));

    g_mutex_lock (&files_mutex);
    g_hash_table_remove (files, priv->location);
    g_mutex_unlock (&files_mutex);

    G_OBJECT_CLASS (nautilus_file_parent_class)->finalize (object);
}

static void
renamed (NautilusFile *file,
         GFile        *new_location)
{
    NautilusFilePrivate *priv;

    priv = nautilus_file_get_instance_private (file);

    g_message ("NautilusFile %p renamed; changing location: %p -> %p",
               (gpointer) file, (gpointer) priv->location,
               (gpointer) new_location);

    g_mutex_lock (&files_mutex);

    g_hash_table_remove (files, priv->location);

    priv->location = g_object_ref (new_location);

    g_assert (g_hash_table_insert (files, new_location, file));

    g_mutex_unlock (&files_mutex);

    nautilus_cache_item_invalidate (priv->cache, priv->cache_items[INFO],
                                    FALSE);
}

static void
nautilus_file_class_init (NautilusFileClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->constructor = constructor;
    object_class->set_property = set_property;
    object_class->finalize = finalize;

    klass->renamed = renamed;

    properties[PROP_LOCATION] =
        g_param_spec_object ("location", "Location", "Location",
                             G_TYPE_FILE,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    signals[RENAMED] = g_signal_new ("renamed",
                                     G_TYPE_FROM_CLASS (klass),
                                     G_SIGNAL_RUN_FIRST,
                                     G_STRUCT_OFFSET (NautilusFileClass, renamed),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__OBJECT,
                                     G_TYPE_NONE,
                                     1,
                                     G_TYPE_OBJECT);
}

static void
nautilus_file_init (NautilusFile *self)
{
    NautilusFilePrivate *priv;

    priv = nautilus_file_get_instance_private (self);

    priv->cache = nautilus_cache_new ();
    priv->cache_items[INFO] = nautilus_cache_install_item (priv->cache,
                                                           g_object_unref);
}

typedef struct
{
    NautilusFile *file;

    NautilusFileInfoCallback callback;
    gpointer callback_data;
} QueryInfoDetails;

static void
on_query_info_finished (NautilusAttributeTask *task,
                        GFile                 *file,
                        GFileInfo             *info,
                        GError                *error,
                        gpointer               data)
{
    QueryInfoDetails *details;
    NautilusFilePrivate *priv;
    NautilusCacheState cache_state;

    details = data;
    priv = nautilus_file_get_instance_private (details->file);
    cache_state = nautilus_cache_item_get_state (priv->cache,
                                                 priv->cache_items[INFO]);

    if (cache_state == NAUTILUS_CACHE_INVALID)
    {
        /* TODO: restart */
        return;
    }

    nautilus_cache_item_set_value (priv->cache, priv->cache_items[INFO],
                                   info);

    details->callback (details->file, g_file_info_dup (info), error,
                       details->callback_data);

    g_free (details);
}

void
nautilus_file_query_info (NautilusFile             *file,
                          GCancellable             *cancellable,
                          NautilusFileInfoCallback  callback,
                          gpointer                  user_data)
{
    NautilusFilePrivate *priv;
    NautilusCacheState cache_state;
    g_autoptr (NautilusTask) task = NULL;
    QueryInfoDetails *details;
    g_autoptr (NautilusTaskManager) manager = NULL;

    g_return_if_fail (NAUTILUS_IS_FILE (file));

    g_debug ("%s: called for %p", __func__, (gpointer) file);

    priv = nautilus_file_get_instance_private (file);
    cache_state = nautilus_cache_item_get_state (priv->cache,
                                                 priv->cache_items[INFO]);

    /* This is not the right thing to do if a cache update is pending.
     * A task reference could be stored and we could connect to the signal,
     * but there might be a better way.
     */
    if (cache_state == NAUTILUS_CACHE_PENDING ||
        cache_state == NAUTILUS_CACHE_VALID)
    {
        GFileInfo *info;

        g_debug ("%s: info for %p is either pending or valid",
                 __func__, (gpointer) file);

        info = nautilus_cache_item_get_value (priv->cache,
                                              priv->cache_items[INFO],
                                              NAUTILUS_COPY_FUNC (g_file_info_dup));

        callback (file, info, NULL, user_data);

        return;
    }

    nautilus_cache_item_set_pending (priv->cache, priv->cache_items[INFO]);

    task = nautilus_attribute_task_new (priv->location,
                                        "standard::*,"
                                        "access::*,"
                                        "mountable::*,"
                                        "time::*,"
                                        "unix::*,"
                                        "owner::*,"
                                        "selinux::*,"
                                        "thumbnail::*,"
                                        "id::filesystem,"
                                        "trash::orig-path,"
                                        "trash::deletion-date,"
                                        "metadata::*,"
                                        "recent::*",
                                        G_FILE_QUERY_INFO_NONE,
                                        cancellable);
    details = g_new0 (QueryInfoDetails, 1);
    manager = nautilus_task_manager_dup_singleton ();

    details->file = file;
    details->callback = callback;
    details->callback_data = user_data;

    g_signal_connect (task, "finished",
                      G_CALLBACK (on_query_info_finished), details);

    nautilus_task_manager_queue_task (manager, task);
}

NautilusFile *
nautilus_file_get_existing (GFile *location)
{
    NautilusFile *file = NULL;

    g_return_val_if_fail (G_IS_FILE (location), NULL);

    g_mutex_lock (&files_mutex);

    if (files != NULL)
    {
        file = g_hash_table_lookup (files, location);
        if (file != NULL)
        {
            file = g_object_ref (file);
        }
    }

    g_mutex_unlock (&files_mutex);

    return file;
}

GFile *
nautilus_file_get_location (NautilusFile *file)
{
    NautilusFilePrivate *priv;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    priv = nautilus_file_get_instance_private (file);

    return g_object_ref (priv->location);
}

NautilusFile *
nautilus_file_get_parent (NautilusFile *file)
{
    NautilusFilePrivate *priv;
    g_autoptr (GFile) parent_location = NULL;
    NautilusFile *parent = NULL;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    priv = nautilus_file_get_instance_private (file);
    parent_location = g_file_get_parent (priv->location);

    if (parent_location != NULL)
    {
        parent = nautilus_file_new (parent_location);
    }

    return parent;
}

NautilusFile *
nautilus_file_new_with_info (GFile     *location,
                             GFileInfo *info)
{
    NautilusFile *instance;
    NautilusFilePrivate *priv;

    g_return_val_if_fail (G_IS_FILE (location), NULL);
    g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

    instance = nautilus_file_new (location);
    priv = nautilus_file_get_instance_private (instance);

    /* Ergh. */
    nautilus_cache_item_set_pending (priv->cache, priv->cache_items[INFO]);
    nautilus_cache_item_set_value (priv->cache, priv->cache_items[INFO],
                                   info);

    return instance;
}

NautilusFile *
nautilus_file_new (GFile *location)
{
    NautilusFile *file;
    GFileType file_type;

    g_return_val_if_fail (G_IS_FILE (location), NULL);

    file = nautilus_file_get_existing (location);
    if (file != NULL)
    {
        return g_object_ref (file);
    }

    /* TODO: extension points? */
    file_type = g_file_query_file_type (location, G_FILE_QUERY_INFO_NONE,
                                        NULL);
    /* File does not exist.
     * Search directory URIs also fall under this category.
     * TODO: creation?
     */
    if (file_type == G_FILE_TYPE_UNKNOWN)
    {
        return NULL;
    }
    else if (file_type == G_FILE_TYPE_DIRECTORY)
    {
        /* Asserts that the constructed file is a directory. */
        return nautilus_directory_new (location);
    }

    return g_object_new (NAUTILUS_TYPE_FILE, "location", location, NULL);
}
