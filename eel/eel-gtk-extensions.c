/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gtk-extensions.c - implementation of new functions that operate on
  			  gtk classes. Perhaps some of these should be
  			  rolled into gtk someday.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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
	    Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "eel-gtk-extensions.h"

#include "eel-gdk-pixbuf-extensions.h"
#include "eel-glib-extensions.h"
#include "eel-gnome-extensions.h"
#include "eel-pango-extensions.h"
#include "eel-string.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdk.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <math.h>
#include "eel-marshal.h"
#include "eel-marshal.c"

/* This number is fairly arbitrary. Long enough to show a pretty long
 * menu title, but not so long to make a menu grotesquely wide.
 */
#define MAXIMUM_MENU_TITLE_LENGTH	48

/* Used for window position & size sanity-checking. The sizes are big enough to prevent
 * at least normal-sized gnome panels from obscuring the window at the screen edges. 
 */
#define MINIMUM_ON_SCREEN_WIDTH		100
#define MINIMUM_ON_SCREEN_HEIGHT	100


/**
 * eel_gtk_window_get_geometry_string:
 * @window: a #GtkWindow
 * 
 * Obtains the geometry string for this window, suitable for
 * set_geometry_string(); assumes the window has NorthWest gravity
 * 
 * Return value: geometry string, must be freed
 **/
char*
eel_gtk_window_get_geometry_string (GtkWindow *window)
{
	char *str;
	int w, h, x, y;
	
	g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);
	g_return_val_if_fail (gtk_window_get_gravity (window) ==
			      GDK_GRAVITY_NORTH_WEST, NULL);

	gtk_window_get_position (window, &x, &y);
	gtk_window_get_size (window, &w, &h);
	
	str = g_strdup_printf ("%dx%d+%d+%d", w, h, x, y);

	return str;
}

static void
send_delete_event (GtkWindow *window)
{
	/* Synthesize delete_event to close window. */
	
	GdkEvent event;
	GtkWidget *widget;
	
	widget = GTK_WIDGET (window);
	
	event.any.type = GDK_DELETE;
	event.any.window = widget->window;
	event.any.send_event = TRUE;
	
	g_object_ref (event.any.window);
	gtk_main_do_event (&event);
	g_object_unref (event.any.window);
}

static int
handle_standard_close_accelerator (GtkWindow *window, 
				   GdkEventKey *event, 
				   gpointer user_data)
{
	g_assert (GTK_IS_WINDOW (window));
	g_assert (event != NULL);
	g_assert (user_data == NULL);

	if (eel_gtk_window_event_is_close_accelerator (window, event)) {
		send_delete_event (window);
		g_signal_stop_emission_by_name (
			G_OBJECT (window), "key_press_event");
		return TRUE;
	}

	return FALSE;
}

/**
 * eel_gtk_window_event_is_close_accelerator:
 * 
 * Tests whether a key event is a standard window close accelerator.
 * Not needed for clients that use eel_gtk_window_set_up_close_accelerator;
 * use only if you must set up your own key_event handler for your own reasons.
 **/
gboolean
eel_gtk_window_event_is_close_accelerator (GtkWindow *window, GdkEventKey *event)
{
	g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (event->state & GDK_CONTROL_MASK) {
		/* Note: menu item equivalents are case-sensitive, so we will
		 * be case-sensitive here too.
		 */		
		if (event->keyval == EEL_STANDARD_CLOSE_WINDOW_CONTROL_KEY) {
			return TRUE;
		}
	}


	return FALSE;	
}

/**
 * eel_gtk_window_set_up_close_accelerator:
 * 
 * Sets up the standard keyboard equivalent to close the window.
 * Call this for windows that don't set up a keyboard equivalent to
 * close the window some other way, e.g. via a menu item accelerator.
 *
 * NOTE: do not use for GtkDialog, it already sets up the right
 * stuff here.
 * 
 * @window: The GtkWindow that should be hidden when the standard
 * keyboard equivalent is typed.
 **/
