/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gtk-extensions.h - interface for new functions that operate on
  			       gtk classes. Perhaps some of these should be
  			       rolled into gtk someday.

   Copyright (C) 1999, 2000 Eazel, Inc.

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: John Sullivan <sullivan@eazel.com>
*/

#ifndef NAUTILUS_GTK_EXTENSIONS_H
#define NAUTILUS_GTK_EXTENSIONS_H

#include <gtk/gtkmenu.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktypeutils.h>
#include <gtk/gtkobject.h>

#define gtk_marshal_NONE__BOXED_BOXED gtk_marshal_NONE__POINTER_POINTER

#define NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT 2

/* signals */
guint             nautilus_gtk_signal_connect_free_data                    (GtkObject              *object,
									    const gchar            *name,
									    GtkSignalFunc           func,
									    gpointer                data);

/* GtkWindow */
void              nautilus_gtk_window_present                              (GtkWindow              *window);

/* selection data */
GtkSelectionData *nautilus_gtk_selection_data_copy_deep                    (const GtkSelectionData *selection_data);
void              nautilus_gtk_selection_data_free_deep                    (GtkSelectionData       *selection_data);

/* GtkMenu */
void              nautilus_pop_up_context_menu                             (GtkMenu                *menu,
									    gint16                  offset_x,
									    gint16                  offset_y);

/* marshals */
void              nautilus_gtk_marshal_NONE__POINTER_INT_INT_DOUBLE        (GtkObject              *object,
									    GtkSignalFunc           func,
									    gpointer                func_data,
									    GtkArg                 *args);
void              nautilus_gtk_marshal_NONE__POINTER_INT_INT_DOUBLE_DOUBLE (GtkObject              *object,
									    GtkSignalFunc           func,
									    gpointer                func_data,
									    GtkArg                 *args);
void              nautilus_gtk_marshal_NONE__DOUBLE                        (GtkObject              *object,
									    GtkSignalFunc           func,
									    gpointer                func_data,
									    GtkArg                 *args);


#endif /* NAUTILUS_GTK_EXTENSIONS_H */
