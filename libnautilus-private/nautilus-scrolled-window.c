/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-scrolled-window.c - Subclass of GtkScrolledWindow that
				emits a "scroll_changed" signal.

   Copyright (C) 2001 Eazel, Inc.

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
#include "nautilus-scrolled-window.h"

#include "nautilus-gtk-macros.h"
#include <gtk/gtksignal.h>

enum {
	SCROLL_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


static void       nautilus_scrolled_window_initialize_class      (NautilusScrolledWindowClass *class);
static void       nautilus_scrolled_window_initialize            (NautilusScrolledWindow      *window);
static void	  real_set_arg 					 (GtkObject        	      *object,
			     					  GtkArg           	      *arg,
			     					  guint             	       arg_id);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusScrolledWindow, nautilus_scrolled_window, GTK_TYPE_SCROLLED_WINDOW)

static void
nautilus_scrolled_window_initialize_class (NautilusScrolledWindowClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->set_arg = real_set_arg;

	signals[SCROLL_CHANGED] =
		gtk_signal_new ("scroll_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusScrolledWindowClass, scroll_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
				
}

static void
nautilus_scrolled_window_initialize (NautilusScrolledWindow *window)
{
}

static GtkAdjustment *
get_hadjustment (NautilusScrolledWindow *window)
{
	return gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (window));
}

static GtkAdjustment *
get_vadjustment (NautilusScrolledWindow *window)
{
	return gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (window));
}

static void
adjustment_changed_callback (GtkAdjustment *adjustment, gpointer callback_data)
{
	g_assert (GTK_IS_ADJUSTMENT (adjustment));
	g_assert (NAUTILUS_IS_SCROLLED_WINDOW (callback_data));

	gtk_signal_emit (GTK_OBJECT (callback_data), signals[SCROLL_CHANGED]);
}

static void
connect_adjustment (NautilusScrolledWindow *window,
		    GtkAdjustment *old_adjustment,
		    GtkAdjustment *new_adjustment)
{
      	if (new_adjustment != old_adjustment) {
      		if (old_adjustment != NULL) {
	      		gtk_signal_disconnect_by_func (GTK_OBJECT (old_adjustment),
	      					       adjustment_changed_callback,
	      					       window);
      		}
      		if (new_adjustment != NULL) {
	      		gtk_signal_connect (GTK_OBJECT (new_adjustment),
	      				    "changed",
	      				    adjustment_changed_callback,
	      				    window);
      		}
      	}
}

static void
real_set_arg (GtkObject        *object,
 	      GtkArg           *arg,
	      guint             arg_id)
{
	NautilusScrolledWindow *scrolled_window;
	GtkAdjustment *old_hadjustment, *old_vadjustment;

	scrolled_window = NAUTILUS_SCROLLED_WINDOW (object);

	old_hadjustment = get_hadjustment (scrolled_window);
	if (old_hadjustment != NULL) {
		gtk_object_ref (GTK_OBJECT (old_hadjustment));
	}
	
	old_vadjustment = get_vadjustment (scrolled_window);
	if (old_hadjustment != NULL) {
		gtk_object_ref (GTK_OBJECT (old_vadjustment));
	}

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, set_arg, (object, arg, arg_id));

      	connect_adjustment (scrolled_window, old_hadjustment, get_hadjustment (scrolled_window));
      	if (old_hadjustment != NULL) {
		gtk_object_unref (GTK_OBJECT (old_hadjustment));
      	}
	
      	connect_adjustment (scrolled_window, old_vadjustment, get_vadjustment (scrolled_window));
      	if (old_vadjustment != NULL) {
		gtk_object_unref (GTK_OBJECT (old_vadjustment));
      	}
}

void
nautilus_scrolled_window_set_vadjustment (NautilusScrolledWindow *scrolled_window,
				     	  GtkAdjustment          *vadjustment)
{
	GtkAdjustment *old_adjustment;
	
	old_adjustment = get_vadjustment (scrolled_window);
	if (old_adjustment != NULL) {
		gtk_object_ref (GTK_OBJECT (old_adjustment));
	}

	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window),
					     vadjustment);

	connect_adjustment (scrolled_window, old_adjustment, get_vadjustment (scrolled_window));
	if (old_adjustment != NULL) {
		gtk_object_unref (GTK_OBJECT (old_adjustment));
	}
}

void
nautilus_scrolled_window_set_hadjustment (NautilusScrolledWindow *scrolled_window,
				     	  GtkAdjustment          *hadjustment)
{
	GtkAdjustment *old_adjustment;
	
	old_adjustment = get_hadjustment (scrolled_window);
	if (old_adjustment != NULL) {
		gtk_object_ref (GTK_OBJECT (old_adjustment));
	}

	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (scrolled_window),
					     hadjustment);

	connect_adjustment (scrolled_window, old_adjustment, get_hadjustment (scrolled_window));
	if (old_adjustment != NULL) {
		gtk_object_unref (GTK_OBJECT (old_adjustment));
	}
}


