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

#include <string.h>
#include "nautilus-adapter-stream-load-strategy.h"

#include <bonobo/bonobo-stream.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <gtk/gtkobject.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus/nautilus-view.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>

struct NautilusAdapterStreamLoadStrategyDetails {
	Bonobo_PersistStream  persist_stream;
};


static void nautilus_adapter_stream_load_strategy_class_init (NautilusAdapterStreamLoadStrategyClass *klass);
static void nautilus_adapter_stream_load_strategy_init       (NautilusAdapterStreamLoadStrategy      *strategy);
static void nautilus_adapter_stream_load_strategy_destroy          (GtkObject                              *object);

static void nautilus_adapter_stream_load_strategy_load_location (NautilusAdapterLoadStrategy *strategy,
								 const char                  *uri);

static void nautilus_adapter_stream_load_strategy_stop_loading  (NautilusAdapterLoadStrategy *strategy);


EEL_CLASS_BOILERPLATE (NautilusAdapterStreamLoadStrategy, nautilus_adapter_stream_load_strategy, NAUTILUS_TYPE_ADAPTER_LOAD_STRATEGY)


static void
nautilus_adapter_stream_load_strategy_class_init (NautilusAdapterStreamLoadStrategyClass *klass)
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
nautilus_adapter_stream_load_strategy_init (NautilusAdapterStreamLoadStrategy *strategy)
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

	strategy = NAUTILUS_ADAPTER_STREAM_LOAD_STRATEGY (g_object_new (NAUTILUS_TYPE_ADAPTER_STREAM_LOAD_STRATEGY, NULL));
	g_object_ref (strategy);
	gtk_object_sink (GTK_OBJECT (strategy));

	strategy->details->persist_stream = persist_stream;

	return NAUTILUS_ADAPTER_LOAD_STRATEGY (strategy);
}

static ORBit_IMethod *
get_stream_load_method (void)
{
	guint i;
	ORBit_IInterface *iface;
	static ORBit_IMethod *method = NULL;
	
	iface = &Bonobo_PersistStream__iinterface;
	if (!method) {
		for (i = 0; i < iface->methods._length; i++) {
			if (!strcmp ("load", iface->methods._buffer [i].name)) {
				method = &iface->methods._buffer [i];
			}
		}
	}
	g_assert (method);

	return method;
}

static void
unref_stream_cb (gpointer user_data)
{
	bonobo_object_release_unref (user_data, NULL);
}

static void
nautilus_adapter_stream_load_strategy_load_location (NautilusAdapterLoadStrategy *abstract_strategy,
						     const char                  *uri)
{
	NautilusAdapterStreamLoadStrategy *strategy;
	Bonobo_Stream stream;
	CORBA_Environment ev;
	char *moniker_str;
	char *escaped_uri;
	char *mime_type;
	gpointer args[2];

	strategy = NAUTILUS_ADAPTER_STREAM_LOAD_STRATEGY (abstract_strategy);
	g_object_ref (strategy);

	CORBA_exception_init (&ev);

	nautilus_adapter_load_strategy_report_load_underway (abstract_strategy);

	/* We must escape the '!' in the URI here, because it is
	 * used as argument delimiter within monikers.
	 */
	escaped_uri = gnome_vfs_escape_set (uri, "!");
	moniker_str = g_strconcat ("vfs:", escaped_uri, NULL);
	stream = bonobo_get_object (moniker_str, "IDL:Bonobo/Stream:1.0", &ev);
	g_free (moniker_str);
	g_free (escaped_uri);

	if (BONOBO_EX (&ev) || CORBA_Object_is_nil (stream, &ev)) {
		nautilus_adapter_load_strategy_report_load_failed (abstract_strategy);
	} else {
		/* This adds an extra sniffing, which is a bit of a problem.
		 * We think this is ok, since it is relatively cheap and
		 * the adapter isn't used often.  When the adapter is
		 * moved into the nautilus process, this should use the
		 * mime type from the NautilusFile. */
		mime_type = gnome_vfs_get_mime_type (uri);
		args [0] = &stream;
		args [1] = &mime_type;

		nautilus_adapter_load_strategy_load_async (
			abstract_strategy,
			strategy->details->persist_stream,
			get_stream_load_method (),
			args,
			unref_stream_cb,
			stream);

		g_free (mime_type);
        }

	g_object_unref (strategy);

	CORBA_exception_free (&ev);
}

static void
nautilus_adapter_stream_load_strategy_stop_loading  (NautilusAdapterLoadStrategy *strategy)
{
	g_return_if_fail (NAUTILUS_IS_ADAPTER_STREAM_LOAD_STRATEGY (strategy));

	/* FIXME bugzilla.gnome.org 43456: is there anything we can do? */
}
