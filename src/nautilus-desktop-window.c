/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nautilus-desktop-window.h"
#include "nautilus-window-private.h"

#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gtk/gtklayout.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-vfs-extensions.h>
#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-private/nautilus-file-utilities.h>

struct NautilusDesktopWindowDetails {
	int dummy;
};

static void set_wmspec_desktop_hint (GdkWindow *window);

GNOME_CLASS_BOILERPLATE (NautilusDesktopWindow, nautilus_desktop_window,
			 NautilusSpatialWindow, NAUTILUS_TYPE_SPATIAL_WINDOW)

static void
nautilus_desktop_window_instance_init (NautilusDesktopWindow *window)
{
	window->details = g_new0 (NautilusDesktopWindowDetails, 1);

	gtk_window_move (GTK_WINDOW (window), 0, 0);

	/* shouldn't really be needed given our semantic type
	 * of _NET_WM_TYPE_DESKTOP, but why not
	 */
	gtk_window_set_resizable (GTK_WINDOW (window),
				  FALSE);

	g_object_set_data (G_OBJECT (window), "is_desktop_window", 
			   GINT_TO_POINTER (1));

	gtk_widget_hide (NAUTILUS_WINDOW (window)->details->statusbar);
	gtk_widget_hide (NAUTILUS_WINDOW (window)->details->menubar);
}

static gint
nautilus_desktop_window_delete_event (NautilusDesktopWindow *window)
{
	/* Returning true tells GTK+ not to delete the window. */
	return TRUE;
}

void
nautilus_desktop_window_update_directory (NautilusDesktopWindow *window)
{
	g_assert (NAUTILUS_IS_DESKTOP_WINDOW (window));
	
	NAUTILUS_SPATIAL_WINDOW (window)->affect_spatial_window_on_next_location_change = TRUE;
	nautilus_window_go_to (NAUTILUS_WINDOW (window), EEL_DESKTOP_URI);
}

static void
nautilus_desktop_window_screen_size_changed (GdkScreen             *screen,
					     NautilusDesktopWindow *window)
{
	int width_request, height_request;

	width_request = gdk_screen_get_width (screen);
	height_request = gdk_screen_get_height (screen);
	
	g_object_set (window,
		      "width_request", width_request,
		      "height_request", height_request,
		      NULL);
}

NautilusDesktopWindow *
nautilus_desktop_window_new (NautilusApplication *application,
			     GdkScreen           *screen)
{
	NautilusDesktopWindow *window;
	int width_request, height_request;

	width_request = gdk_screen_get_width (screen);
	height_request = gdk_screen_get_height (screen);

	window = NAUTILUS_DESKTOP_WINDOW
		(gtk_widget_new (nautilus_desktop_window_get_type(),
				 "app", application,
				 "width_request", width_request,
				 "height_request", height_request,
				 "screen", screen,
				 NULL));

	/* Special sawmill setting*/
	gtk_window_set_wmclass (GTK_WINDOW (window), "desktop_window", "Nautilus");

	g_signal_connect (window, "delete_event", G_CALLBACK (nautilus_desktop_window_delete_event), NULL);

	/* Point window at the desktop folder.
	 * Note that nautilus_desktop_window_init is too early to do this.
	 */
	nautilus_desktop_window_update_directory (window);

	return window;
}

static void
finalize (GObject *object)
{
	NautilusDesktopWindow *window;

	window = NAUTILUS_DESKTOP_WINDOW (object);

	g_free (window->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Disable this code for the time being, since its:
 * a) not working
 * b) crashes nautilus if the root window has a different
 *    depth than the nautilus window
 *
 * The idea behind the code is to grab the old background pixmap
 * and temporarily set it as the background for the nautilus window
 * to avoid flashing before the correct background has been set.
 */
#if 0
static void
set_gdk_window_background (GdkWindow *window,
			   gboolean   have_pixel,
			   Pixmap     pixmap,
			   gulong     pixel)
{
	Window w;

	w = GDK_WINDOW_XWINDOW (window);

	if (pixmap != None) {
		XSetWindowBackgroundPixmap (GDK_DISPLAY (), w,
					    pixmap);
	} else if (have_pixel) {
		XSetWindowBackground (GDK_DISPLAY (), w,
				      pixel);
	} else {
		XSetWindowBackgroundPixmap (GDK_DISPLAY (), w,
					    None);
	}
}

