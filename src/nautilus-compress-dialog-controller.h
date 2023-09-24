/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-directory.h"

#define NAUTILUS_TYPE_COMPRESS_DIALOG nautilus_compress_dialog_get_type ()
G_DECLARE_FINAL_TYPE (
    NautilusCompressDialog, nautilus_compress_dialog, NAUTILUS, COMPRESS_DIALOG, AdwWindow)

typedef void (*CompressCallback) (const char *new_name,
                                  const char *passphrase,
                                  gpointer    user_data);

NautilusCompressDialog *
nautilus_compress_dialog_new (NautilusDirectory *destination_directory,
                              gchar             *initial_name,
                              CompressCallback   callback,
                              gpointer           callback_data);
