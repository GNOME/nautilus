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
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-buffered-widget.h>
#include <libnautilus-extensions/nautilus-graphic-effects.h>
#include <libgnome/gnome-exec.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkeventbox.h>
#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>

#define ICON_NAME "nautilus-launch-icon.png"
#define VERTICAL_OFFSET 2
#define HORIZONTAL_OFFSET 2

static GdkPixbuf *icon_pixbuf = NULL;
static GdkPixbuf *icon_prelight_pixbuf = NULL;

static void
create_pixbufs ()
{
	if (icon_pixbuf == NULL) {
		char *path;
		
		path = g_strdup_printf ("%s/pixmaps/%s", DATADIR, ICON_NAME);
		
		icon_pixbuf = gdk_pixbuf_new_from_file (path);
		g_free (path);
		
		g_assert (icon_pixbuf != NULL);

		icon_prelight_pixbuf = nautilus_create_spotlight_pixbuf (icon_pixbuf);
	}
}

static void
applet_change_pixel_size(GtkWidget *widget, int size, gpointer data)
{
	/* need to change the size of the button here */
}

static gint
image_enter_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (client_data), TRUE);
	
	nautilus_image_set_pixbuf (NAUTILUS_IMAGE (client_data), icon_prelight_pixbuf);

	return TRUE;
}

static gint
image_leave_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (client_data), TRUE);
	
	nautilus_image_set_pixbuf (NAUTILUS_IMAGE (client_data), icon_pixbuf);
	gtk_object_set_data (GTK_OBJECT (event_box), "was-pressed", FALSE);
	nautilus_buffered_widget_set_vertical_offset (NAUTILUS_BUFFERED_WIDGET (client_data), 0);
	nautilus_buffered_widget_set_horizontal_offset (NAUTILUS_BUFFERED_WIDGET (client_data), 0);

	return TRUE;
}

#if 0
static GdkWindow *
get_root_window (void)
{
	return GDK_ROOT_PARENT ();
}

static void
root_window_set_busy (void)
{
	GdkWindow *root;
	GdkCursor *cursor;

	root = get_root_window ();

	cursor = gdk_cursor_new (GDK_WATCH);

 	gdk_window_set_cursor (root, cursor);

	gdk_cursor_destroy (cursor);
}

static void
root_window_set_not_busy (void)
{
	GdkWindow *root;

	root = get_root_window ();

	gdk_window_set_cursor (root, NULL);
}
#endif

static gint
image_button_press_event (GtkWidget *event_box,
			  GdkEventButton *event,
			  gpointer client_data)
{
	GtkWidget *icon = GTK_WIDGET (client_data);

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (icon), TRUE);

	gtk_object_set_data (GTK_OBJECT (event_box), "was-pressed", GINT_TO_POINTER (TRUE));
	nautilus_buffered_widget_set_vertical_offset (NAUTILUS_BUFFERED_WIDGET (icon), VERTICAL_OFFSET);
	nautilus_buffered_widget_set_horizontal_offset (NAUTILUS_BUFFERED_WIDGET (icon), HORIZONTAL_OFFSET);
		
	return TRUE;
}

static gint
image_button_release_event (GtkWidget *event_box,
			    GdkEventButton *event,
			    gpointer client_data)
{
	char *path;
	gint pid;
	GtkWidget *icon = GTK_WIDGET (client_data);
	gboolean was_pressed;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (icon), TRUE);

	was_pressed = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (event_box), "was-pressed"));
	if (was_pressed) {
		gtk_object_set_data (GTK_OBJECT (event_box), "was-pressed", FALSE);
		nautilus_buffered_widget_set_vertical_offset (NAUTILUS_BUFFERED_WIDGET (icon), 0);
		nautilus_buffered_widget_set_horizontal_offset (NAUTILUS_BUFFERED_WIDGET (icon), 0);

		path = g_strdup_printf ("%s/%s", BINDIR, "run-nautilus");
		
		pid = gnome_execute_async (NULL, 1, &path);
	
		if (pid != 0) {
			//root_window_set_busy ();
		}

		g_free (path);
	}

	return TRUE;
}


int
main (int argc, char **argv)
{
	GtkWidget *applet;
	GtkWidget *icon;
	GtkWidget *event_box;
	int size;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	
	applet_widget_init ("nautilus_launcher_applet", VERSION, argc,
			    argv, NULL, 0, NULL);

	applet = applet_widget_new ("nautilus_launcher_applet");
	if (applet == NULL)
		g_error (_("Can't create nautilus-launcher-applet!"));

	create_pixbufs ();

	event_box = gtk_event_box_new ();
	gtk_object_set_data (GTK_OBJECT (event_box), "was-pressed", FALSE);

	icon = nautilus_image_new ();
	gtk_misc_set_padding (GTK_MISC (icon), 2, 2);
	nautilus_buffered_widget_set_vertical_offset (NAUTILUS_BUFFERED_WIDGET (icon), 0);
	nautilus_buffered_widget_set_horizontal_offset (NAUTILUS_BUFFERED_WIDGET (icon), 0);


	gtk_signal_connect (GTK_OBJECT (event_box), "enter_notify_event", GTK_SIGNAL_FUNC (image_enter_event), icon);
	gtk_signal_connect (GTK_OBJECT (event_box), "leave_notify_event", GTK_SIGNAL_FUNC (image_leave_event), icon);
	gtk_signal_connect (GTK_OBJECT (event_box), "button_press_event", GTK_SIGNAL_FUNC (image_button_press_event), icon);
	gtk_signal_connect (GTK_OBJECT (event_box), "button_release_event", GTK_SIGNAL_FUNC (image_button_release_event), icon);

	nautilus_image_set_pixbuf (NAUTILUS_IMAGE (icon), icon_pixbuf);
	//nautilus_image_set_pixbuf (NAUTILUS_IMAGE (icon), icon_prelight_pixbuf);

	gtk_container_add (GTK_CONTAINER (event_box), icon);

	//gtk_object_set_data (GTK_OBJECT (applet), "widget", icon);
	gtk_object_set_data (GTK_OBJECT (applet), "widget", event_box);
	//applet_widget_add (APPLET_WIDGET (applet), icon);
	applet_widget_add (APPLET_WIDGET (applet), event_box);

	size = applet_widget_get_panel_pixel_size(APPLET_WIDGET(applet)) - 2;
	applet_change_pixel_size (GTK_WIDGET (applet), size, NULL);

	gtk_widget_show_all (applet);

	gtk_signal_connect(GTK_OBJECT(applet),"change_pixel_size",
			   GTK_SIGNAL_FUNC(applet_change_pixel_size),
			   NULL);

	applet_widget_gtk_main ();

	return 0;
}
