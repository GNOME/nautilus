/*  -*- Mode: C; c-set-style: linux; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * Desktop component of GNOME file manager
 * 
 * Copyright (C) 1999 Red Hat Inc., Free Software Foundation
 * (based on Midnight Commander code by Federico Mena Quintero and Miguel de Icaza)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef GD_DESKTOP_WINDOW_H
#define GD_DESKTOP_WINDOW_H

#include <libgnome/gnome-defs.h>
#include <gtk/gtkwindow.h>

BEGIN_GNOME_DECLS

#define TYPE_DESKTOP_WINDOW            (desktop_window_get_type ())
#define DESKTOP_WINDOW(obj)            (GTK_CHECK_CAST ((obj), TYPE_DESKTOP_WINDOW, DesktopWindow))
#define DESKTOP_WINDOW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_DESKTOP_WINDOW, DesktopWindowClass))
#define IS_DESKTOP_WINDOW(obj)         (GTK_CHECK_TYPE ((obj), TYPE_DESKTOP_WINDOW))
#define IS_DESKTOP_WINDOW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_DESKTOP_WINDOW))


typedef struct _DesktopWindow DesktopWindow;
typedef struct _DesktopWindowClass DesktopWindowClass;

struct _DesktopWindow {
        GtkWindow window;

};

struct _DesktopWindowClass {
	GtkWindowClass parent_class;
};


/* Standard Gtk function */
GtkType desktop_window_get_type (void);

GtkWidget *desktop_window_new (void);

END_GNOME_DECLS

#endif
