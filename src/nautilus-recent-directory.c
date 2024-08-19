/*
 * Copyright (C) 2024 Corey Berla <coreyberla@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-recent-directory.h"

#include <gio/gio.h>

#include "nautilus-directory-private.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-internal-place-file.h"
#include "nautilus-recent-servers.h"
#include "nautilus-scheme.h"
#include "nautilus-window.h"
#include "nautilus-window-slot.h"

#define NUM_RECENT_WINDOWS 5

struct _NautilusRecentDirectory
{
    NautilusDirectory parent_slot;

    GList *recent_windows;
    GHashTable *files_hash;
    gboolean include_windows;

    GList *callback_list;
    NautilusDirectory *backend;
};

typedef struct
{
    NautilusRecentDirectory *self;

    NautilusDirectoryCallback callback;
    gpointer callback_data;
} RecentCallbackData;

G_DEFINE_TYPE_WITH_CODE (NautilusRecentDirectory, nautilus_recent_directory, NAUTILUS_TYPE_DIRECTORY,
                         nautilus_ensure_extension_points ();
                         /* It looks like you’re implementing an extension point.
                          * Did you modify nautilus_ensure_extension_builtins() accordingly?
                          *
                          * • Yes
                          * • Doing it right now
                          */
                         g_io_extension_point_implement (NAUTILUS_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         NAUTILUS_RECENT_DIRECTORY_PROVIDER_NAME,
                                                         0));


static RecentCallbackData *
recent_callback_find (NautilusRecentDirectory   *self,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    for (GList *l = self->callback_list; l != NULL; l = l->next)
    {
        RecentCallbackData *recent_callback = l->data;

        if (recent_callback->callback == callback &&
            recent_callback->callback_data == callback_data)
        {
            return recent_callback;
        }
    }
    return NULL;
}

static RecentCallbackData *
recent_callback_data_new (NautilusRecentDirectory  *self,
                          NautilusDirectoryCallback callback,
                          gpointer                  callback_data)
{
    RecentCallbackData *data = g_new0 (RecentCallbackData, 1);

    data->self = self;
    data->callback = callback;
    data->callback_data = callback_data;

    return data;
}

static gboolean
nautilus_recent_directory_file_is_recent_window (NautilusRecentDirectory *self,
                                                 NautilusFile            *file)
{
    g_autofree char *uri = nautilus_file_get_activation_uri (file);

    for (GList *l = self->recent_windows; l != NULL; l = l->next)
    {
        g_autofree char *activation_uri = nautilus_file_get_activation_uri (l->data);

        if (g_strcmp0 (uri, activation_uri) == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
real_are_all_files_seen (NautilusDirectory *directory)
{
    NautilusRecentDirectory *self = NAUTILUS_RECENT_DIRECTORY (directory);

    return nautilus_directory_are_all_files_seen (self->backend);
}

static gboolean
real_contains_file (NautilusDirectory *directory,
                    NautilusFile      *file)
{
    NautilusRecentDirectory *self = NAUTILUS_RECENT_DIRECTORY (directory);

    if (nautilus_file_get_directory (file) == directory)
    {
        /* Recent server files are directly owned by NautilusNetworkDirectory. */
        return TRUE;
    }

    return nautilus_directory_contains_file (self->backend, file);
}

static void
real_force_reload (NautilusDirectory *directory)
{
    NautilusRecentDirectory *self = NAUTILUS_RECENT_DIRECTORY (directory);

    g_clear_pointer (&self->recent_windows, nautilus_file_list_free);

    nautilus_directory_force_reload (self->backend);
}

static NautilusFile *
get_virtual_file (GFile *location,
                  gint64 unix_time)
{
    g_autofree char *uri = g_file_get_uri (location);
    g_autoptr (NautilusFile) orig_file = nautilus_file_get (location);
    const char *display_name = nautilus_file_get_display_name (orig_file);

    NautilusFile *file;

    g_autoptr (GFileInfo) info = g_file_info_new ();
    g_autofree char *random_name = g_dbus_generate_guid ();
    g_autoptr (GIcon) icon = g_themed_icon_new ("folder");
    g_autoptr (GIcon) symbolic_icon = g_themed_icon_new ("folder-symbolic");

    g_file_info_set_name (info, random_name);
    g_file_info_set_icon (info, icon);
    g_file_info_set_symbolic_icon (info, symbolic_icon);
    g_file_info_set_content_type (info, "inode/directory");
    g_file_info_set_file_type (info, G_FILE_TYPE_SHORTCUT);
    g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, display_name);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL, TRUE);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, FALSE);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME, FALSE);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE, FALSE);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH, FALSE);

    g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI, uri);
    g_file_info_set_attribute_int64 (info, "recent::modified", unix_time);

    g_autofree char *virtual_uri = g_strconcat (SCHEME_NAUTILUS_RECENT ":///", random_name, NULL);
    file = nautilus_file_get_by_uri (virtual_uri);
    nautilus_file_update_info (file, info);

    return file;
}

