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

#include "nautilus-adapter-embed-strategy-private.h"
#include "nautilus-adapter-embed-strategy.h"
#include "nautilus-adapter-load-strategy.h"
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-item-container.h>
#include <bonobo/bonobo-object-client.h>
#include <bonobo/bonobo-view-frame.h>
#include <eel/eel-generous-bin.h>
#include <eel/eel-gtk-macros.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus-adapter/nautilus-adapter-factory.h>
#include <libnautilus/nautilus-bonobo-ui.h>

struct NautilusAdapterDetails {
	NautilusView *nautilus_view;
	NautilusAdapterEmbedStrategy *embed_strategy;
	NautilusAdapterLoadStrategy *load_strategy;
	guint report_load_underway_id;
	guint report_load_progress_id;
	guint report_load_complete_id;
        guint report_load_failed_id;
};


static void nautilus_adapter_load_location_callback (NautilusView                 *view,
						     const char                   *uri,
						     NautilusAdapter              *adapter);
static void nautilus_adapter_stop_loading_callback  (NautilusView                 *view,
						     NautilusAdapter              *adapter);
static void nautilus_adapter_activate_callback      (BonoboControl                *control,
						     gboolean                      state,
						     NautilusAdapter              *adapter);
static void nautilus_adapter_open_location_callback (NautilusAdapterEmbedStrategy *strategy,
						     const char                   *uri,
						     NautilusAdapter              *adapter);
static void nautilus_adapter_load_underway_callback (NautilusAdapter              *adapter);
static void nautilus_adapter_load_progress_callback (NautilusAdapter              *adapter,
						     double                        fraction_complete);
static void nautilus_adapter_load_complete_callback (NautilusAdapter              *adapter);
static void nautilus_adapter_load_failed_callback   (NautilusAdapter              *adapter);
static void nautilus_adapter_initialize_class       (NautilusAdapterClass         *klass);
static void nautilus_adapter_initialize             (NautilusAdapter              *server);
static void nautilus_adapter_destroy                (GtkObject                    *object);


EEL_DEFINE_CLASS_BOILERPLATE (NautilusAdapter,
			      nautilus_adapter,
			      GTK_TYPE_OBJECT)

     
static void
nautilus_adapter_initialize_class (NautilusAdapterClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_adapter_destroy;
}

static void
nautilus_adapter_initialize (NautilusAdapter *adapter)
{
	adapter->details = g_new0 (NautilusAdapterDetails, 1);
	gtk_object_ref (GTK_OBJECT (adapter));
	gtk_object_sink (GTK_OBJECT (adapter));
}

static void
nautilus_adapter_destroy (GtkObject *object)
{
	NautilusAdapter *adapter;
	
	adapter = NAUTILUS_ADAPTER (object);

	gtk_signal_disconnect (GTK_OBJECT (adapter->details->load_strategy),
			       adapter->details->report_load_underway_id);
	gtk_signal_disconnect (GTK_OBJECT (adapter->details->load_strategy),
			       adapter->details->report_load_progress_id);
	gtk_signal_disconnect (GTK_OBJECT (adapter->details->load_strategy),
			       adapter->details->report_load_complete_id);
	gtk_signal_disconnect (GTK_OBJECT (adapter->details->load_strategy),
			       adapter->details->report_load_failed_id);

	if (adapter->details->embed_strategy != NULL) {
		nautilus_adapter_embed_strategy_deactivate (adapter->details->embed_strategy);
	}

	if (adapter->details->load_strategy != NULL) {
		nautilus_adapter_load_strategy_stop_loading (adapter->details->load_strategy);
		gtk_object_unref (GTK_OBJECT (adapter->details->load_strategy));
	}

	if (adapter->details->embed_strategy != NULL) {
		gtk_object_unref (GTK_OBJECT (adapter->details->embed_strategy));
	}

	g_free (adapter->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusAdapter *
nautilus_adapter_new (Bonobo_Unknown component)
{
	NautilusAdapter      *adapter;
	BonoboControl        *control;
	GtkWidget            *bin;
	BonoboObject         *zoomable;

	/* FIXME bugzilla.gnome.org 44405: should be done with
	 * construct args 
	 */

	adapter = NAUTILUS_ADAPTER (gtk_object_new (NAUTILUS_TYPE_ADAPTER, NULL));

	/* Set up a few wrapper framework details */
	bin = gtk_widget_new (EEL_TYPE_GENEROUS_BIN, NULL);
	gtk_widget_show (bin);
	control = bonobo_control_new (bin);
	adapter->details->nautilus_view = nautilus_view_new_from_bonobo_control (control);

	gtk_signal_connect_object (GTK_OBJECT (adapter->details->nautilus_view),
				   "destroy",
				   gtk_object_unref,
				   GTK_OBJECT (adapter));

	/* Get the class to handle embedding this kind of component. */
	adapter->details->embed_strategy = nautilus_adapter_embed_strategy_get (component);
	if (adapter->details->embed_strategy == NULL) {
		gtk_object_unref (GTK_OBJECT (adapter));
		return NULL;
	}

	/* Get the NautilusAdapterZoomable proxy object. */
	zoomable = nautilus_adapter_embed_strategy_get_zoomable (adapter->details->embed_strategy);
	if (zoomable != NULL)
		bonobo_object_add_interface (BONOBO_OBJECT (control), zoomable);

	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    GTK_SIGNAL_FUNC (nautilus_adapter_activate_callback),
			    adapter);

	gtk_signal_connect (GTK_OBJECT (adapter->details->embed_strategy), "open_location", 
			    nautilus_adapter_open_location_callback, adapter);


	/* Get the class to handle loading this kind of component. */
	adapter->details->load_strategy = nautilus_adapter_load_strategy_get (component);
	if (adapter->details->load_strategy == NULL) {
		gtk_object_unref (GTK_OBJECT (adapter));
		return NULL;
	}

	/* hook up load strategy signals */
	adapter->details->report_load_underway_id =
		gtk_signal_connect_object (GTK_OBJECT (adapter->details->load_strategy),
					   "report_load_underway",
					   nautilus_adapter_load_underway_callback,
					   GTK_OBJECT (adapter));
	adapter->details->report_load_progress_id =
		gtk_signal_connect_object (GTK_OBJECT (adapter->details->load_strategy),
					   "report_load_progress",
					   nautilus_adapter_load_progress_callback,
					   GTK_OBJECT (adapter));
	adapter->details->report_load_complete_id =
		gtk_signal_connect_object (GTK_OBJECT (adapter->details->load_strategy),
					   "report_load_complete",
					   nautilus_adapter_load_complete_callback,
					   GTK_OBJECT (adapter));
	adapter->details->report_load_failed_id =
		gtk_signal_connect_object (GTK_OBJECT (adapter->details->load_strategy),
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
	nautilus_view_open_location_in_this_window
		(adapter->details->nautilus_view, uri);
}


static void
nautilus_adapter_activate_callback (BonoboControl   *control,
				    gboolean         state,
				    NautilusAdapter *adapter)
{
	g_return_if_fail (control != NULL);
	g_return_if_fail (BONOBO_IS_CONTROL (control));
	g_return_if_fail (adapter != NULL);
	g_return_if_fail (NAUTILUS_IS_ADAPTER (adapter));

	if (state) {
		Bonobo_UIContainer corba_container;

		corba_container = bonobo_control_get_remote_ui_container (control);
		nautilus_adapter_embed_strategy_activate (adapter->details->embed_strategy,
							  corba_container);
	} else
		nautilus_adapter_embed_strategy_deactivate (adapter->details->embed_strategy);
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
