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
#include "nautilus-adapter-embed-strategy.h"

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-item-container.h>
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

struct NautilusAdapterDetails {
	NautilusAdapterLoadStrategy  *load_strategy;
	NautilusAdapterEmbedStrategy *embed_strategy;
	NautilusView                 *nautilus_view;
};


static void nautilus_adapter_load_location_callback (NautilusView    *view,
						     const char      *uri,
						     NautilusAdapter *adapter);
static void nautilus_adapter_stop_loading_callback  (NautilusView    *view,
						     NautilusAdapter *adapter);

static void nautilus_adapter_open_location_callback (NautilusAdapterEmbedStrategy *strategy,
						     const char                   *uri,
						     NautilusAdapter              *adapter);

static void nautilus_adapter_load_underway_callback (NautilusAdapter             *adapter);
static void nautilus_adapter_load_progress_callback (NautilusAdapter             *adapter,
						     double                       fraction_complete);
static void nautilus_adapter_load_complete_callback (NautilusAdapter             *adapter);
static void nautilus_adapter_load_failed_callback   (NautilusAdapter             *adapter);



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
	
	if (server->details->load_strategy != NULL) {
		nautilus_adapter_load_strategy_stop_loading (server->details->load_strategy);
		gtk_object_unref (GTK_OBJECT (server->details->load_strategy));
	}

	if (server->details->embed_strategy != NULL) {
		gtk_object_unref (GTK_OBJECT (server->details->embed_strategy));
	}

	g_free (server->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


NautilusAdapter *
nautilus_adapter_new (Bonobo_Unknown component)
{
	NautilusAdapter      *adapter;
	BonoboControl        *control;
	GtkWidget            *bin;
	CORBA_Environment     ev;


	/* FIXME: should be done with construct args */

	CORBA_exception_init (&ev);

	adapter = NAUTILUS_ADAPTER (gtk_object_new (NAUTILUS_TYPE_ADAPTER, NULL));


	/* Set up a few wrapper framework details */
	bin =  gtk_widget_new (NAUTILUS_TYPE_GENEROUS_BIN, NULL);
	gtk_widget_show (bin);
	control = bonobo_control_new (bin);
	adapter->details->nautilus_view = nautilus_view_new_from_bonobo_control (control);

	gtk_signal_connect_object (GTK_OBJECT (adapter->details->nautilus_view),
				   "destroy",
				   gtk_object_unref,
				   GTK_OBJECT (adapter));

	/* Get the class to handle embedding this kind of component. */
	adapter->details->embed_strategy = nautilus_adapter_embed_strategy_get
		(component, bonobo_control_get_remote_ui_container (control));

	if (adapter->details->embed_strategy == NULL) {
		gtk_object_unref (GTK_OBJECT (adapter));
		
		return NULL;
	}

	gtk_signal_connect (GTK_OBJECT (adapter->details->embed_strategy), "open_location", 
			    nautilus_adapter_open_location_callback, adapter);


	/* Get the class to handle loading this kind of component. */

	adapter->details->load_strategy = nautilus_adapter_load_strategy_get
		(component);

	if (adapter->details->load_strategy == NULL) {
		gtk_object_unref (GTK_OBJECT (adapter));
		
		return NULL;
	}

	/* hook up load strategy signals */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (adapter->details->load_strategy),
					       "report_load_underway",
					       nautilus_adapter_load_underway_callback,
					       GTK_OBJECT (adapter));

	/* hook up load strategy signals */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (adapter->details->load_strategy),
					       "report_load_progress",
					       nautilus_adapter_load_progress_callback,
					       GTK_OBJECT (adapter));

	/* hook up load strategy signals */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (adapter->details->load_strategy),
					       "report_load_complete",
					       nautilus_adapter_load_complete_callback,
					       GTK_OBJECT (adapter));

	/* hook up load strategy signals */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (adapter->details->load_strategy),
					       "report_load_failed",
					       nautilus_adapter_load_failed_callback,
					       GTK_OBJECT (adapter));

	/* complete the embedding */

	gtk_container_add (GTK_CONTAINER (bin), 
			   nautilus_adapter_embed_strategy_get_widget (adapter->details->embed_strategy));

			   
	/* hook up view signals. */

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


static void
nautilus_adapter_open_location_callback  (NautilusAdapterEmbedStrategy *strategy,
					  const char                   *uri,
					  NautilusAdapter              *adapter)
{
	nautilus_view_open_location (adapter->details->nautilus_view,
				     uri);
}



static void
nautilus_adapter_load_underway_callback (NautilusAdapter             *adapter)
{
	nautilus_view_report_load_underway (adapter->details->nautilus_view);
}

static void
nautilus_adapter_load_progress_callback (NautilusAdapter             *adapter,
					 double                       fraction_complete)
{
	nautilus_view_report_load_progress (adapter->details->nautilus_view,
					    fraction_complete);
}

static void
nautilus_adapter_load_complete_callback (NautilusAdapter             *adapter)
{
	nautilus_view_report_load_complete (adapter->details->nautilus_view);
}

static void
nautilus_adapter_load_failed_callback   (NautilusAdapter             *adapter)
{
	nautilus_view_report_load_failed (adapter->details->nautilus_view);
}

