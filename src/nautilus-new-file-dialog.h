/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2022 Ignacy Kuchciński
 *
 * Authors: Ignacy Kuchciński <ignacykuchcinski@gmail.com>
 */

#pragma once

#include <adwaita.h>

#include "nautilus-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_NEW_FILE_DIALOG (nautilus_new_file_dialog_get_type ())

G_DECLARE_FINAL_TYPE (NautilusNewFileDialog, nautilus_new_file_dialog, NAUTILUS, NEW_FILE_DIALOG, AdwDialog)

typedef void (*NewFileCallback) (gchar *, NautilusFile *, gpointer *callback_data);

void
nautilus_new_file_dialog_new (GtkWidget         *parent_window,
                              NautilusDirectory *destination_directory,
                              NewFileCallback    callback,
                              gpointer           callback_data);

G_END_DECLS
