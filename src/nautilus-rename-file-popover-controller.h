/* nautilus-rename-file-popover-controller.h
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
#include "nautilus-file.h"

#define NAUTILUS_TYPE_RENAME_FILE_POPOVER_CONTROLLER nautilus_rename_file_popover_controller_get_type ()
G_DECLARE_FINAL_TYPE (NautilusRenameFilePopoverController, nautilus_rename_file_popover_controller, NAUTILUS, RENAME_FILE_POPOVER_CONTROLLER, NautilusFileNameWidgetController)

char *
nautilus_rename_file_popover_get_name (NautilusRenameFilePopoverController *controller);

NautilusRenameFilePopoverController * nautilus_rename_file_popover_controller_new (GtkWidget *relative_to);

NautilusFile * nautilus_rename_file_popover_controller_get_target_file (NautilusRenameFilePopoverController *controller);

void           nautilus_rename_file_popover_controller_show_for_file   (NautilusRenameFilePopoverController *controller,
                                                                        NautilusFile                        *target_file,
                                                                        GdkRectangle                        *pointing_to);