void
eel_gtk_window_set_up_close_accelerator (GtkWindow *window)
{
	g_return_if_fail (GTK_IS_WINDOW (window));

	if (GTK_IS_DIALOG (window)) {
		g_warning ("eel_gtk_window_set_up_close_accelerator: Should not mess with close accelerator on GtkDialogs");
		return;
	}
	
	g_signal_connect (window,
			    "key_press_event",
			    G_CALLBACK (handle_standard_close_accelerator),
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
sanity_check_window_dimensions (guint *width, guint *height)
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
 * eel_gtk_window_set_initial_geometry:
 * 
 * Sets the position and size of a GtkWindow before the
 * GtkWindow is shown. It is an error to call this on a window that
 * is already on-screen. Takes into account screen size, and does
 * some sanity-checking on the passed-in values.
 * 
 * @window: A non-visible GtkWindow
 * @geometry_flags: A EelGdkGeometryFlags value defining which of
 * the following parameters have defined values
 * @left: pixel coordinate for left of window
 * @top: pixel coordinate for top of window
 * @width: width of window in pixels
 * @height: height of window in pixels
 */
void
eel_gtk_window_set_initial_geometry (GtkWindow *window, 
					  EelGdkGeometryFlags geometry_flags,
					  int left,
					  int top,
					  guint width,
					  guint height)
{
	GdkScreen *screen;
	int real_left, real_top;
	int screen_width, screen_height;

	g_return_if_fail (GTK_IS_WINDOW (window));

	/* Setting the default size doesn't work when the window is already showing.
	 * Someday we could make this move an already-showing window, but we don't
	 * need that functionality yet. 
	 */
	g_return_if_fail (!GTK_WIDGET_VISIBLE (window));

	if ((geometry_flags & EEL_GDK_X_VALUE) && (geometry_flags & EEL_GDK_Y_VALUE)) {
		real_left = left;
		real_top = top;

		screen = gtk_window_get_screen (window);
		screen_width  = gdk_screen_get_width  (screen);
		screen_height = gdk_screen_get_height (screen);

		/* This is sub-optimal. GDK doesn't allow us to set win_gravity
		 * to South/East types, which should be done if using negative
		 * positions (so that the right or bottom edge of the window
		 * appears at the specified position, not the left or top).
		 * However it does seem to be consistent with other GNOME apps.
		 */
		if (geometry_flags & EEL_GDK_X_NEGATIVE) {
			real_left = screen_width - real_left;
		}
		if (geometry_flags & EEL_GDK_Y_NEGATIVE) {
			real_top = screen_height - real_top;
		}

		sanity_check_window_position (&real_left, &real_top);
		gtk_window_move (window, real_left, real_top);
	}

	if ((geometry_flags & EEL_GDK_WIDTH_VALUE) && (geometry_flags & EEL_GDK_HEIGHT_VALUE)) {
		sanity_check_window_dimensions (&width, &height);
		gtk_window_set_default_size (GTK_WINDOW (window), (int)width, (int)height);
	}
}

/**
 * eel_gtk_window_set_initial_geometry_from_string:
 * 
 * Sets the position and size of a GtkWindow before the
 * GtkWindow is shown. The geometry is passed in as a string. 
 * It is an error to call this on a window that
 * is already on-screen. Takes into account screen size, and does
 * some sanity-checking on the passed-in values.
 * 
 * @window: A non-visible GtkWindow
 * @geometry_string: A string suitable for use with eel_gdk_parse_geometry
 * @minimum_width: If the width from the string is smaller than this,
 * use this for the width.
 * @minimum_height: If the height from the string is smaller than this,
 * use this for the height.
 * @ignore_position: If true position data from string will be ignored.
 */
void
eel_gtk_window_set_initial_geometry_from_string (GtkWindow *window, 
						 const char *geometry_string,
						 guint minimum_width,
						 guint minimum_height,
						 gboolean ignore_position)
{
	int left, top;
	guint width, height;
	EelGdkGeometryFlags geometry_flags;

	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (geometry_string != NULL);

	/* Setting the default size doesn't work when the window is already showing.
	 * Someday we could make this move an already-showing window, but we don't
	 * need that functionality yet. 
	 */
	g_return_if_fail (!GTK_WIDGET_VISIBLE (window));

	geometry_flags = eel_gdk_parse_geometry (geometry_string, &left, &top, &width, &height);

	/* Make sure the window isn't smaller than makes sense for this window.
	 * Other sanity checks are performed in set_initial_geometry.
	 */
	if (geometry_flags & EEL_GDK_WIDTH_VALUE) {
		width = MAX (width, minimum_width);
	}
	if (geometry_flags & EEL_GDK_HEIGHT_VALUE) {
		height = MAX (height, minimum_height);
	}
	
	/* Ignore saved window position if requested. */
	if (ignore_position) {
		geometry_flags &= ~(EEL_GDK_X_VALUE | EEL_GDK_Y_VALUE);
	}

	eel_gtk_window_set_initial_geometry (window, geometry_flags, left, top, width, height);
}

/**
 * eel_pop_up_context_menu:
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
eel_pop_up_context_menu (GtkMenu	     *menu,
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

	if (event) {
		button = event->type == GDK_BUTTON_RELEASE
			? 0
			: event->button;
	} else {
		button = 0;
	}
	
	gtk_menu_popup (menu,					/* menu */
			NULL,					/* parent_menu_shell */
			NULL,					/* parent_menu_item */
			NULL,
			&offset,			        /* data */
			button,					/* button */
			event ? event->time : GDK_CURRENT_TIME); /* activate_time */

	g_object_ref_sink (menu);
	g_object_unref (menu);
}

