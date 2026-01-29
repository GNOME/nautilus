
/* fm-properties-window.h - interface for window that lets user modify 
                            icon properties

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Authors: Darin Adler <darin@bentspoon.com>
*/

#pragma once

#include "nautilus-types.h"

#include <adwaita.h>
#include <gtk/gtk.h>

typedef void (* NautilusPropertiesWindowCallback) (gpointer    callback_data);

void
nautilus_properties_present_dialog (NautilusFileList *files,
                                    GtkWidget        *parent_widget);

void nautilus_properties_window_present (GList                            *files,
                                         GtkWidget                        *parent_widget,
                                         const gchar                      *startup_id,
                                         NautilusPropertiesWindowCallback  callback,
                                         gpointer                          unused);
