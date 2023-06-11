/* nautilus-starred-directory.c
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

#include "nautilus-starred-directory.h"
#include "nautilus-tag-manager.h"
#include "nautilus-file-utilities.h"
#include "nautilus-directory-private.h"
#include <glib/gi18n.h>

struct _NautilusFavoriteDirectory
{
    NautilusDirectory parent_slot;

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
    NautilusFavoriteDirectory *starred_directory;

    NautilusDirectoryCallback callback;
    gpointer callback_data;

    NautilusFileAttributes wait_for_attributes;
    gboolean wait_for_file_list;
    GList *file_list;
} FavoriteCallback;

G_DEFINE_TYPE_WITH_CODE (NautilusFavoriteDirectory, nautilus_starred_directory, NAUTILUS_TYPE_DIRECTORY,
                         nautilus_ensure_extension_points ();
                         /* It looks like you’re implementing an extension point.
                          * Did you modify nautilus_ensure_extension_builtins() accordingly?
                          *
                          * • Yes
                          * • Doing it right now
                          */
                         g_io_extension_point_implement (NAUTILUS_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         NAUTILUS_STARRED_DIRECTORY_PROVIDER_NAME,
                                                         0));

static void
file_changed (NautilusFile              *file,
              NautilusFavoriteDirectory *starred)
{
    GList list;

    list.data = file;
    list.next = NULL;

    nautilus_directory_emit_files_changed (NAUTILUS_DIRECTORY (starred), &list);
}

static void
disconnect_and_unmonitor_file (NautilusFile              *file,
                               NautilusFavoriteDirectory *self)
{
    /* Disconnect change handler */
    g_signal_handlers_disconnect_by_func (file, file_changed, self);

    /* Remove monitors */
    for (GList *m = self->monitor_list; m != NULL; m = m->next)
    {
        nautilus_file_monitor_remove (file, m->data);
    }
}

static void
nautilus_starred_directory_update_files (NautilusFavoriteDirectory *self,
                                         GList                     *changed_files)
{
    NautilusTagManager *tag_manager = nautilus_tag_manager_get ();
    GList *monitor_list;
    FavoriteMonitor *monitor;
    g_autoptr (GHashTable) uri_table = NULL;
    GList *next;
    g_autolist (NautilusFile) files_added = NULL;
    g_autolist (NautilusFile) files_removed = NULL;

    uri_table = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       (GDestroyNotify) g_free,
                                       NULL);

    for (GList *l = self->files; l != NULL; l = l->next)
    {
        g_hash_table_add (uri_table, nautilus_file_get_uri (NAUTILUS_FILE (l->data)));
    }

    for (GList *l = changed_files; l != NULL; l = next)
    {
        NautilusFile *file = l->data;
        g_autofree char *uri = nautilus_file_get_uri (file);

        next = l->next;
        if (g_hash_table_contains (uri_table, uri) &&
            !nautilus_tag_manager_file_is_starred (tag_manager, uri))
        {
            disconnect_and_unmonitor_file (file, self);
            nautilus_file_unref (file);
            files_removed = g_list_prepend (files_removed, nautilus_file_ref (file));
            self->files = g_list_remove (self->files, file);
        }
        else if (!g_hash_table_contains (uri_table, uri) &&
                 nautilus_tag_manager_file_is_starred (tag_manager, uri))
        {
            for (monitor_list = self->monitor_list; monitor_list; monitor_list = monitor_list->next)
            {
                monitor = monitor_list->data;

                /* Add monitors */
                nautilus_file_monitor_add (file, monitor, monitor->monitor_attributes);
            }

            g_signal_connect (file, "changed", G_CALLBACK (file_changed), self);

            files_added = g_list_prepend (files_added, nautilus_file_ref (file));
            self->files = g_list_prepend (self->files, nautilus_file_ref (file));
        }
        else
        {
            /* This may happen if we have moved the file and updated the starred
             * database for the new URI. In that case, the NautilusFile instance
             * has already updated its own URI. There's no need to re-add it. */
        }
    }

    if (files_added)
    {
        nautilus_directory_emit_files_added (NAUTILUS_DIRECTORY (self), files_added);
    }

    if (files_removed)
    {
        nautilus_directory_emit_files_changed (NAUTILUS_DIRECTORY (self), files_removed);
    }
}

