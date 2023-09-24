/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-file.h"

typedef void (*NautilusRenameCallback) (NautilusFile *target_file,
                                        const char   *new_name,
                                        gpointer      user_data);

#define NAUTILUS_TYPE_RENAME_FILE_POPOVER nautilus_rename_file_popover_get_type ()
G_DECLARE_FINAL_TYPE (NautilusRenameFilePopover, nautilus_rename_file_popover, NAUTILUS, RENAME_FILE_POPOVER, GtkPopover)

GtkWidget * nautilus_rename_file_popover_new (void);

void        nautilus_rename_file_popover_show_for_file (NautilusRenameFilePopover *self,
                                                        NautilusFile              *target_file,
                                                        GdkRectangle              *pointing_to,
                                                        NautilusRenameCallback     callback,
                                                        gpointer                   callback_data);
