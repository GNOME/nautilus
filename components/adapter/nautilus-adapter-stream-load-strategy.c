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


/* nautilus-adapter-stream-load-strategy.c - 
 */


#include <config.h>

#include "nautilus-adapter-stream-load-strategy.h"
#include "bonobo-stream-vfs.h"

#include <gtk/gtkobject.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus/nautilus-view.h>

struct NautilusAdapterStreamLoadStrategyDetails {
	Bonobo_PersistStream  persist_stream;
};


static void nautilus_adapter_stream_load_strategy_initialize_class (NautilusAdapterStreamLoadStrategyClass *klass);
static void nautilus_adapter_stream_load_strategy_initialize       (NautilusAdapterStreamLoadStrategy      *strategy);
static void nautilus_adapter_stream_load_strategy_destroy          (GtkObject                              *object);

static void nautilus_adapter_stream_load_strategy_load_location (NautilusAdapterLoadStrategy *strategy,
								 const char                  *uri);

static void nautilus_adapter_stream_load_strategy_stop_loading  (NautilusAdapterLoadStrategy *strategy);


EEL_DEFINE_CLASS_BOILERPLATE (NautilusAdapterStreamLoadStrategy, nautilus_adapter_stream_load_strategy, NAUTILUS_TYPE_ADAPTER_LOAD_STRATEGY)


static void
nautilus_adapter_stream_load_strategy_initialize_class (NautilusAdapterStreamLoadStrategyClass *klass)
{
	GtkObjectClass                   *object_class;
	NautilusAdapterLoadStrategyClass *adapter_load_strategy_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = nautilus_adapter_stream_load_strategy_destroy;

	adapter_load_strategy_class = NAUTILUS_ADAPTER_LOAD_STRATEGY_CLASS (klass);

	adapter_load_strategy_class->load_location = nautilus_adapter_stream_load_strategy_load_location;
	adapter_load_strategy_class->stop_loading = nautilus_adapter_stream_load_strategy_stop_loading;
}

static void
nautilus_adapter_stream_load_strategy_initialize (NautilusAdapterStreamLoadStrategy *strategy)
{
	strategy->details = g_new0 (NautilusAdapterStreamLoadStrategyDetails, 1);
}

static void
nautilus_adapter_stream_load_strategy_destroy (GtkObject *object)
{
	NautilusAdapterStreamLoadStrategy *strategy;
	CORBA_Environment ev;

	strategy = NAUTILUS_ADAPTER_STREAM_LOAD_STRATEGY (object);

	if (strategy->details->persist_stream != CORBA_OBJECT_NIL) {
		CORBA_exception_init (&ev);
		bonobo_object_release_unref (strategy->details->persist_stream, &ev);
		CORBA_exception_free (&ev);
	}

	g_free (strategy->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}


NautilusAdapterLoadStrategy *
nautilus_adapter_stream_load_strategy_new (Bonobo_PersistStream  persist_stream)
{
	NautilusAdapterStreamLoadStrategy *strategy;

	strategy = NAUTILUS_ADAPTER_STREAM_LOAD_STRATEGY (gtk_object_new (NAUTILUS_TYPE_ADAPTER_STREAM_LOAD_STRATEGY, NULL));
	gtk_object_ref (GTK_OBJECT (strategy));
	gtk_object_sink (GTK_OBJECT (strategy));

	strategy->details->persist_stream = persist_stream;

	return NAUTILUS_ADAPTER_LOAD_STRATEGY (strategy);
}


static void
nautilus_adapter_stream_load_strategy_load_location (NautilusAdapterLoadStrategy *abstract_strategy,
						     const char                  *uri)
{
	NautilusAdapterStreamLoadStrategy *strategy;
	BonoboStream *stream;
	CORBA_Environment ev;

	strategy = NAUTILUS_ADAPTER_STREAM_LOAD_STRATEGY (abstract_strategy);
	gtk_object_ref (GTK_OBJECT (strategy));

	CORBA_exception_init (&ev);

	nautilus_adapter_load_strategy_report_load_underway (abstract_strategy);
	
	stream = bonobo_stream_vfs_open (uri, Bonobo_Storage_READ);

	if (stream == NULL) {
		nautilus_adapter_load_strategy_report_load_failed (abstract_strategy);
	} else {
		/* FIXME bugzilla.gnome.org 41248: 
		 * Dan Winship points out that we should pass the
		 * MIME type here to work with new implementers of
		 * PersistStream that pay attention to the MIME type. It
		 * doesn't matter right now, but we should fix it
		 * eventually. Currently, we don't store the MIME type, but
		 * it should be easy to keep it around and pass it in here.
		 */

		Bonobo_PersistStream_load
			(strategy->details->persist_stream,
			 bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
			 "", /* MIME type of stream */
			 &ev);

		bonobo_object_unref (BONOBO_OBJECT (stream));

		if (ev._major == CORBA_NO_EXCEPTION) {
			nautilus_adapter_load_strategy_report_load_complete (abstract_strategy);
		} else {
			nautilus_adapter_load_strategy_report_load_failed (abstract_strategy);
		}
        }


	gtk_object_unref (GTK_OBJECT (strategy));

	CORBA_exception_free (&ev);
}

static void
nautilus_adapter_stream_load_strategy_stop_loading  (NautilusAdapterLoadStrategy *strategy)
{
	g_return_if_fail (NAUTILUS_IS_ADAPTER_STREAM_LOAD_STRATEGY (strategy));

	/* FIXME bugzilla.gnome.org 43456: is there anything we can do? */
}
