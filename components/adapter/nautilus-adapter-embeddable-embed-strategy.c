/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */


/* nautilus-adapter-embeddable-embed-strategy.c - 
 */


#include <config.h>

#include "nautilus-adapter-embeddable-embed-strategy.h"
#include "nautilus-adapter-embed-strategy-private.h"
#include "nautilus-zoomable-proxy.h"

#include <bonobo/bonobo-client-site.h>
#include <bonobo/bonobo-item-container.h>
#include <gtk/gtksignal.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus/nautilus-view.h>

struct NautilusAdapterEmbeddableEmbedStrategyDetails {
	BonoboItemContainer *container;
      	BonoboClientSite   *client_site;
	BonoboViewFrame    *view_frame;
	GtkWidget          *client_widget;
	BonoboObject       *zoomable;
};


static void nautilus_adapter_embeddable_embed_strategy_initialize_class (NautilusAdapterEmbeddableEmbedStrategyClass *klass);
static void nautilus_adapter_embeddable_embed_strategy_initialize       (NautilusAdapterEmbeddableEmbedStrategy      *strategy);
static void nautilus_adapter_embeddable_embed_strategy_destroy          (GtkObject                              *object);
static void nautilus_adapter_embeddable_embed_strategy_activate         (NautilusAdapterEmbedStrategy                *object,
									 gpointer                                     ui_container);
static void nautilus_adapter_embeddable_embed_strategy_deactivate       (NautilusAdapterEmbedStrategy                *object);


static GtkWidget *nautilus_adapter_embeddable_embed_strategy_get_widget (NautilusAdapterEmbedStrategy *strategy);
static BonoboObject *nautilus_adapter_embeddable_embed_strategy_get_zoomable (NautilusAdapterEmbedStrategy *strategy);


EEL_DEFINE_CLASS_BOILERPLATE (NautilusAdapterEmbeddableEmbedStrategy, nautilus_adapter_embeddable_embed_strategy, NAUTILUS_TYPE_ADAPTER_EMBED_STRATEGY)


static void
nautilus_adapter_embeddable_embed_strategy_initialize_class (NautilusAdapterEmbeddableEmbedStrategyClass *klass)
{
	GtkObjectClass                    *object_class;
	NautilusAdapterEmbedStrategyClass *adapter_embed_strategy_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = nautilus_adapter_embeddable_embed_strategy_destroy;

	adapter_embed_strategy_class = NAUTILUS_ADAPTER_EMBED_STRATEGY_CLASS (klass);

	adapter_embed_strategy_class->get_widget = nautilus_adapter_embeddable_embed_strategy_get_widget;
	adapter_embed_strategy_class->get_zoomable = nautilus_adapter_embeddable_embed_strategy_get_zoomable;
	adapter_embed_strategy_class->activate = nautilus_adapter_embeddable_embed_strategy_activate;
	adapter_embed_strategy_class->deactivate = nautilus_adapter_embeddable_embed_strategy_deactivate;
}

static void
nautilus_adapter_embeddable_embed_strategy_initialize (NautilusAdapterEmbeddableEmbedStrategy *strategy)
{
	strategy->details = g_new0 (NautilusAdapterEmbeddableEmbedStrategyDetails, 1);
}

static void
nautilus_adapter_embeddable_embed_strategy_destroy (GtkObject *object)
{
	NautilusAdapterEmbeddableEmbedStrategy *strategy;

	strategy = NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY (object);

 	if (strategy->details->view_frame != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (strategy->details->view_frame));
	}
 	if (strategy->details->client_site != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (strategy->details->client_site));
	}
 	if (strategy->details->container != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (strategy->details->container));
	}

	g_free (strategy->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}


static void 
nautilus_adapter_embeddable_embed_strategy_activate (NautilusAdapterEmbedStrategy *object,
						     gpointer                      ui_container)
{
	NautilusAdapterEmbeddableEmbedStrategy *strategy;
	Bonobo_UIContainer corba_container = ui_container;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY (object));

	strategy = NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY (object);

	bonobo_control_frame_set_ui_container (BONOBO_CONTROL_FRAME (strategy->details->view_frame),
					       corba_container);

	bonobo_control_frame_control_activate (BONOBO_CONTROL_FRAME (strategy->details->view_frame));
}

