/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ramiro Estrugo <ramiro@eazel.com>
 *          Andy Hertzfeld <andy@eazel.com>
 *          J Shane Culpepper <pepper@eazel.com>
 */

/* nautilus-service-startup-view.c - The bootstrap controller for
 * eazel services.
 */
   
#include <config.h>

#include "nautilus-service-startup-view.h"
#include "shared-service-widgets.h"
#include "shared-service-utilities.h"

#include <unistd.h>
#include <gnome-xml/tree.h>
#include <gtk/gtkpixmap.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <stdio.h>

char 	*redirect_location;

struct _NautilusServiceStartupViewDetails {
	char		*uri;
	NautilusView	*nautilus_view;
	GtkWidget	*form;
	GtkWidget	*form_title;
	GtkWidget	*progress_bar;
	GtkWidget	*feedback_text;
};

#define STARTUP_VIEW_DEFAULT_BACKGROUND_COLOR  "rgb:0000/6666/6666"
#define SERVICE_DOMAIN_NAME		       "testmachine.eazel.com"



static void       nautilus_service_startup_view_initialize_class (NautilusServiceStartupViewClass *klass);
static void       nautilus_service_startup_view_initialize       (NautilusServiceStartupView      *view);
static void       nautilus_service_startup_view_destroy          (GtkObject                       *object);
static void       service_load_location_callback                 (NautilusView                    *view,
								  const char                      *location,
								  NautilusServiceStartupView      *services);
static void       generate_form_logo                             (NautilusServiceStartupView      *view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusServiceStartupView, nautilus_service_startup_view, GTK_TYPE_EVENT_BOX)

/* create the startup view */
static void
generate_startup_form (NautilusServiceStartupView	*view) 
{
	GtkWidget		*temp_widget;
	GtkWidget		*temp_box;
	GtkWidget		*align;
	int			counter;
	char			*font_color;
	GdkFont			*font;

	/* allocate the parent box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* setup the title */
	generate_form_logo (view);

	/* create a fill space box */
	temp_box = gtk_hbox_new (TRUE, 30);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, 0, 0, 12);
	gtk_widget_show (temp_box);

	/* Add the watch icon */
	temp_box = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, 0, 0, 40);
	gtk_widget_show (temp_box);

	temp_widget = create_image_widget ("service-watch.png",
					   STARTUP_VIEW_DEFAULT_BACKGROUND_COLOR,
					   NAUTILUS_IMAGE_PLACEMENT_CENTER);
	g_assert (temp_widget != NULL);

	gtk_box_pack_start (GTK_BOX (temp_box), temp_widget, 0, 0, 8);
	gtk_widget_show (temp_widget);

	/* Add a label for error status messages */
	view->details->feedback_text = gtk_label_new ("");
	font = nautilus_font_factory_get_font_from_preferences (12);
	font_color = g_strdup ("rgb:FFFF/FFFF/FFFF");
	set_widget_foreground_color (view->details->feedback_text, font_color);
	g_free (font_color);
	nautilus_gtk_widget_set_font (view->details->feedback_text, font);
	gdk_font_unref (font);
	gtk_box_pack_end (GTK_BOX (view->details->form), view->details->feedback_text, 0, 0, 15);

	/* Create a center alignment object */
	align = gtk_alignment_new (0.5, 0.5, 0, 0);
	gtk_box_pack_end (GTK_BOX (view->details->form), align, FALSE, FALSE, 5);
	gtk_widget_show (align);

	/* Add the progress meter */
	view->details->progress_bar = gtk_progress_bar_new ();
	gtk_container_add (GTK_CONTAINER (align), view->details->progress_bar);
	gtk_widget_show (view->details->progress_bar);

	/* bogus progress loop */
	for (counter = 1; counter <= 20000; counter++) {
		float value;

		value = (float) counter / 20000;

		if (counter == 1) {
			show_feedback (view->details->feedback_text, "Initializing eazel-proxy ...");
		}
		if (counter == 5000) {
			show_feedback (view->details->feedback_text, "Contacting www.eazel.com ...");
		}
		if (counter == 10000) {
			show_feedback (view->details->feedback_text, "Authenticating user anonymous ...");
		}
		if (counter == 15000) {
			show_feedback (view->details->feedback_text, "Retreiving services list ...");
		}
		if (counter == 20000) {
			go_to_uri (view->details->nautilus_view, "eazel-login:");
		}
		else {
			gtk_progress_bar_update (GTK_PROGRESS_BAR (view->details->progress_bar), value);
			while (gtk_events_pending ()) {
				gtk_main_iteration ();
			}
		}
	}

}