GtkMenuItem *
eel_gtk_menu_append_separator (GtkMenu *menu)
{
	return eel_gtk_menu_insert_separator (menu, -1);
}

GtkMenuItem *
eel_gtk_menu_insert_separator (GtkMenu *menu, int index)
{
	GtkWidget *menu_item;

	menu_item = gtk_separator_menu_item_new ();
	gtk_widget_show (menu_item);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menu_item, index);

	return GTK_MENU_ITEM (menu_item);
}

void
eel_gtk_menu_set_item_visibility (GtkMenu *menu, int index, gboolean visible)
{
	GList *children;
	GtkWidget *menu_item;

	g_return_if_fail (GTK_IS_MENU (menu));

	children = gtk_container_get_children (GTK_CONTAINER (menu));
	g_return_if_fail (index >= 0 && index < (int) g_list_length (children));

	menu_item = GTK_WIDGET (g_list_nth_data (children, index));
	if (visible) {
		gtk_widget_show (menu_item);
	} else {
		gtk_widget_hide (menu_item);
	}

	g_list_free (children);
}

GtkWidget *
eel_gtk_menu_tool_button_get_button (GtkMenuToolButton *tool_button)
{
	GtkContainer *container;
	GList *children;
	GtkWidget *button;

	g_return_val_if_fail (GTK_IS_MENU_TOOL_BUTTON (tool_button), NULL);

	/* The menu tool button's button is the first child
	 * of the child hbox. */
	container = GTK_CONTAINER (gtk_bin_get_child (GTK_BIN (tool_button)));
	children = gtk_container_get_children (container);
	button = GTK_WIDGET (children->data);

	g_list_free (children);

	return button;
}

gboolean
eel_point_in_allocation (const GtkAllocation *allocation,
			      int x, int y)
{
	g_return_val_if_fail (allocation != NULL, FALSE);
	return x >= allocation->x
		&& y >= allocation->y
		&& x < allocation->x + allocation->width 
		&& y < allocation->y + allocation->height;
}

/* FIXME this function is dangerous, because widget->window coords (or
 * other window-belonging-to-widget coords) do not need to be in the
 * same coordinate system as widget->allocation.
 * If you use this function, be aware of that. Someone should probably
 * audit all uses, too.
 */
gboolean
eel_point_in_widget (GtkWidget *widget,
			  int x, int y)
{
	if (widget == NULL) {
		return FALSE;
	}
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	return eel_point_in_allocation (&widget->allocation, x, y);
}