static guint
add_recent_window (NautilusRecentDirectory *self,
                   GFile                   *location,
                   gint64                   unix_time)
{
    g_return_val_if_fail (location != NULL, 0);

    g_autofree char *scheme = g_file_get_uri_scheme (location);
    g_autoptr (NautilusFile) file = get_virtual_file (location, unix_time);


    if (nautilus_scheme_is_internal (scheme) ||
        nautilus_recent_directory_file_is_recent_window (self, file))
    {
        return 0;
    }

    self->recent_windows = g_list_prepend (self->recent_windows, g_steal_pointer (&file));
    return 1;
}

static void
update_recent_windows (NautilusRecentDirectory *self)
{
    GApplication *app = g_application_get_default ();
    GList *windows = gtk_application_get_windows (GTK_APPLICATION (app));
    g_autoptr (GDateTime) now = g_date_time_new_now_local ();
    gint64 unix_time = g_date_time_to_unix (now);
    guint i = 0;

    g_clear_pointer (&self->recent_windows, nautilus_file_list_free);

    /* First add the active slots which are more likely "recent" */

    for (GList *l = windows; l != NULL && i < NUM_RECENT_WINDOWS; l = l->next)
    {
        NautilusWindowSlot *slot = nautilus_window_get_active_slot (l->data);
        i += add_recent_window (self, nautilus_window_slot_get_location (slot), unix_time - i);
    }

    for (GList *l = windows; l != NULL && i < NUM_RECENT_WINDOWS; l = l->next)
    {
        GList *slots = nautilus_window_get_slots (l->data);
        for (GList *slot = slots; slot != NULL && i < NUM_RECENT_WINDOWS; slot = slot->next)
        {
            i += add_recent_window (self, nautilus_window_slot_get_location (slot->data), unix_time - i);
        }
    }

    self->recent_windows = g_list_reverse (self->recent_windows);
}

void
nautilus_recent_directory_set_include_windows (NautilusRecentDirectory *self,
                                               gboolean                 include)
{
    if (self->include_windows == include)
    {
        return;
    }

    self->include_windows = include;

    if (include)
    {
        update_recent_windows (self);
    }
    else
    {
        GList *removed_files = g_steal_pointer (&self->recent_windows);
        nautilus_directory_emit_files_changed (NAUTILUS_DIRECTORY (self), removed_files);
        g_clear_pointer (&self->recent_windows, nautilus_file_list_free);
    }
}

static void
directory_ready (NautilusDirectory *directory,
                 GList             *files,
                 gpointer           callback_data)
{
    RecentCallbackData *data = (RecentCallbackData *) callback_data;
    NautilusRecentDirectory *self = data->self;

    data->callback (NAUTILUS_DIRECTORY (self), files, data->callback_data);
    self->callback_list = g_list_remove (self->callback_list, data);

    g_free (data);
}

static void
real_call_when_ready (NautilusDirectory         *directory,
                      NautilusFileAttributes     file_attributes,
                      gboolean                   wait_for_file_list,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    NautilusRecentDirectory *self = NAUTILUS_RECENT_DIRECTORY (directory);
    RecentCallbackData *data = recent_callback_find (self, callback, callback_data);

    if (data != NULL)
    {
        return;
    }

    data = recent_callback_data_new (self, callback, callback_data);
    self->callback_list = g_list_prepend (self->callback_list, data);

    nautilus_directory_call_when_ready (self->backend,
                                        file_attributes,
                                        wait_for_file_list,
                                        directory_ready,
                                        data);
}

static void
real_cancel_callback (NautilusDirectory         *directory,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    NautilusRecentDirectory *self = NAUTILUS_RECENT_DIRECTORY (directory);
    RecentCallbackData *data = recent_callback_find (self, callback, callback_data);

    if (data == NULL)
    {
        return;
    }

    self->callback_list = g_list_remove (self->callback_list, data);
    g_free (data);

    nautilus_directory_cancel_callback (self->backend, callback, callback_data);
}

