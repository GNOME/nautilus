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


/* nautilus-adapter-load-strategy.h
 */


#include <config.h>

#include "nautilus-adapter-load-strategy.h"
#include "nautilus-adapter-stream-load-strategy.h"
#include "nautilus-adapter-file-load-strategy.h"
#include "nautilus-adapter-progressive-load-strategy.h"

#include <gtk/gtkobject.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>


static void nautilus_adapter_load_strategy_initialize_class (NautilusAdapterLoadStrategyClass *klass);
static void nautilus_adapter_load_strategy_initialize       (NautilusAdapterLoadStrategy      *strategy);
static void nautilus_adapter_load_strategy_destroy          (GtkObject                        *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusAdapterLoadStrategy, nautilus_adapter_load_strategy, GTK_TYPE_OBJECT)

NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_adapter_load_strategy, load_location)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_adapter_load_strategy, stop_loading)

static void
nautilus_adapter_load_strategy_initialize_class (NautilusAdapterLoadStrategyClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	object_class->destroy = nautilus_adapter_load_strategy_destroy;

	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_adapter_load_strategy, load_location);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_adapter_load_strategy, stop_loading);
}

static void
nautilus_adapter_load_strategy_initialize (NautilusAdapterLoadStrategy *strategy)
{

}

static void
nautilus_adapter_load_strategy_destroy (GtkObject *object)
{
	NautilusAdapterLoadStrategy *strategy;

	strategy = NAUTILUS_ADAPTER_LOAD_STRATEGY (object);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}




NautilusAdapterLoadStrategy *
nautilus_adapter_load_strategy_get (Bonobo_Unknown  component,
				    NautilusView   *view)
{
	Bonobo_PersistStream persist_stream;
	Bonobo_PersistFile persist_file;
	Bonobo_ProgressiveDataSink progressive_data_sink;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	progressive_data_sink = Bonobo_Unknown_query_interface (component,
								"IDL:Bonobo/ProgressiveDataSink:1.0", &ev);
	

	if (ev._major == CORBA_NO_EXCEPTION && !CORBA_Object_is_nil (progressive_data_sink, &ev)) {
		CORBA_exception_free (&ev);
		
		return nautilus_adapter_progressive_load_strategy_new (progressive_data_sink, view);
	}


	persist_stream = Bonobo_Unknown_query_interface (component,
							 "IDL:Bonobo/PersistStream:1.0", &ev);
	
	if (ev._major == CORBA_NO_EXCEPTION && !CORBA_Object_is_nil (persist_stream, &ev)) {
		CORBA_exception_free (&ev);
		
		return nautilus_adapter_stream_load_strategy_new (persist_stream, view);		
	}


	persist_file = Bonobo_Unknown_query_interface (component,
						       "IDL:Bonobo/PersistFile:1.0", &ev);
	

	if (ev._major == CORBA_NO_EXCEPTION && !CORBA_Object_is_nil (persist_file, &ev)) {
		CORBA_exception_free (&ev);
		
		return nautilus_adapter_file_load_strategy_new (persist_file, view);		
	}


	CORBA_exception_free (&ev);

	return NULL;
}


void
nautilus_adapter_load_strategy_load_location (NautilusAdapterLoadStrategy *strategy,
					      const char                  *uri)
{
	g_return_if_fail (NAUTILUS_IS_ADAPTER_LOAD_STRATEGY (strategy));

	NAUTILUS_CALL_VIRTUAL (NAUTILUS_ADAPTER_LOAD_STRATEGY_CLASS, strategy,
			       load_location, (strategy, uri));
}

void
nautilus_adapter_load_strategy_stop_loading  (NautilusAdapterLoadStrategy *strategy)
{

	g_return_if_fail (NAUTILUS_IS_ADAPTER_LOAD_STRATEGY (strategy));

	NAUTILUS_CALL_VIRTUAL (NAUTILUS_ADAPTER_LOAD_STRATEGY_CLASS, strategy,
			       stop_loading, (strategy));
}
