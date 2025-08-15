/*
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.1-or-later
 */
#pragma once

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SIDEBAR_ROW (nautilus_sidebar_row_get_type ())
G_DECLARE_FINAL_TYPE (NautilusSidebarRow,
                      nautilus_sidebar_row,
                      NAUTILUS, SIDEBAR_ROW,
                      GtkListBoxRow)

NautilusSidebarRow *nautilus_sidebar_row_new    (void);
NautilusSidebarRow *nautilus_sidebar_row_new_placeholder (void);

NautilusSidebarRow *nautilus_sidebar_row_clone  (NautilusSidebarRow *self);

/* Use these methods instead of gtk_widget_hide/show to use an animation */
void           nautilus_sidebar_row_hide   (NautilusSidebarRow *self,
                                       gboolean       immediate);
void           nautilus_sidebar_row_reveal (NautilusSidebarRow *self);

GtkWidget     *nautilus_sidebar_row_get_eject_button (NautilusSidebarRow *self);
void           nautilus_sidebar_row_set_start_icon   (NautilusSidebarRow *self,
                                                 GIcon         *icon);
void           nautilus_sidebar_row_set_end_icon     (NautilusSidebarRow *self,
                                                 GIcon         *icon);
void           nautilus_sidebar_row_set_busy         (NautilusSidebarRow *row,
                                                 gboolean       is_busy);

G_END_DECLS
