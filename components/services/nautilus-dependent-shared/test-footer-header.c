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
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>

/* #include <libnautilus-extensions/nautilus-gdk-extensions.h> */
/* #include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h> */
/* #include <libnautilus-extensions/nautilus-image.h> */
/* #include <libnautilus-extensions/nautilus-label.h> */
/* #include <libnautilus-extensions/nautilus-theme.h> */

#include "eazel-services-footer.h"
#include "eazel-services-header.h"
#include "eazel-services-constants.h"

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

static const char *footer_uris[] = 
{
	"eazel:register",
	"eazel:login",
	"eazel:terms",
	"eazel:privacy"
};

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *header;
	GtkWidget *footer;
	GtkWidget *content;
	
	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	nautilus_gtk_widget_set_background_color (window, EAZEL_SERVICES_BACKGROUND_COLOR_STRING);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_window_set_title (GTK_WINDOW (window), "Nautilus Wrapped Label Test");
	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);
	gtk_widget_set_usize (window, 640, 480);
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	header = eazel_services_header_new ("Welcome back, Arlo!");
	content = gtk_vbox_new (FALSE, 0);

	footer = eazel_services_footer_new ();
	eazel_services_footer_update (EAZEL_SERVICES_FOOTER (footer),
				      footer_items,
				      footer_uris,
				      NAUTILUS_N_ELEMENTS (footer_items));
	
	gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), content, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), footer, FALSE, FALSE, 0);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
