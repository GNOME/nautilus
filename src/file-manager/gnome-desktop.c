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

/* NOTE this is just a test program, the desktop will probably be in
   the file manager process, to share the icon cache and such */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "desktop-window.h"
#include "desktop-canvas.h"

static gint
delete_event_cb(GtkWidget* window, GdkEventAny* event, gpointer data)
{
  gtk_main_quit();
}

int
main (int argc, char *argv[])
{
        GtkWidget *window;
        GtkWidget *canvas;

        (void) bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
        (void) textdomain (PACKAGE);
        
        gnome_init ("gnome-desktop", VERSION, argc, argv);
        
        window = desktop_window_new();
        canvas = desktop_canvas_new();

        gtk_container_add(GTK_CONTAINER(window), canvas);
        
        gtk_signal_connect(GTK_OBJECT(window), "delete_event",
                           GTK_SIGNAL_FUNC(delete_event_cb), NULL);
        
        gtk_widget_show_all(window);
        
        gtk_main ();
        
        return 0;
}