/**
 * eel_gtk_widget_set_shown
 *
 * Show or hide a widget.
 * @widget: The widget.
 * @shown: Boolean value indicating whether the widget should be shown or hidden.
 **/
void
eel_gtk_widget_set_shown (GtkWidget *widget, gboolean shown)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (shown) {
		gtk_widget_show (widget);
	} else {
		gtk_widget_hide (widget);
	}
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
	
	g_signal_handler_disconnect (info->object1, info->disconnect_handler1);
	g_signal_handler_disconnect (info->object1, info->signal_handler);
	g_signal_handler_disconnect (info->object2, info->disconnect_handler2);
	
	g_free (info);
}

/**
 * eel_gtk_signal_connect_full_while_alive
 *
 * Like gtk_signal_connect_while_alive, but works with full parameters.
 **/
void
eel_gtk_signal_connect_full_while_alive (GtkObject *object,
					 const gchar *name,
					 GCallback func,
					 GtkCallbackMarshal marshal,
					 gpointer data,
					 GDestroyNotify destroy_func,
					 gboolean object_signal,
					 gboolean after,
					 GtkObject *alive_object)
{
	DisconnectInfo *info;
	
	g_return_if_fail (GTK_IS_OBJECT (object));
	g_return_if_fail (name != NULL);
	g_return_if_fail (func != NULL || marshal != NULL);
	g_return_if_fail (object_signal == FALSE || object_signal == TRUE);
	g_return_if_fail (after == FALSE || after == TRUE);
	g_return_if_fail (GTK_IS_OBJECT (alive_object));
	
	info = g_new (DisconnectInfo, 1);
	info->object1 = object;
	info->object2 = alive_object;
	

	info->signal_handler = g_signal_connect_closure (
		object, name,
		(object_signal
		 ? g_cclosure_new_swap
		 : g_cclosure_new) (func, data, (GClosureNotify) destroy_func),
		after);

	info->disconnect_handler1 = g_signal_connect (G_OBJECT (object),
						      "destroy",
						      G_CALLBACK (alive_disconnecter),
						      info);
	info->disconnect_handler2 = g_signal_connect (G_OBJECT (alive_object),
						      "destroy",
						      G_CALLBACK (alive_disconnecter),
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
	g_assert (GTK_IS_OBJECT (object));
	g_assert (info != NULL);
	g_assert (GTK_IS_OBJECT (info->object));
	g_assert (info->object_destroy_handler != 0);
	g_assert (info->object_destroy_handler != 0);
	g_assert (info->realized_widget_destroy_handler != 0);
	g_assert (info->realized_widget_unrealized_handler != 0);

 	g_signal_handler_disconnect (info->object, info->object_destroy_handler);
 	g_signal_handler_disconnect (info->object, info->signal_handler);
 	g_signal_handler_disconnect (info->realized_widget, info->realized_widget_destroy_handler);
 	g_signal_handler_disconnect (info->realized_widget, info->realized_widget_unrealized_handler);
	g_free (info);
}

/**
 * eel_gtk_signal_connect_while_realized:
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
eel_gtk_signal_connect_while_realized (GtkObject *object,
					    const char *name,
					    GCallback callback,
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
		g_signal_connect (G_OBJECT (info->object),
				  "destroy",
				  G_CALLBACK (while_realized_disconnecter),
				  info);
	
	info->realized_widget = realized_widget;
	info->realized_widget_destroy_handler = 
		g_signal_connect (G_OBJECT (info->realized_widget),
				  "destroy",
				  G_CALLBACK (while_realized_disconnecter),
				  info);
	info->realized_widget_unrealized_handler = 
		g_signal_connect_after (G_OBJECT (info->realized_widget),
					"unrealize",
					G_CALLBACK (while_realized_disconnecter),
					info);

	info->signal_handler = g_signal_connect (G_OBJECT (info->object),
						 name, callback, callback_data);
}

/**
 * eel_gtk_container_get_first_child.
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
eel_gtk_container_get_first_child (GtkContainer *container)
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
eel_gtk_container_foreach_deep (GtkContainer *container,
				     GtkCallback callback,
				     gpointer callback_data)
{
	container_foreach_deep_callback_data deep_data;
	deep_data.callback = callback;
	deep_data.callback_data = callback_data;
	gtk_container_foreach (container, container_foreach_deep_callback, &deep_data);
}

/* The standard gtk_adjustment_set_value ignores page size, which
 * disagrees with the logic used by scroll bars, for example.
 */
void
eel_gtk_adjustment_set_value (GtkAdjustment *adjustment,
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
eel_gtk_adjustment_clamp_value (GtkAdjustment *adjustment)
{
	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	
	eel_gtk_adjustment_set_value (adjustment, adjustment->value);
}

/**
 * eel_gtk_label_make_bold.
 *
 * Switches the font of label to a bold equivalent.
 * @label: The label.
 **/
void
eel_gtk_label_make_bold (GtkLabel *label)
{
	PangoFontDescription *font_desc;

	font_desc = pango_font_description_new ();

	pango_font_description_set_weight (font_desc,
					   PANGO_WEIGHT_BOLD);

	/* This will only affect the weight of the font, the rest is
	 * from the current state of the widget, which comes from the
	 * theme or user prefs, since the font desc only has the
	 * weight flag turned on.
	 */
	gtk_widget_modify_font (GTK_WIDGET (label), font_desc);

	pango_font_description_free (font_desc);
}

/**
 * eel_gtk_label_set_scale:
 * @label: 
 * @num_steps: 
 *
 * Function is broken, see eel_gtk_label_make_larger() for explanation
 * 
 **/
void
eel_gtk_label_set_scale (GtkLabel *label,
			 double scale_factor)
{
	PangoAttrList *old_attr_list;
	PangoAttrList *attr_list;
	
	g_return_if_fail (GTK_IS_LABEL (label));
	g_return_if_fail (scale_factor > 0);

	old_attr_list = gtk_label_get_attributes (label);
	attr_list = eel_pango_attr_list_apply_global_attribute (old_attr_list,
								pango_attr_scale_new (scale_factor));
	gtk_label_set_attributes (label, attr_list);
	pango_attr_list_unref (attr_list);
}

static void
get_layout_location (GtkLabel  *label,
                     gint      *xp,
                     gint      *yp)
{
  GtkMisc *misc;
  GtkWidget *widget;
  float xalign;
  int x, y;
  int shadow_offset;
  
  shadow_offset = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (label),
						      "eel-label-shadow-offset"));
  
  misc = GTK_MISC (label);
  widget = GTK_WIDGET (label);
  
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    xalign = misc->xalign;
  else
    xalign = 1.0 - misc->xalign;
  
  x = floor (widget->allocation.x + (int)misc->xpad
             + ((widget->allocation.width - widget->requisition.width - shadow_offset) * xalign)
             + 0.5);
  
  y = floor (widget->allocation.y + (int)misc->ypad 
             + ((widget->allocation.height - widget->requisition.height - shadow_offset) * misc->yalign)
             + 0.5);
  

  if (xp)
    *xp = x;

  if (yp)
    *yp = y;
}

