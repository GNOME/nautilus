/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gtk-extensions.c - implementation of new functions that operate on
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

#include "nautilus-gtk-extensions.h"
#include <gnome.h>


/**
 * nautilus_gtk_signal_connect_free_data:
 * 
 * Attach a function pointer and user data to a signal, and free
 * the user data when the signal is disconnected.
 * @object: the object which emits the signal. For example, a button in the button press signal.
 * @name: the name of the signal.
 * @func: function pointer to attach to the signal.
 * @data: the user data associated with the function. g_free() will be called on
 * this user data when the signal is disconnected.
 **/
guint nautilus_gtk_signal_connect_free_data (GtkObject *object,
				  	     const gchar *name,
				  	     GtkSignalFunc func,
				  	     gpointer data)
{
	return gtk_signal_connect_full (object, 
					name, 
					func, 
					NULL, /* marshal */
					data, 
					(GtkDestroyNotify)g_free, 
					FALSE, /* is this an object signal? */
					FALSE); /* invoke func after signal? */
}

/**
 * nautilus_gtk_window_hide_retain_geometry:
 * 
 * Hide a GtkWindow such that when reopened it will be in the same
 * place it is now.
 * @window: The GtkWindow to be hidden.
 **/
static void
nautilus_gtk_window_hide_retain_geometry (GtkWindow *window) {
	gchar *geometry_string;
	int left, top, width, height;

	g_return_if_fail (GTK_IS_WINDOW (window));

	/* Save and restore position to keep it in same position when next shown. */

	geometry_string = gnome_geometry_string(GTK_WIDGET (window)->window);
    
	gtk_widget_hide (GTK_WIDGET (window));

	if (gnome_parse_geometry (geometry_string, &left, &top, &width, &height)) 
	{
		gtk_window_set_default_size (window, width, height);
		gtk_widget_set_uposition (GTK_WIDGET (window), left, top);
	}

	g_free (geometry_string);
}

/**
 * nautilus_gtk_window_present:
 * 
 * Presents to the user a window that may be hidden, iconified, or buried.
 * @window: The GtkWindow to be presented to the user.
 **/
void
nautilus_gtk_window_present (GtkWindow *window) {
	g_assert (GTK_IS_WINDOW (window));

	/* Hide first if already showing, so it will reappear on top.
	 * This works with iconified windows as well.
	 */
	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (window))) 
	{
		nautilus_gtk_window_hide_retain_geometry (window);
	}
    
	gtk_widget_show (GTK_WIDGET (window));
}

/**
 * nautilus_gtk_selection_data_copy_deep:
 * 
 * Copies a GtkSelectionData, and copies the data too.
 * @data: The GtkSelectionData to be copied.
 **/
GtkSelectionData *
nautilus_gtk_selection_data_copy_deep (const GtkSelectionData *data)
{
	GtkSelectionData *copy;

	copy = g_new0 (GtkSelectionData, 1);
	gtk_selection_data_set (copy, data->type, data->format, data->data, data->length);

	return copy;
}

/**
 * nautilus_gtk_selection_data_free_deep:
 * 
 * Frees a GtkSelectionData, and frees the data too.
 * @data: The GtkSelectionData to be freed.
 **/
void
nautilus_gtk_selection_data_free_deep (GtkSelectionData *data)
{
	g_free (data->data);
	gtk_selection_data_free (data);
}

/**
 * nautilus_gtk_signal_connect_free_data:
 * 
 * Function to displace the popup menu some, otherwise the first item
 * gets selected right away.
 * This function gets called by gtk_menu_popup ().
 *
 * @menu: the popup menu.
 * @x: x coord where gtk want to place the menu
 * @y: y coord where gtk want to place the menu
 * @user_data: something
 **/
static void
nautilus_popup_menu_position_func (GtkMenu   *menu,
				   int      *x,
				   int      *y,
				   gpointer  user_data)
{
	GdkPoint *offset;

	g_assert (x != NULL);
	g_assert (y != NULL);

	offset = (GdkPoint*) user_data;

	g_assert (offset != NULL);

	/*
	 * XXX: Check for screen boundaries.  Also, the cast from 
	 * gint16 might cause problems.  Unfortunately, GdkPoint
	 * uses gint16.
	 */
	*x += (int) offset->x;
	*y += (int) offset->y;
}

/**
 * nautilus_pop_up_context_menu:
 * 
 * Pop up a context menu under the mouse. This assumes that
 * a mouse down event just occurred, with the 3rd button pressed.
 * (Context menus only appear with the 3rd mouse button, by UI
 * convention.) The menu is sunk after use, so it will be destroyed
 * unless the caller first ref'ed it.
 * 
 * This function is more of a helper function than a gtk extension,
 * so perhaps it belongs in a different file.
 * 
 * @menu: The menu to pop up under the mouse.
 * @offset_x: Number of pixels to displace the popup menu vertically
 * @offset_y: Number of pixels to displace the popup menu horizontally
 **/
void 
nautilus_pop_up_context_menu (GtkMenu	*menu,
			      gint16	offset_x,
			      gint16	offset_y)
{
	GdkPoint offset;

	g_return_if_fail (GTK_IS_MENU (menu));

	offset.x = offset_x;
	offset.y = offset_y;

	/* We pass current time here instead of extracting it from
	 * the event, for API simplicity. This does not seem to make
	 * any practical difference. See man XGrabPointer for details.
	 */
	gtk_menu_popup (menu,					/* menu */
			NULL,					/* parent_menu_shell */
			NULL,					/* parent_menu_item */
			nautilus_popup_menu_position_func,	/* func */
			(gpointer) &offset,			/* data */
			3,					/* button */
			GDK_CURRENT_TIME);			/* activate_time */

	gtk_object_sink (GTK_OBJECT(menu));
}


typedef void (*NautilusGtkSignal_NONE__POINTER_INT_INT_DOUBLE) (GtkObject * object,
								gpointer arg1,
								int arg2,
								int arg3,
								double arg4,
								gpointer user_data);

typedef void (*NautilusGtkSignal_NONE__POINTER_INT_INT_DOUBLE_DOUBLE) (GtkObject * object,
								       gpointer arg1,
								       int arg2,
								       int arg3,
								       double arg4,
								       double arg5,
								       gpointer user_data);

void
nautilus_gtk_marshal_NONE__POINTER_INT_INT_DOUBLE (GtkObject * object,
						   GtkSignalFunc func,
						   gpointer func_data,
						   GtkArg * args)
{
	(* (NautilusGtkSignal_NONE__POINTER_INT_INT_DOUBLE) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_INT (args[1]),
		 GTK_VALUE_INT (args[2]),
		 GTK_VALUE_DOUBLE (args[3]),
		 func_data);
}

void
nautilus_gtk_marshal_NONE__POINTER_INT_INT_DOUBLE_DOUBLE (GtkObject * object,
							  GtkSignalFunc func,
							  gpointer func_data,
							  GtkArg * args)
{
	(* (NautilusGtkSignal_NONE__POINTER_INT_INT_DOUBLE_DOUBLE) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_INT (args[1]), 
		 GTK_VALUE_INT (args[2]), 
		 GTK_VALUE_DOUBLE (args[3]), 
		 GTK_VALUE_DOUBLE (args[4]), 
		 func_data);
}
