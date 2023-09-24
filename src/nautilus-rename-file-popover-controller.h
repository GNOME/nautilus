/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-file.h"

#define NAUTILUS_TYPE_RENAME_FILE_POPOVER nautilus_rename_file_popover_get_type ()
G_DECLARE_FINAL_TYPE (NautilusRenameFilePopover, nautilus_rename_file_popover, NAUTILUS, RENAME_FILE_POPOVER, GtkPopover)

typedef void (*NautilusRenameCallback) (NautilusFile *target_file,
                                        const char   *new_name,
                                        gpointer      user_data);

NautilusRenameFilePopover *
nautilus_rename_file_popover_new (NautilusRenameCallback  callback);

void
nautilus_rename_file_popover_show_for_file   (NautilusRenameFilePopover *popover,
                                              NautilusFile              *target_file);
