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

#include <config.h>
#include "nautilus-gtk-extensions.h"
#include "nautilus-gdk-extensions.h"

#include <gtk/gtkselection.h>
#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-geometry.h>
#include "nautilus-glib-extensions.h"

/* This number should be large enough to be visually noticeable,
 * but small enough to not allow the user to perform other actions.
 */
#define BUTTON_AUTO_HIGHLIGHT_MILLISECONDS	100

static gboolean
finish_button_activation (gpointer data)
{
	GtkButton *button;
	
	g_assert (GTK_IS_BUTTON (data));

	button = GTK_BUTTON (data);

	if (!button->in_button) {
		gtk_button_clicked (GTK_BUTTON (data));
	}
	gtk_button_released (GTK_BUTTON (data));

	return FALSE;	
}

/**
 * nautilus_gtk_button_auto_click:
 * 
 * Programatically activate a button as if the user had clicked on it,
 * including briefly drawing the button's pushed-in state.
 * @button: Any GtkButton.
 **/
void
nautilus_gtk_button_auto_click (GtkButton *button)
{
	g_return_if_fail (GTK_IS_BUTTON (button));

	button->in_button = TRUE;
	gtk_button_pressed (button);
	button->in_button = FALSE;

	/* FIXME:
	 * Nothing is preventing other events from occuring between
	 * now and when this timeout function fires, which means in
	 * theory the user could click on a different row or otherwise
	 * get in between the double-click and the button activation.
	 * In practice the timeout is short enough that this probably
	 * isn't a problem.
	 */
	g_timeout_add (BUTTON_AUTO_HIGHLIGHT_MILLISECONDS, 
		       finish_button_activation, button);
}

/**
 * nautilus_gtk_button_set_padding
 * 
 * Adds some padding around the contained widget in the button (typically the label).
 * @button: a GtkButton
 * @pad_amount: number of pixels of space to add around the button's contents.
 * GNOME_PAD_SMALL is a typical value.
 **/
void
nautilus_gtk_button_set_padding (GtkButton *button, int pad_amount)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (pad_amount > 0);

	gtk_misc_set_padding (GTK_MISC (GTK_BIN(button)->child), 
			      pad_amount, 
			      pad_amount);
}


/**
 * nautilus_gtk_clist_get_first_selected_row:
 * 
 * Get the index of the first selected row, or -1 if no rows are selected.
 * @list: Any GtkCList
 **/
int 
nautilus_gtk_clist_get_first_selected_row (GtkCList *list)
{
	GtkCListRow *row;
	GList *p;
	int row_number;

	g_return_val_if_fail (GTK_IS_CLIST (list), -1);

	row_number = 0;
	for (p = GTK_CLIST (list)->row_list; p != NULL; p = p->next) {
		row = p->data;
		if (row->state == GTK_STATE_SELECTED) {
			return row_number;	
		}

		++row_number;
	}

	return -1;
}

/**
 * nautilus_gtk_clist_get_last_selected_row:
 * 
 * Get the index of the last selected row, or -1 if no rows are selected.
 * @list: Any GtkCList
 **/
int 
nautilus_gtk_clist_get_last_selected_row (GtkCList *list)
{
	GtkCListRow *row;
	GList *p;
	int row_number;

	g_return_val_if_fail (GTK_IS_CLIST (list), -1);

	row_number = GTK_CLIST (list)->rows - 1;
	for (p = GTK_CLIST (list)->row_list_end; p != NULL; p = p->prev) {
		row = p->data;
		if (row->state == GTK_STATE_SELECTED) {
			return row_number;	
		}

		--row_number;
	}

	return -1;
}

static gint
activate_button_on_double_click (GtkWidget *widget,
		     		 GdkEventButton *event,
		     		 gpointer user_data)
{
	g_assert (GTK_IS_CLIST (widget));
	g_assert (GTK_IS_BUTTON (user_data));

	/* Treat double-click like single click followed by
	 * click on specified button.
	 */
	if (event->type == GDK_2BUTTON_PRESS 
	    && GTK_WIDGET_SENSITIVE (GTK_WIDGET (user_data))) {
		nautilus_gtk_button_auto_click (GTK_BUTTON (user_data));
	}
	
	return FALSE;
}	