static void 
nautilus_adapter_embeddable_embed_strategy_deactivate (NautilusAdapterEmbedStrategy *object)
{
	NautilusAdapterEmbeddableEmbedStrategy *strategy;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY (object));

	strategy = NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY (object);

	/* This is not strictly necessary, but it makes sure that the component's menus
	 * and toolbars are really unmerged even if our component is badly behaving or
	 * leaking a reference somewhere. */
	bonobo_control_frame_control_deactivate (BONOBO_CONTROL_FRAME (strategy->details->view_frame));
}

static void
activate_uri_callback (BonoboControlFrame *frame,
		       const char *uri, 
		       gboolean relative,
		       NautilusAdapterEmbeddableEmbedStrategy *strategy)
{
	/* FIXME: ignoring `relative' parameter as the concept is kind
           of broken. */

	nautilus_adapter_embed_strategy_emit_open_location (NAUTILUS_ADAPTER_EMBED_STRATEGY (strategy), 
							    uri);
}

NautilusAdapterEmbedStrategy *
nautilus_adapter_embeddable_embed_strategy_new (Bonobo_Embeddable embeddable,
						Bonobo_UIContainer ui_container)
{
	NautilusAdapterEmbeddableEmbedStrategy *strategy;
	BonoboObjectClient *embeddable_wrapper;
	Bonobo_Zoomable corba_zoomable;
	Bonobo_View corba_view;
	CORBA_Environment ev;

	strategy = NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY (gtk_object_new (NAUTILUS_TYPE_ADAPTER_EMBEDDABLE_EMBED_STRATEGY, NULL));
	gtk_object_ref (GTK_OBJECT (strategy));
	gtk_object_sink (GTK_OBJECT (strategy));

	embeddable_wrapper = bonobo_object_client_from_corba
		(bonobo_object_dup_ref (embeddable, NULL));

	strategy->details->container = bonobo_item_container_new ();
      	strategy->details->client_site = bonobo_client_site_new (strategy->details->container);

	bonobo_client_site_bind_embeddable (strategy->details->client_site, embeddable_wrapper);

	bonobo_object_unref (BONOBO_OBJECT (embeddable_wrapper));

	strategy->details->view_frame = bonobo_client_site_new_view (strategy->details->client_site, ui_container);
	strategy->details->client_widget = bonobo_view_frame_get_wrapper (strategy->details->view_frame);

	CORBA_exception_init (&ev);
	corba_view = bonobo_view_frame_get_view (strategy->details->view_frame);

	corba_zoomable = Bonobo_Unknown_queryInterface (corba_view,
							"IDL:Bonobo/Zoomable:1.0",
							&ev);
	if ((ev._major == CORBA_NO_EXCEPTION) &&
	    !CORBA_Object_is_nil (corba_zoomable, &ev)) {
		strategy->details->zoomable = nautilus_zoomable_proxy_get
			(corba_zoomable);
	}

	CORBA_exception_free (&ev);


     	bonobo_wrapper_set_visibility (BONOBO_WRAPPER (strategy->details->client_widget), FALSE);
	bonobo_view_frame_set_covered (strategy->details->view_frame, FALSE); 

	gtk_signal_connect (GTK_OBJECT (strategy->details->view_frame),
			    "activate_uri", GTK_SIGNAL_FUNC (activate_uri_callback), strategy);

	gtk_widget_show (strategy->details->client_widget);

	return NAUTILUS_ADAPTER_EMBED_STRATEGY (strategy);
}

static GtkWidget *
nautilus_adapter_embeddable_embed_strategy_get_widget (NautilusAdapterEmbedStrategy *abstract_strategy)
{
	NautilusAdapterEmbeddableEmbedStrategy *strategy;

	strategy = NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY (abstract_strategy);
	return strategy->details->client_widget;
}

static BonoboObject *
nautilus_adapter_embeddable_embed_strategy_get_zoomable (NautilusAdapterEmbedStrategy *abstract_strategy)
{
	NautilusAdapterEmbeddableEmbedStrategy *strategy;

	strategy = NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY (abstract_strategy);

	return strategy->details->zoomable;
}


