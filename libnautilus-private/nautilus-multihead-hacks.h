/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-multihead-hacks.h - Remove this when we require gtk+ with multihead.

   Copyright (C) 2002 Sun Microsystems, Inc.

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

   Authors: Mark McLoughlin <mark@skynet.ie>
*/

#ifndef __MULTIHEAD_HACKS__
#define __MULTIHEAD_HACKS__

#include "config.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#ifndef HAVE_GTK_MULTIHEAD

#include <X11/Xlib.h>
#include <gdk/gdkx.h>

#define gtk_window_get_screen(a)	NULL
#define gtk_widget_get_screen(a)	NULL
#define gdk_drawable_get_screen(a)	NULL

typedef struct _GdkDisplay GdkDisplay;

#define gdk_display_get_default()	NULL
#define gdk_display_get_screen(a,b)	NULL
#define gdk_display_get_n_screens(a)	1

#define gdk_screen_get_default()	NULL
#define gdk_screen_get_width(a)		gdk_screen_width ()
#define gdk_screen_get_height(a)	gdk_screen_height ()
#define gdk_screen_get_number(a)	DefaultScreen (GDK_DISPLAY ())
#define gdk_screen_get_root_window(a)	gdk_get_default_root_window ()

#define gtk_window_get_screen(a)	NULL
#define gtk_window_set_screen(a,b)
#define gtk_menu_set_screen(a, b)

#define GDK_DISPLAY_XDISPLAY(a)		GDK_DISPLAY ()

#endif

G_END_DECLS

#endif /* __MULTIHEAD_HACKS__ */
