/* nautilus-favorite-directory.c
 *
 * Copyright (C) 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-favorite-directory.h"
#include "nautilus-tag-manager.h"
#include "nautilus-file-utilities.h"
#include "nautilus-directory-private.h"
#include <glib/gi18n.h>

struct NautilusFavoriteDirectoryDetails
{
    NautilusTagManager *tag_manager;
    GList *files;

    GList *monitor_list;
    GList *callback_list;
    GList *pending_callback_list;
};

typedef struct
{
    gboolean monitor_hidden_files;
    NautilusFileAttributes monitor_attributes;

    gconstpointer client;
} FavoriteMonitor;

typedef struct
{
    NautilusFavoriteDirectory *favorite_directory;

    NautilusDirectoryCallback callback;
    gpointer callback_data;

    NautilusFileAttributes wait_for_attributes;
    gboolean wait_for_file_list;
    GList *file_list;
} FavoriteCallback;

G_DEFINE_TYPE_WITH_CODE (NautilusFavoriteDirectory, nautilus_favorite_directory, NAUTILUS_TYPE_DIRECTORY,
                         nautilus_ensure_extension_points ();
                         g_io_extension_point_implement (NAUTILUS_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         NAUTILUS_FAVORITE_DIRECTORY_PROVIDER_NAME,
                                                         0));

static void
file_changed (NautilusFile              *file,
              NautilusFavoriteDirectory *favorite)
{
    GList list;

    list.data = file;
    list.next = NULL;

    nautilus_directory_emit_files_changed (NAUTILUS_DIRECTORY (favorite), &list);
}

static void
nautilus_favorite_directory_update_files (NautilusFavoriteDirectory *self)
{
    GList *l;
    GList *tmp_l;
    GList *new_favorite_files;
    GList *monitor_list;
    FavoriteMonitor *monitor;
    NautilusFile *file;
    GHashTable *uri_table;
    GList *files_added;
    GList *files_removed;
    gchar *uri;

    files_added = NULL;
    files_removed = NULL;

    uri_table = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       (GDestroyNotify) g_free,
                                       NULL);

    for (l = self->details->files; l != NULL; l = l->next)
    {
        g_hash_table_add (uri_table, nautilus_file_get_uri (NAUTILUS_FILE (l->data)));
    }

    new_favorite_files = nautilus_tag_manager_get_favorite_files (self->details->tag_manager);

    for (l = new_favorite_files; l != NULL; l = l->next)
    {
        if (!g_hash_table_contains (uri_table, l->data))
        {
            file = nautilus_file_get_by_uri ((gchar*) l->data);

            for (monitor_list = self->details->monitor_list; monitor_list; monitor_list = monitor_list->next)
            {
                monitor = monitor_list->data;

                /* Add monitors */
                nautilus_file_monitor_add (file, monitor, monitor->monitor_attributes);
            }

            g_signal_connect (file, "changed", G_CALLBACK (file_changed), self);

            files_added = g_list_prepend (files_added, file);
        }
    }

    l = self->details->files;
    while (l != NULL)
    {
        uri = nautilus_file_get_uri (NAUTILUS_FILE (l->data));

        if (!nautilus_tag_manager_file_is_favorite (self->details->tag_manager, uri))
        {
            files_removed = g_list_prepend (files_removed,
                                            nautilus_file_ref (NAUTILUS_FILE (l->data)));

            g_signal_handlers_disconnect_by_func (NAUTILUS_FILE (l->data),
                                                  file_changed,
                                                  self);

            /* Remove monitors */
            for (monitor_list = self->details->monitor_list; monitor_list;
                 monitor_list = monitor_list->next)
            {
                monitor = monitor_list->data;
                nautilus_file_monitor_remove (NAUTILUS_FILE (l->data), monitor);
            }

            if (l == self->details->files)
            {
                self->details->files = g_list_delete_link (self->details->files, l);
                l = self->details->files;
            }
            else
            {
                tmp_l = l->prev;
                self->details->files = g_list_delete_link (self->details->files, l);
                l = tmp_l->next;
            }
        }
        else
        {
            l = l->next;
        }

        g_free (uri);
    }

    if (files_added)
    {
        nautilus_directory_emit_files_added (NAUTILUS_DIRECTORY (self), files_added);

        for (l = files_added; l != NULL; l = l->next)
        {
            self->details->files = g_list_prepend (self->details->files, nautilus_file_ref (NAUTILUS_FILE (l->data)));
        }
    }

    if (files_removed)
    {
        nautilus_directory_emit_files_changed (NAUTILUS_DIRECTORY (self), files_removed);
    }

    nautilus_file_list_free (files_added);
    nautilus_file_list_free (files_removed);
    g_hash_table_destroy (uri_table);
}

