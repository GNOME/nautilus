/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
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

#include "nautilus-adapter-control-embed-strategy.h"
#include "nautilus-adapter-embed-strategy-private.h"
#include "nautilus-adapter-embeddable-embed-strategy.h"
#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>
#include <eel/eel-gtk-macros.h>
#include <stdio.h>

enum {
	ACTIVATE,
	DEACTIVATE,
	OPEN_LOCATION,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void nautilus_adapter_embed_strategy_initialize_class (NautilusAdapterEmbedStrategyClass *klass);
static void nautilus_adapter_embed_strategy_initialize       (NautilusAdapterEmbedStrategy      *strategy);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusAdapterEmbedStrategy,
				   nautilus_adapter_embed_strategy,
				   GTK_TYPE_OBJECT)

EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_adapter_embed_strategy, get_widget)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_adapter_embed_strategy, get_zoomable)

static void
nautilus_adapter_embed_strategy_initialize_class (NautilusAdapterEmbedStrategyClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	signals[ACTIVATE] =
		gtk_signal_new ("activate",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusAdapterEmbedStrategyClass, activate),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_POINTER, 0);
	signals[DEACTIVATE] =
		gtk_signal_new ("deactivate",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusAdapterEmbedStrategyClass, deactivate),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[OPEN_LOCATION] =
		gtk_signal_new ("open_location",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusAdapterEmbedStrategyClass, open_location),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_STRING, 0);
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
	
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_adapter_embed_strategy, get_widget);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_adapter_embed_strategy, get_zoomable);
}

static void
nautilus_adapter_embed_strategy_initialize (NautilusAdapterEmbedStrategy *strategy)
{
}

NautilusAdapterEmbedStrategy *
nautilus_adapter_embed_strategy_get (Bonobo_Unknown component)
{
	Bonobo_Control    control;
	Bonobo_Embeddable embeddable;
	CORBA_Environment ev;
	NautilusAdapterEmbedStrategy *strategy;

	CORBA_exception_init (&ev);

	strategy = NULL;

	control = Bonobo_Unknown_queryInterface
		(component, "IDL:Bonobo/Control:1.0", &ev);
	if (ev._major == CORBA_NO_EXCEPTION && !CORBA_Object_is_nil (control, &ev)) {
		strategy = nautilus_adapter_control_embed_strategy_new
			(control, CORBA_OBJECT_NIL);
		bonobo_object_release_unref (control, NULL);
	}
	
	if (strategy != NULL) {
		embeddable = Bonobo_Unknown_queryInterface
			(component, "IDL:Bonobo/Embeddable:1.0", &ev);
		if (ev._major == CORBA_NO_EXCEPTION && !CORBA_Object_is_nil (embeddable, &ev)) {
			strategy = nautilus_adapter_embeddable_embed_strategy_new
				(embeddable, CORBA_OBJECT_NIL);
			bonobo_object_release_unref (embeddable, NULL);
		}
	}

	CORBA_exception_free (&ev);

	return strategy;
}

GtkWidget *
nautilus_adapter_embed_strategy_get_widget (NautilusAdapterEmbedStrategy *strategy)
{
	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_ADAPTER_EMBED_STRATEGY_CLASS, strategy,
		 get_widget, (strategy));
}

BonoboObject *
nautilus_adapter_embed_strategy_get_zoomable (NautilusAdapterEmbedStrategy *strategy)
{
	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_ADAPTER_EMBED_STRATEGY_CLASS, strategy,
		 get_zoomable, (strategy));
}


void 
nautilus_adapter_embed_strategy_activate (NautilusAdapterEmbedStrategy *strategy,
					  Bonobo_UIContainer            ui_container)
{
	gtk_signal_emit (GTK_OBJECT (strategy),
			 signals[ACTIVATE],
			 ui_container);
}

void 
nautilus_adapter_embed_strategy_deactivate (NautilusAdapterEmbedStrategy *strategy)
{
	gtk_signal_emit (GTK_OBJECT (strategy),
			 signals[DEACTIVATE]);
}

void 
nautilus_adapter_embed_strategy_emit_open_location (NautilusAdapterEmbedStrategy *strategy,
						    const char                   *uri)
{
	gtk_signal_emit (GTK_OBJECT (strategy),
			 signals[OPEN_LOCATION],
			 uri);
}