static void
set_window_background (GtkWidget *widget,
		       gboolean   already_have_root_bg,
		       gboolean   have_pixel,
		       Pixmap     pixmap,
		       gulong     pixel)
{
	Atom type;
	gulong nitems, bytes_after;
	gint format;
	guchar *data;
	
	/* Set the background to show the root window to avoid a flash that
	 * would otherwise occur.
	 */

	if (GTK_IS_WINDOW (widget)) {
		gtk_widget_set_app_paintable (widget, TRUE);
	}
	
	if (!already_have_root_bg) {
		have_pixel = FALSE;
		already_have_root_bg = TRUE;
		
		/* We want to do this round-trip-to-server work only
		 * for the first invocation, not on recursions
		 */
		
		XGetWindowProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
				    gdk_x11_get_xatom_by_name ("_XROOTPMAP_ID"),
				    0L, 1L, False, XA_PIXMAP,
				    &type, &format, &nitems, &bytes_after,
				    &data);
  
		if (type == XA_PIXMAP) {
			if (format == 32 && nitems == 1 && bytes_after == 0) {
				pixmap = *(Pixmap *) data;
			}
  
			XFree (data);
		}

		if (pixmap == None) {
			XGetWindowProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
					    gdk_x11_get_xatom_by_name ("_XROOTCOLOR_PIXEL"),
					    0L, 1L, False, AnyPropertyType,
					    &type, &format, &nitems, &bytes_after,
					    &data);
			
			if (type != None) {
				if (format == 32 && nitems == 1 && bytes_after == 0) {
					pixel = *(gulong *) data;
					have_pixel = TRUE;
				}
				
				XFree (data);
			}
		}
	}
	
	set_gdk_window_background (widget->window,
				   have_pixel, pixmap, pixel);
	
	if (GTK_IS_BIN (widget) &&
	    GTK_BIN (widget)->child) {
		/* Ensure we're realized */
		gtk_widget_realize (GTK_BIN (widget)->child);
		
		set_window_background (GTK_BIN (widget)->child,
				       already_have_root_bg, have_pixel,
				       pixmap, pixel);
	}

	/* For both parent and child, if it's a layout then set on the
	 * bin window as well.
	 */
	if (GTK_IS_LAYOUT (widget)) {
		set_gdk_window_background (GTK_LAYOUT (widget)->bin_window,
					   have_pixel,
					   pixmap, pixel);
	}
}
#endif
static void
map (GtkWidget *widget)
{
	/* Disable for now, see above for comments */
#if 0
	set_window_background (widget, FALSE, FALSE, None, 0);
#endif	
	/* Chain up to realize our children */
	GTK_WIDGET_CLASS (parent_class)->map (widget);
	gdk_window_lower (widget->window);
}


static void
unrealize (GtkWidget *widget)
{
	NautilusDesktopWindow *window;
	GdkWindow *root_window;

	window = NAUTILUS_DESKTOP_WINDOW (widget);

	root_window = gdk_screen_get_root_window (
				gtk_window_get_screen (GTK_WINDOW (window)));

	gdk_property_delete (root_window,
			     gdk_atom_intern ("NAUTILUS_DESKTOP_WINDOW_ID", TRUE));

	g_signal_handlers_disconnect_by_func (gtk_window_get_screen (GTK_WINDOW (window)),
					      G_CALLBACK (nautilus_desktop_window_screen_size_changed),
					      window);
		
	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
set_wmspec_desktop_hint (GdkWindow *window)
{
	GdkAtom atom;

	atom = gdk_atom_intern ("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
        
	gdk_property_change (window,
			     gdk_atom_intern ("_NET_WM_WINDOW_TYPE", FALSE),
			     gdk_x11_xatom_to_atom (XA_ATOM), 32,
			     GDK_PROP_MODE_REPLACE, (guchar *) &atom, 1);
}

static void
set_desktop_window_id (NautilusDesktopWindow *window,
		       GdkWindow             *gdkwindow)
{
	/* Tuck the desktop windows xid in the root to indicate we own the desktop.
	 */
	Window window_xid;
	GdkWindow *root_window;

	root_window = gdk_screen_get_root_window (
				gtk_window_get_screen (GTK_WINDOW (window)));

	window_xid = GDK_WINDOW_XWINDOW (gdkwindow);

	gdk_property_change (root_window,
			     gdk_atom_intern ("NAUTILUS_DESKTOP_WINDOW_ID", FALSE),
			     gdk_x11_xatom_to_atom (XA_WINDOW), 32,
			     GDK_PROP_MODE_REPLACE, (guchar *) &window_xid, 1);
}

static void
realize (GtkWidget *widget)
{
	NautilusDesktopWindow *window;

	window = NAUTILUS_DESKTOP_WINDOW (widget);

	/* Make sure we get keyboard events */
	gtk_widget_set_events (widget, gtk_widget_get_events (widget) 
			      | GDK_KEY_PRESS_MASK | GDK_KEY_PRESS_MASK);
			      
	/* Do the work of realizing. */
	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	/* This is the new way to set up the desktop window */
	set_wmspec_desktop_hint (widget->window);

	set_desktop_window_id (window, widget->window);

	g_signal_connect (gtk_window_get_screen (GTK_WINDOW (window)), "size_changed",
			  G_CALLBACK (nautilus_desktop_window_screen_size_changed), window);
}

static void
real_add_current_location_to_history_list (NautilusWindow *window)
{
	/* Do nothing. The desktop window's location should not
	 * show up in the history list.
	 */
}

static char *
real_get_title (NautilusWindow *window)
{
	return g_strdup (_("Desktop"));
}

static void
nautilus_desktop_window_class_init (NautilusDesktopWindowClass *class)
{
	G_OBJECT_CLASS (class)->finalize = finalize;
	GTK_WIDGET_CLASS (class)->realize = realize;
	GTK_WIDGET_CLASS (class)->unrealize = unrealize;


	GTK_WIDGET_CLASS (class)->map = map;

	NAUTILUS_WINDOW_CLASS (class)->window_type = NAUTILUS_WINDOW_DESKTOP;

	NAUTILUS_WINDOW_CLASS (class)->add_current_location_to_history_list 
		= real_add_current_location_to_history_list;
	NAUTILUS_WINDOW_CLASS (class)->get_title 
		= real_get_title;
}
