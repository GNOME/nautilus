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

#include <config.h>
#include "desktopwindow.h"
#include <libgnomeui/gnome-winhints.h>

static void desktop_window_class_init (DesktopWindowClass *class);
static void desktop_window_init       (DesktopWindow      *dwindow);
static void desktop_window_realize    (GtkWidget        *widget);


static GtkWindowClass *parent_class;


/**
 * desktop_window_get_type
 *
 * Returns the Gtk type assigned to the DesktopWindow class.
 */
GtkType
desktop_window_get_type (void)
{
	static GtkType desktop_window_type = 0;

	if (!desktop_window_type) {
		GtkTypeInfo desktop_window_info = {
			"DesktopWindow",
			sizeof (DesktopWindow),
			sizeof (DesktopWindowClass),
			(GtkClassInitFunc) desktop_window_class_init,
			(GtkObjectInitFunc) desktop_window_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		desktop_window_type = gtk_type_unique (gtk_window_get_type (), &desktop_window_info);
	}

	return desktop_window_type;
}

static void
desktop_window_class_init (DesktopWindowClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (gtk_window_get_type ());

	widget_class->realize = desktop_window_realize;
}

static void
desktop_window_init (DesktopWindow *dwindow)
{
        /* Should never resize this thing */
        gtk_window_set_policy(GTK_WINDOW(dwindow),
                              FALSE, FALSE, FALSE);

        /* Match the screen size */
        gtk_widget_set_usize(GTK_WIDGET(dwindow),
                             gdk_screen_width(),
                             gdk_screen_height());

}

static void
desktop_window_realize (GtkWidget *widget)
{
	DesktopWindow *dwindow;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_DESKTOP_WINDOW (widget));

	dwindow = DESKTOP_WINDOW (widget);

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

        /* Turn off all decorations and window manipulation functions */
        gdk_window_set_decorations(widget->window, 0);
        gdk_window_set_functions(widget->window, 0);

        /* Set the proper GNOME hints */
	
	gnome_win_hints_init ();
        
	if (gnome_win_hints_wm_exists ()) {
		gnome_win_hints_set_layer (widget, WIN_LAYER_DESKTOP);
		gnome_win_hints_set_state (widget,
					   WIN_STATE_FIXED_POSITION
					   | WIN_STATE_ARRANGE_IGNORE
					   | WIN_STATE_STICKY);
		gnome_win_hints_set_hints (widget,
					   WIN_HINTS_SKIP_FOCUS
					   | WIN_HINTS_SKIP_WINLIST
					   | WIN_HINTS_SKIP_TASKBAR);
	} else {
                g_warning("window manager doesn't like us");
        }
}

GtkWidget*
desktop_window_new (void)
{
        DesktopWindow *dwindow;

        dwindow = gtk_type_new(desktop_window_get_type());
        
        return GTK_WIDGET(dwindow);
}