static void
on_favorites_files_changed (NautilusTagManager        *tag_manager,
                            GList                     *changed_files,
                            gpointer                   user_data)
{
    NautilusFavoriteDirectory *self;

    self = NAUTILUS_FAVORITE_DIRECTORY (user_data);

    nautilus_favorite_directory_update_files (self);
}

static gboolean
real_contains_file (NautilusDirectory *directory,
                    NautilusFile      *file)
{
    NautilusFavoriteDirectory *self;
    g_autofree gchar *uri = NULL;

    self = NAUTILUS_FAVORITE_DIRECTORY (directory);

    uri = nautilus_file_get_uri (file);

    return nautilus_tag_manager_file_is_favorite (self->details->tag_manager, uri);
}

static gboolean
real_is_editable (NautilusDirectory *directory)
{
    return FALSE;
}

static void
real_force_reload (NautilusDirectory *directory)
{
    nautilus_favorite_directory_update_files (NAUTILUS_FAVORITE_DIRECTORY (directory));
}

static void
real_call_when_ready (NautilusDirectory         *directory,
                      NautilusFileAttributes     file_attributes,
                      gboolean                   wait_for_file_list,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    GList *file_list;
    NautilusFavoriteDirectory *favorite;

    favorite = NAUTILUS_FAVORITE_DIRECTORY (directory);

    file_list = nautilus_file_list_copy (favorite->details->files);

    callback (NAUTILUS_DIRECTORY (directory),
                                  file_list,
                                  callback_data);
}

static gboolean
real_are_all_files_seen (NautilusDirectory *directory)
{
    return TRUE;
}

static void
real_file_monitor_add (NautilusDirectory         *directory,
                       gconstpointer              client,
                       gboolean                   monitor_hidden_files,
                       NautilusFileAttributes     file_attributes,
                       NautilusDirectoryCallback  callback,
                       gpointer                   callback_data)
{
    GList *list;
    FavoriteMonitor *monitor;
    NautilusFavoriteDirectory *favorite;
    NautilusFile *file;

    favorite = NAUTILUS_FAVORITE_DIRECTORY (directory);

    monitor = g_new0 (FavoriteMonitor, 1);
    monitor->monitor_hidden_files = monitor_hidden_files;
    monitor->monitor_attributes = file_attributes;
    monitor->client = client;

    favorite->details->monitor_list = g_list_prepend (favorite->details->monitor_list, monitor);

    if (callback != NULL)
    {
        (*callback) (directory, favorite->details->files, callback_data);
    }

    for (list = favorite->details->files; list != NULL; list = list->next)
    {
        file = list->data;

        /* Add monitors */
        nautilus_file_monitor_add (file, monitor, file_attributes);
    }
}

static void
favorite_monitor_destroy (FavoriteMonitor           *monitor,
                          NautilusFavoriteDirectory *favorite)
{
    GList *l;
    NautilusFile *file;

    for (l = favorite->details->files; l != NULL; l = l->next)
    {
        file = l->data;

        nautilus_file_monitor_remove (file, monitor);
    }

    g_free (monitor);
}

