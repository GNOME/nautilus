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

   Authors: Mathieu Lacage  <mathieu@eazel.com>
            Ramiro Estrugo <ramiro@eazel.com>
 */

/* Everything beyond this point is pure evil. */

#include <config.h>
#include <applet-widget.h>
#include <eel/eel-image.h>
#include <eel/eel-graphic-effects.h>
#include <libgnome/gnome-exec.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkeventbox.h>
#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>

/*
 * The purpose of this applet is to launch Nautilus.  The applet tries very hard to 
 * give good feedback to the user by changing the appearance of the launch icon.
 * The launch icon cursor and the desktop cursor are also updated in concert when 
 * a new Nautilus window is launching.
 */

#define ICON_NAME "nautilus-launch-icon.png"
#define VERTICAL_OFFSET 2
#define HORIZONTAL_OFFSET 2

static GdkPixbuf *icon_pixbuf = NULL;
static GdkPixbuf *icon_prelight_pixbuf = NULL;
static long last_window_realize_time = 0;
static GtkWidget *icon_image = NULL;
static GtkWidget *icon_event_box = NULL;

static void     set_is_launching (gboolean state);
static gboolean get_is_launching (void);

static void
create_pixbufs ()
{
	if (icon_pixbuf == NULL) {
		char *path;
		
		path = g_strdup_printf ("%s/pixmaps/%s", DATADIR, ICON_NAME);
		
		icon_pixbuf = gdk_pixbuf_new_from_file (path);
		g_free (path);
		
		g_assert (icon_pixbuf != NULL);

		icon_prelight_pixbuf = eel_create_spotlight_pixbuf (icon_pixbuf);
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
	g_return_val_if_fail (EEL_IS_IMAGE (client_data), TRUE);

	if (!get_is_launching ()) {
		eel_image_set_pixbuf (EEL_IMAGE (client_data), icon_prelight_pixbuf);
	}

	return TRUE;
}

static gint
image_leave_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (EEL_IS_IMAGE (client_data), TRUE);
	
	eel_image_set_pixbuf (EEL_IMAGE (client_data), icon_pixbuf);
	gtk_object_set_data (GTK_OBJECT (event_box), "was-pressed", FALSE);

	return TRUE;
}

static GdkWindow *
get_root_window (void)
{
	return GDK_ROOT_PARENT ();
}

static void
window_set_cursor_for_state (GdkWindow *window, gboolean busy)
{
	GdkCursor *cursor;

	g_return_if_fail (window != NULL);

	cursor = gdk_cursor_new (busy ? GDK_WATCH : GDK_LEFT_PTR);
 	gdk_window_set_cursor (window, cursor);
	gdk_cursor_destroy (cursor);
}

static gboolean is_launching = FALSE;

static void
set_is_launching (gboolean state)
{
	if (is_launching == state) {
		return;
	}

	is_launching = state;

	window_set_cursor_for_state (get_root_window (), is_launching);
	window_set_cursor_for_state (GTK_WIDGET (icon_event_box)->window, is_launching);	

	eel_image_set_pixbuf_opacity (EEL_IMAGE (icon_image), is_launching ? 128 : 255);
}

static gboolean
get_is_launching (void)
{
	return is_launching;
}

static gint
image_button_press_event (GtkWidget *event_box,
			  GdkEventButton *event,
			  gpointer client_data)
{
	GtkWidget *icon = GTK_WIDGET (client_data);

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (EEL_IS_IMAGE (icon), TRUE);

	if (!get_is_launching ()) {
		gtk_object_set_data (GTK_OBJECT (event_box), "was-pressed", GINT_TO_POINTER (TRUE));
	}
		
	return TRUE;
}

static gint
image_button_release_event (GtkWidget *event_box,
			    GdkEventButton *event,
			    gpointer client_data)
{
	GtkWidget *icon = GTK_WIDGET (client_data);

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (EEL_IS_IMAGE (icon), TRUE);

	if (GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (event_box), "was-pressed"))) {
		gtk_object_set_data (GTK_OBJECT (event_box), "was-pressed", FALSE);

		if (!get_is_launching ())
		{
			char *path;
			gint pid;

			path = g_strdup_printf ("%s/%s", BINDIR, "run-nautilus");
			
			pid = gnome_execute_async (NULL, 1, &path);
			
			if (pid != 0) {
				set_is_launching (TRUE);
			}

			g_free (path);
		}
	}
	
	return TRUE;
}

