/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
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
 */

#include <config.h>
#include <applet-widget.h>

#include <gtk/gtkobject.h>
#include <gtk/gtkbutton.h>





static void
applet_change_pixel_size(GtkWidget *widget, int size, gpointer data)
{
	/* need to change the size of the button here */
}


int
main (int argc, char **argv)
{
	GtkWidget *applet;
	GtkWidget *icon_applet;
	int size;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	
	applet_widget_init ("nautilus-icon-applet", VERSION, argc,
			    argv, NULL, 0, NULL);

	applet = applet_widget_new ("nauilus-icon-applet");
	if (applet == NULL)
		g_error (_("Can't create nautilus-icon-applet!"));

	icon_applet = gtk_button_new_with_label ("nautilus");
	gtk_object_set_data (GTK_OBJECT (applet), "widget", icon_applet);
	applet_widget_add (APPLET_WIDGET (applet), icon_applet);

	size = applet_widget_get_panel_pixel_size(APPLET_WIDGET(applet)) - 2;
	applet_change_pixel_size (GTK_WIDGET (applet), size, NULL);

	gtk_widget_show (icon_applet);
	gtk_widget_show (applet);

	gtk_signal_connect(GTK_OBJECT(applet),"change_pixel_size",
			   GTK_SIGNAL_FUNC(applet_change_pixel_size),
			   NULL);

	applet_widget_gtk_main ();

	return 0;
}
