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
 * Authors: Darin Adler <darin@eazel.com>
 */

/* nautilus-desktop-window.c
 */

#include <config.h>
#include "nautilus-desktop-window.h"

#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-find-directory.h>
#include <libgnomeui/gnome-winhints.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>

struct NautilusDesktopWindowDetails {
	GList *unref_list;
};

static void nautilus_desktop_window_initialize_class (NautilusDesktopWindowClass *klass);
static void nautilus_desktop_window_initialize       (NautilusDesktopWindow      *window);
static void destroy                                  (GtkObject                  *object);
static void realize                                  (GtkWidget                  *widget);
static void real_add_current_location_to_history_list (NautilusWindow		 *window);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusDesktopWindow, nautilus_desktop_window, NAUTILUS_TYPE_WINDOW)

static void
nautilus_desktop_window_initialize_class (NautilusDesktopWindowClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = destroy;
	GTK_WIDGET_CLASS (klass)->realize = realize;
	NAUTILUS_WINDOW_CLASS (klass)->add_current_location_to_history_list 
		= real_add_current_location_to_history_list;
}

static void
nautilus_desktop_window_initialize (NautilusDesktopWindow *window)
{
	window->details = g_new0 (NautilusDesktopWindowDetails, 1);

	/* FIXME bugzilla.eazel.com 1251: 
	 * Although Havoc had this call to set_default_size in
	 * his code, it seems to have no effect for me. But the
	 * set_usize below does seem to work.
	 */
	gtk_window_set_default_size (GTK_WINDOW (window),
				     gdk_screen_width (),
				     gdk_screen_height ());

	/* These calls seem to have some effect, but it's not clear if
	 * they are the right thing to do.
	 */
	gtk_widget_set_uposition (GTK_WIDGET (window), 0, 0);
	gtk_widget_set_usize (GTK_WIDGET (window),
			      gdk_screen_width (),
			      gdk_screen_height ());

	/* Tell the window manager to never resize this. This is not
	 * known to have any specific beneficial effect with any
	 * particular window manager for the case of the desktop
	 * window, but it doesn't seem to do any harm.
	 */
	gtk_window_set_policy (GTK_WINDOW (window),
			       FALSE, FALSE, FALSE);
}

static void
nautilus_desktop_window_realized (NautilusDesktopWindow *window)
{
	/* Tuck the desktop windows xid in the root to indicate we own the desktop.
	 */
	Window window_xid;
	window_xid = GDK_WINDOW_XWINDOW (GTK_WIDGET (window)->window);
	gdk_property_change (NULL, gdk_atom_intern ("NAUTILUS_DESKTOP_WINDOW_ID", FALSE),
			     XA_WINDOW, 32, PropModeReplace, (guchar *) &window_xid, 1);
}

static gint
nautilus_desktop_window_delete_event (NautilusDesktopWindow *window)
{
	/* Returning true tells GTK+ not to delete the window. */
	return TRUE;
}

NautilusDesktopWindow *
nautilus_desktop_window_new (NautilusApplication *application)
{
	NautilusDesktopWindow *window;
	char *desktop_directory_path;
	char *desktop_directory_uri;

	window = NAUTILUS_DESKTOP_WINDOW
		(gtk_widget_new (nautilus_desktop_window_get_type(),
				 "app", application,
				 "app_id", "nautilus",
				 NULL));

	/* Special sawmill setting*/
	gtk_window_set_wmclass (GTK_WINDOW (window), "desktop_window", "Nautilus");

	gtk_signal_connect (GTK_OBJECT (window), "realize", GTK_SIGNAL_FUNC (nautilus_desktop_window_realized), NULL);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event", GTK_SIGNAL_FUNC (nautilus_desktop_window_delete_event), NULL);

	desktop_directory_path = nautilus_get_desktop_directory ();
	
	/* Point window at the desktop folder.
	 * Note that nautilus_desktop_window_initialize is too early to do this.
	 */
	desktop_directory_uri = gnome_vfs_get_uri_from_local_path (desktop_directory_path);
	g_free (desktop_directory_path);
	nautilus_window_go_to (NAUTILUS_WINDOW (window), desktop_directory_uri);
	g_free (desktop_directory_uri);
	
	gtk_widget_show (GTK_WIDGET (window));

	return window;
}

static void
destroy (GtkObject *object)
{
	NautilusDesktopWindow *window;

	window = NAUTILUS_DESKTOP_WINDOW (object);

	gdk_property_delete (NULL, gdk_atom_intern ("NAUTILUS_DESKTOP_WINDOW_ID", TRUE));

	eel_gtk_object_list_free (window->details->unref_list);
	g_free (window->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
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
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, realize, (widget));

	/* FIXME bugzilla.eazel.com 1253: 
	 * Looking at the gnome_win_hints implementation,
	 * it looks like you can call these with an unmapped window,
	 * but when I tried doing it in initialize it didn't work.
	 * We'd like to set these earlier so the window doesn't show
	 * up in front of everything before going to the back.
	 */

	/* Put this window behind all the others. */
	gnome_win_hints_set_layer (widget, WIN_LAYER_DESKTOP);

	/* Make things like the task list ignore this window and make
	 * it clear that it it's at its full size.
	 *
	 * We originally included WIN_STATE_HIDDEN here, but IceWM
	 * interprets that (wrongly, imho) as meaning `don't display
	 * this window'. Not including this bit seems to make
	 * no difference though, so..
	 */
	gnome_win_hints_set_state (widget,
				   WIN_STATE_STICKY
				   | WIN_STATE_MAXIMIZED_VERT
				   | WIN_STATE_MAXIMIZED_HORIZ
				   | WIN_STATE_FIXED_POSITION
				   | WIN_STATE_ARRANGE_IGNORE);

	/* Make sure that focus, and any window lists or task bars also
	 * skip the window.
	 */
	gnome_win_hints_set_hints (widget,
				   WIN_HINTS_SKIP_WINLIST
				   | WIN_HINTS_SKIP_TASKBAR
				   | WIN_HINTS_SKIP_FOCUS);

	/* FIXME bugzilla.eazel.com 1255: 
	 * Should we do a gdk_window_move_resize here, in addition to
	 * the calls in initialize above that set the size?
	 */
	gdk_window_move_resize (widget->window,
				0, 0,
				gdk_screen_width (),
				gdk_screen_height ());

	/* Get rid of the things that window managers add to resize
	 * and otherwise manipulate the window.
	 */
        gdk_window_set_decorations (widget->window, 0);
        gdk_window_set_functions (widget->window, 0);
}

static void
real_add_current_location_to_history_list (NautilusWindow *window)
{
	/* Do nothing. The desktop window's location should not
	 * show up in the history list.
	 */
}
