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
            Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-gtk-extensions.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-font-extensions.h"

#include <gdk/gdk.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <gtk/gtkselection.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkrc.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-geometry.h>
#include <libgnomeui/gnome-winhints.h>
#include "nautilus-glib-extensions.h"
#include "nautilus-string.h"

/* This number should be large enough to be visually noticeable,
 * but small enough to not allow the user to perform other actions.
 */
#define BUTTON_AUTO_HIGHLIGHT_MILLISECONDS	100

/* This number is fairly arbitrary. Long enough to show a pretty long
 * menu title, but not so long to make a menu grotesquely wide.
 */
#define MAXIMUM_MENU_TITLE_LENGTH	48

/* Used for window position & size sanity-checking. The sizes are big enough to prevent
 * at least normal-sized gnome panels from obscuring the window at the screen edges. 
 */
#define MINIMUM_ON_SCREEN_WIDTH		100
#define MINIMUM_ON_SCREEN_HEIGHT	100

/* GTK buttons cram the text too close to the edge of the buttons by default.
 * This is the standard padding used to make them look non-crammed.
 */
#define NAUTILUS_STANDARD_BUTTON_PADDING 1

/* How far down the window tree will we search when looking for top-level
 * windows? Some window managers doubly-reparent the client, so account
 * for that, and add some slop.
 */
#define MAXIMUM_WM_REPARENTING_DEPTH 4