static gboolean
eel_gtk_label_expose_event (GtkLabel *label, GdkEventExpose *event, gpointer user_data)
{
	int x, y;
	GdkColor color;
	GtkWidget *widget;
	GdkGC *gc;
	guint32 shadow_color;
	int shadow_offset;
	
	shadow_color = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (label),
							   "eel-label-shadow-color"));
	shadow_offset = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (label),
							    "eel-label-shadow-offset"));

	color = eel_gdk_rgb_to_color (shadow_color);
	
	get_layout_location (label, &x, &y);

	widget = GTK_WIDGET (label);
	if (shadow_offset > 0) {
		gc = gdk_gc_new (widget->window);
		gdk_gc_set_rgb_fg_color (gc, &color);
		gdk_gc_set_clip_rectangle (gc, &event->area);
		
		gdk_draw_layout (widget->window,
				 gc,
				 x + shadow_offset, y + shadow_offset,
				 label->layout);
		g_object_unref (gc);
	}
	
	gtk_paint_layout (widget->style,
			  widget->window,
			  GTK_WIDGET_STATE (widget),
			  FALSE,
			  &event->area,
			  widget,
			  "label",
			  x, y,
			  label->layout);

	return TRUE;
}

static void
eel_gtk_label_size_request (GtkLabel *label, GtkRequisition *requisition, gpointer user_data)
{
	gint shadow_offset;

	shadow_offset = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (label),
							    "eel-label-shadow-offset"));

	requisition->width += shadow_offset;
	requisition->height += shadow_offset;
}

