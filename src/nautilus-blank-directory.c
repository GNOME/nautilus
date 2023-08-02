/*
 * Copyright (C) 2023 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-blank-directory.h"

#include "nautilus-directory-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-scheme.h"

struct _NautilusBlankDirectory
{
    NautilusDirectory parent_slot;
};

G_DEFINE_TYPE_WITH_CODE (NautilusBlankDirectory, nautilus_blank_directory, NAUTILUS_TYPE_DIRECTORY,
                         nautilus_ensure_extension_points ();
                         /* It looks like you’re implementing an extension point.
                          * Did you modify nautilus_ensure_extension_builtins() accordingly?
                          *
                          * • Yes
                          * • Doing it right now
                          */
                         g_io_extension_point_implement (NAUTILUS_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         NAUTILUS_BLANK_DIRECTORY_PROVIDER_NAME,
                                                         0));

static void
real_call_when_ready (NautilusDirectory         *directory,
                      NautilusFileAttributes     file_attributes,
                      gboolean                   wait_for_file_list,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    (*callback)(directory, NULL, callback_data);
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

    nautilus_directory_emit_done_loading (directory);
}

static gboolean
real_handles_location (GFile *location)
{
    return g_file_has_uri_scheme (location, SCHEME_BLANK);
}

static gboolean
real_is_editable (NautilusDirectory *directory)
{
    return FALSE;
}

static void
no_op (void)
{
}

static void
nautilus_blank_directory_init (NautilusBlankDirectory *self)
{
}

static void
nautilus_blank_directory_class_init (NautilusBlankDirectoryClass *klass)
{
    NautilusDirectoryClass *directory_class = NAUTILUS_DIRECTORY_CLASS (klass);

    directory_class->are_all_files_seen = real_are_all_files_seen;
    directory_class->call_when_ready = real_call_when_ready;
    directory_class->cancel_callback = (void (*)(NautilusDirectory *, NautilusDirectoryCallback, gpointer)) no_op;
    directory_class->file_monitor_add = real_file_monitor_add;
    directory_class->file_monitor_remove = (void (*)(NautilusDirectory *, gconstpointer)) no_op;
    directory_class->force_reload = (void (*)(NautilusDirectory *)) no_op;
    directory_class->handles_location = real_handles_location;
    directory_class->is_editable = real_is_editable;
}