static void
real_monitor_remove (NautilusDirectory *directory,
                     gconstpointer      client)
{
    NautilusFavoriteDirectory *favorite;
    FavoriteMonitor *monitor;
    GList *list;

    favorite = NAUTILUS_FAVORITE_DIRECTORY (directory);

    for (list = favorite->details->monitor_list; list != NULL; list = list->next)
    {
        monitor = list->data;

        if (monitor->client != client)
            continue;

        favorite->details->monitor_list = g_list_delete_link (favorite->details->monitor_list, list);

        favorite_monitor_destroy (monitor, favorite);

        break;
    }
}

static gboolean
real_handles_location (GFile *location)
{
    g_autofree gchar *uri = NULL;

    uri = g_file_get_uri (location);

    if (eel_uri_is_favorites (uri))
    {
        return TRUE;
    }

    return FALSE;
}

static FavoriteCallback*
favorite_callback_find_pending (NautilusFavoriteDirectory *favorite,
                                NautilusDirectoryCallback  callback,
                                gpointer                   callback_data)
{
    FavoriteCallback *favorite_callback;
    GList *list;

    for (list = favorite->details->pending_callback_list; list != NULL; list = list->next)
    {
        favorite_callback = list->data;

        if (favorite_callback->callback == callback &&
            favorite_callback->callback_data == callback_data)
        {
            return favorite_callback;
        }
    }

    return NULL;
}

static FavoriteCallback*
favorite_callback_find (NautilusFavoriteDirectory *favorite,
                        NautilusDirectoryCallback  callback,
                        gpointer                   callback_data)
{
    FavoriteCallback *favorite_callback;
    GList *list;

    for (list = favorite->details->callback_list; list != NULL; list = list->next)
    {
        favorite_callback = list->data;

        if (favorite_callback->callback == callback &&
            favorite_callback->callback_data == callback_data)
        {
            return favorite_callback;
        }
    }

    return NULL;
}

static void
favorite_callback_destroy (FavoriteCallback *favorite_callback)
{
    nautilus_file_list_free (favorite_callback->file_list);

    g_free (favorite_callback);
}

static void
real_cancel_callback (NautilusDirectory        *directory,
                      NautilusDirectoryCallback callback,
                      gpointer                  callback_data)
{
    NautilusFavoriteDirectory *favorite;
    FavoriteCallback *favorite_callback;

    favorite = NAUTILUS_FAVORITE_DIRECTORY (directory);
    favorite_callback = favorite_callback_find (favorite, callback, callback_data);

    if (favorite_callback)
    {
        favorite->details->callback_list = g_list_remove (favorite->details->callback_list, favorite_callback);

        favorite_callback_destroy (favorite_callback);

        return;
    }

    /* Check for a pending callback */
    favorite_callback = favorite_callback_find_pending (favorite, callback, callback_data);

    if (favorite_callback)
    {
        favorite->details->pending_callback_list = g_list_remove (favorite->details->pending_callback_list, favorite_callback);

        favorite_callback_destroy (favorite_callback);
    }
}

static GList*
real_get_file_list (NautilusDirectory *directory)
{
    NautilusFavoriteDirectory *favorite;

    favorite = NAUTILUS_FAVORITE_DIRECTORY (directory);

    return nautilus_file_list_copy (favorite->details->files);
}

static void
nautilus_favorite_directory_set_files (NautilusFavoriteDirectory *self)
{
    GList *favorite_files;
    NautilusFile *file;
    GList *l;
    GList *file_list;
    FavoriteMonitor *monitor;
    GList *monitor_list;

    file_list = NULL;

    favorite_files = nautilus_tag_manager_get_favorite_files (self->details->tag_manager);

    for (l = favorite_files; l != NULL; l = l->next)
    {
        file = nautilus_file_get_by_uri ((gchar*) l->data);

        g_signal_connect (file, "changed", G_CALLBACK (file_changed), self);

        for (monitor_list = self->details->monitor_list; monitor_list; monitor_list = monitor_list->next)
        {
            monitor = monitor_list->data;

            /* Add monitors */
            nautilus_file_monitor_add (file, monitor, monitor->monitor_attributes);
        }

        file_list = g_list_prepend (file_list, file);
    }

    nautilus_directory_emit_files_added (NAUTILUS_DIRECTORY (self), file_list);

    self->details->files = file_list;
}