static void
real_file_monitor_add (NautilusDirectory         *directory,
                       gconstpointer              client,
                       gboolean                   monitor_hidden_files,
                       NautilusFileAttributes     file_attributes,
                       NautilusDirectoryCallback  callback,
                       gpointer                   callback_data)
{
    NautilusRecentDirectory *self = NAUTILUS_RECENT_DIRECTORY (directory);

    nautilus_directory_file_monitor_add (self->backend, client, monitor_hidden_files, file_attributes, callback, callback_data);

    if (callback != NULL)
    {
        g_autolist (NautilusFile) file_list = nautilus_directory_get_file_list (directory);

        callback (directory, file_list, callback_data);
    }
}

static void
real_file_monitor_remove (NautilusDirectory *directory,
                          gconstpointer      client)
{
    NautilusRecentDirectory *self = NAUTILUS_RECENT_DIRECTORY (directory);

    nautilus_directory_file_monitor_remove (self->backend, client);
}

static GList *
real_get_file_list (NautilusDirectory *directory)
{
    NautilusRecentDirectory *self = NAUTILUS_RECENT_DIRECTORY (directory);

    /* Don't bother waiting because these directories have already been loaded */
    update_recent_windows (self);

    return g_list_concat (nautilus_directory_get_file_list (self->backend),
                          nautilus_file_list_copy (self->recent_windows));
}

static gboolean
real_is_not_empty (NautilusDirectory *directory)
{
    return nautilus_directory_is_not_empty (directory);
}

static gboolean
real_is_editable (NautilusDirectory *directory)
{
    return FALSE;
}

static gboolean
real_handles_location (GFile *location)
{
    return g_file_has_uri_scheme (location, SCHEME_NAUTILUS_RECENT);
}

static NautilusFile *
real_new_file_from_filename (NautilusDirectory *directory,
                             const char        *filename,
                             gboolean           self_owned)
{
    if (!self_owned)
    {
        /* Children are regular vfs locations. */
        return NAUTILUS_DIRECTORY_CLASS (nautilus_recent_directory_parent_class)->new_file_from_filename (directory, filename, self_owned);
    }

    return NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_INTERNAL_PLACE_FILE,
                                        "directory", directory,
                                        NULL));
}

static void
nautilus_recent_directory_dispose (GObject *object)
{
    NautilusRecentDirectory *self = NAUTILUS_RECENT_DIRECTORY (object);

    g_clear_object (&self->backend);
    g_clear_pointer (&self->recent_windows, nautilus_file_list_free);

    G_OBJECT_CLASS (nautilus_recent_directory_parent_class)->dispose (object);
}

static void
nautilus_recent_directory_init (NautilusRecentDirectory *self)
{
    self->backend = nautilus_directory_get_by_uri (SCHEME_RECENT ":///");

    g_signal_connect_object (self->backend, "files-added",
                             G_CALLBACK (nautilus_directory_emit_files_added), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->backend, "files-changed",
                             G_CALLBACK (nautilus_directory_emit_files_changed), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->backend, "done-loading",
                             G_CALLBACK (nautilus_directory_emit_done_loading), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->backend, "load-error",
                             G_CALLBACK (nautilus_directory_emit_load_error), self, G_CONNECT_SWAPPED);
}

static void
nautilus_recent_directory_class_init (NautilusRecentDirectoryClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusDirectoryClass *directory_class = NAUTILUS_DIRECTORY_CLASS (klass);

    object_class->dispose = nautilus_recent_directory_dispose;

    directory_class->are_all_files_seen = real_are_all_files_seen;
    directory_class->contains_file = real_contains_file;
    directory_class->force_reload = real_force_reload;
    directory_class->call_when_ready = real_call_when_ready;
    directory_class->cancel_callback = real_cancel_callback;
    directory_class->file_monitor_add = real_file_monitor_add;
    directory_class->file_monitor_remove = real_file_monitor_remove;
    directory_class->get_file_list = real_get_file_list;
    directory_class->is_not_empty = real_is_not_empty;
    directory_class->is_editable = real_is_editable;
    directory_class->handles_location = real_handles_location;
    directory_class->new_file_from_filename = real_new_file_from_filename;
}