static void
on_starred_files_changed (NautilusTagManager *tag_manager,
                          GList              *changed_files,
                          gpointer            user_data)
{
    NautilusFavoriteDirectory *self;

    self = NAUTILUS_STARRED_DIRECTORY (user_data);

    nautilus_starred_directory_update_files (self, changed_files);
}

static gboolean
real_contains_file (NautilusDirectory *directory,
                    NautilusFile      *file)
{
    g_autofree gchar *uri = NULL;

    uri = nautilus_file_get_uri (file);

    return nautilus_tag_manager_file_is_starred (nautilus_tag_manager_get (), uri);
}

static gboolean
real_is_editable (NautilusDirectory *directory)
{
    return FALSE;
}

static void
real_call_when_ready (NautilusDirectory         *directory,
                      NautilusFileAttributes     file_attributes,
                      gboolean                   wait_for_file_list,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    GList *file_list;
    NautilusFavoriteDirectory *starred;

    starred = NAUTILUS_STARRED_DIRECTORY (directory);

    file_list = nautilus_file_list_copy (starred->files);

    callback (NAUTILUS_DIRECTORY (directory),
              file_list,
              callback_data);

    nautilus_file_list_free (file_list);
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
    NautilusFavoriteDirectory *starred;
    NautilusFile *file;

    starred = NAUTILUS_STARRED_DIRECTORY (directory);

    monitor = g_new0 (FavoriteMonitor, 1);
    monitor->monitor_hidden_files = monitor_hidden_files;
    monitor->monitor_attributes = file_attributes;
    monitor->client = client;

    starred->monitor_list = g_list_prepend (starred->monitor_list, monitor);

    if (callback != NULL)
    {
        (*callback)(directory, starred->files, callback_data);
    }

    for (list = starred->files; list != NULL; list = list->next)
    {
        file = list->data;

        /* Add monitors */
        nautilus_file_monitor_add (file, monitor, file_attributes);
    }
}

static void
starred_monitor_destroy (FavoriteMonitor           *monitor,
                         NautilusFavoriteDirectory *starred)
{
    GList *l;
    NautilusFile *file;

    for (l = starred->files; l != NULL; l = l->next)
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
    NautilusFavoriteDirectory *starred;
    FavoriteMonitor *monitor;
    GList *list;

    starred = NAUTILUS_STARRED_DIRECTORY (directory);

    for (list = starred->monitor_list; list != NULL; list = list->next)
    {
        monitor = list->data;

        if (monitor->client != client)
        {
            continue;
        }

        starred->monitor_list = g_list_delete_link (starred->monitor_list, list);

        starred_monitor_destroy (monitor, starred);

        break;
    }
}

static gboolean
real_handles_location (GFile *location)
{
    g_autofree gchar *uri = NULL;

    uri = g_file_get_uri (location);

    if (eel_uri_is_starred (uri))
    {
        return TRUE;
    }

    return FALSE;
}

static FavoriteCallback *
starred_callback_find_pending (NautilusFavoriteDirectory *starred,
                               NautilusDirectoryCallback  callback,
                               gpointer                   callback_data)
{
    FavoriteCallback *starred_callback;
    GList *list;

    for (list = starred->pending_callback_list; list != NULL; list = list->next)
    {
        starred_callback = list->data;

        if (starred_callback->callback == callback &&
            starred_callback->callback_data == callback_data)
        {
            return starred_callback;
        }
    }

    return NULL;
}

static FavoriteCallback *
starred_callback_find (NautilusFavoriteDirectory *starred,
                       NautilusDirectoryCallback  callback,
                       gpointer                   callback_data)
{
    FavoriteCallback *starred_callback;
    GList *list;

    for (list = starred->callback_list; list != NULL; list = list->next)
    {
        starred_callback = list->data;

        if (starred_callback->callback == callback &&
            starred_callback->callback_data == callback_data)
        {
            return starred_callback;
        }
    }

    return NULL;
}

static void
starred_callback_destroy (FavoriteCallback *starred_callback)
{
    nautilus_file_list_free (starred_callback->file_list);

    g_free (starred_callback);
}

