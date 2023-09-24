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

typedef void (*NewFolderCallback) (const char *new_name,
                                   gboolean    from_selection,
                                   gpointer    user_data);

#define NAUTILUS_TYPE_NEW_FOLDER_DIALOG nautilus_new_folder_dialog_get_type ()
G_DECLARE_FINAL_TYPE (NautilusNewFolderDialog, nautilus_new_folder_dialog, NAUTILUS, NEW_FOLDER_DIALOG, AdwWindow)

NautilusNewFolderDialog * nautilus_new_folder_dialog_new (GtkWindow         *parent_window,
                                                          NautilusDirectory *destination_directory,
                                                          gboolean           with_selection,
                                                          gchar             *initial_name,
                                                          NewFolderCallback  callback,
                                                          gpointer           callback_data);
