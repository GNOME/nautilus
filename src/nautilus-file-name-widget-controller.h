/* nautilus-file-name-widget-controller.h
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

#include "nautilus-file.h"
#include "nautilus-directory.h"

#define NAUTILUS_TYPE_FILE_NAME_WIDGET_CONTROLLER nautilus_file_name_widget_controller_get_type ()
G_DECLARE_DERIVABLE_TYPE (NautilusFileNameWidgetController, nautilus_file_name_widget_controller, NAUTILUS, FILE_NAME_WIDGET_CONTROLLER, GObject)

struct _NautilusFileNameWidgetControllerClass
{
        GObjectClass parent_class;

        void     (*name_accepted)        (NautilusFileNameWidgetController *controller);
};

gchar * nautilus_file_name_widget_controller_get_new_name (NautilusFileNameWidgetController *controller);

void    nautilus_file_name_widget_controller_set_containing_directory (NautilusFileNameWidgetController *controller,
                                                                       NautilusDirectory                *directory);
void    nautilus_file_name_widget_controller_set_target_is_folder (NautilusFileNameWidgetController *controller,
                                                                   gboolean                          is_folder);
void    nautilus_file_name_widget_controller_set_original_name (NautilusFileNameWidgetController *controller,
                                                                const char                       *original_name);
void    nautilus_file_name_widget_controller_set_extension (NautilusFileNameWidgetController *controller,
                                                            const char                       *extension);
