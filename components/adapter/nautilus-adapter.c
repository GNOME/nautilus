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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-adapter.c - Class for adapting bonobo controls/embeddables
 * to look like Nautilus views.
 */

#include <config.h>
#include "nautilus-adapter.h"
#include "nautilus-adapter-load-strategy.h"

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-container.h>
#include <bonobo/bonobo-view-frame.h>
#include <bonobo/bonobo-object-client.h>
#include <gtk/gtksignal.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-generous-bin.h>
#include <libnautilus-adapter/nautilus-adapter-factory.h>

#include <stdio.h>

struct NautilusAdapterDetails {
	NautilusAdapterLoadStrategy *load_strategy;
	NautilusView                *nautilus_view;
};


static void nautilus_adapter_load_location_callback (NautilusView    *view,
						     const char      *uri,
						     NautilusAdapter *adapter);


static void nautilus_adapter_stop_loading_callback  (NautilusView    *view,
						     NautilusAdapter *adapter);



static void nautilus_adapter_initialize_class (NautilusAdapterClass *klass);
static void nautilus_adapter_initialize       (NautilusAdapter      *server);
static void nautilus_adapter_destroy          (GtkObject                      *object);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusAdapter,
				   nautilus_adapter,
				   GTK_TYPE_OBJECT)

     
static void
nautilus_adapter_initialize_class (NautilusAdapterClass *klass)
{
	GtkObjectClass *object_class;
	
	g_assert (NAUTILUS_IS_ADAPTER_CLASS (klass));

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_adapter_destroy;
}

static void
nautilus_adapter_initialize (NautilusAdapter *adapter)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	g_assert (NAUTILUS_IS_ADAPTER (adapter));

	adapter->details = g_new0 (NautilusAdapterDetails, 1);
	
	CORBA_exception_free (&ev);
}

static void
nautilus_adapter_destroy (GtkObject *object)
{
	NautilusAdapter *server;
	
	server = NAUTILUS_ADAPTER (object);
	
	gtk_object_unref (GTK_OBJECT (server->details->load_strategy));

	g_free (server->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


NautilusAdapter *
nautilus_adapter_new (Bonobo_Unknown component)
{
	NautilusAdapter      *adapter;
	Bonobo_Embeddable     embeddable;
	BonoboControl        *control;
	BonoboContainer      *container;
	BonoboClientSite     *client_site;
	BonoboViewFrame      *view_frame;
	Bonobo_UIHandler      uih;
	GtkWidget            *bin;
	CORBA_Environment     ev;
	BonoboObjectClient   *component_wrapper;
	GtkWidget            *client_widget;


	/* FIXME: should be done with construct args */

	CORBA_exception_init (&ev);

	adapter = NAUTILUS_ADAPTER (gtk_type_new (NAUTILUS_TYPE_ADAPTER));

	embeddable = Bonobo_Unknown_query_interface (component,
						     "IDL:Bonobo/Embeddable:1.0",
						     &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION || CORBA_Object_is_nil (embeddable, &ev)) {
		CORBA_exception_free (&ev);
		
		gtk_object_unref (GTK_OBJECT (adapter));

		return NULL;
	}

	bin =  gtk_widget_new (NAUTILUS_TYPE_GENEROUS_BIN, NULL);
	control = bonobo_control_new (bin);
	adapter->details->nautilus_view = nautilus_view_new_from_bonobo_control (control);


	adapter->details->load_strategy = nautilus_adapter_load_strategy_get
		(component, adapter->details->nautilus_view);

	if (adapter->details->load_strategy == NULL) {
		bonobo_object_release_unref (embeddable, &ev);
		CORBA_exception_free (&ev);

		bonobo_object_unref (BONOBO_OBJECT (control));
		bonobo_object_unref (BONOBO_OBJECT (adapter->details->nautilus_view));

		gtk_object_unref (GTK_OBJECT (adapter));
		
		return NULL;
	}

	gtk_signal_connect_object (GTK_OBJECT (adapter->details->nautilus_view),
				   "destroy",
				   gtk_object_unref,
				   GTK_OBJECT (adapter));


	component_wrapper = bonobo_object_client_from_corba (embeddable);


	gtk_widget_show (bin);


	uih = bonobo_object_corba_objref (BONOBO_OBJECT (bonobo_control_get_ui_handler 
							 (control)));

	container = bonobo_container_new();
      	client_site = bonobo_client_site_new (container);
	bonobo_client_site_bind_embeddable (client_site, component_wrapper);
	bonobo_container_add (container, BONOBO_OBJECT (client_site));

	view_frame = bonobo_client_site_new_view (client_site, uih);
	client_widget = bonobo_view_frame_get_wrapper(view_frame);

	gtk_widget_show (client_widget);

	gtk_container_add (GTK_CONTAINER (bin), client_widget);
     	bonobo_wrapper_set_visibility (BONOBO_WRAPPER (client_widget), FALSE);
	bonobo_view_frame_set_covered (view_frame, FALSE); 

	
	gtk_signal_connect (GTK_OBJECT (adapter->details->nautilus_view),
			    "load_location",
			    nautilus_adapter_load_location_callback,
			    adapter);

	gtk_signal_connect (GTK_OBJECT (adapter->details->nautilus_view),
			    "stop_loading",
			    nautilus_adapter_stop_loading_callback,
			    adapter);

	return adapter;
}


NautilusView *
nautilus_adapter_get_nautilus_view (NautilusAdapter *adapter) 
{
	return adapter->details->nautilus_view;
}



static void 
nautilus_adapter_load_location_callback (NautilusView    *view,
					 const char      *location,
					 NautilusAdapter *adapter)
{
	nautilus_adapter_load_strategy_load_location (adapter->details->load_strategy,
						      location);
}



static void
nautilus_adapter_stop_loading_callback  (NautilusView    *view,
					 NautilusAdapter *adapter)
{
	nautilus_adapter_load_strategy_stop_loading (adapter->details->load_strategy);
}


