/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-desktop-window.c

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-desktop-window.h"

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libgnomeui/gnome-winhints.h>

static void nautilus_desktop_window_initialize_class (NautilusDesktopWindowClass *klass);
static void nautilus_desktop_window_initialize       (NautilusDesktopWindow      *window);
static void realize                                  (GtkWidget                  *widget);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusDesktopWindow, nautilus_desktop_window, NAUTILUS_TYPE_WINDOW)

static void
nautilus_desktop_window_initialize_class (NautilusDesktopWindowClass *klass)
{
	GTK_WIDGET_CLASS (klass)->realize = realize;
}

static void
nautilus_desktop_window_initialize (NautilusDesktopWindow *window)
{
	/* FIXME: Although Havoc had this in his code, it seems to
	 * have no effect for me. But the gdk_window_move_resize
	 * below does seem to work. Not sure if this should stay
	 * here or not.
	 */
	gtk_window_set_default_size (GTK_WINDOW (window),
				     gdk_screen_width (),
				     gdk_screen_height ());
	gtk_widget_set_uposition (GTK_WIDGET (window), 0, 0);
	gtk_widget_set_usize (GTK_WIDGET (window),
			      gdk_screen_width (),
			      gdk_screen_height ());

	/* Tell the window manager to never resize this. */
#if 0
	gtk_window_set_policy (GTK_WINDOW (window),
			       FALSE, FALSE, FALSE);
#endif
}

NautilusDesktopWindow *
nautilus_desktop_window_new (NautilusApp *application)
{
	NautilusDesktopWindow *window;
	char *desktop_directory;

	window = NAUTILUS_DESKTOP_WINDOW
		(gtk_object_new (nautilus_desktop_window_get_type(),
				 "app", application,
				 "app_id", "nautilus",
				 NULL));

	/* Point window at the desktop folder.
	 * Note that nautilus_desktop_window_initialize is too early to do this.
	 */
	desktop_directory = g_strconcat ("file://", nautilus_get_desktop_directory (), NULL);
	nautilus_window_goto_uri (NAUTILUS_WINDOW (window), desktop_directory);
	g_free (desktop_directory);

	gtk_widget_show (GTK_WIDGET (window));

	return window;
}

static void
realize (GtkWidget *widget)
{
	/* Hide unused pieces of the GnomeApp. */
	gtk_widget_hide (GNOME_APP (widget)->menubar);
	gtk_widget_hide (GNOME_APP (widget)->dock);

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));

        /* Set some hints for the window manager. */
	/* FIXME: Looking at the gnome_win_hints implementation,
	 * it looks like you can call these with an unmapped window,
	 * but when I tried doing it in initialize it didn't work.
	 */
	gnome_win_hints_set_layer (widget, WIN_LAYER_DESKTOP);
	gnome_win_hints_set_state (widget,
				   WIN_STATE_STICKY
				   | WIN_STATE_MAXIMIZED_VERT
				   | WIN_STATE_MAXIMIZED_HORIZ
				   | WIN_STATE_FIXED_POSITION
				   | WIN_STATE_HIDDEN
				   | WIN_STATE_ARRANGE_IGNORE);
	gnome_win_hints_set_hints (widget,
				   WIN_HINTS_SKIP_FOCUS
				   | WIN_HINTS_SKIP_WINLIST
				   | WIN_HINTS_SKIP_TASKBAR);

	/* More ways to tell the window manager to treat this like a desktop. */
	/* FIXME: Should we do a gdk_window_set_hints here? */
	gdk_window_move_resize (widget->window,
				0, 0,
				gdk_screen_width (),
				gdk_screen_height ());
        gdk_window_set_decorations (widget->window, 0);
        gdk_window_set_functions (widget->window, 0);
}
