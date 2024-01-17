/*
 * Copyright (C) 2024 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-network-directory.h"

#include <gio/gio.h>

#include "nautilus-directory-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-scheme.h"


struct _NautilusNetworkDirectory
{
    NautilusDirectory parent_slot;

    NautilusDirectory *computer_backend_directory;
    gboolean computer_backend_done_loading;

    NautilusDirectory *network_backend_directory;
    gboolean network_backend_done_loading;

    GList /*<owned NetworkCallback>*/ *callback_list;
};

G_DEFINE_TYPE_WITH_CODE (NautilusNetworkDirectory, nautilus_network_directory, NAUTILUS_TYPE_DIRECTORY,
                         nautilus_ensure_extension_points ();
                         /* It looks like you’re implementing an extension point.
                          * Did you modify nautilus_ensure_extension_builtins() accordingly?
                          *
                          * • Yes
                          * • Doing it right now
                          */
                         g_io_extension_point_implement (NAUTILUS_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         NAUTILUS_NETWORK_DIRECTORY_PROVIDER_NAME,
                                                         0));

typedef struct
{
    NautilusNetworkDirectory *self;

    NautilusFileAttributes file_attributes;
    NautilusDirectoryCallback callback;
    gpointer callback_data;

    gboolean computer_backend_ready;
    gboolean network_backend_ready;
} NetworkCallback;

