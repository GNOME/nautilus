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
 		    Andy Hertzfeld <andy@eazel.com>
 */

/* nautilus-services-content-view.c - services content view
   component. This component supplies some UI for the service and
   invokes other functions like configuration upload */
   
#include <config.h>

#include "nautilus-service-startup-view.h"
#include "eazel-register.h"

#include <gnome-xml/tree.h>
#include <libnautilus/nautilus-background.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-glib-extensions.h>
#include <libnautilus/nautilus-file-utilities.h>

#include <stdio.h>

struct _NautilusServicesContentViewDetails {
	gchar                     *uri;
	NautilusContentViewFrame  *view_frame;
	GtkWidget		  *form;
};

#define SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR  "rgb:BBBB/DDDD/FFFF"

static void nautilus_service_startup_view_initialize_class (NautilusServicesContentViewClass *klass);
static void nautilus_service_startup_view_initialize       (NautilusServicesContentView *view);
static void nautilus_service_startup_view_destroy          (GtkObject *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusServicesContentView, nautilus_service_startup_view, 
				   GTK_TYPE_EVENT_BOX)
     
static void service_main_notify_location_change_cb              (NautilusContentViewFrame  *view, 
							   Nautilus_NavigationInfo   *navinfo, 
							   NautilusServicesContentView *services);

/* temporary callback to handle the configuration button */    
static void
config_button_cb (GtkWidget *button, char *data)
{
	xmlDocPtr packages_file = create_configuration_metafile();
	if (packages_file != NULL) {
  		gchar *temp_str = g_strdup_printf("%s/configuration.xml", g_get_home_dir());
		xmlSaveFile(temp_str, packages_file);
  		xmlFreeDoc(packages_file);
  		g_free(temp_str);
	}
}

/* this creates the simple test form (just temporary) */

static void setup_test_form(NautilusServicesContentView *view)
{
	GtkWidget *temp_widget;
	GtkWidget *config_button;
	GtkWidget *config_label;
	GtkWidget *temp_box;

	/* allocate a box to hold everything */	
	view->details->form = gtk_vbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show(view->details->form);
	
	/* put a label here as a placeholder just to show something */
	temp_widget = gtk_label_new ("Service View");
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_widget, 0, 0, 0);	
	gtk_widget_show (temp_widget);

	/* add a button to create the configuration file */

	config_button = gtk_button_new ();		    
	config_label = gtk_label_new ("Create Configuration Inventory");
	gtk_widget_show (config_label);
	gtk_container_add (GTK_CONTAINER (config_button), config_label); 	
	gtk_button_set_relief (GTK_BUTTON (config_button), GTK_RELIEF_NORMAL);
	
	temp_box = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(temp_box);
	gtk_box_pack_start (GTK_BOX (temp_box), config_button, FALSE, FALSE, 16);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 16);
	
	gtk_signal_connect (GTK_OBJECT (config_button), "clicked",
			    GTK_SIGNAL_FUNC (config_button_cb), NULL);	
	gtk_widget_show (config_button);
}

/* create the signup form */

static void setup_signup_form(NautilusServicesContentView *view)
{
	/* put a label here as a placeholder just to show something */
	view->details->form = gtk_label_new ("Signup Form");
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show (view->details->form);
}

/* create the config form */

