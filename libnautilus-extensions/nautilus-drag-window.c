/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: John Harper <jsh@eazel.com>
 *
 */

/* Registering that a window may be used as a drag 'n drop source */

#include <config.h>
#include "nautilus-drag-window.h"

#include <eel/eel-gdk-extensions.h>
#include <gtk/gtk.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>

/* State for handling focus/raise */
typedef struct NautilusDragWindowDetails NautilusDragWindowDetails;
struct NautilusDragWindowDetails {
	gboolean in_button_press;
	gboolean pending_focus;
	gboolean pending_raise;

	Time focus_timestamp;

	guint focus_timeout_tag;
};
	
/* Delay in milliseconds between receiving a TAKE_FOCUS or RAISE_WINDOW
 * client message, and assuming that there's no following button-press
 * event. This seems to be large enough to work, but small enough to be
 * unnoticeable to the user.
 */
#define WINDOW_FOCUS_TIMEOUT 50

/* Key used to store a NautilusDragWindowDetails structure in each
 * registered window's object data hash
 */
#define NAUTILUS_DRAG_WINDOW_DETAILS_KEY "nautilus-drag-window-details"

static Atom sawfish_wm_raise_window = 0;

/* Return the nearest ancestor of WIDGET that has type WIDGET_TYPE. But only
 * if there's no widget between the two with type BLOCKING_TYPE.
 */
