
/*
 * nautilus-program-choosing.h - functions for selecting and activating
 * programs for opening/viewing particular files.
 *
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Author: John Sullivan <sullivan@eazel.com>
 */

#pragma once

#include "nautilus-types.h"

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef void (*NautilusApplicationChoiceCallback)   (GAppInfo                          *application,
                                                     gpointer                           callback_data);

void nautilus_launch_application                    (GAppInfo                          *application,
                                                     NautilusFileList                  *files,
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