static void 
generate_form_logo (NautilusServiceStartupView	*view) {

	GtkWidget	*logo_container;
	GtkWidget	*logo_widget;

	logo_container = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), logo_container, 0, 0, 4);	

	logo_widget = create_image_widget ("startup-logo.png",
					   STARTUP_VIEW_DEFAULT_BACKGROUND_COLOR,
					   NAUTILUS_IMAGE_PLACEMENT_CENTER);

	g_assert (logo_widget != NULL);

	gtk_box_pack_start (GTK_BOX(logo_container), logo_widget, 0, 0, 4);
	gtk_widget_show_all (logo_container);
}

static void
nautilus_service_startup_view_initialize_class (NautilusServiceStartupViewClass	*klass) {

	GtkObjectClass	*object_class;
	GtkWidgetClass	*widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
 	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_service_startup_view_destroy;
}

static void
nautilus_service_startup_view_initialize (NautilusServiceStartupView	*view) {

  	NautilusBackground	*background;

	view->details = g_new0 (NautilusServiceStartupViewDetails, 1);
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (service_load_location_callback),
			    view);

  	background = nautilus_get_widget_background (GTK_WIDGET (view));
  	nautilus_background_set_color (background, STARTUP_VIEW_DEFAULT_BACKGROUND_COLOR);

	gtk_widget_show_all (GTK_WIDGET (view));
}

static void
nautilus_service_startup_view_destroy (GtkObject	*object) {

	NautilusServiceStartupView	*view;

	view = NAUTILUS_SERVICE_STARTUP_VIEW (object);

	g_free (view->details->uri);
	g_free (view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* Component embedding support */
NautilusView *
nautilus_service_startup_view_get_nautilus_view (NautilusServiceStartupView	*view) {

	return view->details->nautilus_view;

}

/* URI handling */
void
nautilus_service_startup_view_load_uri (NautilusServiceStartupView	*view,
				        const char			*uri) {

	char	*document_name;
	
	/* dispose of the old uri and copy the new one */
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy (view->details->form);
		view->details->form = NULL;
	}

	/* extract the document part of the uri */
	document_name = strchr (uri, ':');

	/* load the appropriate form, based on the uri and the registration state */
	if (is_location (document_name, "startup")) {
		generate_startup_form (view);
	}
	else if (is_location (document_name, "login")) {
		redirect_location = g_strdup_printf ("eazel-login:");
	}
	else if (is_location(document_name, "summary")) {
		redirect_location = g_strdup_printf ("eazel-summary:");
	}
	else if (is_location(document_name, "inventory")) {
		redirect_location = g_strdup_printf ("eazel-inventory:");
	}
	else if (is_location(document_name, "install")) {
		redirect_location = g_strdup_printf ("eazel-install:");
	}
	else if (is_location(document_name, "time")) {
		redirect_location = g_strdup_printf ("eazel-time:");
	}
	else if (is_location(document_name, "vault")) {
		redirect_location = g_strdup_printf ("eazel-vault:");
	}
	else if (is_location(document_name, "register")) {
		redirect_location = g_strdup_printf ("http://www.eazel.com/registration.html");
	}
	else if (is_location(document_name, "serviceinfo")) {
		redirect_location = g_strdup_printf ("http://www.eazel.com/services.html");
	}
	else {
		generate_startup_form (view); /* eventually, this should be setup_bad_location_form */
	}
}

static void
service_load_location_callback (NautilusView			*view,
				const char			*location,
				NautilusServiceStartupView	*services) {

	nautilus_view_report_load_underway (services->details->nautilus_view);
	nautilus_service_startup_view_load_uri (services, location);
	nautilus_view_report_load_complete (services->details->nautilus_view);
	go_to_uri (view, redirect_location);
}

