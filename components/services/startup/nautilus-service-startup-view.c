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
 * Author: Ramiro Estrugo
 */

/* nautilus-services-content-view.c - services content view
   component. This component just displays a simple label of the URI
   and does nothing else. It should be a good basis for writing
   out-of-proc content views.*/

#include <config.h>

#include "nautilus-service-startup-view.h"
#include "eazel-register.h"

#include <gnome-xml/tree.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-glib-extensions.h>
#include <libnautilus/nautilus-file-utilities.h>

#include <stdio.h>

struct _NautilusServicesContentViewDetails {
	gchar                     *uri;
	NautilusContentViewFrame  *view_frame;
	GtkWidget		  *title_label;
};

static void nautilus_service_startup_view_initialize_class (NautilusServicesContentViewClass *klass);
static void nautilus_service_startup_view_initialize       (NautilusServicesContentView *view);
static void nautilus_service_startup_view_destroy          (GtkObject *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusServicesContentView, nautilus_service_startup_view, 
				   GTK_TYPE_VBOX)
     
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
   
static void
nautilus_service_startup_view_initialize_class (NautilusServicesContentViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);

 	parent_class = gtk_type_class (gtk_vbox_get_type ());
	
	object_class->destroy = nautilus_service_startup_view_destroy;
}

static void
nautilus_service_startup_view_initialize (NautilusServicesContentView *view)
{
	GtkWidget *config_button;
	GtkWidget *config_label;
	GtkWidget *temp_box;
	
	view->details = g_new0 (NautilusServicesContentViewDetails, 1);
	
	view->details->view_frame = nautilus_content_view_frame_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->details->view_frame), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (service_main_notify_location_change_cb), 
			    view);

	/* put a label here as a placeholder just to show something */
	view->details->title_label = gtk_label_new ("Service View");
	gtk_box_pack_start (GTK_BOX (view), view->details->title_label, 0, 0, 0);	
	gtk_widget_show (view->details->title_label);

	/* add a temporary, placeholder button to create the configuration file */

	config_button = gtk_button_new ();		    
	config_label = gtk_label_new ("Create Configuration Inventory");
	gtk_widget_show (config_label);
	gtk_container_add (GTK_CONTAINER (config_button), config_label); 	
	gtk_button_set_relief (GTK_BUTTON (config_button), GTK_RELIEF_NORMAL);
	
	temp_box = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(temp_box);
	gtk_box_pack_start (GTK_BOX (temp_box), config_button, FALSE, FALSE, 16);
	gtk_box_pack_start (GTK_BOX (view), temp_box, FALSE, FALSE, 16);
	
	gtk_signal_connect (GTK_OBJECT (config_button), "clicked",
			    GTK_SIGNAL_FUNC (config_button_cb), NULL);	
	gtk_widget_show (config_button);
	
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
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);
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
