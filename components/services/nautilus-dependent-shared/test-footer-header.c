/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* test-footer-header.c - Test for footer/header widgetry.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include <gtk/gtk.h>
#include "eazel-services-extensions.h"
#include "eazel-services-footer.h"
#include "eazel-services-header.h"

static void
delete_event (GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	gtk_main_quit ();
}

static const char *footer_items[] = 
{
	"Register",
	"Login",
	"Terms of Use",
	"Privacy Statement"
};

static void
footer_item_clicked_callback (GtkWidget *widget, int index, gpointer callback_data)
{
	g_print ("footer_item_clicked_callback(%d)\n", index);
}

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *header;
	GtkWidget *footer;
	GtkWidget *content;
	GtkWidget *middle;
	
	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	eel_gtk_widget_set_background_color (window, EAZEL_SERVICES_BACKGROUND_COLOR_SPEC);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_window_set_title (GTK_WINDOW (window), "Nautilus Wrapped Label Test");
	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);
	gtk_widget_set_usize (window, 640, 480);
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	
	header = eazel_services_header_title_new ("Welcome back, Arlo!");
	content = gtk_vbox_new (FALSE, 0);

	middle = eazel_services_header_middle_new ("Left", "Right");
	gtk_box_pack_start (GTK_BOX (content), middle, FALSE, FALSE, 0);

	footer = eazel_services_footer_new ();
	gtk_signal_connect (GTK_OBJECT (footer), "item_clicked", GTK_SIGNAL_FUNC (footer_item_clicked_callback), NULL);

	eazel_services_footer_update (EAZEL_SERVICES_FOOTER (footer),
				      footer_items,
				      EEL_N_ELEMENTS (footer_items));
	
	gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), content, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), footer, FALSE, FALSE, 0);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