static void
set_up_label (GtkLabel *label)
{

	if (g_object_get_data (G_OBJECT (label), "eel-label-set-up") != NULL) {
		return;
	}

	g_signal_connect (label, "expose_event",
			  G_CALLBACK (eel_gtk_label_expose_event), NULL);
	g_signal_connect_after (label, "size_request",
				G_CALLBACK (eel_gtk_label_size_request), NULL);

	g_object_set_data (G_OBJECT (label), "eel-label-set-up", "eel-label-set-up");
}

void
eel_gtk_label_set_drop_shadow_color (GtkLabel *label,
				     guint32 color)
{
	set_up_label (label);

	g_object_set_data (G_OBJECT (label), "eel-label-shadow-color",
			   GINT_TO_POINTER (color));
		
	gtk_widget_queue_draw (GTK_WIDGET (label));
}

void
eel_gtk_label_set_drop_shadow_offset (GtkLabel *label,
				      gint offset)
{
	set_up_label (label);

	g_object_set_data (G_OBJECT (label), "eel-label-shadow-offset",
			   GINT_TO_POINTER (offset));
	
	gtk_widget_queue_draw (GTK_WIDGET (label));
}

void
eel_gtk_widget_set_background_color (GtkWidget *widget,
				     const char *color_spec)
{
	GdkColor color;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	eel_gdk_color_parse_with_white_default (color_spec, &color);

	gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_base (widget, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_bg (widget, GTK_STATE_ACTIVE, &color);
	gtk_widget_modify_base (widget, GTK_STATE_ACTIVE, &color);
}

void
eel_gtk_widget_set_foreground_color (GtkWidget *widget,
				     const char *color_spec)
{
	GdkColor color;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	eel_gdk_color_parse_with_white_default (color_spec, &color);

	gtk_widget_modify_fg (widget, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_text (widget, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_fg (widget, GTK_STATE_ACTIVE, &color);
	gtk_widget_modify_text (widget, GTK_STATE_ACTIVE, &color);
}

GtkWidget *
eel_gtk_widget_find_windowed_ancestor (GtkWidget *widget)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	while (widget && GTK_WIDGET_NO_WINDOW (widget)) {
		widget = widget->parent;
	}

	return widget;
}

/* eel_gtk_get_system_font:
 *
 * Return the system font as selected in the control center. Need to 
 * g_object_unref() the result when done with it.
 *
 * Perhaps there is a better way to figure out what that font is, but
 * the following is simple enough and it works.
 */
PangoFontDescription *
eel_gtk_get_system_font (void)
{
	GtkWidget *label;
	PangoFontDescription *font;

	label = gtk_label_new ("");
	
	gtk_widget_ensure_style (label);

	font = pango_font_description_copy (label->style->font_desc);

	g_object_ref_sink (label);
	g_object_unref (label);

	return font;
}

void
eel_gtk_widget_get_button_event_location (GtkWidget *widget,
					  const GdkEventButton *event,
					  int *x,
					  int *y)
{
	int window_x, window_y;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (event != NULL);

	gdk_window_get_position (event->window, &window_x, &window_y);
	if (x != NULL) {
		*x = event->x + window_x - widget->allocation.x;
	}
	if (y != NULL) {
		*y = event->y + window_y - widget->allocation.y;
	}
}

void
eel_gtk_widget_get_motion_event_location (GtkWidget *widget,
					  const GdkEventMotion *event,
					  int *x,
					  int *y)
{
	eel_gtk_widget_get_button_event_location (widget, (const GdkEventButton *) event, x, y);
}

static gboolean 
tree_view_button_press_callback (GtkWidget *tree_view,
				 GdkEventButton *event,
				 gpointer data)
{
	GtkTreePath *path;
	GtkTreeViewColumn *column;

	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
		if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view),
						   event->x, event->y,
						   &path,
						   &column,
						   NULL, 
						   NULL)) {
			gtk_tree_view_row_activated
				(GTK_TREE_VIEW (tree_view), path, column);
		}
	}

	return FALSE;
}

