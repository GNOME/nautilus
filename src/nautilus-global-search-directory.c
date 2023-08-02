/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-global-search-directory.h"

#include "nautilus-directory-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-scheme.h"

struct _NautilusGlobalSearchDirectory
{
    NautilusDirectory parent_slot;
};

G_DEFINE_TYPE_WITH_CODE (NautilusGlobalSearchDirectory, nautilus_global_search_directory, NAUTILUS_TYPE_DIRECTORY,
                         nautilus_ensure_extension_points ();
                         /* It looks like you’re implementing an extension point.
                          * Did you modify nautilus_ensure_extension_builtins() accordingly?
                          *
                          * • Yes
                          * • Doing it right now
                          */
                         g_io_extension_point_implement (NAUTILUS_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         NAUTILUS_GLOBAL_SEARCH_DIRECTORY_PROVIDER_NAME,
                                                         0));

static gboolean
real_contains_file (NautilusDirectory *directory,
                    NautilusFile      *file)
{
    return FALSE;
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
    GList *file_list = NULL;

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
    if (callback != NULL)
    {
        (*callback)(directory, NULL, callback_data);
    }
}

static void
real_monitor_remove (NautilusDirectory *directory,
                     gconstpointer      client)
{
}

static gboolean
real_handles_location (GFile *location)
{
    return g_file_has_uri_scheme (location, SCHEME_GLOBAL_SEARCH);
}

static void
real_cancel_callback (NautilusDirectory         *directory,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
}

static GList *
real_get_file_list (NautilusDirectory *directory)
{
    return NULL;
}

static void
real_force_reload (NautilusDirectory *directory)
{
}

static void
nautilus_global_search_directory_class_init (NautilusGlobalSearchDirectoryClass *klass)
{
    NautilusDirectoryClass *directory_class = NAUTILUS_DIRECTORY_CLASS (klass);

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

NautilusGlobalSearchDirectory *
nautilus_global_search_directory_new (void)
{
    return g_object_new (NAUTILUS_TYPE_GLOBAL_SEARCH_DIRECTORY, NULL);
}

static void
nautilus_global_search_directory_init (NautilusGlobalSearchDirectory *self)
{
}

