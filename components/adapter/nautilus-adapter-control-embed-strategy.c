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


/* nautilus-adapter-control-embed-strategy.c - 
 */


#include <config.h>

#include "nautilus-adapter-control-embed-strategy.h"
#include "nautilus-adapter-embed-strategy-private.h"
#include "nautilus-zoomable-proxy.h"

#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>
#include <eel/eel-gtk-macros.h>
#include <bonobo/bonobo-control.h>


struct NautilusAdapterControlEmbedStrategyDetails {
	BonoboControlFrame *control_frame;
	BonoboObject       *zoomable;
	GtkWidget          *widget;
};


static void nautilus_adapter_control_embed_strategy_initialize_class (NautilusAdapterControlEmbedStrategyClass *klass);
static void nautilus_adapter_control_embed_strategy_initialize       (NautilusAdapterControlEmbedStrategy      *strategy);
static void nautilus_adapter_control_embed_strategy_activate         (NautilusAdapterEmbedStrategy           *object,
								      gpointer                                ui_container);
static void nautilus_adapter_control_embed_strategy_deactivate       (NautilusAdapterEmbedStrategy           *object);
static void nautilus_adapter_control_embed_strategy_destroy          (GtkObject                              *object);

static GtkWidget *nautilus_adapter_control_embed_strategy_get_widget (NautilusAdapterEmbedStrategy *strategy);
static BonoboObject *nautilus_adapter_control_embed_strategy_get_zoomable (NautilusAdapterEmbedStrategy *strategy);


EEL_DEFINE_CLASS_BOILERPLATE (NautilusAdapterControlEmbedStrategy, nautilus_adapter_control_embed_strategy, NAUTILUS_TYPE_ADAPTER_EMBED_STRATEGY)


static void
nautilus_adapter_control_embed_strategy_initialize_class (NautilusAdapterControlEmbedStrategyClass *klass)
{
	GtkObjectClass                    *object_class;
	NautilusAdapterEmbedStrategyClass *adapter_embed_strategy_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = nautilus_adapter_control_embed_strategy_destroy;

	adapter_embed_strategy_class = NAUTILUS_ADAPTER_EMBED_STRATEGY_CLASS (klass);

	adapter_embed_strategy_class->get_widget = nautilus_adapter_control_embed_strategy_get_widget;
	adapter_embed_strategy_class->get_zoomable = nautilus_adapter_control_embed_strategy_get_zoomable;
	adapter_embed_strategy_class->activate = nautilus_adapter_control_embed_strategy_activate;
	adapter_embed_strategy_class->deactivate = nautilus_adapter_control_embed_strategy_deactivate;
}

static void
nautilus_adapter_control_embed_strategy_initialize (NautilusAdapterControlEmbedStrategy *strategy)
{
	strategy->details = g_new0 (NautilusAdapterControlEmbedStrategyDetails, 1);
}

static void
nautilus_adapter_control_embed_strategy_destroy (GtkObject *object)
{
	NautilusAdapterControlEmbedStrategy *strategy;

	strategy = NAUTILUS_ADAPTER_CONTROL_EMBED_STRATEGY (object);

	if (strategy->details->control_frame != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (strategy->details->control_frame));
	}

	g_free (strategy->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void 
nautilus_adapter_control_embed_strategy_activate (NautilusAdapterEmbedStrategy *object,
						  gpointer                      ui_container)
{
	NautilusAdapterControlEmbedStrategy *strategy;
	Bonobo_UIContainer corba_container = ui_container;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_ADAPTER_CONTROL_EMBED_STRATEGY (object));

	strategy = NAUTILUS_ADAPTER_CONTROL_EMBED_STRATEGY (object);

	bonobo_control_frame_set_ui_container (strategy->details->control_frame,
					       corba_container);

	bonobo_control_frame_control_activate (strategy->details->control_frame);
}

static void 
nautilus_adapter_control_embed_strategy_deactivate (NautilusAdapterEmbedStrategy *object)
{
	NautilusAdapterControlEmbedStrategy *strategy;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_ADAPTER_CONTROL_EMBED_STRATEGY (object));

	strategy = NAUTILUS_ADAPTER_CONTROL_EMBED_STRATEGY (object);

	/* This is not strictly necessary, but it makes sure that the component's menus
	 * and toolbars are really unmerged even if our component is badly behaving or
	 * leaking a reference somewhere. */
	bonobo_control_frame_control_deactivate (strategy->details->control_frame);
}

static void
activate_uri_callback (BonoboControlFrame *frame,
		       const char *uri, 
		       gboolean relative,
		       NautilusAdapterControlEmbedStrategy *strategy)
{
	/* FIXME bugzilla.gnome.org 44404: ignoring `relative' parameter as
	 * the concept is kind of broken. 
	 */
	nautilus_adapter_embed_strategy_emit_open_location (NAUTILUS_ADAPTER_EMBED_STRATEGY (strategy), 
							    uri);
}

NautilusAdapterEmbedStrategy *
nautilus_adapter_control_embed_strategy_new (Bonobo_Control control,
					     Bonobo_UIContainer ui_container)
{
	NautilusAdapterControlEmbedStrategy *strategy;
	Bonobo_Zoomable corba_zoomable;
	CORBA_Environment ev;

	strategy = NAUTILUS_ADAPTER_CONTROL_EMBED_STRATEGY (gtk_object_new (NAUTILUS_TYPE_ADAPTER_CONTROL_EMBED_STRATEGY, NULL));
	gtk_object_ref (GTK_OBJECT (strategy));
	gtk_object_sink (GTK_OBJECT (strategy));

	strategy->details->control_frame = bonobo_control_frame_new (ui_container);

	bonobo_control_frame_bind_to_control (strategy->details->control_frame, control);

	strategy->details->widget = bonobo_control_frame_get_widget (strategy->details->control_frame);
  
	gtk_signal_connect (GTK_OBJECT (strategy->details->control_frame),
			    "activate_uri", GTK_SIGNAL_FUNC (activate_uri_callback), strategy);

	CORBA_exception_init (&ev);
	corba_zoomable = Bonobo_Unknown_queryInterface (control,
							"IDL:Bonobo/Zoomable:1.0",
							&ev);
	if ((ev._major == CORBA_NO_EXCEPTION) &&
	    !CORBA_Object_is_nil (corba_zoomable, &ev)) {
		strategy->details->zoomable = nautilus_zoomable_proxy_get
			(corba_zoomable);
		bonobo_object_release_unref (corba_zoomable, &ev);
	}

	CORBA_exception_free (&ev);

	return NAUTILUS_ADAPTER_EMBED_STRATEGY (strategy);
}

static GtkWidget *
nautilus_adapter_control_embed_strategy_get_widget (NautilusAdapterEmbedStrategy *abstract_strategy)
{
	NautilusAdapterControlEmbedStrategy *strategy;

	strategy = NAUTILUS_ADAPTER_CONTROL_EMBED_STRATEGY (abstract_strategy);

	return strategy->details->widget;
}

static BonoboObject *
nautilus_adapter_control_embed_strategy_get_zoomable (NautilusAdapterEmbedStrategy *abstract_strategy)
{
	NautilusAdapterControlEmbedStrategy *strategy;

	strategy = NAUTILUS_ADAPTER_CONTROL_EMBED_STRATEGY (abstract_strategy);

	return strategy->details->zoomable;
}


