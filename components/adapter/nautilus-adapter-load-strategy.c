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
#include <eel/eel-gtk-macros.h>
#include <eel/eel-gtk-extensions.h>



enum {
	REPORT_LOAD_UNDERWAY,
	REPORT_LOAD_PROGRESS,
	REPORT_LOAD_COMPLETE,
	REPORT_LOAD_FAILED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];



static void nautilus_adapter_load_strategy_initialize_class (NautilusAdapterLoadStrategyClass *klass);
static void nautilus_adapter_load_strategy_initialize       (NautilusAdapterLoadStrategy      *strategy);
static void nautilus_adapter_load_strategy_destroy          (GtkObject                        *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusAdapterLoadStrategy, nautilus_adapter_load_strategy, GTK_TYPE_OBJECT)

EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_adapter_load_strategy, load_location)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_adapter_load_strategy, stop_loading)

static void
nautilus_adapter_load_strategy_initialize_class (NautilusAdapterLoadStrategyClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	object_class->destroy = nautilus_adapter_load_strategy_destroy;

	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_adapter_load_strategy, load_location);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_adapter_load_strategy, stop_loading);


	signals[REPORT_LOAD_UNDERWAY] =
		gtk_signal_new ("report_load_underway",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusAdapterLoadStrategyClass, report_load_underway),
			       gtk_marshal_NONE__NONE,
			       GTK_TYPE_NONE, 0);
	signals[REPORT_LOAD_PROGRESS] =
		gtk_signal_new ("report_load_progress",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusAdapterLoadStrategyClass, report_load_progress),
			       eel_gtk_marshal_NONE__DOUBLE,
			       GTK_TYPE_NONE, 1, GTK_TYPE_DOUBLE);
	signals[REPORT_LOAD_COMPLETE] =
		gtk_signal_new ("report_load_complete",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusAdapterLoadStrategyClass, report_load_complete),
			       gtk_marshal_NONE__NONE,
			       GTK_TYPE_NONE, 0);
	signals[REPORT_LOAD_FAILED] =
		gtk_signal_new ("report_load_failed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (NautilusAdapterLoadStrategyClass, report_load_failed),
			       gtk_marshal_NONE__NONE,
			       GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
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

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}




NautilusAdapterLoadStrategy *
nautilus_adapter_load_strategy_get (Bonobo_Unknown  component)
{
	Bonobo_PersistStream persist_stream;
	Bonobo_PersistFile persist_file;
	Bonobo_ProgressiveDataSink progressive_data_sink;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	progressive_data_sink = Bonobo_Unknown_queryInterface (component,
							       "IDL:Bonobo/ProgressiveDataSink:1.0", &ev);
	

	if (ev._major == CORBA_NO_EXCEPTION && !CORBA_Object_is_nil (progressive_data_sink, &ev)) {
		CORBA_exception_free (&ev);
		
		return nautilus_adapter_progressive_load_strategy_new (progressive_data_sink);
	}


	persist_stream = Bonobo_Unknown_queryInterface (component,
							"IDL:Bonobo/PersistStream:1.0", &ev);
	
	if (ev._major == CORBA_NO_EXCEPTION && !CORBA_Object_is_nil (persist_stream, &ev)) {
		CORBA_exception_free (&ev);
		
		return nautilus_adapter_stream_load_strategy_new (persist_stream);		
	}


	persist_file = Bonobo_Unknown_queryInterface (component,
						      "IDL:Bonobo/PersistFile:1.0", &ev);
	
	if (ev._major == CORBA_NO_EXCEPTION && !CORBA_Object_is_nil (persist_file, &ev)) {
		CORBA_exception_free (&ev);
		
		return nautilus_adapter_file_load_strategy_new (persist_file);		
	}

	CORBA_exception_free (&ev);

	return NULL;
}


void
nautilus_adapter_load_strategy_load_location (NautilusAdapterLoadStrategy *strategy,
					      const char                  *uri)
{
	g_return_if_fail (NAUTILUS_IS_ADAPTER_LOAD_STRATEGY (strategy));

	EEL_CALL_METHOD (NAUTILUS_ADAPTER_LOAD_STRATEGY_CLASS, strategy,
			      load_location, (strategy, uri));
}

void
nautilus_adapter_load_strategy_stop_loading  (NautilusAdapterLoadStrategy *strategy)
{

	g_return_if_fail (NAUTILUS_IS_ADAPTER_LOAD_STRATEGY (strategy));

	EEL_CALL_METHOD (NAUTILUS_ADAPTER_LOAD_STRATEGY_CLASS, strategy,
			      stop_loading, (strategy));
}


/* "protected" calls, should only be called by subclasses */

void
nautilus_adapter_load_strategy_report_load_underway  (NautilusAdapterLoadStrategy *strategy)
{
	gtk_signal_emit (GTK_OBJECT (strategy),
			 signals[REPORT_LOAD_UNDERWAY]);
}

void
nautilus_adapter_load_strategy_report_load_progress  (NautilusAdapterLoadStrategy *strategy,
						      double                       fraction_done)
{
	gtk_signal_emit (GTK_OBJECT (strategy),
			 signals[REPORT_LOAD_PROGRESS],
			 fraction_done);
}

void
nautilus_adapter_load_strategy_report_load_complete  (NautilusAdapterLoadStrategy *strategy)
{
	gtk_signal_emit (GTK_OBJECT (strategy),
			 signals[REPORT_LOAD_COMPLETE]);
}

void
nautilus_adapter_load_strategy_report_load_failed    (NautilusAdapterLoadStrategy *strategy)
{
	gtk_signal_emit (GTK_OBJECT (strategy),
			 signals[REPORT_LOAD_FAILED]);
}
