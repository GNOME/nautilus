/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>

#include <trilobite-service.h>
#include <trilobite-service-public.h>

#include "sample-service.h"
#include "sample-service-public.h"

/*
  These are enums used for the signals binding. 
  LAST_SIGNAL indicates the end.
*/
enum {
	REMEMBER,
	SAY_IT,
	LAST_SIGNAL
};

/* The signal array  and prototypes for default handlers*/

void sample_service_remember (SampleService *service, const char *something);
void sample_service_say_it   (SampleService *service);

static guint sample_service_signals[LAST_SIGNAL] = { 0 };

static GtkObjectClass *sample_service_parent_class;

/*****************************************
  Corba stuff
*****************************************/

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

typedef struct {
	POA_Trilobite_Eazel_Sample poa;
	SampleService *object;
} impl_POA_Sample_Service;

static void
impl_Trilobite_Eazel_Sample_remember(impl_POA_Sample_Service *service,
				     const CORBA_char *something,
				     CORBA_Environment *ev) 
{
	gtk_signal_emit (GTK_OBJECT (service->object), sample_service_signals[REMEMBER], something);
}

static void
impl_Trilobite_Eazel_Sample_say_it(impl_POA_Sample_Service *service,
				   CORBA_Environment *ev) 
{
	gtk_signal_emit (GTK_OBJECT (service->object), sample_service_signals[SAY_IT]);
}

POA_Trilobite_Eazel_Sample__epv* 
sample_service_get_epv() 
{
	POA_Trilobite_Eazel_Sample__epv *epv;

	epv = g_new0 (POA_Trilobite_Eazel_Sample__epv, 1);

	epv->remember         = (gpointer) &impl_Trilobite_Eazel_Sample_remember;
	epv->say_it           = (gpointer) &impl_Trilobite_Eazel_Sample_say_it;
		
	return epv;
};

/*****************************************
  GTK+ object stuff
*****************************************/

void
sample_service_destroy (GtkObject *object)
{
	g_message ("in sample_service_destroy");
}

static void
sample_service_class_initialize (SampleServiceClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->destroy = (void(*)(GtkObject*))sample_service_destroy;

	sample_service_parent_class = gtk_type_class (gtk_object_get_type ());

	klass->servant_vepv = g_new0 (POA_Trilobite_Eazel_Sample__vepv,1);
	((POA_Trilobite_Eazel_Sample__vepv*)klass->servant_vepv)->_base_epv = &base_epv; 
	((POA_Trilobite_Eazel_Sample__vepv*)klass->servant_vepv)->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	((POA_Trilobite_Eazel_Sample__vepv*)klass->servant_vepv)->Trilobite_Eazel_Sample_epv = sample_service_get_epv ();

	sample_service_signals[REMEMBER] = 
		gtk_signal_new ("remember",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (SampleServiceClass, remember),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE,1,GTK_TYPE_POINTER);
	sample_service_signals[SAY_IT] = 
		gtk_signal_new ("say_it",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (SampleServiceClass, say_it),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,0);
	gtk_object_class_add_signals (object_class, sample_service_signals, LAST_SIGNAL);

	klass->remember = sample_service_remember;
	klass->say_it = sample_service_say_it;
}

static Trilobite_Eazel_Sample
sample_service_create_corba_object (BonoboObject *service) {
	impl_POA_Sample_Service *servant;
	CORBA_Environment ev;

	g_assert (service != NULL);
	
	CORBA_exception_init (&ev);
	
	servant = (impl_POA_Sample_Service*)g_new0 (PortableServer_Servant,1);
	((POA_Trilobite_Eazel_Sample*) servant)->vepv = SAMPLE_SERVICE_CLASS ( GTK_OBJECT (service)->klass)->servant_vepv;

	POA_Trilobite_Eazel_Sample__init (servant, &ev);
	ORBIT_OBJECT_KEY (((POA_Trilobite_Eazel_Sample*)servant)->_private)->object = NULL;

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot instantiate Trilobite_Eazel_Sample corba object");
		g_free (servant);
		CORBA_exception_free (&ev);		
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);		

	return (Trilobite_Eazel_Sample) bonobo_object_activate_servant (service, servant);
}

GtkType
sample_service_get_type() {
	static GtkType trilobite_service_type = 0;

	g_message ("into sample_service_get_type"); 

	/* First time it's called ? */
	if (!trilobite_service_type)
	{
		static const GtkTypeInfo trilobite_service_info =
		{
			"TrilobiteEazelSampleService",
			sizeof (TrilobiteService),
			sizeof (TrilobiteServiceClass),
			(GtkClassInitFunc) sample_service_class_initialize,
			(GtkObjectInitFunc) NULL,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		/* Get a unique GtkType */
		trilobite_service_type = gtk_type_unique (bonobo_object_get_type (), &trilobite_service_info);
	}

	return trilobite_service_type;
}

SampleService*
sample_service_new()
{
	SampleService *service;
	Trilobite_Eazel_Sample corba_service;

	g_message ("in sample_service_new");
	
	service = SAMPLE_SERVICE (gtk_type_new (SAMPLE_TYPE_SERVICE));

	corba_service = sample_service_create_corba_object (BONOBO_OBJECT (service));

	if (!bonobo_object_construct (BONOBO_OBJECT (service), corba_service)) {
		g_warning ("bonobo_object_construct failed");
	}
	
	return service;
}

/**************************************************/
/* Signal receivers                               */
/**************************************************/

void 
sample_service_remember (SampleService *service, 
			 const char *something) 
{
	if (service->my_string) g_free (service->my_string);
	service->my_string = g_strdup (something);
}

void 
sample_service_say_it   (SampleService *service)
{
	if (service->my_string) {
		g_message (service->my_string);
	} else {
		g_message ("call remember first");
	}
}