/**
 * nautilus_gtk_clist_set_double_click_button:
 * 
 * Set a button to be auto-clicked when a clist gets a double-click.
 * @clist: Any GtkCList
 * @button: A GtkButton that will be auto-clicked when the clist gets
 * a double-click event. If the button is not sensitive, this function
 * does nothing.
 **/
void
nautilus_gtk_clist_set_double_click_button (GtkCList *clist, GtkButton *button)
{
	g_return_if_fail (GTK_IS_CLIST (clist));
	g_return_if_fail (GTK_IS_BUTTON (button));

	gtk_signal_connect (GTK_OBJECT (clist), 
			    "button_press_event",
			    (GtkSignalFunc) activate_button_on_double_click,
			    button);

}

/**
 * nautilus_gtk_signal_connect_free_data_custom:
 * 
 * Attach a function pointer and user data to a signal, and call a
 * a destroy function on the user data when the signal is disconnected.
 * @object: the object which emits the signal. For example, a button in the button press signal.
 * @name: the name of the signal.
 * @func: function pointer to attach to the signal.
 * @data: the user data associated with the function.
 * @destroy_func: the function to call on the user data when the signal
 * is disconnected.
 **/
guint nautilus_gtk_signal_connect_free_data_custom (GtkObject *object,
				  	     	    const gchar *name,
				  	     	    GtkSignalFunc func,
				  	     	    gpointer data,
				  	     	    GtkDestroyNotify destroy_func)
{
	return gtk_signal_connect_full (object, 
					name, 
					func, 
					NULL, /* marshal */
					data, 
					destroy_func, 
					FALSE, /* is this an object signal? */
					FALSE); /* invoke func after signal? */
}

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
	return nautilus_gtk_signal_connect_free_data_custom
		(object, name, func, data, g_free);
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

	geometry_string = gnome_geometry_string (GTK_WIDGET (window)->window);
    
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
	GtkRequisition requisition;

	g_assert (x != NULL);
	g_assert (y != NULL);

	offset = (GdkPoint*) user_data;

	g_assert (offset != NULL);

	/*
	 * FIXME: The cast from gint16 might cause problems.  
	 * Unfortunately, GdkPoint uses gint16.
	 */
	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);
	  
	*x = CLAMP (*x + (int) offset->x, 0, MAX (0, gdk_screen_width () - requisition.width));
	*y = CLAMP (*y + (int) offset->y, 0, MAX (0, gdk_screen_height () - requisition.height));
}

/**
 * nautilus_pop_up_context_menu:
 * 
 * Pop up a context menu under the mouse.
 * The menu is sunk after use, so it will be destroyed unless the 
 * caller first ref'ed it.
 * 
 * This function is more of a helper function than a gtk extension,
 * so perhaps it belongs in a different file.
 * 
 * When calling from a callback other than button_press, make sure to pass
 * 0 for button because otherwise the first button click after the button came
 * up would not be handled properly (a subtle fragility of gtk_menu_popup).
 * 
 * @menu: The menu to pop up under the mouse.
 * @offset_x: Number of pixels to displace the popup menu vertically
 * @offset_y: Number of pixels to displace the popup menu horizontally
 * @button: current button if called from button_press.
 **/
void 
nautilus_pop_up_context_menu (GtkMenu	*menu,
			      gint16	offset_x,
			      gint16	offset_y,
			      int	button)
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
			&offset,			        /* data */
			button,					/* button */
			GDK_CURRENT_TIME);			/* activate_time */

	gtk_object_sink (GTK_OBJECT(menu));
}


