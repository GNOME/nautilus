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


/* nautilus-adapter-embed-strategy.h
 */


#include <config.h>

#include "nautilus-adapter-embed-strategy.h"
#include "nautilus-adapter-embed-strategy-private.h"
#include "nautilus-adapter-control-embed-strategy.h"
#include "nautilus-adapter-embeddable-embed-strategy.h"

#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <stdio.h>

enum {
	OPEN_LOCATION,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


static void nautilus_adapter_embed_strategy_initialize_class (NautilusAdapterEmbedStrategyClass *klass);
static void nautilus_adapter_embed_strategy_initialize       (NautilusAdapterEmbedStrategy      *strategy);
static void nautilus_adapter_embed_strategy_destroy          (GtkObject                        *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusAdapterEmbedStrategy, nautilus_adapter_embed_strategy, GTK_TYPE_OBJECT)

NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_adapter_embed_strategy, get_widget)

static void
nautilus_adapter_embed_strategy_initialize_class (NautilusAdapterEmbedStrategyClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	object_class->destroy = nautilus_adapter_embed_strategy_destroy;

	signals[OPEN_LOCATION] =
		gtk_signal_new ("open_location",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusAdapterEmbedStrategyClass, open_location),
			       gtk_marshal_NONE__STRING,
			       GTK_TYPE_STRING, 0);
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_adapter_embed_strategy, get_widget);
}

static void
nautilus_adapter_embed_strategy_initialize (NautilusAdapterEmbedStrategy *strategy)
{
}

static void
nautilus_adapter_embed_strategy_destroy (GtkObject *object)
{
	NautilusAdapterEmbedStrategy *strategy;

	strategy = NAUTILUS_ADAPTER_EMBED_STRATEGY (object);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}




NautilusAdapterEmbedStrategy *
nautilus_adapter_embed_strategy_get (Bonobo_Unknown   component,
				     Bonobo_UIHandler uih)
{
	Bonobo_Control    control;
	Bonobo_Embeddable embeddable;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	control = Bonobo_Unknown_query_interface (component,
						  "IDL:Bonobo/Control:1.0", &ev);
	

	if (ev._major == CORBA_NO_EXCEPTION && !CORBA_Object_is_nil (control, &ev)) {
		CORBA_exception_free (&ev);
		
		return nautilus_adapter_control_embed_strategy_new (control, uih);
	}

	embeddable = Bonobo_Unknown_query_interface (component,
						     "IDL:Bonobo/Embeddable:1.0", &ev);
	
	if (ev._major == CORBA_NO_EXCEPTION && !CORBA_Object_is_nil (embeddable, &ev)) {
		CORBA_exception_free (&ev);
		
		return nautilus_adapter_embeddable_embed_strategy_new (embeddable, uih);		
	}

	CORBA_exception_free (&ev);

	return NULL;
}


GtkWidget *
nautilus_adapter_embed_strategy_get_widget (NautilusAdapterEmbedStrategy *strategy)
{
	return NAUTILUS_CALL_VIRTUAL (NAUTILUS_ADAPTER_EMBED_STRATEGY_CLASS, strategy,
				      get_widget, (strategy));
}


void 
nautilus_adapter_embed_strategy_emit_open_location (NautilusAdapterEmbedStrategy *strategy,
						    const char                   *uri)
{
	gtk_signal_emit (GTK_OBJECT (strategy),
			 signals[OPEN_LOCATION],
			 uri);
}

