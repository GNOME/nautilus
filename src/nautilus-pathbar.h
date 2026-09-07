/*
 * SPDX-FileCopyrightText: 2005 Alexander Larsson <alexl@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>

#define NAUTILUS_TYPE_PATH_BAR (nautilus_path_bar_get_type ())
G_DECLARE_FINAL_TYPE (NautilusPathBar, nautilus_path_bar, NAUTILUS, PATH_BAR, GtkBox)

void     nautilus_path_bar_set_path                       (NautilusPathBar *path_bar,
                                                           GFile           *file);

void     nautilus_path_bar_set_extensions_background_menu (NautilusPathBar *path_bar,
                                                           GMenuModel      *menu);
void     nautilus_path_bar_set_templates_menu             (NautilusPathBar *path_bar,
                                                           GMenuModel      *menu);
void     nautilus_path_bar_show_current_location_menu     (NautilusPathBar *path_bar);
