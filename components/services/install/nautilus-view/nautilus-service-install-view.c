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
 * Author: J Shane Culpepper <pepper@eazel.com>
 */

#include <config.h>

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
#include <unistd.h>

#include "nautilus-service-install-view.h"



/* A NautilusContentView's private information. */
struct _NautilusServiceInstallViewDetails {
	char 		*uri;
	NautilusView	*nautilus_view;
	GtkWidget	*form;
	GtkWidget	*form_title;
	GtkWidget	*feedback_text;
};

#define SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR	"rgb:FFFF/FFFF/FFFF"
#define SERVICE_DOMAIN_NAME			"testmachine.eazel.com"

static void	nautilus_service_install_view_initialize_class	(NautilusServiceInstallViewClass	*klass);
static void	nautilus_service_install_view_initialize	(NautilusServiceInstallView		*view);
static void	nautilus_service_install_view_destroy		(GtkObject				*object);
static void	service_install_load_location_callback		(NautilusView				*nautilus_view,
								 const char				*location,
		 						 NautilusServiceInstallView		*view);
static void	generate_install_form			(NautilusServiceInstallView		*view);
static void	generate_form_title			(NautilusServiceInstallView		*view,
							 const char				*title_text);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusServiceInstallView, nautilus_service_install_view, GTK_TYPE_EVENT_BOX)

static void
generate_install_form (NautilusServiceInstallView	*view) {

	GtkWidget	*temp_widget;

	/* allocate the parent box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* setup the title */
	generate_form_title (view, "Easy Install");

	/* put a mystery label here as a placeholder. */
	temp_widget = gtk_label_new ("I am just a view.  One day I will be gone.");
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_widget, 0, 0, 0);
	gtk_widget_show (temp_widget);

}

static void
generate_form_title (NautilusServiceInstallView	*view,
                     const char			*title_text) {

        GtkWidget	*temp_widget;
        char		*file_name;
        GtkWidget	*temp_container;
        GdkFont		*font;

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
nautilus_service_install_view_initialize_class (NautilusServiceInstallViewClass *klass) {

	GtkObjectClass	*object_class;
	GtkWidgetClass	*widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_service_install_view_destroy;
}

static void
nautilus_service_install_view_initialize (NautilusServiceInstallView *view) {

	NautilusBackground	*background;

	view->details = g_new0 (NautilusServiceInstallViewDetails, 1);
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (service_install_load_location_callback), 
			    view);

	background = nautilus_get_widget_background (GTK_WIDGET (view));
	nautilus_background_set_color (background, SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR);

	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_service_install_view_destroy (GtkObject *object) {

	NautilusServiceInstallView	*view;
	
	view = NAUTILUS_SERVICE_INSTALL_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

}

NautilusView *
nautilus_service_install_view_get_nautilus_view (NautilusServiceInstallView *view) {

	return view->details->nautilus_view;

}

void
nautilus_service_install_view_load_uri (NautilusServiceInstallView	*view,
			     	        const char			*uri) {

	/* dispose of any old uri and copy in the new one */	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy (view->details->form);
		view->details->form = NULL;
	}

	generate_install_form (view);
}

static void
service_install_load_location_callback (NautilusView			*nautilus_view, 
			  	        const char			*location,
			       		NautilusServiceInstallView	*view) {

	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	nautilus_service_install_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);

}

