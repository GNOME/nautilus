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
#include <ghttp.h>
#include <unistd.h>
#include <gnome-xml/tree.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <stdio.h>

#include "nautilus-service-startup-view.h"

struct _NautilusServiceStartupViewDetails {
	char		*uri;
	NautilusView	*nautilus_view;
	GtkWidget	*form;
	GtkWidget	*form_title;
	GtkWidget	*feedback_text;
};


#define SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR  "rgb:0000/6666/6666"
#define SERVICE_DOMAIN_NAME		       "testmachine.eazel.com"

static void nautilus_service_startup_view_initialize_class	(NautilusServiceStartupViewClass	*klass);
static void nautilus_service_startup_view_initialize		(NautilusServiceStartupView		*view);
static void nautilus_service_startup_view_destroy		(GtkObject				*object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusServiceStartupView, nautilus_service_startup_view, GTK_TYPE_EVENT_BOX)

static void 	service_load_location_callback			(NautilusView 				*view, 
								 const char				*location,
								 NautilusServiceStartupView		*services);
static gboolean is_location					(char 					*document_str,
								 const char				*place_str);
static void	generate_form_title				(NautilusServiceStartupView		*view,
								 const char				*title_text);


/* create the startup view */

static void
generate_startup_form (NautilusServiceStartupView	*view) {

	GtkWidget	*temp_widget;

	/* allocate the parent box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* setup the title */
	generate_form_title (view, "Initializing Eazel Services ...");

	/* put a mystery label here as a placeholder. */
	temp_widget = gtk_label_new ("I am just a view.  One day I will be gone.");
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_widget, 0, 0, 0);
	gtk_widget_show (temp_widget);
}

/* utility routine to go to another uri */

static void
go_to_uri(NautilusServiceStartupView* view, char* uri) {

  	nautilus_view_open_location (view->details->nautilus_view, uri);

}

/* shared utility to allocate a title for a form */

static void 
generate_form_title (NautilusServiceStartupView	*view,
		     const char			*title_text) {

	GtkWidget	*temp_widget;
	char		*file_name;	
	GtkWidget	*temp_container;
	GdkFont 	*font;

	temp_container = gtk_hbox_new (FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_container, 0, 0, 4);	
	gtk_widget_show (temp_container);
	
 	file_name = nautilus_pixmap_file ("eazel-cloud-logo.png");
  	temp_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
	gtk_box_pack_start (GTK_BOX(temp_container), temp_widget, 0, 0, 8);		
  	gtk_widget_show (temp_widget);
  	g_free (file_name);

 	view->details->form_title = gtk_label_new (title_text);

        font = nautilus_font_factory_get_font_from_preferences (18);
	nautilus_gtk_widget_set_font (view->details->form_title, font);
        gdk_font_unref (font);

	gtk_box_pack_start (GTK_BOX (temp_container), view->details->form_title, 0, 0, 8);			 	
	gtk_widget_show (view->details->form_title);
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
  	nautilus_background_set_color (background, SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR);

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

/* utility for checking uri */
static gboolean
is_location (char	*document_str, const char	*place_str) {

	return document_str && !strncmp (document_str + 1, place_str, strlen (place_str));

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
		go_to_uri (view, "eazel-login:");
	}
	else if (is_location(document_name, "summary")) {
		go_to_uri (view, "eazel-summary:");
	}
	else if (is_location(document_name, "inventory")) {
		go_to_uri (view, "eazel-inventory:");
	}
	else if (is_location(document_name, "install")) {
		go_to_uri (view, "eazel-install:");
	}
	else if (is_location(document_name, "time")) {
		go_to_uri (view, "eazel-time:");
	}
	else if (is_location(document_name, "vault")) {
		go_to_uri (view, "eazel-vault:");
	}
	else {
		generate_startup_form (view); /* eventually, this should be setup_bad_location_form */
	}
}

static void
service_load_location_callback (NautilusView			*view,
				const char			*location,
				NautilusServiceStartupView	*services)
{
	nautilus_view_report_load_underway (services->details->nautilus_view);
	nautilus_service_startup_view_load_uri (services, location);
	nautilus_view_report_load_complete (services->details->nautilus_view);
}