static gboolean
target_is_local (NautilusFile *file)
{
    g_autoptr (GIcon) icon = nautilus_file_get_gicon (file, NAUTILUS_FILE_ICON_FLAGS_NONE);

    /* It would be nice to have a "mountable::remote" attribute instead. */
    if (G_IS_THEMED_ICON (icon))
    {
        const gchar * const *names = g_themed_icon_get_names (G_THEMED_ICON (icon));

        for (int i = 0; names[i] != NULL; i++)
        {
            if (strstr (names[i], "network") || strstr (names[i], "remote"))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

static void
remove_local (NautilusFile  *file,
              GList        **list)
{
    if (target_is_local (file))
    {
        *list = g_list_remove (*list, file);
        g_object_unref (file);
    }
}

static gboolean
real_are_all_files_seen (NautilusDirectory *directory)
{
    NautilusNetworkDirectory *self = NAUTILUS_NETWORK_DIRECTORY (directory);

    return (nautilus_directory_are_all_files_seen (self->computer_backend_directory) &&
            nautilus_directory_are_all_files_seen (self->network_backend_directory));
}

static gboolean
real_contains_file (NautilusDirectory *directory,
                    NautilusFile      *file)
{
    NautilusNetworkDirectory *self = NAUTILUS_NETWORK_DIRECTORY (directory);

    if (nautilus_directory_contains_file (self->network_backend_directory, file))
    {
        return TRUE;
    }
    if (nautilus_directory_contains_file (self->computer_backend_directory, file))
    {
        return !target_is_local (file);
    }

    return FALSE;
}

static void
on_backend_directory_done_loading (NautilusDirectory *backend_directory,
                                   gpointer           callback_data)
{
    NautilusNetworkDirectory *self = callback_data;

    if (backend_directory == self->computer_backend_directory)
    {
        self->computer_backend_done_loading = TRUE;
    }
    else if (backend_directory == self->network_backend_directory)
    {
        self->network_backend_done_loading = TRUE;
    }
    else
    {
        g_assert_not_reached ();
    }

    if (self->computer_backend_done_loading &&
        self->network_backend_done_loading)
    {
        nautilus_directory_emit_done_loading (NAUTILUS_DIRECTORY (self));
    }
}

static void
real_force_reload (NautilusDirectory *directory)
{
    NautilusNetworkDirectory *self = NAUTILUS_NETWORK_DIRECTORY (directory);

    self->computer_backend_done_loading = FALSE;
    nautilus_directory_force_reload (self->computer_backend_directory);

    self->network_backend_done_loading = FALSE;
    nautilus_directory_force_reload (self->network_backend_directory);
}

static void
on_backend_directory_ready (NautilusDirectory *backend_directory,
                            GList             *unused_parameter,
                            gpointer           callback_data)
{
    NetworkCallback *network_callback = callback_data;
    NautilusNetworkDirectory *self = network_callback->self;

    if (backend_directory == self->computer_backend_directory)
    {
        network_callback->computer_backend_ready = TRUE;
    }
    else if (backend_directory == self->network_backend_directory)
    {
        network_callback->network_backend_ready = TRUE;
    }
    else
    {
        g_assert_not_reached ();
    }

    if (network_callback->computer_backend_ready &&
        network_callback->network_backend_ready)
    {
        g_autolist (NautilusFile) files = nautilus_directory_get_file_list (NAUTILUS_DIRECTORY (self));

        /* Invoke ready callback */
        (*network_callback->callback)(NAUTILUS_DIRECTORY (self), files, network_callback->callback_data);

        /* Remove it from pending callbacks list and free it */
        self->callback_list = g_list_remove (self->callback_list, network_callback);
        g_free (network_callback);
    }
}

static GList *
filter_local (GList *list)
{
    g_list_foreach (list, (GFunc) remove_local, &list);
    return list;
}

static void
on_computer_backend_directory_files_added (NautilusNetworkDirectory *self,
                                           GList                    *added_files)
{
    g_autolist (NautilusFile) computer_files = filter_local (nautilus_file_list_copy (added_files));

    nautilus_directory_emit_files_added (NAUTILUS_DIRECTORY (self), computer_files);
}

static void
on_computer_backend_directory_files_changed (NautilusNetworkDirectory *self,
                                             GList                    *changed_files)
{
    g_autolist (NautilusFile) computer_files = filter_local (nautilus_file_list_copy (changed_files));

    nautilus_directory_emit_files_changed (NAUTILUS_DIRECTORY (self), computer_files);
}

static NetworkCallback *
network_callback_find (NautilusNetworkDirectory  *self,
                       NautilusDirectoryCallback  callback,
                       gpointer                   callback_data)
{
    for (GList *l = self->callback_list; l != NULL; l = l->next)
    {
        NetworkCallback *network_callback = l->data;

        if (network_callback->callback == callback &&
            network_callback->callback_data == callback_data)
        {
            return network_callback;
        }
    }
    return NULL;
}

static void
real_call_when_ready (NautilusDirectory         *directory,
                      NautilusFileAttributes     file_attributes,
                      gboolean                   wait_for_file_list,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    NautilusNetworkDirectory *self = NAUTILUS_NETWORK_DIRECTORY (directory);
    NetworkCallback *network_callback;

    network_callback = network_callback_find (self, callback, callback_data);
    if (network_callback != NULL)
    {
        g_warning ("tried to add a new callback while an old one was pending");
        return;
    }

    network_callback = g_new0 (NetworkCallback, 1);
    network_callback->self = self;
    network_callback->file_attributes = file_attributes;
    network_callback->callback = callback;
    network_callback->callback_data = callback_data;

    self->callback_list = g_list_prepend (self->callback_list, network_callback);

    nautilus_directory_call_when_ready (self->computer_backend_directory,
                                        file_attributes,
                                        wait_for_file_list,
                                        on_backend_directory_ready, network_callback);
    nautilus_directory_call_when_ready (self->network_backend_directory,
                                        file_attributes,
                                        wait_for_file_list,
                                        on_backend_directory_ready, network_callback);
}

static void
real_cancel_callback (NautilusDirectory         *directory,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    NautilusNetworkDirectory *self = NAUTILUS_NETWORK_DIRECTORY (directory);
    NetworkCallback *network_callback;

    network_callback = network_callback_find (self, callback, callback_data);
    if (network_callback == NULL)
    {
        /* No warning needed, because nautilus_directory_cancel_callback() is
         * meant to be used unconditionally as cleanup. */
        return;
    }

    if (!network_callback->computer_backend_ready)
    {
        nautilus_directory_cancel_callback (self->computer_backend_directory, on_backend_directory_ready, network_callback);
    }
    if (!network_callback->network_backend_ready)
    {
        nautilus_directory_cancel_callback (self->network_backend_directory, on_backend_directory_ready, network_callback);
    }

    self->callback_list = g_list_remove (self->callback_list, network_callback);
    g_free (network_callback);
}

static void
real_file_monitor_add (NautilusDirectory         *directory,
                       gconstpointer              client,
                       gboolean                   monitor_hidden_files,
                       NautilusFileAttributes     file_attributes,
                       NautilusDirectoryCallback  callback,
                       gpointer                   callback_data)
{
    NautilusNetworkDirectory *self = NAUTILUS_NETWORK_DIRECTORY (directory);

    nautilus_directory_file_monitor_add (self->computer_backend_directory,
                                         client,
                                         monitor_hidden_files,
                                         file_attributes,
                                         NULL, NULL);
    nautilus_directory_file_monitor_add (self->network_backend_directory,
                                         client,
                                         monitor_hidden_files,
                                         file_attributes,
                                         NULL, NULL);
    if (callback != NULL)
    {
        g_autolist (NautilusFile) files = nautilus_directory_get_file_list (directory);

        (*callback)(directory, files, callback_data);
    }
}

static void
real_file_monitor_remove (NautilusDirectory *directory,
                          gconstpointer      client)
{
    NautilusNetworkDirectory *self = NAUTILUS_NETWORK_DIRECTORY (directory);

    nautilus_directory_file_monitor_remove (self->computer_backend_directory, client);
    nautilus_directory_file_monitor_remove (self->network_backend_directory, client);
}

static GList *
real_get_file_list (NautilusDirectory *directory)
{
    NautilusNetworkDirectory *self = NAUTILUS_NETWORK_DIRECTORY (directory);
    g_autolist (NautilusFile) computer_list = filter_local (nautilus_directory_get_file_list (self->computer_backend_directory));
    g_autolist (NautilusFile) network_list = nautilus_directory_get_file_list (self->network_backend_directory);

    return g_list_concat (g_steal_pointer (&computer_list),
                          g_steal_pointer (&network_list));
}

static gboolean
real_is_not_empty (NautilusDirectory *directory)
{
    NautilusNetworkDirectory *self = NAUTILUS_NETWORK_DIRECTORY (directory);

    return (nautilus_directory_is_not_empty (self->computer_backend_directory) &&
            nautilus_directory_is_not_empty (self->network_backend_directory));
}

static gboolean
real_is_editable (NautilusDirectory *directory)
{
    return FALSE;
}

static gboolean
real_handles_location (GFile *location)
{
    return g_file_has_uri_scheme (location, SCHEME_NETWORK_VIEW);
}

static void
nautilus_network_directory_dispose (GObject *object)
{
    NautilusNetworkDirectory *self = NAUTILUS_NETWORK_DIRECTORY (object);

    g_clear_object (&self->computer_backend_directory);
    g_clear_object (&self->network_backend_directory);
    g_clear_list (&self->callback_list, g_free);

    G_OBJECT_CLASS (nautilus_network_directory_parent_class)->dispose (object);
}

static void
nautilus_network_directory_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_network_directory_parent_class)->finalize (object);
}

static void
nautilus_network_directory_init (NautilusNetworkDirectory *self)
{
    self->computer_backend_directory = nautilus_directory_get_by_uri ("computer:///");
    g_signal_connect_object (self->computer_backend_directory, "files-added",
                             G_CALLBACK (on_computer_backend_directory_files_added), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->computer_backend_directory, "files-changed",
                             G_CALLBACK (on_computer_backend_directory_files_changed), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->computer_backend_directory, "done-loading",
                             G_CALLBACK (on_backend_directory_done_loading), self, G_CONNECT_DEFAULT);
    g_signal_connect_object (self->computer_backend_directory, "load-error",
                             G_CALLBACK (nautilus_directory_emit_load_error), self, G_CONNECT_SWAPPED);

    self->network_backend_directory = nautilus_directory_get_by_uri (SCHEME_NETWORK ":///");
    g_signal_connect_object (self->network_backend_directory, "files-added",
                             G_CALLBACK (nautilus_directory_emit_files_added), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->network_backend_directory, "files-changed",
                             G_CALLBACK (nautilus_directory_emit_files_changed), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->network_backend_directory, "done-loading",
                             G_CALLBACK (on_backend_directory_done_loading), self, G_CONNECT_DEFAULT);
    g_signal_connect_object (self->network_backend_directory, "load-error",
                             G_CALLBACK (nautilus_directory_emit_load_error), self, G_CONNECT_SWAPPED);
}

static void
nautilus_network_directory_class_init (NautilusNetworkDirectoryClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusDirectoryClass *directory_class = NAUTILUS_DIRECTORY_CLASS (klass);

    object_class->finalize = nautilus_network_directory_finalize;
    object_class->dispose = nautilus_network_directory_dispose;

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
}

NautilusNetworkDirectory *
nautilus_network_directory_new (void)
{
    return g_object_new (NAUTILUS_TYPE_NETWORK_DIRECTORY, NULL);
}
