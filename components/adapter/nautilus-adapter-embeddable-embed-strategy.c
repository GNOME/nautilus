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

#include <bonobo/bonobo-client-site.h>
#include <bonobo/bonobo-item-container.h>
#include <gtk/gtksignal.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-view.h>

struct NautilusAdapterEmbeddableEmbedStrategyDetails {
	BonoboObjectClient *embeddable_wrapper;
	BonoboItemContainer *container;
      	BonoboClientSite   *client_site;
	BonoboViewFrame    *view_frame;
	GtkWidget          *client_widget;
};


static void nautilus_adapter_embeddable_embed_strategy_initialize_class (NautilusAdapterEmbeddableEmbedStrategyClass *klass);
static void nautilus_adapter_embeddable_embed_strategy_initialize       (NautilusAdapterEmbeddableEmbedStrategy      *strategy);
static void nautilus_adapter_embeddable_embed_strategy_destroy          (GtkObject                              *object);

static GtkWidget *nautilus_adapter_embeddable_embed_strategy_get_widget (NautilusAdapterEmbedStrategy *strategy);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusAdapterEmbeddableEmbedStrategy, nautilus_adapter_embeddable_embed_strategy, NAUTILUS_TYPE_ADAPTER_EMBED_STRATEGY)


static void
nautilus_adapter_embeddable_embed_strategy_initialize_class (NautilusAdapterEmbeddableEmbedStrategyClass *klass)
{
	GtkObjectClass                    *object_class;
	NautilusAdapterEmbedStrategyClass *adapter_embed_strategy_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = nautilus_adapter_embeddable_embed_strategy_destroy;

	adapter_embed_strategy_class = NAUTILUS_ADAPTER_EMBED_STRATEGY_CLASS (klass);

	adapter_embed_strategy_class->get_widget = nautilus_adapter_embeddable_embed_strategy_get_widget;
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

 	if (strategy->details->embeddable_wrapper != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (strategy->details->embeddable_wrapper));
	}
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

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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

	strategy = NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY (gtk_object_new (NAUTILUS_TYPE_ADAPTER_EMBEDDABLE_EMBED_STRATEGY, NULL));
	gtk_object_ref (GTK_OBJECT (strategy));
	gtk_object_sink (GTK_OBJECT (strategy));

	strategy->details->embeddable_wrapper = bonobo_object_client_from_corba (embeddable);

	strategy->details->container = bonobo_item_container_new ();
      	strategy->details->client_site = bonobo_client_site_new (strategy->details->container);

	bonobo_client_site_bind_embeddable (strategy->details->client_site, strategy->details->embeddable_wrapper);
	bonobo_item_container_add (strategy->details->container, BONOBO_OBJECT (strategy->details->client_site));

	strategy->details->view_frame = bonobo_client_site_new_view (strategy->details->client_site, ui_container);
	strategy->details->client_widget = bonobo_view_frame_get_wrapper (strategy->details->view_frame);

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