static void
nautilus_favorite_directory_finalize (GObject *object)
{
    NautilusFavoriteDirectory *self;

    self = NAUTILUS_FAVORITE_DIRECTORY (object);

    g_signal_handlers_disconnect_by_func (self->details->tag_manager,
                                          on_favorites_files_changed,
                                          self);

    g_object_unref (self->details->tag_manager);
    nautilus_file_list_free (self->details->files);

    G_OBJECT_CLASS (nautilus_favorite_directory_parent_class)->finalize (object);
}

static void
nautilus_favorite_directory_dispose (GObject *object)
{
    NautilusFavoriteDirectory *favorite;
    GList *l;
    GList *monitor_list;
    FavoriteMonitor *monitor;
    NautilusFile *file;

    favorite = NAUTILUS_FAVORITE_DIRECTORY (object);

    /* Remove file connections */
    for (l = favorite->details->files; l != NULL; l = l->next)
    {
        file = l->data;

        /* Disconnect change handler */
        g_signal_handlers_disconnect_by_func (file, file_changed, favorite);

        /* Remove monitors */
        for (monitor_list = favorite->details->monitor_list; monitor_list;
             monitor_list = monitor_list->next)
        {
            monitor = monitor_list->data;
            nautilus_file_monitor_remove (file, monitor);
        }
    }

    /* Remove search monitors */
    if (favorite->details->monitor_list)
    {
        for (l = favorite->details->monitor_list; l != NULL; l = l->next)
        {
            favorite_monitor_destroy ((FavoriteMonitor*) l->data, favorite);
        }

        g_list_free (favorite->details->monitor_list);
        favorite->details->monitor_list = NULL;
    }

    G_OBJECT_CLASS (nautilus_favorite_directory_parent_class)->dispose (object);
}

static void
nautilus_favorite_directory_class_init (NautilusFavoriteDirectoryClass *klass)
{
    GObjectClass *oclass;
    NautilusDirectoryClass *directory_class;

    oclass = G_OBJECT_CLASS (klass);
    directory_class = NAUTILUS_DIRECTORY_CLASS (klass);

    oclass->finalize = nautilus_favorite_directory_finalize;
    oclass->dispose = nautilus_favorite_directory_dispose;

    directory_class->handles_location = real_handles_location;
    directory_class->contains_file = real_contains_file;
    directory_class->is_editable = real_is_editable;
    directory_class->force_reload = real_force_reload;
    directory_class->call_when_ready = real_call_when_ready;
    directory_class->are_all_files_seen = real_are_all_files_seen;
    directory_class->file_monitor_add = real_file_monitor_add;
    directory_class->file_monitor_remove = real_monitor_remove;
    directory_class->cancel_callback = real_cancel_callback;
    directory_class->get_file_list = real_get_file_list;

    g_type_class_add_private (klass, sizeof (NautilusFavoriteDirectoryDetails));
}

NautilusFavoriteDirectory*
nautilus_favorite_directory_new ()
{
    NautilusFavoriteDirectory *self;

    self = g_object_new (NAUTILUS_TYPE_FAVORITE_DIRECTORY, NULL);

    return self;
}

static void
nautilus_favorite_directory_init (NautilusFavoriteDirectory *self)
{
    NautilusTagManager *tag_manager;

    self->details = G_TYPE_INSTANCE_GET_PRIVATE (self, NAUTILUS_TYPE_FAVORITE_DIRECTORY,
                                                 NautilusFavoriteDirectoryDetails);

    tag_manager = nautilus_tag_manager_get ();

    g_signal_connect (tag_manager,
                      "favorites-changed",
                      (GCallback) on_favorites_files_changed,
                      self);

    self->details->tag_manager = tag_manager;

    nautilus_favorite_directory_set_files (self);

}
