
/* nautilus-program-choosing.h - functions for selecting and activating
                                 programs for opening/viewing particular files.

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

   Author: John Sullivan <sullivan@eazel.com>
*/

#pragma once

#include "nautilus-types.h"

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef void (*NautilusApplicationChoiceCallback)   (GAppInfo                          *application,
                                                     gpointer                           callback_data);

void nautilus_launch_application                    (GAppInfo                          *application,
                                                     GList                             *files,
                                                     GtkWindow                         *parent_window);
void nautilus_launch_application_by_uri             (GAppInfo                          *application,
                                                     GList                             *uris,
                                                     GtkWindow                         *parent_window);
void nautilus_launch_application_for_mount          (GAppInfo                          *app_info,
                                                     GMount                            *mount,
                                                     GtkWindow                         *parent_window);
void nautilus_launch_application_from_command       (GdkDisplay                        *display,
                                                     const char                        *command_string,
                                                     gboolean                           use_terminal,
                                                     ...) G_GNUC_NULL_TERMINATED;
void nautilus_launch_application_from_command_array (GdkDisplay                        *display,
                                                     const char                        *command_string,
                                                     gboolean                           use_terminal,
                                                     const char * const *               parameters);
void nautilus_launch_desktop_file                   (const char                        *desktop_file_uri,
                                                     const GList                       *parameter_uris,
                                                     GtkWindow                         *parent_window);