static GtkWidget *
get_ancestor_blocked_by (GtkWidget *widget,
			 GtkType widget_type,
			 GtkType blocking_type)
{
	g_return_val_if_fail (widget != NULL, NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	while (widget != NULL)
	{
		if (gtk_type_is_a (GTK_WIDGET_TYPE (widget), widget_type))
			return widget;
		else if (gtk_type_is_a (GTK_WIDGET_TYPE (widget), blocking_type))
			return NULL;
		widget = widget->parent;
	}

	return NULL;
}

/* Returns the details structure associated with WINDOW, or a null pointer
 * if no such structure exists
 */
static NautilusDragWindowDetails *
get_details (GtkWindow *window)
{
	NautilusDragWindowDetails *details;

	details = gtk_object_get_data (GTK_OBJECT (window),
				       NAUTILUS_DRAG_WINDOW_DETAILS_KEY);
	return details;
}

/* Commit any pending focus/raise requests for WINDOW. */
static void
execute_pending_requests (GtkWindow *window,
			  NautilusDragWindowDetails *details)
{
	if (GTK_WIDGET_REALIZED (window)) {
		if (details->pending_focus) {
			eel_gdk_window_focus (GTK_WIDGET (window)->window,
						   details->focus_timestamp);
			details->pending_focus = FALSE;
		}
		if (details->pending_raise) {
			gdk_window_raise (GTK_WIDGET (window)->window);
			details->pending_raise = FALSE;
		}
	}
}

/* Called when no button-press event arrived occurred shortly after
 * receiving a TAKE_FOCUS or RAISE_WINDOW request. So just commit
 * the pending requests.
 */
static gint
focus_timeout_callback (gpointer data)
{
	GtkWindow *window;
	NautilusDragWindowDetails *details;

	window = GTK_WINDOW (data);
	details = get_details (window);

	if (details != NULL) {
		execute_pending_requests (window, details);

		details->focus_timeout_tag = 0;
	}

	/* Only ever a one-shot timeout */
	return FALSE;
}

static void
remove_focus_timeout (GtkWindow *window)
{
	NautilusDragWindowDetails *details;

	details = get_details (window);

	if (details != NULL && details->focus_timeout_tag != 0) {
		gtk_timeout_remove (details->focus_timeout_tag);
		details->focus_timeout_tag = 0;
	}
}

static void
set_focus_timeout (GtkWindow *window)
{
	NautilusDragWindowDetails *details;

	details = get_details (window);

	if (details != NULL) {
		remove_focus_timeout (window);
		details->focus_timeout_tag
		    = gtk_timeout_add (WINDOW_FOCUS_TIMEOUT,
				       focus_timeout_callback, window);
	}
}

/* Called for all button-press events; sets the `in_button_press' flag */
static gboolean
button_press_emission_callback (GtkObject *object, guint signal_id,
				guint n_params, GtkArg *params,
				gpointer data)
{
	GtkWidget *window;
	NautilusDragWindowDetails *details;

	/* This blocking is kind of a hack. But it seems necessary,
	 * otherwise we can get duped into counting unbalanced
	 * press/release events, which isn't healthy
	 */
	window = get_ancestor_blocked_by (GTK_WIDGET (object),
					  GTK_TYPE_WINDOW,
					  GTK_TYPE_MENU_SHELL);
	if (window != NULL) {
		details = get_details (GTK_WINDOW (window));
		if (details != NULL) {
			remove_focus_timeout (GTK_WINDOW (window));

			if (!details->in_button_press) {
				details->in_button_press = TRUE;
			} else {
				/* We never got the last button
				 * release. Adapt.
				 */
				execute_pending_requests (GTK_WINDOW (window),
							  details);
				details->in_button_press = FALSE;
			}
		}
	}

	return TRUE;
}

/* Called for button-release events; commits any pending focus/raise */
static gboolean
button_release_emission_callback (GtkObject *object, guint signal_id,
				  guint n_params, GtkArg *params,
				  gpointer data)
{
	GtkWidget *window;
	NautilusDragWindowDetails *details;

	window = get_ancestor_blocked_by (GTK_WIDGET (object),
					  GTK_TYPE_WINDOW,
					  GTK_TYPE_MENU_SHELL);
	if (window != NULL) {
		details = get_details (GTK_WINDOW (window));
		if (details != NULL) {
			execute_pending_requests (GTK_WINDOW (window),
						  details);
			details->in_button_press = FALSE;
		}
	}

	return TRUE;
}

/* Called when a drag is started. If a drag-window is found above the
 * widget emitting the signal, cancel any pending focus/raise requests
 */
static gboolean
drag_begin_emission_callback (GtkObject *object, guint signal_id,
			      guint n_params, GtkArg *params,
			      gpointer data)
{
	GtkWidget *window;
	NautilusDragWindowDetails *details;

	window = gtk_widget_get_ancestor (GTK_WIDGET (object),
					  GTK_TYPE_WINDOW);
	if (window != NULL) {
		details = get_details (GTK_WINDOW (window));
		if (details != NULL) {
			details->pending_focus = FALSE;
			details->pending_raise = FALSE;
		}
	}

	return TRUE;
}

/* The process-wide filter for WM_PROTOCOLS client messages */
static GdkFilterReturn
wm_protocols_filter (GdkXEvent *xev, GdkEvent *event, gpointer data)
{
	XEvent *xevent;
	GtkWindow *window;
	NautilusDragWindowDetails *details;

	xevent = (XEvent *)xev;

	gdk_window_get_user_data (event->any.window, (gpointer *) &window);
	if (window != NULL) {
		details = get_details (window);
	} else {
		details = NULL;
	}

	if ((Atom) xevent->xclient.data.l[0] == gdk_wm_delete_window) {

		/* (copied from gdkevents.c) */

		/* The delete window request specifies a window
		 *  to delete. We don't actually destroy the
		 *  window because "it is only a request". (The
		 *  window might contain vital data that the
		 *  program does not want destroyed). Instead
		 *  the event is passed along to the program,
		 *  which should then destroy the window.
		 */

		event->any.type = GDK_DELETE;
		return GDK_FILTER_TRANSLATE;

	} else if ((Atom) xevent->xclient.data.l[0] == gdk_wm_take_focus) {

		if (details != NULL) {
			details->pending_focus = TRUE;
			details->focus_timestamp = xevent->xclient.data.l[1];

			/* Wait to see if a button-press event
			 * is received in the near future.
			 */
			set_focus_timeout (window);
		}
		return GDK_FILTER_REMOVE;

	} else if ((Atom) xevent->xclient.data.l[0] == sawfish_wm_raise_window) {

		if (details != NULL) {
			details->pending_raise = TRUE;

			/* Wait to see if a button-press event
			 * is received in the near future.
			 */
			set_focus_timeout (window);
		}
		return GDK_FILTER_REMOVE;
	}
	else {
		return GDK_FILTER_CONTINUE;
	}
}

static void
nautilus_drag_window_destroy (GtkObject *object, gpointer data)
{
	NautilusDragWindowDetails *details;

	details = get_details (GTK_WINDOW (object));
	
	remove_focus_timeout (GTK_WINDOW (object));

	/* The `details' will be freed automatically */
}

static void
nautilus_drag_window_realize (GtkWidget *widget, gpointer data)
{
	GdkAtom protocols[3];

	/* Tell the window manager _not_ to focus this window by itself */
	eel_gdk_window_set_wm_hints_input (widget->window, FALSE);

	/* Set WM_PROTOCOLS to the usual two atoms plus something that tells
	 * sawfish to send messages telling us when we might want to raise
	 * the window. (This won't work with other wm's, but it won't
	 * break anything either.)
	 */
	protocols[0] = gdk_wm_delete_window;
	protocols[1] = gdk_wm_take_focus;
	protocols[2] = sawfish_wm_raise_window;
	eel_gdk_window_set_wm_protocols (widget->window, protocols, 3);
}


/* Public entry point */

/* initialize the instance's fields */
void
nautilus_drag_window_register (GtkWindow *window)
{
	static gboolean initialized = FALSE;

	NautilusDragWindowDetails *details;
	guint signal_id;

	if (!initialized) {
		/* Add emission hooks for the signals we need to monitor
		 */
		signal_id = gtk_signal_lookup ("button_press_event",
					       GTK_TYPE_WIDGET);
		gtk_signal_add_emission_hook (signal_id,
					      button_press_emission_callback,
					      NULL);
		signal_id = gtk_signal_lookup ("button_release_event",
					       GTK_TYPE_WIDGET);
		gtk_signal_add_emission_hook (signal_id,
					      button_release_emission_callback,
					      NULL);
		signal_id = gtk_signal_lookup ("drag_begin",
					       GTK_TYPE_WIDGET);
		gtk_signal_add_emission_hook (signal_id,
					      drag_begin_emission_callback,
					      NULL);

		/* Intern the necessary X atoms
		 */
		sawfish_wm_raise_window = XInternAtom (GDK_DISPLAY (),
						       "_SAWFISH_WM_RAISE_WINDOW",
						       False);

		/* Override the standard GTK filter for handling WM_PROTOCOLS
		 * client messages
		 */
		gdk_add_client_message_filter (gdk_wm_protocols,
					       wm_protocols_filter, NULL);

		initialized = TRUE;
	}

	details = g_new0 (NautilusDragWindowDetails, 1);

	gtk_object_set_data_full (GTK_OBJECT (window),
				  NAUTILUS_DRAG_WINDOW_DETAILS_KEY,
				  details, g_free);

	gtk_signal_connect (GTK_OBJECT (window), "realize",
			    nautilus_drag_window_realize, NULL);
	gtk_signal_connect (GTK_OBJECT (window), "destroy",
			    nautilus_drag_window_destroy, NULL);
}
