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
	gint left, top, width, height;

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