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

#include "nautilus-file.h"

#include "nautilus-task-manager.h"
#include "tasks/nautilus-attribute-task.h"

typedef enum
{
    INVALID,
    PENDING,
    VALID
} CacheState;

typedef struct
{
    GFile *location;

    GFileInfo *info;
    CacheState info_state;

    GMutex cache_mutex;
} NautilusFilePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NautilusFile, nautilus_file, G_TYPE_OBJECT)

enum
{
    PROP_LOCATION = 1,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };
static GHashTable *files = NULL;
static GMutex files_mutex;

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

    g_mutex_clear (&priv->cache_mutex);

    G_OBJECT_CLASS (nautilus_file_parent_class)->finalize (object);
}

static void
nautilus_file_class_init (NautilusFileClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = set_property;
    object_class->finalize = finalize;

    properties[PROP_LOCATION] =
        g_param_spec_object ("location", "Location", "Location",
                             G_TYPE_FILE,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
nautilus_file_init (NautilusFile *self)
{
    NautilusFilePrivate *priv;

    priv = nautilus_file_get_instance_private (self);

    priv->info = g_file_info_new ();

    g_mutex_init (&priv->cache_mutex);
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

    details = data;
    priv = nautilus_file_get_instance_private (details->file);

    g_mutex_lock (&priv->cache_mutex);
    g_file_info_copy_into (info, priv->info);
    priv->info_state = VALID;
    g_mutex_unlock (&priv->cache_mutex);

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
    g_autoptr (NautilusTask) task = NULL;
    QueryInfoDetails *details;
    g_autoptr (NautilusTaskManager) manager = NULL;

    priv = nautilus_file_get_instance_private (file);

    g_mutex_lock (&priv->cache_mutex);
    /* This is not the right thing to do if a cache update is pending.
     * A task reference could be stored and we could connect to the signal,
     * but there might be a better way.
     */
    if (priv->info_state == PENDING || priv->info_state == VALID)
    {
        g_mutex_unlock (&priv->cache_mutex);

        callback (file, g_file_info_dup (priv->info), NULL, user_data);
    }
    g_mutex_unlock (&priv->cache_mutex);

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
nautilus_file_new (GFile *location)
{
    NautilusFile *file;

    g_return_val_if_fail (G_IS_FILE (location), NULL);

    g_mutex_lock (&files_mutex);

    if (files == NULL)
    {
        files = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                       g_object_unref, NULL);
    }


    file = g_hash_table_lookup (files, location);
    if (file != NULL)
    {
        file = g_object_ref (file);
    }
    else
    {
        file = g_object_new (NAUTILUS_TYPE_FILE, "location", location, NULL);

        g_assert (g_hash_table_insert (files, location, file));
    }

    g_mutex_unlock (&files_mutex);

    return file;
}
