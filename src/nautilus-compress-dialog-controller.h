/* nautilus-compress-dialog-controller.h
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

#include "nautilus-file-name-widget-controller.h"
#include "nautilus-directory.h"

#define NAUTILUS_TYPE_COMPRESS_DIALOG_CONTROLLER nautilus_compress_dialog_controller_get_type ()
G_DECLARE_FINAL_TYPE (NautilusCompressDialogController, nautilus_compress_dialog_controller, NAUTILUS, COMPRESS_DIALOG_CONTROLLER, NautilusFileNameWidgetController)

NautilusCompressDialogController * nautilus_compress_dialog_controller_new (GtkWindow         *parent_window,
                                                                            NautilusDirectory *destination_directory,
                                                                            const char        *initial_name);

char *
nautilus_compress_dialog_controller_get_new_name (NautilusCompressDialogController *self);

const gchar * nautilus_compress_dialog_controller_get_passphrase (NautilusCompressDialogController *controller);
