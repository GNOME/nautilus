/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 2000, 2001 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Darin Adler <darin@eazel.com>
 *
 */

#include <config.h>
#include "nautilus-bonobo-workarounds.h"

#include <bonobo/bonobo-stream.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

/* FIXME bugzilla.eazel.com 2456: Is a hard-coded 20 seconds wait to
 * detect that a remote object's process is hung acceptable? Can a
 * component that is working still take 20 seconds to respond?
 */
/* in milliseconds */
#define REMOTE_CHECK_TIME_INTERVAL 20000
#define REMOTE_CHECK_DATA_KEY "nautilus-bonobo-workarounds/RemoteCheckData"

typedef struct {
	BonoboObject *object;
	Bonobo_Unknown remote_object;
	guint timeout_id;
	guint destroy_handler_id;
	NautilusBonoboObjectCallback function;
	gpointer callback_data;
} RemoteCheckData;

typedef struct {
	BonoboObject *object;
	guint idle_id;
	guint destroy_handler_id;
} IdleDestroyData;

POA_Bonobo_Unknown__epv *
nautilus_bonobo_object_get_epv (void)
{
	static POA_Bonobo_Unknown__epv bonobo_object_epv;
	static gboolean set_up;
	POA_Bonobo_Unknown__epv *epv;

	/* Make our own copy. */
 	if (!set_up) {
		epv = bonobo_object_get_epv ();
		bonobo_object_epv = *epv;
		g_free (epv);
		set_up = TRUE;
	}

	return &bonobo_object_epv;
}

POA_Bonobo_Stream__epv *
nautilus_bonobo_stream_get_epv (void)
{
	static POA_Bonobo_Stream__epv bonobo_stream_epv;
	static gboolean set_up;
	POA_Bonobo_Stream__epv *epv;

	/* Make our own copy. */
 	if (!set_up) {
		epv = bonobo_stream_get_epv ();
		bonobo_stream_epv = *epv;
		g_free (epv);
		set_up = TRUE;
	}

	return &bonobo_stream_epv;
}

static void
set_gone_flag (GtkObject *object,
	       gpointer callback_data)
{
	gboolean *gone_flag;

	gone_flag = callback_data;
	*gone_flag = TRUE;
}

/* The following is the most evil function in the world.  But on the
 * other hand, it works and prevents us from having tons of lingering
 * processes when Nautilus crashes.
 */
void
nautilus_bonobo_object_force_destroy (BonoboObject *object)
{
	gboolean gone;

	if (object == NULL) {
		return;
	}

	g_return_if_fail (BONOBO_IS_OBJECT (object));
	g_return_if_fail (!GTK_OBJECT_DESTROYED (object));

	gone = FALSE;
	gtk_signal_connect (GTK_OBJECT (object), "destroy",
			    set_gone_flag, &gone);
	do {
		bonobo_object_unref (object);
	} while (!gone);
}

static gboolean
destroy_at_idle_callback (gpointer callback_data)
{
	IdleDestroyData *data;

	data = callback_data;
	g_assert (BONOBO_IS_OBJECT (data->object));

	gtk_signal_disconnect (GTK_OBJECT (data->object),
			       data->destroy_handler_id);
	nautilus_bonobo_object_force_destroy (data->object);
	g_free (data);
	return FALSE;
}

static void
destroyed_before_idle_callback (GtkObject *object,
				gpointer callback_data)
{
	IdleDestroyData *data;

	data = callback_data;
	g_assert (data->object == BONOBO_OBJECT (object));

	gtk_idle_remove (data->idle_id);
	g_free (data);
}

void
nautilus_bonobo_object_force_destroy_at_idle (BonoboObject *object)
{
	IdleDestroyData *data;

	if (object == NULL) {
		return;
	}

	data = g_new (IdleDestroyData, 1);
	data->object = object;
	data->idle_id = gtk_idle_add
		(destroy_at_idle_callback, data);
	data->destroy_handler_id = gtk_signal_connect
		(GTK_OBJECT (object), "destroy",
		 destroyed_before_idle_callback, data);
}