void
nautilus_gtk_marshal_NONE__POINTER_INT_INT_DOUBLE (GtkObject *object,
						   GtkSignalFunc func,
						   gpointer func_data,
						   GtkArg *args)
{
	(* (void (*)(GtkObject *, gpointer, int, int, double, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_INT (args[1]),
		 GTK_VALUE_INT (args[2]),
		 GTK_VALUE_DOUBLE (args[3]),
		 func_data);
}

void
nautilus_gtk_marshal_NONE__INT_INT_INT (GtkObject *object,
					GtkSignalFunc func,
					gpointer func_data,
					GtkArg *args)
{
	(* (void (*)(GtkObject *, int, int, int, gpointer)) func)
		(object,
		 GTK_VALUE_INT (args[0]),
		 GTK_VALUE_INT (args[1]),
		 GTK_VALUE_INT (args[2]),
		 func_data);
}

void
nautilus_gtk_marshal_NONE__POINTER_INT_INT_INT (GtkObject *object,
						GtkSignalFunc func,
						gpointer func_data,
						GtkArg *args)
{
	(* (void (*)(GtkObject *, gpointer, int, int, int, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_INT (args[1]),
		 GTK_VALUE_INT (args[2]),
		 GTK_VALUE_INT (args[3]),
		 func_data);
}

void 
nautilus_gtk_marshal_NONE__POINTER_INT_POINTER_POINTER (GtkObject *object,
							GtkSignalFunc func,
							gpointer func_data,
							GtkArg *args)
{
	(* (void (*)(GtkObject *, gpointer, int, gpointer, gpointer, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_INT (args[1]),
		 GTK_VALUE_POINTER (args[2]),
		 GTK_VALUE_POINTER (args[3]),
		 func_data);
}

void
nautilus_gtk_marshal_NONE__POINTER_POINTER_INT_INT_INT (GtkObject *object,
							GtkSignalFunc func,
							gpointer func_data,
							GtkArg *args)
{
	(* (void (*)(GtkObject *, gpointer, gpointer, int, int, int, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_POINTER (args[1]),
		 GTK_VALUE_INT (args[2]),
		 GTK_VALUE_INT (args[3]),
		 GTK_VALUE_INT (args[4]),
		 func_data);
}

void
nautilus_gtk_marshal_NONE__POINTER_POINTER_POINTER_INT_INT_INT (GtkObject *object,
								GtkSignalFunc func,
								gpointer func_data,
								GtkArg *args)
{
	(* (void (*)(GtkObject *, gpointer, gpointer, gpointer, int, int, int, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_POINTER (args[1]),
		 GTK_VALUE_POINTER (args[2]),
		 GTK_VALUE_INT (args[3]),
		 GTK_VALUE_INT (args[4]),
		 GTK_VALUE_INT (args[5]),
		 func_data);
}

void
nautilus_gtk_marshal_NONE__POINTER_INT_INT_DOUBLE_DOUBLE (GtkObject *object,
							  GtkSignalFunc func,
							  gpointer func_data,
							  GtkArg *args)
{
	(* (void (*)(GtkObject *, gpointer, int, int, double, double, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_INT (args[1]), 
		 GTK_VALUE_INT (args[2]), 
		 GTK_VALUE_DOUBLE (args[3]), 
		 GTK_VALUE_DOUBLE (args[4]), 
		 func_data);
}

void
nautilus_gtk_marshal_NONE__DOUBLE (GtkObject *object,
				   GtkSignalFunc func,
				   gpointer func_data,
				   GtkArg *args)
{
	(* (void (*)(GtkObject *, double, gpointer)) func)
		(object,
		 GTK_VALUE_DOUBLE (args[0]),
		 func_data);
}

void
nautilus_gtk_marshal_NONE__DOUBLE_DOUBLE_DOUBLE (GtkObject *object,
				   GtkSignalFunc func,
				   gpointer func_data,
				   GtkArg *args)
{
	(* (void (*)(GtkObject *, double, double, double, gpointer)) func)
		(object,
		 GTK_VALUE_DOUBLE (args[0]),
		 GTK_VALUE_DOUBLE (args[1]),
		 GTK_VALUE_DOUBLE (args[2]),
		 func_data);
}

void
nautilus_gtk_marshal_POINTER__NONE (GtkObject *object,
				    GtkSignalFunc func,
				    gpointer func_data,
				    GtkArg *args)
{
	* GTK_RETLOC_POINTER (args[0]) =
		(* (void * (*)(GtkObject *, gpointer)) func) 
		 (object,
		 func_data);
}

void 
nautilus_gtk_marshal_INT__NONE (GtkObject *object,
				GtkSignalFunc  func,
				gpointer       func_data,
				GtkArg        *args)
{
	* GTK_RETLOC_INT (args[0]) =
		(* (int (*)(GtkObject *, gpointer)) func)
		 (object,
		 func_data);
}

void
nautilus_gtk_marshal_POINTER__POINTER (GtkObject *object,
				       GtkSignalFunc func,
				       gpointer func_data,
				       GtkArg *args)
{
	* GTK_RETLOC_POINTER (args[1]) =
		(* (gpointer (*)(GtkObject *, gpointer, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 func_data);
}

void
nautilus_gtk_marshal_INT__POINTER_POINTER (GtkObject *object,
					   GtkSignalFunc func,
					   gpointer func_data,
					   GtkArg *args)
{
	* GTK_RETLOC_INT (args[2]) =
		(* (int (*)(GtkObject *, gpointer, gpointer, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_POINTER (args[1]),
		 func_data);
}

void
nautilus_gtk_marshal_INT__POINTER_INT (GtkObject *object,
					   GtkSignalFunc func,
					   gpointer func_data,
					   GtkArg *args)
{
	* GTK_RETLOC_INT (args[2]) =
		(* (int (*)(GtkObject *, gpointer, int, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_INT (args[1]),
		 func_data);
}

void
nautilus_gtk_marshal_POINTER__POINTER_POINTER (GtkObject *object,
					       GtkSignalFunc func,
					       gpointer func_data,
					       GtkArg *args)
{
	* GTK_RETLOC_POINTER (args[2]) =
		(* (gpointer (*)(GtkObject *, gpointer, gpointer, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_POINTER (args[1]),
		 func_data);
}

void
nautilus_gtk_marshal_POINTER__POINTER_POINTER_POINTER (GtkObject *object,
						       GtkSignalFunc func,
						       gpointer func_data,
						       GtkArg *args)
{
	* GTK_RETLOC_POINTER (args[3]) =
		(* (gpointer (*)(GtkObject *, gpointer, gpointer, gpointer, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_POINTER (args[1]),
		 GTK_VALUE_POINTER (args[2]),
		 func_data);
}

void
nautilus_gtk_marshal_NONE__POINTER_POINTER_POINTER (GtkObject *object,
						    GtkSignalFunc func,
						    gpointer func_data,
						    GtkArg *args)
{
	(* (void (*)(GtkObject *, gpointer, gpointer, gpointer, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_POINTER (args[1]),
		 GTK_VALUE_POINTER (args[2]),
		 func_data);
}

void
nautilus_gtk_marshal_POINTER__POINTER_INT_INT_POINTER_POINTER (GtkObject *object,
							       GtkSignalFunc func,
							       gpointer func_data,
							       GtkArg *args)
{
	* GTK_RETLOC_POINTER (args[5]) =
		(* (gpointer (*)(GtkObject *, gpointer, int, int, gpointer, gpointer, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_INT (args[1]),
		 GTK_VALUE_INT (args[2]),
		 GTK_VALUE_POINTER (args[3]),
		 GTK_VALUE_POINTER (args[4]),
		 func_data);
}

void
nautilus_gtk_marshal_NONE__POINTER_POINTER_POINTER_POINTER_POINTER_POINTER (GtkObject *object,
									    GtkSignalFunc func,
									    gpointer func_data,
									    GtkArg *args)
{
	(* (void (*)(GtkObject *, gpointer, gpointer, gpointer,
		     gpointer, gpointer, gpointer, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_POINTER (args[1]),
		 GTK_VALUE_POINTER (args[2]),
		 GTK_VALUE_POINTER (args[3]),
		 GTK_VALUE_POINTER (args[4]),
		 GTK_VALUE_POINTER (args[5]),
		 func_data);
}

gboolean
nautilus_point_in_allocation (const GtkAllocation *allocation,
			      int x, int y)
{
	g_return_val_if_fail (allocation != NULL, FALSE);
	return x >= allocation->x
		&& y >= allocation->y
		&& x < allocation->x + allocation->width 
		&& y < allocation->y + allocation->height;
}

gboolean
nautilus_point_in_widget (GtkWidget *widget,
			  int x, int y)
{
	if (widget == NULL) {
		return FALSE;
	}
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	return nautilus_point_in_allocation (&widget->allocation, x, y);
}

/**
 * nautilus_gtk_object_list_ref
 *
 * Ref all the objects in a list.
 * @list: GList of objects.
 **/
GList *
nautilus_gtk_object_list_ref (GList *list)
{
	g_list_foreach (list, (GFunc) gtk_object_ref, NULL);
	return list;
}

/**
 * nautilus_gtk_object_list_unref
 *
 * Unref all the objects in a list.
 * @list: GList of objects.
 **/
void
nautilus_gtk_object_list_unref (GList *list)
{
	nautilus_g_list_safe_for_each (list, (GFunc) gtk_object_unref, NULL);
}

/**
 * nautilus_gtk_object_list_free
 *
 * Free a list of objects after unrefing them.
 * @list: GList of objects.
 **/
void
nautilus_gtk_object_list_free (GList *list)
{
	nautilus_gtk_object_list_unref (list);
	g_list_free (list);
}

/**
 * nautilus_gtk_object_list_copy
 *
 * Copy the list of objects, ref'ing each one.
 * @list: GList of objects.
 **/
GList *
nautilus_gtk_object_list_copy (GList *list)
{
	return g_list_copy (nautilus_gtk_object_list_ref (list));
}

/**
 * nautilus_gtk_style_set_font
 *
 * Sets the font in a style object, managing the ref. counts.
 * @style: The style to change.
 * @font: The new font.
 **/
void
nautilus_gtk_style_set_font (GtkStyle *style, GdkFont *font)
{
	g_return_if_fail (style != NULL);
	g_return_if_fail (font != NULL);
	
	gdk_font_ref (font);
	gdk_font_unref (style->font);
	style->font = font;
}

/**
 * nautilus_gtk_widget_set_font
 *
 * Sets the font for a widget's style, managing the style objects.
 * @widget: The widget.
 * @font: The font.
 **/
void
nautilus_gtk_widget_set_font (GtkWidget *widget, GdkFont *font)
{
	GtkStyle *new_style;
	
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (font != NULL);
	
	new_style = gtk_style_copy (gtk_widget_get_style (widget));

	nautilus_gtk_style_set_font (new_style, font);
	
	gtk_widget_set_style (widget, new_style);
	gtk_style_unref (new_style);
}

/**
 * nautilus_gtk_widget_set_font_by_name
 *
 * Sets the font for a widget, managing the font and style objects.
 * @widget: The widget
 **/
void
nautilus_gtk_widget_set_font_by_name (GtkWidget *widget, const char *font_name)
{
	GdkFont *font;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (font_name != NULL);
	
	font = gdk_font_load (font_name);
	nautilus_gtk_widget_set_font (widget, font);
	gdk_font_unref (font);
}

/* This stuff is stolen from Gtk. */

typedef struct DisconnectInfo {
	GtkObject *object1;
	guint disconnect_handler1;
	guint signal_handler;
	GtkObject *object2;
	guint disconnect_handler2;
} DisconnectInfo;

static guint
alive_disconnecter (GtkObject *object, DisconnectInfo *info)
{
	g_assert (info != NULL);
	g_assert (GTK_IS_OBJECT (info->object1));
	g_assert (info->disconnect_handler1 != 0);
	g_assert (info->signal_handler != 0);
	g_assert (GTK_IS_OBJECT (info->object2));
	g_assert (info->disconnect_handler2 != 0);
	g_assert (object == info->object1 || object == info->object2);
	
	gtk_signal_disconnect (info->object1, info->disconnect_handler1);
	gtk_signal_disconnect (info->object1, info->signal_handler);
	gtk_signal_disconnect (info->object2, info->disconnect_handler2);
	
	g_free (info);
	
	return 0;
}

/**
 * nautilus_gtk_signal_connect_full_while_alive
 *
 * Like gtk_signal_connect_while_alive, but works with full parameters.
 **/
void
nautilus_gtk_signal_connect_full_while_alive (GtkObject *object,
					      const gchar *name,
					      GtkSignalFunc func,
					      GtkCallbackMarshal marshal,
					      gpointer data,
					      GtkDestroyNotify destroy_func,
					      gboolean object_signal,
					      gboolean after,
					      GtkObject *alive_object)
{
	DisconnectInfo *info;
	
	g_return_if_fail (GTK_IS_OBJECT (object));
	g_return_if_fail (name != NULL);
	g_return_if_fail (func != NULL);
	g_return_if_fail (object_signal == FALSE || object_signal == TRUE);
	g_return_if_fail (after == FALSE || after == TRUE);
	g_return_if_fail (GTK_IS_OBJECT (alive_object));
	
	info = g_new (DisconnectInfo, 1);
	info->object1 = object;
	info->object2 = alive_object;
	
	info->signal_handler =
		gtk_signal_connect_full (object,
					 name,
					 func,
					 marshal,
					 data,
					 destroy_func,
					 object_signal,
					 after);

	info->disconnect_handler1 =
		gtk_signal_connect (object,
				    "destroy",
				    GTK_SIGNAL_FUNC (alive_disconnecter),
				    info);
	info->disconnect_handler2 =
		gtk_signal_connect (alive_object,
				    "destroy",
				    GTK_SIGNAL_FUNC (alive_disconnecter),
				    info);
}

/**
 * nautilus_gtk_container_get_first_child.
 *
 * Returns the first child of a container.
 * @container: The container.
 **/

static void
get_first_callback (GtkWidget *widget, gpointer callback_data)
{
	GtkWidget **first_child_slot;

	g_assert (GTK_IS_WIDGET (widget));
	g_assert (callback_data != NULL);
	
	first_child_slot = callback_data;

	if (*first_child_slot == NULL) {
		*first_child_slot = widget;
		/* We'd stop the iterating now if we could. */
	} else {
		g_assert (GTK_IS_WIDGET (*first_child_slot));
	}
}

GtkWidget *
nautilus_gtk_container_get_first_child (GtkContainer *container)
{
	GtkWidget *first_child;

	g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);
	
	first_child = NULL;
	gtk_container_foreach (container, get_first_callback, &first_child);
	g_assert (first_child == NULL || GTK_IS_WIDGET (first_child));
	return first_child;
}

/* We have to supply a dummy pixmap to avoid the return_if_fail in gtk_pixmap_new. */
GtkPixmap *
nautilus_gtk_pixmap_new_empty (void)
{
	GtkPixmap *pixmap;

	/* Make a GtkPixmap with a dummy GdkPixmap. The
         * gdk_pixmap_new call will fail if passed 0 for height or
	 * width, or if passed a bad depth.
	 */
	pixmap = GTK_PIXMAP (gtk_pixmap_new (gdk_pixmap_new (NULL, 1, 1, gdk_visual_get_best_depth ()), NULL));

	/* Clear out the dummy pixmap. */
	gtk_pixmap_set (pixmap, NULL, NULL);

	return pixmap;
}

/* The standard gtk_adjustment_set_value ignores page size, which
 * disagrees with the logic used by scroll bars, for example.
 */
void
nautilus_gtk_adjustment_set_value (GtkAdjustment *adjustment,
				   float value)
{
	float upper_page_start, clamped_value;

	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	
	upper_page_start = MAX (adjustment->upper - adjustment->page_size, adjustment->lower);
	clamped_value = CLAMP (value, adjustment->lower, upper_page_start);
	if (clamped_value != adjustment->value) {
		adjustment->value = clamped_value;
		gtk_adjustment_value_changed (adjustment);
	}
}

/* Clamp a value if the minimum or maximum has changed. */
void
nautilus_gtk_adjustment_clamp_value (GtkAdjustment *adjustment)
{
	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	
	nautilus_gtk_adjustment_set_value (adjustment, adjustment->value);
}

/**
 * nautilus_gtk_label_make_bold.
 *
 * Switches the font of label to a bold equivalent.
 * @label: The label.
 **/

void
nautilus_gtk_label_make_bold (GtkLabel *label)
{
	GtkStyle *style;
	GdkFont *bold_font;

	g_return_if_fail (GTK_IS_LABEL (label));
	style = gtk_widget_get_style (GTK_WIDGET(label));

	bold_font = nautilus_gdk_font_get_bold (style->font);

	if (bold_font == NULL) {
		return;
	}

	nautilus_gtk_widget_set_font (GTK_WIDGET(label), bold_font);
	gdk_font_unref (bold_font);
}

