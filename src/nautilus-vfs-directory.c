/*
 * nautilus-vfs-directory.c: Subclass of NautilusDirectory to help implement the
 * virtual trash directory.
 *
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nautilus-vfs-directory.h"

#include "nautilus-directory-private.h"
#include "nautilus-file-private.h"

G_DEFINE_TYPE (NautilusVFSDirectory, nautilus_vfs_directory, NAUTILUS_TYPE_DIRECTORY);

static void
nautilus_vfs_directory_init (NautilusVFSDirectory *directory)
{
}

static void
vfs_call_when_ready (NautilusDirectory         *directory,
                     NautilusAttributes         attributes,
                     NautilusDirectoryCallback  callback,
                     gpointer                   callback_data)
{
    g_assert (NAUTILUS_IS_VFS_DIRECTORY (directory));

    nautilus_directory_call_when_ready_internal
        (directory,
        NULL,
        attributes,
        callback,
        NULL,
        callback_data);
}

static void
vfs_cancel_callback (NautilusDirectory         *directory,
                     NautilusDirectoryCallback  callback,
                     gpointer                   callback_data)
{
    g_assert (NAUTILUS_IS_VFS_DIRECTORY (directory));

    nautilus_directory_cancel_callback_internal
        (directory,
        NULL,
        callback,
        NULL,
        callback_data);
}

static void
vfs_file_monitor_add (NautilusDirectory         *directory,
                      gconstpointer              client,
                      gboolean                   monitor_hidden_files,
                      NautilusAttributes         attributes,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    g_assert (NAUTILUS_IS_VFS_DIRECTORY (directory));
    g_assert (client != NULL);

    nautilus_directory_monitor_add_internal
        (directory, NULL,
        client,
        monitor_hidden_files,
        attributes,
        callback, callback_data);
}

static void
vfs_file_monitor_remove (NautilusDirectory *directory,
                         gconstpointer      client)
{
    g_assert (NAUTILUS_IS_VFS_DIRECTORY (directory));
    g_assert (client != NULL);

    nautilus_directory_monitor_remove_internal (directory, NULL, client);
}

static void
vfs_force_reload (NautilusDirectory *directory)
{
    NautilusAttributes all_attributes;

    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    all_attributes = nautilus_file_get_all_attributes ();
    nautilus_directory_force_reload_internal (directory,
                                              all_attributes);
}

static void
nautilus_vfs_directory_class_init (NautilusVFSDirectoryClass *klass)
{
    NautilusDirectoryClass *directory_class = NAUTILUS_DIRECTORY_CLASS (klass);

    directory_class->call_when_ready = vfs_call_when_ready;
    directory_class->cancel_callback = vfs_cancel_callback;
    directory_class->file_monitor_add = vfs_file_monitor_add;
    directory_class->file_monitor_remove = vfs_file_monitor_remove;
    directory_class->force_reload = vfs_force_reload;
}
