/*
 * nautilus-dbus-manager: nautilus DBus interface
 *
 * Copyright (C) 2010, Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nautilus-dbus-manager.h"
#include "nautilus-generated.h"

#include "nautilus-file-operations.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-file.h"

#define DEBUG_FLAG NAUTILUS_DEBUG_DBUS
#include "nautilus-debug.h"

struct _NautilusDBusManager
{
    GObject parent;

    NautilusDBusFileOperations *file_operations;
};

G_DEFINE_TYPE (NautilusDBusManager, nautilus_dbus_manager, G_TYPE_OBJECT);

static void
nautilus_dbus_manager_dispose (GObject *object)
{
    NautilusDBusManager *self = (NautilusDBusManager *) object;

    if (self->file_operations)
    {
        g_object_unref (self->file_operations);
        self->file_operations = NULL;
    }

    G_OBJECT_CLASS (nautilus_dbus_manager_parent_class)->dispose (object);
}

static void
undo_redo_on_finished (gpointer user_data)
{
    NautilusFileUndoManager *undo_manager = NULL;
    int *handler_id = (int *) user_data;

    undo_manager = nautilus_file_undo_manager_get ();
    g_signal_handler_disconnect (undo_manager, *handler_id);
    g_application_release (g_application_get_default ());
    g_free (handler_id);
}

static gboolean
handle_redo (NautilusDBusFileOperations *object,
             GDBusMethodInvocation      *invocation)
{
    NautilusFileUndoManager *undo_manager = NULL;
    gint *handler_id = g_new0(int, 1);

    g_application_hold (g_application_get_default ());

    undo_manager = nautilus_file_undo_manager_get ();
    *handler_id = g_signal_connect_swapped (undo_manager, "undo-changed",
                                            G_CALLBACK (undo_redo_on_finished),
                                            handler_id);
    nautilus_file_undo_manager_redo (NULL);

    nautilus_dbus_file_operations_complete_redo (object, invocation);
    return TRUE; /* invocation was handled */
}

static gboolean
handle_undo (NautilusDBusFileOperations *object,
             GDBusMethodInvocation      *invocation)
{
    NautilusFileUndoManager *undo_manager = NULL;
    gint *handler_id = g_new0(int, 1);

    g_application_hold (g_application_get_default ());

    undo_manager = nautilus_file_undo_manager_get ();
    *handler_id = g_signal_connect_swapped (undo_manager, "undo-changed",
                                            G_CALLBACK (undo_redo_on_finished),
                                            handler_id);
    nautilus_file_undo_manager_undo (NULL);

    nautilus_dbus_file_operations_complete_undo (object, invocation);
    return TRUE; /* invocation was handled */
}

static void
create_folder_on_finished (GFile    *new_file,
                           gboolean  success,
                           gpointer  callback_data)
{
    g_application_release (g_application_get_default ());
}

static gboolean
handle_create_folder (NautilusDBusFileOperations *object,
                      GDBusMethodInvocation      *invocation,
                      const gchar                *uri)
{
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) parent_file = NULL;
    g_autofree gchar *basename = NULL;
    g_autofree gchar *parent_file_uri = NULL;

    file = g_file_new_for_uri (uri);
    basename = g_file_get_basename (file);
    parent_file = g_file_get_parent (file);
    parent_file_uri = g_file_get_uri (parent_file);

    g_application_hold (g_application_get_default ());
    nautilus_file_operations_new_folder (NULL, parent_file_uri, basename,
                                         create_folder_on_finished, NULL);

    nautilus_dbus_file_operations_complete_create_folder (object, invocation);
    return TRUE; /* invocation was handled */
}

static void
copy_move_on_finished (GHashTable *debutting_uris,
                       gboolean    success,
                       gpointer    callback_data)
{
    g_application_release (g_application_get_default ());
}

static gboolean
handle_copy_uris (NautilusDBusFileOperations  *object,
                  GDBusMethodInvocation       *invocation,
                  const gchar                **sources,
                  const gchar                 *destination)
{
    GList *source_files = NULL;
    gint idx;

    for (idx = 0; sources[idx] != NULL; idx++)
    {
        source_files = g_list_prepend (source_files, g_strdup (sources[idx]));
    }

    g_application_hold (g_application_get_default ());
    nautilus_file_operations_copy_move (source_files, destination,
                                        GDK_ACTION_COPY, NULL, copy_move_on_finished, NULL);

    g_list_free_full (source_files, g_free);
    nautilus_dbus_file_operations_complete_copy_uris (object, invocation);
    return TRUE; /* invocation was handled */
}

