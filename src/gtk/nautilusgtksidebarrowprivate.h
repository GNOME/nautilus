/*
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.1-or-later
 */
#pragma once

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_GTK_SIDEBAR_ROW (nautilus_gtk_sidebar_row_get_type ())
G_DECLARE_FINAL_TYPE (NautilusGtkSidebarRow,
                      nautilus_gtk_sidebar_row,
                      NAUTILUS, GTK_SIDEBAR_ROW,
                      GtkListBoxRow)

NautilusGtkSidebarRow *nautilus_gtk_sidebar_row_new    (void);
NautilusGtkSidebarRow *nautilus_sidebar_row_new_placeholder (void);

NautilusGtkSidebarRow *nautilus_gtk_sidebar_row_clone  (NautilusGtkSidebarRow *self);

/* Use these methods instead of gtk_widget_hide/show to use an animation */
void           nautilus_gtk_sidebar_row_hide   (NautilusGtkSidebarRow *self,
                                       gboolean       immediate);
void           nautilus_gtk_sidebar_row_reveal (NautilusGtkSidebarRow *self);

GtkWidget     *nautilus_gtk_sidebar_row_get_eject_button (NautilusGtkSidebarRow *self);
void           nautilus_gtk_sidebar_row_set_start_icon   (NautilusGtkSidebarRow *self,
                                                 GIcon         *icon);
void           nautilus_gtk_sidebar_row_set_end_icon     (NautilusGtkSidebarRow *self,
                                                 GIcon         *icon);
void           nautilus_gtk_sidebar_row_set_busy         (NautilusGtkSidebarRow *row,
                                                 gboolean       is_busy);

G_END_DECLS
