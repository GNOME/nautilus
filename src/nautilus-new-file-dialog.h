/* nautilus-new-file-dialog.h
 *
 * Copyright 2022 Ignacy Kuchciński <ignacykuchcinski@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "nautilus-filename-validator.h"
#include "nautilus-directory.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_NEW_FILE_DIALOG (nautilus_new_file_dialog_get_type ())

G_DECLARE_FINAL_TYPE (NautilusNewFileDialog, nautilus_new_file_dialog, NAUTILUS, NEW_FILE_DIALOG, AdwDialog)

void nautilus_new_file_dialog_new (GtkWidget         *parent_window,
                                   NautilusDirectory *destination_directory);

/**
 * nautilus_new_file_dialog_get_template_name:
 *
 * Returns: the filename of the chosen blank template, or NULL if the chosen
 * file type is a text file
 */
gchar * nautilus_new_file_dialog_get_template_name (NautilusNewFileDialog *self);

G_END_DECLS
