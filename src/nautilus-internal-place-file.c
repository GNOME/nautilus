/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#include "nautilus-internal-place-file.h"

#include <glib/gi18n.h>

#include "nautilus-file-private.h"
#include "nautilus-scheme.h"
#include "nautilus-starred-directory.h"

struct _NautilusInternalPlaceFile
{
    NautilusFile parent_instance;
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

static void
real_call_when_ready (NautilusFile           *file,
                      NautilusFileAttributes  file_attributes,
                      NautilusFileCallback    callback,
                      gpointer                callback_data)
{
    if (callback != NULL)
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

    if (NAUTILUS_IS_STARRED_DIRECTORY (file->details->directory))
    {
        nautilus_file_set_display_name (file, _("Starred"), NULL, TRUE);
    }

    file->details->got_file_info = TRUE;
    file->details->file_info_is_up_to_date = TRUE;
}

static void
nautilus_internal_place_file_class_init (NautilusInternalPlaceFileClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusFileClass *file_class = NAUTILUS_FILE_CLASS (klass);

    /* We need to know the parent directory, which is a construction property.*/
    object_class->constructed = nautilus_internal_place_file_constructed;

    file_class->default_file_type = G_FILE_TYPE_DIRECTORY;

    file_class->monitor_add = real_monitor_add;
    file_class->monitor_remove = real_monitor_remove;
    file_class->call_when_ready = real_call_when_ready;
    file_class->cancel_call_when_ready = real_cancel_call_when_ready;
    file_class->check_if_ready = real_check_if_ready;
}
