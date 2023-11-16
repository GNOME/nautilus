/* nautilus-new-folder-dialog-controller.h
 *
 * Copyright (C) 2016 the Nautilus developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-directory.h"

typedef void (*NewFolderCallback) (const char *new_name,
                                   gboolean    from_selection,
                                   gpointer    user_data);

#define NAUTILUS_TYPE_NEW_FOLDER_DIALOG_CONTROLLER nautilus_new_folder_dialog_controller_get_type ()
G_DECLARE_FINAL_TYPE (NautilusNewFolderDialogController, nautilus_new_folder_dialog_controller, NAUTILUS, NEW_FOLDER_DIALOG_CONTROLLER, GObject)

NautilusNewFolderDialogController * nautilus_new_folder_dialog_controller_new (GtkWindow         *parent_window,
                                                                               NautilusDirectory *destination_directory,
                                                                               gboolean           with_selection,
                                                                               gchar             *initial_name,
                                                                               NewFolderCallback  callback,
                                                                               gpointer           callback_data);
