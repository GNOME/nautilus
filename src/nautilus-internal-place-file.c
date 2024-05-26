/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#include "nautilus-internal-place-file.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "nautilus-file-private.h"
#include "nautilus-scheme.h"
#include "nautilus-network-directory.h"
#include "nautilus-starred-directory.h"

struct _NautilusInternalPlaceFile
{
    NautilusFile parent_instance;

    GList *callbacks;
    GCancellable *network_mount_cancellable;
};

G_DEFINE_TYPE (NautilusInternalPlaceFile, nautilus_internal_place_file, NAUTILUS_TYPE_FILE);

static void
real_monitor_add (NautilusFile           *file,
                  gconstpointer           client,
                  NautilusFileAttributes  attributes)
{
    /* Internal place attributes are static, so there is nothing to monitor. */
}

static void
real_monitor_remove (NautilusFile  *file,
                     gconstpointer  client)
{
}

typedef struct
{
    NautilusFileCallback callback;
    gpointer callback_data;
} InternalPlaceFileCallback;

static void
network_mount_callback (GObject      *source_object,
                        GAsyncResult *result,
                        gpointer      data)
{
    NautilusInternalPlaceFile *self = NAUTILUS_INTERNAL_PLACE_FILE (data);
    g_autoptr (GError) error = NULL;

    (void) g_file_mount_enclosing_volume_finish (G_FILE (source_object), result, &error);
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        return;
    }

    /* Clear cancellable and steal callbacks list because call_when_ready() may
     * be called again inside the `for` loop. */
    g_clear_object (&self->network_mount_cancellable);

    GList *callbacks = g_steal_pointer (&self->callbacks);
    for (GList *l = callbacks; l != NULL; l = l->next)
    {
        InternalPlaceFileCallback *callback = l->data;

        (*callback->callback)(NAUTILUS_FILE (self), callback->callback_data);
    }
    g_list_free_full (callbacks, g_free);
}

static void
real_call_when_ready (NautilusFile           *file,
                      NautilusFileAttributes  file_attributes,
                      NautilusFileCallback    callback,
                      gpointer                callback_data)
{
    NautilusInternalPlaceFile *self = NAUTILUS_INTERNAL_PLACE_FILE (file);

    if (NAUTILUS_IS_NETWORK_DIRECTORY (file->details->directory))
    {
        /* WORKAROUND:
         * We must ensure network:/// is mounted before calling it "ready",
         * otherwise GDaemonFile makes sync D-Bus calls to mount network://
         * when the view tries do add a monitor, blocking the main thread and
         * freezing the UI.
         * See: https://gitlab.gnome.org/GNOME/gvfs/-/issues/455
         */

        if (callback != NULL)
        {
            InternalPlaceFileCallback data = {callback, callback_data};
            self->callbacks = g_list_prepend (self->callbacks,
                                              g_memdup2 (&data, sizeof (data)));
        }

        /* If there is a cancellable, we are already mounting */
        if (self->network_mount_cancellable == NULL)
        {
            g_autoptr (GFile) network_backend = g_file_new_for_uri (SCHEME_NETWORK ":///");

            self->network_mount_cancellable = g_cancellable_new ();
            g_file_mount_enclosing_volume (network_backend,
                                           G_MOUNT_MOUNT_NONE,
                                           NULL,
                                           self->network_mount_cancellable,
                                           network_mount_callback, self);
        }
    }
    else if (callback != NULL)
    {
        /* Internal place attributes are static, so its always ready. */
        (*callback)(file, callback_data);
    }
}

static void
real_cancel_call_when_ready (NautilusFile         *file,
                             NautilusFileCallback  callback,
                             gpointer              callback_data)
{
    NautilusInternalPlaceFile *self = NAUTILUS_INTERNAL_PLACE_FILE (file);

    g_clear_list (&self->callbacks, g_free);
    g_cancellable_cancel (self->network_mount_cancellable);
    g_clear_object (&self->network_mount_cancellable);
}

static gboolean
real_check_if_ready (NautilusFile           *file,
                     NautilusFileAttributes  attributes)
{
    /* Internal place attributes are static, so its always ready. */
    return TRUE;
}

static void
nautilus_internal_place_file_init (NautilusInternalPlaceFile *self)
{
}

static void
nautilus_internal_place_file_constructed (GObject *object)
{
    G_OBJECT_CLASS (nautilus_internal_place_file_parent_class)->constructed (object);

    NautilusInternalPlaceFile *self = NAUTILUS_INTERNAL_PLACE_FILE (object);
    NautilusFile *file = NAUTILUS_FILE (self);

    file->details->mime_type = g_ref_string_new_intern ("inode/directory");
    file->details->size = 0;

    if (NAUTILUS_IS_NETWORK_DIRECTORY (file->details->directory))
    {
        nautilus_file_set_display_name (file, _("Network"), NULL, TRUE);
    }
    else if (NAUTILUS_IS_STARRED_DIRECTORY (file->details->directory))
    {
        nautilus_file_set_display_name (file, _("Starred"), NULL, TRUE);
    }

    file->details->got_file_info = TRUE;
    file->details->file_info_is_up_to_date = TRUE;
}

static void
nautilus_internal_place_file_dispose (GObject *object)
{
    NautilusInternalPlaceFile *self = NAUTILUS_INTERNAL_PLACE_FILE (object);

    g_clear_list (&self->callbacks, g_free);
    g_cancellable_cancel (self->network_mount_cancellable);
    g_clear_object (&self->network_mount_cancellable);

    G_OBJECT_CLASS (nautilus_internal_place_file_parent_class)->dispose (object);
}

static void
nautilus_internal_place_file_class_init (NautilusInternalPlaceFileClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusFileClass *file_class = NAUTILUS_FILE_CLASS (klass);

    /* We need to know the parent directory, which is a construction property.*/
    object_class->constructed = nautilus_internal_place_file_constructed;
    object_class->dispose = nautilus_internal_place_file_dispose;

    file_class->default_file_type = G_FILE_TYPE_DIRECTORY;

    file_class->monitor_add = real_monitor_add;
    file_class->monitor_remove = real_monitor_remove;
    file_class->call_when_ready = real_call_when_ready;
    file_class->cancel_call_when_ready = real_cancel_call_when_ready;
    file_class->check_if_ready = real_check_if_ready;
}