static void setup_config_form(NautilusServicesContentView *view)
{
	gchar *file_name;
	GtkWidget *temp_widget, *temp_container;
	gchar *message;
	/* allocate a vbox as the container */	
	view->details->form = gtk_vbox_new(FALSE,0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show(view->details->form);

	/* build the title in an hbox */	
	
	temp_container = gtk_hbox_new(FALSE,0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_container, 0, 0, 4);	
	gtk_widget_show(temp_container);
	
 	file_name = gnome_pixmap_file ("nautilus/eazel-logo.gif");
  	temp_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
	gtk_box_pack_start(GTK_BOX(temp_container), temp_widget, 0, 0, 8);		
  	gtk_widget_show(temp_widget);
  	g_free (file_name);

	temp_widget = gtk_label_new ("Eazel Service Configuration Gathering");
	gtk_box_pack_start(GTK_BOX(temp_container), temp_widget, 0, 0, 0);			
 	gtk_widget_show (temp_widget);
	
	/* make label containing text about uploading the configuration data */
	/* FIXME: It should get this text from a file or from the service */
	message = "With your permission, the Eazel service will gather data about the hardware and software configuration of your system so it can provide you a customized software catalog with one-click installation.  Your configuration data will be kept strictly confidential and will not be used for any other purpose.  Click the button below to begin gathering the data.";
	temp_widget = gtk_label_new (message);
 	gtk_label_set_line_wrap(GTK_LABEL(temp_widget), TRUE);
	
	gtk_box_pack_start(GTK_BOX(view->details->form), temp_widget, 0, 0, 12);			
 	gtk_widget_show (temp_widget);

	/* add buttons for accepting and declining */	
}

static void
nautilus_service_startup_view_initialize_class (NautilusServicesContentViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);

 	parent_class = gtk_type_class (gtk_event_box_get_type ());
	
	object_class->destroy = nautilus_service_startup_view_destroy;
}

static void
nautilus_service_startup_view_initialize (NautilusServicesContentView *view)
{	
  	NautilusBackground *background;

	view->details = g_new0 (NautilusServicesContentViewDetails, 1);
	
	view->details->view_frame = nautilus_content_view_frame_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->details->view_frame), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (service_main_notify_location_change_cb), 
			    view);

	view->details->form = NULL;

  	background = nautilus_get_widget_background (GTK_WIDGET (view));
  	nautilus_background_set_color (background, SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR);
  		
	gtk_widget_show_all (GTK_WIDGET (view));
}

static void
nautilus_service_startup_view_destroy (GtkObject *object)
{
	NautilusServicesContentView *view;
	
	view = NAUTILUS_SERVICE_STARTUP_VIEW (object);
	
	g_free (view->details->uri);
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


/* Component embedding support */
NautilusContentViewFrame *
nautilus_service_startup_view_get_view_frame (NautilusServicesContentView *view)
{
	return view->details->view_frame;
}

/* URI handling */
void
nautilus_service_startup_view_load_uri (NautilusServicesContentView *view,
				       const gchar               *uri)
{
	gchar *document_name;
	
	/* dispose of the old uri and copy the new one */
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy(view->details->form);
		view->details->form = NULL;	
	}
		
	/* extract the document part of the uri */
	document_name = strchr(uri, ':');
	
	/* load the appropriate form, based on the uri and the registration state */
	if (document_name && !strncmp(document_name + 1, "signup", 6)) {
		setup_signup_form(view);
	}
	else if (document_name && !strncmp(document_name + 1, "config", 5)) {
		setup_config_form(view);
	}	
	else
		setup_test_form(view);
}

static void
service_main_notify_location_change_cb (NautilusContentViewFrame  *view, 
					Nautilus_NavigationInfo   *navinfo, 
					NautilusServicesContentView *services)
{
	Nautilus_ProgressRequestInfo pri;
	
	memset(&pri, 0, sizeof(pri));
	
	/* we must send a PROGRESS_UNDERWAY message */
	pri.type = Nautilus_PROGRESS_UNDERWAY;
	pri.amount = 0.0;
	nautilus_view_frame_request_progress_change (NAUTILUS_VIEW_FRAME (services->details->view_frame), &pri);
	
	nautilus_service_startup_view_load_uri (services, navinfo->actual_uri);
	
	/* likewise, we must send a PROGRESS_DONE message */
	pri.type = Nautilus_PROGRESS_DONE_OK;
	pri.amount = 100.0;
	nautilus_view_frame_request_progress_change (NAUTILUS_VIEW_FRAME (services->details->view_frame), &pri);
}
