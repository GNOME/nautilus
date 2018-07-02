/* nautilusgtksidebarrowprivate.h
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef NAUTILUS_GTK_SIDEBAR_ROW_PRIVATE_H
#define NAUTILUS_GTK_SIDEBAR_ROW_PRIVATE_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_GTK_SIDEBAR_ROW             (nautilus_gtk_sidebar_row_get_type())
#define NAUTILUS_GTK_SIDEBAR_ROW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_GTK_SIDEBAR_ROW, NautilusGtkSidebarRow))
#define NAUTILUS_GTK_SIDEBAR_ROW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_GTK_SIDEBAR_ROW, NautilusGtkSidebarRowClass))
#define NAUTILUS_GTK_IS_SIDEBAR_ROW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_GTK_SIDEBAR_ROW))
#define NAUTILUS_GTK_IS_SIDEBAR_ROW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_GTK_SIDEBAR_ROW))
#define NAUTILUS_GTK_SIDEBAR_ROW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_GTK_SIDEBAR_ROW, NautilusGtkSidebarRowClass))

typedef struct _NautilusGtkSidebarRow NautilusGtkSidebarRow;
typedef struct _NautilusGtkSidebarRowClass NautilusGtkSidebarRowClass;

struct _NautilusGtkSidebarRowClass
{
  GtkListBoxRowClass parent;
};

GType      nautilus_gtk_sidebar_row_get_type   (void) G_GNUC_CONST;

NautilusGtkSidebarRow *nautilus_gtk_sidebar_row_new    (void);
NautilusGtkSidebarRow *nautilus_gtk_sidebar_row_clone  (NautilusGtkSidebarRow *self);

/* Use these methods instead of gtk_widget_hide/show to use an animation */
void           nautilus_gtk_sidebar_row_hide   (NautilusGtkSidebarRow *self,
                                       gboolean       inmediate);
void           nautilus_gtk_sidebar_row_reveal (NautilusGtkSidebarRow *self);

GtkWidget     *nautilus_gtk_sidebar_row_get_eject_button (NautilusGtkSidebarRow *self);
void           nautilus_gtk_sidebar_row_set_start_icon   (NautilusGtkSidebarRow *self,
                                                 GIcon         *icon);
void           nautilus_gtk_sidebar_row_set_end_icon     (NautilusGtkSidebarRow *self,
                                                 GIcon         *icon);

G_END_DECLS

#endif /* NAUTILUS_GTK_SIDEBAR_ROW_PRIVATE_H */