void
eel_gtk_tree_view_set_activate_on_single_click (GtkTreeView *tree_view,
						gboolean should_activate)
{
	guint button_press_id;

	button_press_id = GPOINTER_TO_UINT 
		(g_object_get_data (G_OBJECT (tree_view), 
				    "eel-tree-view-activate"));

	if (button_press_id && !should_activate) {
		g_signal_handler_disconnect (tree_view, button_press_id);
		g_object_set_data (G_OBJECT (tree_view), 
				   "eel-tree-view-activate", 
				   NULL);
	} else if (!button_press_id && should_activate) {
		button_press_id = g_signal_connect 
			(tree_view,
			 "button_press_event",
			 G_CALLBACK  (tree_view_button_press_callback),
			 NULL);
		g_object_set_data (G_OBJECT (tree_view), 
				   "eel-tree-view-activate", 
				   GUINT_TO_POINTER (button_press_id));
	}
}

gboolean
eel_gtk_viewport_get_visible_rect (GtkViewport  *viewport, 
				   GdkRectangle *rect)
{
	GdkRectangle viewport_rect;
	GdkRectangle child_rect;
	gboolean return_val;

	g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), FALSE);
	g_return_val_if_fail (rect != NULL, FALSE);
	
	if (GTK_WIDGET_REALIZED (viewport)) {
		viewport_rect.x = 0;
		viewport_rect.y = 0;
		gdk_drawable_get_size (viewport->view_window, 
				       &viewport_rect.width, 
				       &viewport_rect.height);
		
		gdk_window_get_position (viewport->bin_window,
					 &child_rect.x,
					 &child_rect.y);
		gdk_drawable_get_size (viewport->bin_window,
				       &child_rect.width,
				       &child_rect.height);

		return_val = gdk_rectangle_intersect (&viewport_rect, 
						      &child_rect,
						      rect);
		rect->x -= child_rect.x;
		rect->y -= child_rect.y;
		
		return return_val;
	}
	
	rect->x = rect->y = rect->width = rect->height = 0;
	return FALSE;
}

void
eel_gtk_viewport_scroll_to_rect (GtkViewport  *viewport, 
				 GdkRectangle *rect)
{
	GdkRectangle visible_rect;
	int scroll_x;
	int scroll_y;
	GtkAdjustment *adjustment;

	g_return_if_fail (GTK_IS_VIEWPORT (viewport));
	g_return_if_fail (rect != NULL);

	if (eel_gtk_viewport_get_visible_rect (viewport, &visible_rect)) {
		scroll_x = -1;
		scroll_y = -1;

		if (rect->x + rect->width > visible_rect.x + visible_rect.width) {
			scroll_x = rect->x - (visible_rect.width - rect->width);
		}
		if (rect->y + rect->height > visible_rect.y + visible_rect.height) {
			scroll_y = rect->y - (visible_rect.height - rect->height);
		}

		if (rect->x < visible_rect.x) {
			scroll_x = rect->x;
		}

		if (rect->y < visible_rect.y) {
			scroll_y = rect->y;
		}

		adjustment = gtk_viewport_get_hadjustment (viewport);
		if (adjustment && scroll_x != -1) {
			eel_gtk_adjustment_set_value (adjustment,
						      (double)scroll_x);
		}

		adjustment = gtk_viewport_get_vadjustment (viewport);
		if (adjustment && scroll_y != -1) {
			eel_gtk_adjustment_set_value (adjustment,
						      (double)scroll_y);
		}
	}
}