/* Same as bonobo_unknown_ping, but this one works. */
static gboolean
object_is_gone (Bonobo_Unknown object)
{
	CORBA_Environment ev;
	gboolean gone;

	CORBA_exception_init (&ev);
	gone = CORBA_Object_non_existent (object, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		gone = TRUE;
	}
	CORBA_exception_free (&ev);
	
	return gone;
}

static void
remote_check_data_free (RemoteCheckData *data)
{
	CORBA_Environment ev;

	if (data == NULL) {
		return;
	}

	gtk_object_remove_data (GTK_OBJECT (data->object), REMOTE_CHECK_DATA_KEY);

	CORBA_exception_init (&ev);
	CORBA_Object_release (data->remote_object, &ev);
	CORBA_exception_free (&ev);

	if (data->timeout_id != 0) {
		g_source_remove (data->timeout_id);
	}

	if (data->destroy_handler_id != 0) {
		gtk_signal_disconnect (GTK_OBJECT (data->object),
				       data->destroy_handler_id);
	}

	g_free (data);
}

static gboolean
remote_check_timed_callback (gpointer callback_data)
{
	RemoteCheckData *data;
	BonoboObject *object;
	NautilusBonoboObjectCallback function;
	gpointer function_data;

	data = callback_data;
	g_assert (BONOBO_IS_OBJECT (data->object));
	g_assert (!GTK_OBJECT_DESTROYED (data->object));
	g_assert (data->remote_object != CORBA_OBJECT_NIL);
	g_assert (data->timeout_id != 0);
	g_assert (data->destroy_handler_id != 0);
	g_assert (data->function != NULL);

	if (!object_is_gone (data->remote_object)) {
		return TRUE;
	}

	object = data->object;
	function = data->function;
	function_data = data->callback_data;

	data->timeout_id = 0;
	remote_check_data_free (data);

	(* function) (object, function_data);

	return FALSE;
}

static void
remote_check_destroy_callback (GtkObject *object,
			       gpointer callback_data)
{
	RemoteCheckData *data;

	g_assert (BONOBO_IS_OBJECT (object));

	data = callback_data;
	g_assert (data->object == BONOBO_OBJECT (object));
	g_assert (data->remote_object != CORBA_OBJECT_NIL);
	g_assert (data->timeout_id != 0);
	g_assert (data->destroy_handler_id != 0);
	g_assert (data->function != NULL);

	remote_check_data_free (data);
}

void
nautilus_bonobo_object_call_when_remote_object_disappears (BonoboObject *object,
							   Bonobo_Unknown remote_object,
							   NautilusBonoboObjectCallback function,
							   gpointer callback_data)
{
	RemoteCheckData *data;
	CORBA_Environment ev;

	g_return_if_fail (BONOBO_IS_OBJECT (object));

	data = gtk_object_get_data (GTK_OBJECT (object), REMOTE_CHECK_DATA_KEY);

	if (GTK_OBJECT_DESTROYED (object)
	    || remote_object == CORBA_OBJECT_NIL
	    || function == NULL) {
		remote_check_data_free (data);
		return;
	}

	if (data == NULL) {
		data = g_new0 (RemoteCheckData, 1);
		data->object = object;
	}
	CORBA_exception_init (&ev);
	data->remote_object = CORBA_Object_duplicate (remote_object, &ev);
	CORBA_exception_free (&ev);
	data->function = function;
	data->callback_data = callback_data;

	if (data->timeout_id == 0) {
		data->timeout_id = g_timeout_add
			(REMOTE_CHECK_TIME_INTERVAL,
			 remote_check_timed_callback, data);
	}
	if (data->destroy_handler_id == 0) {
		data->destroy_handler_id = gtk_signal_connect
			(GTK_OBJECT (object), "destroy",
			 remote_check_destroy_callback, data);
	}

	gtk_object_set_data (GTK_OBJECT (object), REMOTE_CHECK_DATA_KEY, data);
}

static void
force_destroy_cover (BonoboObject *object,
		     gpointer callback_data)
{
	nautilus_bonobo_object_force_destroy (object);
}

void
nautilus_bonobo_object_force_destroy_when_owner_disappears (BonoboObject *object,
							    Bonobo_Unknown owner)
{
	nautilus_bonobo_object_call_when_remote_object_disappears
		(object, owner, force_destroy_cover, NULL);
}
