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

#include <gtk/gtkclist.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtkwindow.h>

#define NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT 2

/* GtkButton */
void              nautilus_gtk_button_auto_click               (GtkButton              *button);
void              nautilus_gtk_button_set_padding              (GtkButton              *button,
								int                     pad_amount);

/* GtkCList */
int               nautilus_gtk_clist_get_first_selected_row    (GtkCList               *list);
int               nautilus_gtk_clist_get_last_selected_row     (GtkCList               *list);
void              nautilus_gtk_clist_set_double_click_button   (GtkCList               *clist,
								GtkButton              *button);

/* signals */
guint             nautilus_gtk_signal_connect_free_data        (GtkObject              *object,
								const gchar            *name,
								GtkSignalFunc           func,
								gpointer                data);
guint             nautilus_gtk_signal_connect_free_data_custom (GtkObject              *object,
								const gchar            *name,
								GtkSignalFunc           func,
								gpointer                data,
								GtkDestroyNotify        destroy_func);
void              nautilus_gtk_signal_connect_full_while_alive (GtkObject              *object,
								const gchar            *name,
								GtkSignalFunc           func,
								GtkCallbackMarshal      marshal,
								gpointer                data,
								GtkDestroyNotify        destroy_func,
								gboolean                object_signal,
								gboolean                after,
								GtkObject              *alive_object);

/* list of GtkObject */
GList *           nautilus_gtk_object_list_ref                 (GList                  *list);
void              nautilus_gtk_object_list_unref               (GList                  *list);
void              nautilus_gtk_object_list_free                (GList                  *list);
GList *           nautilus_gtk_object_list_copy                (GList                  *list);

/* GtkWidget */
gboolean          nautilus_point_in_allocation                 (const GtkAllocation    *allocation,
								int                     x,
								int                     y);
void              nautilus_gtk_widget_set_font                 (GtkWidget              *widget,
								GdkFont                *font);
void              nautilus_gtk_widget_set_font_by_name         (GtkWidget              *widget,
								const char             *font_name);
gboolean          nautilus_point_in_widget                     (GtkWidget              *widget,
								int                     x,
								int                     y);

/* GtkContainer */
GtkWidget        *nautilus_gtk_container_get_first_child       (GtkContainer           *container);

/* GtkWindow */
void              nautilus_gtk_window_present                  (GtkWindow              *window);

/* selection data */
GtkSelectionData *nautilus_gtk_selection_data_copy_deep        (const GtkSelectionData *selection_data);
void              nautilus_gtk_selection_data_free_deep        (GtkSelectionData       *selection_data);

/* GtkMenu */
void              nautilus_pop_up_context_menu                 (GtkMenu                *menu,
								gint16                  offset_x,
								gint16                  offset_y,
								int			button);
/* GtkStyle */
void              nautilus_gtk_style_set_font                  (GtkStyle               *style,
								GdkFont                *font);
void              nautilus_gtk_style_set_font_by_name          (GtkStyle               *style,
								const char             *font_name);

/* GtkLabel */
void		  nautilus_gtk_label_make_bold		       (GtkLabel 	       *label);

/* GtkPixmap */
GtkPixmap        *nautilus_gtk_pixmap_new_empty                (void);

/* GtkAdjustment */
void              nautilus_gtk_adjustment_set_value            (GtkAdjustment          *adjustment,
								float                   value);
void              nautilus_gtk_adjustment_clamp_value          (GtkAdjustment          *adjustment);

/* marshals */

#define nautilus_gtk_marshal_NONE__BOXED_BOXED gtk_marshal_NONE__POINTER_POINTER
#define nautilus_gtk_marshal_NONE__POINTER_STRING_STRING nautilus_gtk_marshal_NONE__POINTER_POINTER_POINTER
#define nautilus_gtk_marshal_BOOL__POINTER_POINTER nautilus_gtk_marshal_INT__POINTER_POINTER
#define nautilus_gtk_marshal_INT__POINTER_STRING nautilus_gtk_marshal_INT__POINTER_POINTER
#define nautilus_gtk_marshal_POINTER__POINTER_STRING_POINTER nautilus_gtk_marshal_POINTER__POINTER_POINTER_POINTER
#define nautilus_gtk_marshal_POINTER__POINTER_INT_INT_STRING_POINTER nautilus_gtk_marshal_POINTER__POINTER_INT_INT_POINTER_POINTER
#define nautilus_gtk_marshal_STRING__NONE nautilus_gtk_marshal_POINTER__NONE
#define nautilus_gtk_marshal_STRING__POINTER nautilus_gtk_marshal_POINTER__POINTER
#define nautilus_gtk_marshal_STRING__POINTER_POINTER nautilus_gtk_marshal_POINTER__POINTER_POINTER
#define nautilus_gtk_marshal_STRING__POINTER_STRING nautilus_gtk_marshal_POINTER__POINTER_POINTER
#define nautilus_gtk_marshal_STRING__POINTER_POINTER_POINTER nautilus_gtk_marshal_POINTER__POINTER_POINTER_POINTER
#define nautilus_gtk_marshal_STRING__POINTER_POINTER_STRING nautilus_gtk_marshal_POINTER__POINTER_POINTER_POINTER

void nautilus_gtk_marshal_INT__NONE                                             (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_NONE__DOUBLE                                          (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_NONE__DOUBLE_DOUBLE_DOUBLE           	                (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_NONE__INT_INT_INT        		                (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_NONE__POINTER_INT_INT_DOUBLE                          (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_NONE__POINTER_POINTER_INT_INT_INT                     (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_NONE__POINTER_INT_INT_INT        	                (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_NONE__POINTER_POINTER_POINTER                         (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_NONE__POINTER_INT_POINTER_POINTER                     (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_NONE__POINTER_POINTER_POINTER_POINTER_POINTER_POINTER (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_NONE__POINTER_POINTER_POINTER_INT_INT_INT             (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_NONE__POINTER_INT_INT_DOUBLE_DOUBLE                   (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_INT__POINTER_POINTER                                  (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_INT__POINTER_INT					(GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_POINTER__NONE                                         (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_POINTER__POINTER                                      (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_POINTER__POINTER_POINTER                              (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_POINTER__POINTER_POINTER_POINTER                      (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);
void nautilus_gtk_marshal_POINTER__POINTER_INT_INT_POINTER_POINTER              (GtkObject     *object,
										 GtkSignalFunc  func,
										 gpointer       func_data,
										 GtkArg        *args);

#endif /* NAUTILUS_GTK_EXTENSIONS_H */
