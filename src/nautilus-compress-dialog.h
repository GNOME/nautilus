/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>
#include <adwaita.h>

#include "nautilus-directory.h"

typedef void (*CompressCallback) (const char *new_name,
                                  const char *passphrase,
                                  gpointer    user_data);

#define NAUTILUS_TYPE_COMPRESS_DIALOG nautilus_compress_dialog_get_type ()
G_DECLARE_FINAL_TYPE (NautilusCompressDialog, nautilus_compress_dialog, NAUTILUS, COMPRESS_DIALOG, AdwDialog)

NautilusCompressDialog * nautilus_compress_dialog_new (GtkWindow         *parent_window,
                                                       NautilusDirectory *destination_directory,
                                                       const char        *initial_name,
                                                       CompressCallback   callback,
                                                       gpointer           callback_data);