static void
real_cancel_callback (NautilusDirectory         *directory,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    NautilusFavoriteDirectory *starred;
    FavoriteCallback *starred_callback;

    starred = NAUTILUS_STARRED_DIRECTORY (directory);
    starred_callback = starred_callback_find (starred, callback, callback_data);

    if (starred_callback)
    {
        starred->callback_list = g_list_remove (starred->callback_list, starred_callback);

        starred_callback_destroy (starred_callback);

        return;
    }

    /* Check for a pending callback */
    starred_callback = starred_callback_find_pending (starred, callback, callback_data);

    if (starred_callback)
    {
        starred->pending_callback_list = g_list_remove (starred->pending_callback_list, starred_callback);

        starred_callback_destroy (starred_callback);
    }
}

static GList *
real_get_file_list (NautilusDirectory *directory)
{
    NautilusFavoriteDirectory *starred;

    starred = NAUTILUS_STARRED_DIRECTORY (directory);

    return nautilus_file_list_copy (starred->files);
}

static void
nautilus_starred_directory_set_files (NautilusFavoriteDirectory *self)
{
    g_autolist (NautilusFile) starred_files = NULL;
    GList *l;
    GList *file_list;
    FavoriteMonitor *monitor;
    GList *monitor_list;

    file_list = NULL;

    starred_files = nautilus_tag_manager_get_starred_files (nautilus_tag_manager_get ());

    for (l = starred_files; l != NULL; l = l->next)
    {
        NautilusFile *file = l->data;

        g_signal_connect (file, "changed", G_CALLBACK (file_changed), self);

        for (monitor_list = self->monitor_list; monitor_list; monitor_list = monitor_list->next)
        {
            monitor = monitor_list->data;

            /* Add monitors */
            nautilus_file_monitor_add (file, monitor, monitor->monitor_attributes);
        }

        file_list = g_list_prepend (file_list, nautilus_file_ref (file));
    }

    nautilus_directory_emit_files_added (NAUTILUS_DIRECTORY (self), file_list);

    self->files = file_list;
}

static void
real_force_reload (NautilusDirectory *directory)
{
    NautilusFavoriteDirectory *self = NAUTILUS_STARRED_DIRECTORY (directory);

    /* Unset current file list */
    g_list_foreach (self->files, (GFunc) disconnect_and_unmonitor_file, self);
    g_clear_list (&self->files, g_object_unref);

    /* Set a fresh file list  */
    nautilus_starred_directory_set_files (self);
}

static void
nautilus_starred_directory_finalize (GObject *object)
{
    NautilusFavoriteDirectory *self;

    self = NAUTILUS_STARRED_DIRECTORY (object);

    g_signal_handlers_disconnect_by_func (nautilus_tag_manager_get (),
                                          on_starred_files_changed,
                                          self);

    nautilus_file_list_free (self->files);

    G_OBJECT_CLASS (nautilus_starred_directory_parent_class)->finalize (object);
}

static void
nautilus_starred_directory_dispose (GObject *object)
{
    NautilusFavoriteDirectory *starred;
    GList *l;

    starred = NAUTILUS_STARRED_DIRECTORY (object);

    /* Remove file connections */
    g_list_foreach (starred->files, (GFunc) disconnect_and_unmonitor_file, starred);

    /* Remove search monitors */
    if (starred->monitor_list)
    {
        for (l = starred->monitor_list; l != NULL; l = l->next)
        {
            starred_monitor_destroy ((FavoriteMonitor *) l->data, starred);
        }

        g_list_free (starred->monitor_list);
        starred->monitor_list = NULL;
    }

    G_OBJECT_CLASS (nautilus_starred_directory_parent_class)->dispose (object);
}

static void
nautilus_starred_directory_class_init (NautilusFavoriteDirectoryClass *klass)
{
    GObjectClass *oclass;
    NautilusDirectoryClass *directory_class;

    oclass = G_OBJECT_CLASS (klass);
    directory_class = NAUTILUS_DIRECTORY_CLASS (klass);

    oclass->finalize = nautilus_starred_directory_finalize;
    oclass->dispose = nautilus_starred_directory_dispose;

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
}

NautilusFavoriteDirectory *
nautilus_starred_directory_new (void)
{
    NautilusFavoriteDirectory *self;

    self = g_object_new (NAUTILUS_TYPE_STARRED_DIRECTORY, NULL);

    return self;
}

static void
nautilus_starred_directory_init (NautilusFavoriteDirectory *self)
{
    g_signal_connect (nautilus_tag_manager_get (),
                      "starred-changed",
                      (GCallback) on_starred_files_changed,
                      self);

    nautilus_starred_directory_set_files (self);
}