static gboolean
handle_move_uris (NautilusDBusFileOperations  *object,
                  GDBusMethodInvocation       *invocation,
                  const gchar                **sources,
                  const gchar                 *destination)
{
    GList *source_files = NULL;
    gint idx;

    for (idx = 0; sources[idx] != NULL; idx++)
    {
        source_files = g_list_prepend (source_files, g_strdup (sources[idx]));
    }

    g_application_hold (g_application_get_default ());
    nautilus_file_operations_copy_move (source_files, destination,
                                        GDK_ACTION_MOVE, NULL, copy_move_on_finished, NULL);

    g_list_free_full (source_files, g_free);
    nautilus_dbus_file_operations_complete_copy_uris (object, invocation);
    return TRUE; /* invocation was handled */
}

/* FIXME: Needs a callback for maintaining alive the application */
static gboolean
handle_empty_trash (NautilusDBusFileOperations *object,
                    GDBusMethodInvocation      *invocation)
{
    nautilus_file_operations_empty_trash (NULL);

    nautilus_dbus_file_operations_complete_empty_trash (object, invocation);
    return TRUE; /* invocation was handled */
}

static void
trash_on_finished (GHashTable *debutting_uris,
                   gboolean    user_cancel,
                   gpointer    callback_data)
{
    g_application_release (g_application_get_default ());
}

static gboolean
handle_trash_files (NautilusDBusFileOperations  *object,
                    GDBusMethodInvocation       *invocation,
                    const gchar                **sources)
{
    g_autolist (GFile) source_files = NULL;
    gint idx;

    for (idx = 0; sources[idx] != NULL; idx++)
    {
        source_files = g_list_prepend (source_files,
                                       g_file_new_for_uri (sources[idx]));
    }

    g_application_hold (g_application_get_default ());
    nautilus_file_operations_trash_or_delete_async (source_files, NULL,
                                                    trash_on_finished, NULL);

    nautilus_dbus_file_operations_complete_trash_files (object, invocation);
    return TRUE; /* invocation was handled */
}

static void
rename_file_on_finished (NautilusFile *file,
                         GFile        *result_location,
                         GError       *error,
                         gpointer      callback_data)
{
    g_application_release (g_application_get_default ());
}

static gboolean
handle_rename_file (NautilusDBusFileOperations *object,
                    GDBusMethodInvocation      *invocation,
                    const gchar                *uri,
                    const gchar                *new_name)
{
    NautilusFile *file = NULL;

    file = nautilus_file_get_by_uri (uri);

    g_application_hold (g_application_get_default ());
    nautilus_file_rename (file, new_name,
                          rename_file_on_finished, NULL);

    nautilus_dbus_file_operations_complete_rename_file (object, invocation);

    return TRUE; /* invocation was handled */
}


static void
undo_manager_changed (NautilusDBusManager *self)
{
    NautilusFileUndoManagerState undo_state;

    undo_state = nautilus_file_undo_manager_get_state ();
    nautilus_dbus_file_operations_set_undo_status (self->file_operations,
                                                   undo_state);
}

static void
nautilus_dbus_manager_init (NautilusDBusManager *self)
{
    self->file_operations = nautilus_dbus_file_operations_skeleton_new ();

    g_signal_connect (self->file_operations,
                      "handle-copy-uris",
                      G_CALLBACK (handle_copy_uris),
                      self);
    g_signal_connect (self->file_operations,
                      "handle-move-uris",
                      G_CALLBACK (handle_move_uris),
                      self);
    g_signal_connect (self->file_operations,
                      "handle-empty-trash",
                      G_CALLBACK (handle_empty_trash),
                      self);
    g_signal_connect (self->file_operations,
                      "handle-trash-files",
                      G_CALLBACK (handle_trash_files),
                      self);
    g_signal_connect (self->file_operations,
                      "handle-create-folder",
                      G_CALLBACK (handle_create_folder),
                      self);
    g_signal_connect (self->file_operations,
                      "handle-undo",
                      G_CALLBACK (handle_undo),
                      self);
    g_signal_connect (self->file_operations,
                      "handle-redo",
                      G_CALLBACK (handle_redo),
                      self);
}

static void
nautilus_dbus_manager_class_init (NautilusDBusManagerClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->dispose = nautilus_dbus_manager_dispose;
}

NautilusDBusManager *
nautilus_dbus_manager_new (void)
{
    return g_object_new (NAUTILUS_TYPE_DBUS_MANAGER, NULL);
}

gboolean
nautilus_dbus_manager_register (NautilusDBusManager  *self,
                                GDBusConnection      *connection,
                                GError              **error)
{
    gboolean succes;

    succes = g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->file_operations),
                                               connection, "/org/gnome/Nautilus" PROFILE, error);
    if (succes)
    {
        g_signal_connect_object (nautilus_file_undo_manager_get (),
                                 "undo-changed",
                                 G_CALLBACK (undo_manager_changed),
                                 self,
                                 G_CONNECT_SWAPPED);

        undo_manager_changed (self);
    }

    return succes;
}

void
nautilus_dbus_manager_unregister (NautilusDBusManager *self)
{
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->file_operations));

    g_signal_handlers_disconnect_by_data (nautilus_file_undo_manager_get (), self);
}