static GdkFilterReturn
event_filter (GdkXEvent *gdk_xevent,
	      GdkEvent *event,
	      gpointer client_data)
{
	XEvent *xevent = (XEvent *) gdk_xevent;
	
	if (xevent->type == PropertyNotify) {
		GdkAtom actual_property_type;
		gint actual_format;
		gint actual_length;
		guchar *data;
	
		if (gdk_property_get (get_root_window (),
				      gdk_atom_intern ("_NAUTILUS_LAST_WINDOW_REALIZE_TIME", FALSE),
				      0,
				      0,
				      1L,
				      FALSE,
				      &actual_property_type,
				      &actual_format,
				      &actual_length,
				      &data)) {
			
			if (actual_format == 32 && actual_length == 4) {
				long realize_time;
				
				realize_time = *((long *) data);
				
				if (last_window_realize_time != realize_time) {
					last_window_realize_time = realize_time;
					set_is_launching (FALSE);
				}
			}
			
			g_free (data);
			
		}
	}

	return GDK_FILTER_CONTINUE;
}

static void
root_listen_for_property_changes (void)
{
	XWindowAttributes attribs = { 0 };

	gdk_window_add_filter (get_root_window (), event_filter, NULL);
	
	XGetWindowAttributes (GDK_DISPLAY (), GDK_ROOT_WINDOW (), &attribs);

	XSelectInput (GDK_DISPLAY (), GDK_ROOT_WINDOW (), attribs.your_event_mask | PropertyChangeMask);
	
	gdk_flush ();
}

int
main (int argc, char **argv)
{
	GtkWidget *applet;
	int size;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	
	applet_widget_init ("nautilus_launcher_applet", VERSION, argc,
			    argv, NULL, 0, NULL);

	applet = applet_widget_new ("nautilus_launcher_applet");
	if (applet == NULL)
		g_error (_("Can't create nautilus-launcher-applet!"));

	root_listen_for_property_changes ();

	create_pixbufs ();

	icon_event_box = gtk_event_box_new ();
	gtk_object_set_data (GTK_OBJECT (icon_event_box), "was-pressed", FALSE);

	icon_image = eel_image_new (NULL);
	gtk_misc_set_padding (GTK_MISC (icon_image), 2, 2);

	gtk_signal_connect (GTK_OBJECT (icon_event_box), "enter_notify_event", GTK_SIGNAL_FUNC (image_enter_event), icon_image);
	gtk_signal_connect (GTK_OBJECT (icon_event_box), "leave_notify_event", GTK_SIGNAL_FUNC (image_leave_event), icon_image);
	gtk_signal_connect (GTK_OBJECT (icon_event_box), "button_press_event", GTK_SIGNAL_FUNC (image_button_press_event), icon_image);
	gtk_signal_connect (GTK_OBJECT (icon_event_box), "button_release_event", GTK_SIGNAL_FUNC (image_button_release_event), icon_image);

	eel_image_set_pixbuf (EEL_IMAGE (icon_image), icon_pixbuf);

	gtk_container_add (GTK_CONTAINER (icon_event_box), icon_image);
	gtk_object_set_data (GTK_OBJECT (applet), "widget", icon_event_box);
	applet_widget_add (APPLET_WIDGET (applet), icon_event_box);

	size = applet_widget_get_panel_pixel_size(APPLET_WIDGET(applet)) - 2;
	applet_change_pixel_size (GTK_WIDGET (applet), size, NULL);

	gtk_widget_show_all (applet);

	gtk_signal_connect(GTK_OBJECT(applet),"change_pixel_size",
			   GTK_SIGNAL_FUNC(applet_change_pixel_size),
			   NULL);

	applet_widget_gtk_main ();

	return 0;
}