static gboolean
finish_button_activation (gpointer data)
{
	GtkButton *button;
	
	button = GTK_BUTTON (data);

	if (!GTK_OBJECT_DESTROYED (button) && !button->in_button) {
		gtk_button_clicked (button);
	}

	/* Check again--button can be destroyed during call to gtk_button_clicked */
	if (!GTK_OBJECT_DESTROYED (button)) {
		gtk_button_released (button);
	}

	/* this was ref'd in nautilus_gtk_button_auto_click */
	gtk_object_unref (GTK_OBJECT(button));

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

	if (!GTK_WIDGET_IS_SENSITIVE (GTK_WIDGET (button))) {
		return;
	}

	button->in_button = TRUE;
	gtk_button_pressed (button);
	button->in_button = FALSE;

	/* FIXME bugzilla.eazel.com 2562:
	 * Nothing is preventing other events from occuring between
	 * now and when this timeout function fires, which means in
	 * theory the user could click on a different row or otherwise
	 * get in between the double-click and the button activation.
	 * In practice the timeout is short enough that this probably
	 * isn't a problem.
	 */

	/* This is unref'ed in finish_button_activation */
	gtk_object_ref (GTK_OBJECT(button));

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
 * nautilus_gtk_button_set_standard_padding
 * 
 * Adds the standard amount of padding around the contained widget in the button 
 * (typically the label). Use this rather than nautilus_gtk_button_set_padding
 * unless you have a specific reason to use a non-standard amount.
 * @button: a GtkButton
 **/
void
nautilus_gtk_button_set_standard_padding (GtkButton *button)
{
	g_return_if_fail (GTK_IS_BUTTON (button));

	nautilus_gtk_button_set_padding (button, NAUTILUS_STANDARD_BUTTON_PADDING);
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
 * nautilus_gtk_window_present:
 * 
 * Presents to the user a window that may be hidden, iconified, or buried.
 * @window: The GtkWindow to be presented to the user.
 **/
void
nautilus_gtk_window_present (GtkWindow *window)
{
	GdkWindow *gdk_window;
	int current_workspace, window_workspace;

	g_return_if_fail (GTK_IS_WINDOW (window));

	/* Ensure that the window is on the current desktop */
	if (GTK_WIDGET_REALIZED (GTK_WIDGET (window))) {
		window_workspace = gnome_win_hints_get_workspace (GTK_WIDGET (window));
		current_workspace = gnome_win_hints_get_current_workspace ();
		if (window_workspace != current_workspace) {
			gtk_widget_hide (GTK_WIDGET (window));
			gnome_win_hints_set_workspace (GTK_WIDGET (window),
						       current_workspace);
		}
	}
		
	/* If we have no gdk window, then it's OK to just show, since
	 * the window is new and presumably will show up in front.
	 */
	gdk_window = GTK_WIDGET (window)->window;
	if (gdk_window != NULL) {
		nautilus_gdk_window_bring_to_front (gdk_window);
	}

	gtk_widget_show (GTK_WIDGET (window));
}

static int
handle_standard_close_accelerator (GtkWindow *window, 
				   GdkEventKey *event, 
				   gpointer user_data)
{
	g_assert (GTK_IS_WINDOW (window));
	g_assert (event != NULL);
	g_assert (user_data == NULL);

	if (nautilus_gtk_window_event_is_close_accelerator (window, event)) {
		if (GNOME_IS_DIALOG (window)) {
			gnome_dialog_close (GNOME_DIALOG (window));
		} else {
			gtk_widget_hide (GTK_WIDGET (window));
		}
		gtk_signal_emit_stop_by_name 
			(GTK_OBJECT (window), "key_press_event");
		return TRUE;
	}

	return FALSE;
}

/**
 * nautilus_gtk_window_event_is_close_accelerator:
 * 
 * Tests whether a key event is the standard window close accelerator.
 * Not needed for clients that use nautilus_gtk_window_set_up_close_accelerator;
 * use only if you must set up your own key_event handler for your own reasons.
 **/
gboolean
nautilus_gtk_window_event_is_close_accelerator (GtkWindow *window, GdkEventKey *event)
{
	g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (event->state & GDK_CONTROL_MASK) {
		/* Note: menu item equivalents are case-sensitive, so we will
		 * be case-sensitive here too.
		 */		
		if (event->keyval == NAUTILUS_STANDARD_CLOSE_WINDOW_CONTROL_KEY) {
			return TRUE;
		}
	}

	return FALSE;	
}

/**
 * nautilus_gtk_window_set_up_close_accelerator:
 * 
 * Sets up the standard keyboard equivalent to close the window.
 * Call this for windows that don't set up a keyboard equivalent to
 * close the window some other way, e.g. via a menu item accelerator.
 * 
 * @window: The GtkWindow that should be hidden when the standard
 * keyboard equivalent is typed.
 **/
void
nautilus_gtk_window_set_up_close_accelerator (GtkWindow *window)
{
	g_return_if_fail (GTK_IS_WINDOW (window));

	gtk_signal_connect (GTK_OBJECT (window),
			    "key_press_event",
			    GTK_SIGNAL_FUNC (handle_standard_close_accelerator),
			    NULL);
}

static void
sanity_check_window_position (int *left, int *top)
{
	g_assert (left != NULL);
	g_assert (top != NULL);

	/* Make sure the top of the window is on screen, for
	 * draggability (might not be necessary with all window managers,
	 * but seems reasonable anyway). Make sure the top of the window
	 * isn't off the bottom of the screen, or so close to the bottom
	 * that it might be obscured by the panel.
	 */
	*top = CLAMP (*top, 0, gdk_screen_height() - MINIMUM_ON_SCREEN_HEIGHT);
	
	/* FIXME bugzilla.eazel.com 669: 
	 * If window has negative left coordinate, set_uposition sends it
	 * somewhere else entirely. Not sure what level contains this bug (XWindows?).
	 * Hacked around by pinning the left edge to zero, which just means you
	 * can't set a window to be partly off the left of the screen using
	 * this routine.
	 */
	/* Make sure the left edge of the window isn't off the right edge of
	 * the screen, or so close to the right edge that it might be
	 * obscured by the panel.
	 */
	*left = CLAMP (*left, 0, gdk_screen_width() - MINIMUM_ON_SCREEN_WIDTH);
}

static void
sanity_check_window_dimensions (int *width, int *height)
{
	g_assert (width != NULL);
	g_assert (height != NULL);

	/* Pin the size of the window to the screen, so we don't end up in
	 * a state where the window is so big essential parts of it can't
	 * be reached (might not be necessary with all window managers,
	 * but seems reasonable anyway).
	 */
	*width = MIN (*width, gdk_screen_width());
	*height = MIN (*height, gdk_screen_height());
}

/**
 * nautilus_gtk_window_set_initial_geometry:
 * 
 * Sets the position and size of a GtkWindow before the
 * GtkWindow is shown. It is an error to call this on a window that
 * is already on-screen. Takes into account screen size, and does
 * some sanity-checking on the passed-in values.
 * 
 * @window: A non-visible GtkWindow
 * @geometry_flags: A NautilusGdkGeometryFlags value defining which of
 * the following parameters have defined values
 * @left: pixel coordinate for left of window
 * @top: pixel coordinate for top of window
 * @width: width of window in pixels
 * @height: height of window in pixels
 */
void
nautilus_gtk_window_set_initial_geometry (GtkWindow *window, 
					  NautilusGdkGeometryFlags geometry_flags,
					  int left,
					  int top,
					  guint width,
					  guint height)
{
	int real_left, real_top;

	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (!(geometry_flags & NAUTILUS_GDK_WIDTH_VALUE) || width > 0);
	g_return_if_fail (!(geometry_flags & NAUTILUS_GDK_HEIGHT_VALUE) || height > 0);

	/* Setting the default size doesn't work when the window is already showing.
	 * Someday we could make this move an already-showing window, but we don't
	 * need that functionality yet. 
	 */
	g_return_if_fail (!GTK_WIDGET_VISIBLE (window));

	if ((geometry_flags & NAUTILUS_GDK_X_VALUE) && (geometry_flags & NAUTILUS_GDK_Y_VALUE)) {
		real_left = left;
		real_top = top;

		/* This is sub-optimal. GDK doesn't allow us to set win_gravity
		 * to South/East types, which should be done if using negative
		 * positions (so that the right or bottom edge of the window
		 * appears at the specified position, not the left or top).
		 * However it does seem to be consistent with other GNOME apps.
		 */
		if (geometry_flags & NAUTILUS_GDK_X_NEGATIVE) {
			real_left = gdk_screen_width () - real_left;
		}
		if (geometry_flags & NAUTILUS_GDK_Y_NEGATIVE) {
			real_top = gdk_screen_height () - real_top;
		}

		sanity_check_window_position (&real_left, &real_top);
		gtk_widget_set_uposition (GTK_WIDGET (window), real_left, real_top);
	}

	if ((geometry_flags & NAUTILUS_GDK_WIDTH_VALUE) && (geometry_flags & NAUTILUS_GDK_HEIGHT_VALUE)) {
		sanity_check_window_dimensions (&width, &height);
		gtk_window_set_default_size (GTK_WINDOW (window), width, height);
	}
}

/**
 * nautilus_gtk_window_set_initial_geometry_from_string:
 * 
 * Sets the position and size of a GtkWindow before the
 * GtkWindow is shown. The geometry is passed in as a string. 
 * It is an error to call this on a window that
 * is already on-screen. Takes into account screen size, and does
 * some sanity-checking on the passed-in values.
 * 
 * @window: A non-visible GtkWindow
 * @geometry_string: A string suitable for use with gnome_parse_geometry
 * @minimum_width: If the width from the string is smaller than this,
 * use this for the width.
 * @minimum_height: If the height from the string is smaller than this,
 * use this for the height.
 */
void
nautilus_gtk_window_set_initial_geometry_from_string (GtkWindow *window, 
					  	      const char *geometry_string,
					  	      guint minimum_width,
					  	      guint minimum_height)
{
	int left, top;
	guint width, height;
	NautilusGdkGeometryFlags geometry_flags;

	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (geometry_string != NULL);

	/* Setting the default size doesn't work when the window is already showing.
	 * Someday we could make this move an already-showing window, but we don't
	 * need that functionality yet. 
	 */
	g_return_if_fail (!GTK_WIDGET_VISIBLE (window));

	geometry_flags = nautilus_gdk_parse_geometry (geometry_string, &left, &top, &width, &height);

	/* Make sure the window isn't smaller than makes sense for this window.
	 * Other sanity checks are performed in set_initial_geometry.
	 */
	if (geometry_flags & NAUTILUS_GDK_WIDTH_VALUE) {
		width = MAX (width, minimum_width);
	}
	if (geometry_flags & NAUTILUS_GDK_HEIGHT_VALUE) {
		height = MAX (height, minimum_height);
	}

	nautilus_gtk_window_set_initial_geometry (window, geometry_flags, left, top, width, height);
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
	 * FIXME bugzilla.eazel.com 2561: The cast from gint16 might cause problems.  
	 * Unfortunately, GdkPoint uses gint16.
	 */
	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);
	  
	*x = CLAMP (*x + (int) offset->x, 0, MAX (0, gdk_screen_width () - requisition.width));
	*y = CLAMP (*y + (int) offset->y, 0, MAX (0, gdk_screen_height () - requisition.height));
}

/**
 * nautilus_truncate_text_for_menu_item:
 * 
 * Given an arbitrary string, returns a newly-allocated string
 * suitable for use as a menu item label. Truncates long strings 
 * in the middle.
 */
char *
nautilus_truncate_text_for_menu_item (const char *text)
{
	return nautilus_str_middle_truncate (text, MAXIMUM_MENU_TITLE_LENGTH);
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
 * @menu: The menu to pop up under the mouse.
 * @offset_x: Number of pixels to displace the popup menu vertically
 * @offset_y: Number of pixels to displace the popup menu horizontally
 * @event: The event that invoked this popup menu.
 **/
void 
nautilus_pop_up_context_menu (GtkMenu	     *menu,
			      gint16	      offset_x,
			      gint16	      offset_y,
			      GdkEventButton *event)
{
	GdkPoint offset;
	int button;

	g_return_if_fail (GTK_IS_MENU (menu));

	offset.x = offset_x;
	offset.y = offset_y;

	/* The event button needs to be 0 if we're popping up this menu from
	 * a button release, else a 2nd click outside the menu with any button
	 * other than the one that invoked the menu will be ignored (instead
	 * of dismissing the menu). This is a subtle fragility of the GTK menu code.
	 */
	button = event->type == GDK_BUTTON_RELEASE
		? 0
		: event->button;
	
	gtk_menu_popup (menu,					/* menu */
			NULL,					/* parent_menu_shell */
			NULL,					/* parent_menu_item */
			nautilus_popup_menu_position_func,	/* func */
			&offset,			        /* data */
			button,					/* button */
			event->time);				/* activate_time */

	gtk_object_sink (GTK_OBJECT(menu));
}

GtkMenuItem *
nautilus_gtk_menu_append_separator (GtkMenu *menu)
{
	return nautilus_gtk_menu_insert_separator (menu, -1);
}

GtkMenuItem *
nautilus_gtk_menu_insert_separator (GtkMenu *menu, int index)
{
	GtkWidget *menu_item;

	menu_item = gtk_menu_item_new ();
	gtk_widget_set_sensitive (menu_item, FALSE);
	gtk_widget_show (menu_item);
	gtk_menu_insert (menu, menu_item, index);

	return GTK_MENU_ITEM (menu_item);
}

void
nautilus_gtk_menu_set_item_visibility (GtkMenu *menu, int index, gboolean visible)
{
	GList *children;
	GtkWidget *menu_item;

	g_return_if_fail (GTK_IS_MENU (menu));

	children = gtk_container_children (GTK_CONTAINER (menu));
	g_return_if_fail (index >= 0 && index < (int) g_list_length (children));

	menu_item = GTK_WIDGET (g_list_nth_data (children, index));
	if (visible) {
		gtk_widget_show (menu_item);
	} else {
		gtk_widget_hide (menu_item);
	}

	g_list_free (children);
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
nautilus_gtk_marshal_BOOL__INT_POINTER_INT_INT_UINT (GtkObject *object,
						     GtkSignalFunc func,
						     gpointer func_data,
						     GtkArg *args)
{
	* GTK_RETLOC_BOOL (args[5]) = (* (gboolean (*)(GtkObject *, int, gpointer, int, int, guint, gpointer)) func)
		(object,
		 GTK_VALUE_INT (args[0]),
		 GTK_VALUE_POINTER (args[1]),
		 GTK_VALUE_INT (args[2]),
		 GTK_VALUE_INT (args[3]),
		 GTK_VALUE_UINT (args[4]),
		 func_data);
}


void
nautilus_gtk_marshal_NONE__INT_POINTER_INT_INT_UINT (GtkObject *object,
						     GtkSignalFunc func,
						     gpointer func_data,
						     GtkArg *args)
{
	(* (void (*)(GtkObject *, int, gpointer, int, int, guint, gpointer)) func)
		(object,
		 GTK_VALUE_INT (args[0]),
		 GTK_VALUE_POINTER (args[1]),
		 GTK_VALUE_INT (args[2]),
		 GTK_VALUE_INT (args[3]),
		 GTK_VALUE_UINT (args[4]),
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
nautilus_gtk_marshal_NONE__POINTER_POINTER_POINTER_POINTER_INT_INT_UINT (GtkObject *object,
									 GtkSignalFunc func,
									 gpointer func_data,
									 GtkArg *args)
{
	(* (void (*)(GtkObject *, gpointer, gpointer, gpointer, gpointer, int, int, guint, gpointer)) func)
		(object,
		 GTK_VALUE_POINTER (args[0]),
		 GTK_VALUE_POINTER (args[1]),
		 GTK_VALUE_POINTER (args[2]),
		 GTK_VALUE_POINTER (args[3]),
		 GTK_VALUE_INT (args[4]),
		 GTK_VALUE_INT (args[5]),
		 GTK_VALUE_INT (args[6]),
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
	
	font = gdk_fontset_load (font_name);
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

static void
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
	
	info->signal_handler = gtk_signal_connect_full (object,
							name,
							func,
							marshal,
							data,
							destroy_func,
							object_signal,
							after);
	info->disconnect_handler1 = gtk_signal_connect (object,
							"destroy",
							alive_disconnecter,
							info);
	info->disconnect_handler2 = gtk_signal_connect (alive_object,
							"destroy",
							alive_disconnecter,
							info);
}

typedef struct
{
	GtkObject *object;
	guint object_destroy_handler;
	
	GtkWidget *realized_widget;
	guint realized_widget_destroy_handler;
	guint realized_widget_unrealized_handler;

	guint signal_handler;
} RealizeDisconnectInfo;

static void
while_realized_disconnecter (GtkObject *object,
			     RealizeDisconnectInfo *info)
{
	g_return_if_fail (GTK_IS_OBJECT (object));
	g_return_if_fail (info != NULL);
	g_return_if_fail (GTK_IS_OBJECT (info->object));
	g_return_if_fail (info->object_destroy_handler != 0);
	g_return_if_fail (info->object_destroy_handler != 0);
	g_return_if_fail (info->realized_widget_destroy_handler != 0);
	g_return_if_fail (info->realized_widget_unrealized_handler != 0);

 	gtk_signal_disconnect (info->object, info->object_destroy_handler);
 	gtk_signal_disconnect (info->object, info->signal_handler);
 	gtk_signal_disconnect (GTK_OBJECT (info->realized_widget), info->realized_widget_destroy_handler);
 	gtk_signal_disconnect (GTK_OBJECT (info->realized_widget), info->realized_widget_unrealized_handler);
	g_free (info);
}

/**
 * nautilus_gtk_signal_connect_while_realized:
 *
 * @object: Object to connect to.
 * @name: Name of signal to connect to.
 * @callback: Caller's callback.
 * @callback_data: Caller's callback_data.
 * @realized_widget: Widget to monitor for realized state.  Signal is connected
 *                   while this wigget is realized.
 *
 * Connect to a signal of an object while another widget is realized.  This is 
 * useful for non windowed widgets that need to monitor events in their ancestored
 * windowed widget.  The signal is automatically disconnected when &widget is
 * unrealized.  Also, the signal is automatically disconnected when either &object
 * or &widget are destroyed.
 **/
void
nautilus_gtk_signal_connect_while_realized (GtkObject *object,
					    const char *name,
					    GtkSignalFunc callback,
					    gpointer callback_data,
					    GtkWidget *realized_widget)
{
	RealizeDisconnectInfo *info;

	g_return_if_fail (GTK_IS_OBJECT (object));
	g_return_if_fail (name != NULL);
	g_return_if_fail (name[0] != '\0');
	g_return_if_fail (callback != NULL);
	g_return_if_fail (GTK_IS_WIDGET (realized_widget));
	g_return_if_fail (GTK_WIDGET_REALIZED (realized_widget));

	info = g_new0 (RealizeDisconnectInfo, 1);
	
	info->object = object;
	info->object_destroy_handler = 
		gtk_signal_connect (info->object,
				    "destroy",
				    while_realized_disconnecter,
				    info);
	
	info->realized_widget = realized_widget;
	info->realized_widget_destroy_handler = 
		gtk_signal_connect (GTK_OBJECT (info->realized_widget),
				    "destroy",
				    while_realized_disconnecter,
				    info);
	info->realized_widget_unrealized_handler = 
		gtk_signal_connect_after (GTK_OBJECT (info->realized_widget),
					  "unrealize",
					  while_realized_disconnecter,
					  info);

	info->signal_handler = gtk_signal_connect (info->object, name, callback, callback_data);
}

static void
null_the_reference (GtkObject *object, gpointer callback_data)
{
	g_assert (* (GtkObject **) callback_data == object);

	* (gpointer *) callback_data = NULL;
}

/**
 * nautilus_nullify_when_destroyed.
 *
 * Nulls out a saved reference to an object when the object gets destroyed.
 * @data: Address of the saved reference.
 **/

void 
nautilus_nullify_when_destroyed (gpointer data)
{
	GtkObject **object_reference;

	object_reference = (GtkObject **)data;	
	if (*object_reference == NULL) {
		/* the reference is  NULL, nothing to do. */
		return;
	}

	g_assert (GTK_IS_OBJECT (*object_reference));

	gtk_signal_connect (*object_reference, "destroy",
		null_the_reference, object_reference);
}

/**
 * nautilus_nullify_cancel.
 *
 * Disconnects the signal used to make nautilus_nullify_when_destroyed.
 * Used when the saved reference is no longer needed, the structure it is in is
 * being destroyed, etc. Nulls out the refernce when done.
 * @data: Address of the saved reference.
 **/

void 
nautilus_nullify_cancel (gpointer data)
{
	GtkObject **object_reference;

	object_reference = (GtkObject **)data;	
	if (*object_reference == NULL) {
		/* the object was already destroyed and the reference nulled out,
		 * nothing to do.
		 */
		return;
	}

	g_assert (GTK_IS_OBJECT (*object_reference));

	gtk_signal_disconnect_by_func (*object_reference,
		null_the_reference, object_reference);
	
	*object_reference = NULL;
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

typedef struct {
	GtkCallback   callback;
	gpointer      callback_data;
} container_foreach_deep_callback_data;

static void
container_foreach_deep_callback (GtkWidget *child, gpointer data)
{
	container_foreach_deep_callback_data *deep_data;

	deep_data = (container_foreach_deep_callback_data *) data;

	deep_data->callback (child, deep_data->callback_data);

	if (GTK_IS_CONTAINER (child)) {
		gtk_container_foreach (GTK_CONTAINER (child), container_foreach_deep_callback, data);
	}
}

void
nautilus_gtk_container_foreach_deep (GtkContainer *container,
				     GtkCallback callback,
				     gpointer callback_data)
{
	container_foreach_deep_callback_data deep_data;
	deep_data.callback = callback;
	deep_data.callback_data = callback_data;
	gtk_container_foreach (container, container_foreach_deep_callback, &deep_data);
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

	gtk_widget_ensure_style (GTK_WIDGET (label));
	style = gtk_widget_get_style (GTK_WIDGET (label));

	bold_font = nautilus_gdk_font_get_bold (style->font);
	if (bold_font == NULL) {
		return;
	}
	nautilus_gtk_widget_set_font (GTK_WIDGET (label), bold_font);
	gdk_font_unref (bold_font);
}

/**
 * nautilus_gtk_label_make_larger.
 *
 * Switches the font of label to a larger version of the font.
 * @label: The label.
 **/
void
nautilus_gtk_label_make_larger (GtkLabel *label,
				guint num_steps)
{
	GtkStyle *style;
	GdkFont *larger_font;

	g_return_if_fail (GTK_IS_LABEL (label));

	gtk_widget_ensure_style (GTK_WIDGET (label));
	style = gtk_widget_get_style (GTK_WIDGET (label));

	larger_font = nautilus_gdk_font_get_larger (style->font, num_steps);
	if (larger_font == NULL) {
		return;
	}
	nautilus_gtk_widget_set_font (GTK_WIDGET (label), larger_font);
	gdk_font_unref (larger_font);
}

/**
 * nautilus_gtk_label_make_smaller.
 *
 * Switches the font of label to a smaller version of the font.
 * @label: The label.
 **/
void
nautilus_gtk_label_make_smaller (GtkLabel *label,
				 guint num_steps)
{
	GtkStyle *style;
	GdkFont *smaller_font;

	g_return_if_fail (GTK_IS_LABEL (label));

	gtk_widget_ensure_style (GTK_WIDGET (label));
	style = gtk_widget_get_style (GTK_WIDGET (label));

	smaller_font = nautilus_gdk_font_get_smaller (style->font, num_steps);
	if (smaller_font == NULL) {
		return;
	}
	nautilus_gtk_widget_set_font (GTK_WIDGET (label), smaller_font);
	gdk_font_unref (smaller_font);
}

void
nautilus_gtk_widget_set_background_color (GtkWidget              *widget,
					  const char		*color_spec)
{
	GtkStyle	*style;
	GdkColor	color;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	style = gtk_widget_get_style (widget);

	/* Make a copy of the style. */
	style = gtk_style_copy (style);

	nautilus_gdk_color_parse_with_white_default (color_spec, &color);
	style->bg[GTK_STATE_NORMAL] = color;
	style->base[GTK_STATE_NORMAL] = color;
	style->bg[GTK_STATE_ACTIVE] = color;
	style->base[GTK_STATE_ACTIVE] = color;

	/* Put the style in the widget. */
	gtk_widget_set_style (widget, style);
	gtk_style_unref (style);
}

void
nautilus_gtk_widget_set_foreground_color (GtkWidget              *widget,
					  const char		*color_spec)
{
	GtkStyle	*style;
	GdkColor	color;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	style = gtk_widget_get_style (widget);

	/* Make a copy of the style. */
	style = gtk_style_copy (style);

	nautilus_gdk_color_parse_with_white_default (color_spec, &color);
	style->fg[GTK_STATE_NORMAL] = color;
	style->fg[GTK_STATE_ACTIVE] = color;

	/* Put the style in the widget. */
	gtk_widget_set_style (widget, style);
	gtk_style_unref (style);
}

GtkWidget *
nautilus_gtk_widget_find_windowed_ancestor (GtkWidget *widget)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	while (widget && GTK_WIDGET_NO_WINDOW (widget)) {
		widget = widget->parent;
	}

	return widget;
}



/*following code shamelessly stolen from gtk*/
static void
rgb_to_hls (gdouble *r,
            gdouble *g,
            gdouble *b)
{
	gdouble min;
	gdouble max;
	gdouble red;
	gdouble green;
	gdouble blue;
	gdouble h, l, s;
	gdouble delta;

	red = *r;
	green = *g;
	blue = *b;

	if (red > green) {
		if (red > blue)
			max = red;
		else
			max = blue;
		
		if (green < blue)
			min = green;
		else
			min = blue;
	} else {
		if (green > blue)
			max = green;
		else
			max = blue;
		
		if (red < blue)
			min = red;
		else
			min = blue;
	}

	l = (max + min) / 2;
	s = 0;
	h = 0;

	if (max != min) {
		if (l <= 0.5)
			s = (max - min) / (max + min);
		else
			s = (max - min) / (2 - max - min);
		
		delta = max -min;
		if (red == max)
			h = (green - blue) / delta;
		else if (green == max)
			h = 2 + (blue - red) / delta;
		else if (blue == max)
			h = 4 + (red - green) / delta;
		
		h *= 60;
		if (h < 0.0)
			h += 360;
	}
	
	*r = h;
	*g = l;
	*b = s;
}

static void
hls_to_rgb (gdouble *h,
            gdouble *l,
            gdouble *s)
{
	gdouble hue;
	gdouble lightness;
	gdouble saturation;
	gdouble m1, m2;
	gdouble r, g, b;
	
	lightness = *l;
	saturation = *s;

	if (lightness <= 0.5)
		m2 = lightness * (1 + saturation);
	else
		m2 = lightness + saturation - lightness * saturation;
	m1 = 2 * lightness - m2;

	if (saturation == 0) {
		*h = lightness;
		*l = lightness;
		*s = lightness;
	} else {
		hue = *h + 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;
		
		if (hue < 60)
			r = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			r = m2;
		else if (hue < 240)
			r = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			r = m1;

		hue = *h;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;
		
		if (hue < 60)
			g = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			g = m2;
		else if (hue < 240)
			g = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			g = m1;
		
		hue = *h - 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;
		
		if (hue < 60)
			b = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			b = m2;
		else if (hue < 240)
			b = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			b = m1;
		
		*h = r;
		*l = g;
		*s = b;
	}
}

void
nautilus_gtk_style_shade (GdkColor *a,
			  GdkColor *b,
			  gdouble   k)
{

	gdouble red;
	gdouble green;
	gdouble blue;
	
	red = (gdouble) a->red / 65535.0;
	green = (gdouble) a->green / 65535.0;
	blue = (gdouble) a->blue / 65535.0;
	
	rgb_to_hls (&red, &green, &blue);

	green *= k;
	if (green > 1.0)
		green = 1.0;
	else if (green < 0.0)
		green = 0.0;

	blue *= k;
	if (blue > 1.0)
		blue = 1.0;
	else if (blue < 0.0)
		blue = 0.0;

	hls_to_rgb (&red, &green, &blue);

	b->red = red * 65535.0;
	b->green = green * 65535.0;
	b->blue = blue * 65535.0;
}

/**
 * nautilus_gtk_class_name_make_like_existing_type:
 * @class_name: The class name for the custom widget.
 * @existing_gtk_type: The GtkType of the existing GtkWidget.
 *
 * Make the given class name act like an existing GtkType for
 * gtk theme/style purposes.  This can be used by custom 
 * widget to emulate the styles of stock Gtk widgets.  For
 * example:
 *
 * nautilus_gtk_class_name_make_like_existing ("NautilusCustomButton",
 *                                             GTK_TYPE_BUTTON);
 *
 *
 * You should call this function only once from the class_initialize()
 * method of the custom widget.
 **/
void
nautilus_gtk_class_name_make_like_existing_type (const char *class_name,
						 GtkType existing_gtk_type)
{
	GtkWidget *temporary;
	GtkStyle *style;

	g_return_if_fail (class_name != NULL);

	temporary = gtk_widget_new (existing_gtk_type, NULL);
	gtk_widget_ensure_style (temporary);
		
	style = gtk_widget_get_style (temporary);

	if (style->rc_style != NULL) {
		gtk_rc_add_widget_class_style (style->rc_style, class_name);
	}
		
	gtk_widget_destroy (temporary);
}

/* helper function for nautilus_get_window_list_ordered_front_to_back () */
static GtkWidget *
window_at_or_below (int depth, Window xid, gboolean *keep_going)
{
	static Atom wm_state = 0;

	GtkWidget *widget;

	Atom actual_type;
	int actual_format;
	gulong nitems, bytes_after;
	gulong *prop;

	GdkWindow *window;
	gpointer data;

	Window root, parent, *children;
	int nchildren, i;

	if (wm_state == 0) {
		wm_state = XInternAtom (GDK_DISPLAY (), "WM_STATE", False);
	}

	/* Check if the window is a top-level client window.
	 * Windows will have a WM_STATE property iff they're top-level.
	 */
	if (XGetWindowProperty (GDK_DISPLAY (), xid, wm_state, 0, 1,
				False, AnyPropertyType, &actual_type,
				&actual_format, &nitems, &bytes_after,
				(guchar **) &prop) == Success
	    && prop != NULL && actual_format == 32 && prop[0] == NormalState)
	{
		/* Found a top-level window */

		if (prop != NULL) {
			XFree (prop);
		}

		/* Does GDK know anything about this window? */
		window = gdk_window_lookup (xid);
		if (window != NULL) {
			gdk_window_get_user_data (window, &data);
			if (data != NULL)
			{
				/* Found one of the widgets we're after */
				*keep_going = FALSE;
				return GTK_WIDGET (data);
			}
		}

		/* No point in searching past here. It's a top-level
		 * window, but not from this application.
		 */
		*keep_going = FALSE;
		return NULL;
	}

	/* Not found a top-level window yet, so keep recursing. */
	if (depth < MAXIMUM_WM_REPARENTING_DEPTH) {
		if (XQueryTree (GDK_DISPLAY (), xid, &root,
				&parent, &children, &nchildren) != 0)
		{
			widget = NULL;

			for (i = 0; *keep_going && i < nchildren; i++) {
				widget = window_at_or_below (depth + 1,
							     children[i],
							     keep_going);
			}

			if (children != NULL) {
				XFree (children);
			}

			if (! *keep_going) {
				return widget;
			}
		}
	}

	return NULL;
}

/* nautilus_get_window_list_ordered_front_to_back:
 *
 * Return a list of GtkWindows's, representing the stacking order (top to
 * bottom) of all windows (known to the local GDK).
 *
 * (Involves a large number of X server round trips, so call sparingly)
 */
GList *
nautilus_get_window_list_ordered_front_to_back (void)
{
	Window root, parent, *children;
	int nchildren, i;
	GList *windows;
	GtkWidget *widget;
	gboolean keep_going;

	/* There's a possibility that a window will be closed in
	 * the period between us querying the child-of-root windows
	 * and getting round to search _their_ children. So arrange
	 * for errors to be caught and ignored.
	 */

	gdk_error_trap_push ();

	windows = NULL;

	if (XQueryTree (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
			&root, &parent, &children, &nchildren) != 0)
	{
		for (i = 0; i < nchildren; i++) {
			keep_going = TRUE;
			widget = window_at_or_below (0, children[i],
						     &keep_going);
			if (widget != NULL) {
				/* XQueryTree returns window in bottom ->
				 * top order, so consing up the list in
				 * the normal manner will reverse this
				 * giving the desired top -> bottom order
				 */
				windows = g_list_prepend (windows, widget);
			}
		}
		if (children != NULL) {
			XFree (children);
		}
	}

	gdk_flush ();
	gdk_error_trap_pop ();

	return windows;
}
