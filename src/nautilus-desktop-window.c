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

static void nautilus_desktop_window_initialize_class (NautilusDesktopWindowClass *klass);
static void nautilus_desktop_window_initialize       (NautilusDesktopWindow      *window);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusDesktopWindow, nautilus_desktop_window, NAUTILUS_TYPE_WINDOW)

static void
nautilus_desktop_window_initialize_class (NautilusDesktopWindowClass *klass)
{
}

static void
nautilus_desktop_window_initialize (NautilusDesktopWindow *window)
{
}

NautilusDesktopWindow *
nautilus_desktop_window_new (NautilusApp *application)
{
	NautilusDesktopWindow *window;

	window = NAUTILUS_DESKTOP_WINDOW
		(gtk_object_new (nautilus_desktop_window_get_type(),
				 "app", application,
				 "app_id", "nautilus",
				 NULL));

	gtk_widget_show (GTK_WIDGET (window));

	/* behind all otehr windows */
	/* no decorations? */
	/* other stuff cribbed from Havoc's code */
	/* always use icon view */
	/* no/hidden status bar */
	/* no/hidden location bar */
	/* no/hidden toolbars */
	/* no/hidden menus */
	/* no/hidden sidebar */

	return window;
}
