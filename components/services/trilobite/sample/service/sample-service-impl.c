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

#include <libtrilobite/libtrilobite.h>
#include <libtrilobite/libtrilobite-service.h>

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

/* The signal array, used for building the signal bindings */

static guint sample_service_signals[LAST_SIGNAL] = { 0 };

/* This is the parent class pointer */

static BonoboObjectClass *sample_service_parent_class;

/*****************************************
  Corba stuff
*****************************************/

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

/*
  This is POA_Trilobite_Sample structure we will use,
  as it will let us access the SampleService object in 
  the corba methods
 */
typedef struct {
	POA_Trilobite_Eazel_Sample poa;
	SampleService *object;
} impl_POA_Sample_Service;

/*
  The implementation of 
  void remember (in string something);
 */
static void
impl_Trilobite_Eazel_Sample_remember(impl_POA_Sample_Service *service,
				     const CORBA_char *something,
				     CORBA_Environment *ev) 
{
	/* This does not have to be a signal, it's just done
	   as a signal for the educational purpose.
	   Since service->object is the SampleService object, you
	   could just access the data fields here directly */

	gtk_signal_emit (GTK_OBJECT (service->object), sample_service_signals[REMEMBER], something);
}

/*
  The implementation of
  void say_id();
*/
static void
impl_Trilobite_Eazel_Sample_say_it(impl_POA_Sample_Service *service,
				   CORBA_Environment *ev) 
{
	gtk_signal_emit (GTK_OBJECT (service->object), sample_service_signals[SAY_IT]);
}

/*
  The implementation of 
  void list_it (in string dir);
 */
static void
impl_Trilobite_Eazel_Sample_list_it(impl_POA_Sample_Service *service,
				    const CORBA_char *dir,
				    CORBA_Environment *ev) 
{
	TrilobiteRootHelper *helper;
	GList *args;
	char *tmp;
	int fd;
	FILE *f;

	/* Get the TrilobiteRootHelper object. This datafield is set
	   when the factory object in main.c called trilobite_passwordquery_add_interface */
	helper = gtk_object_get_data (GTK_OBJECT (service->object), "trilobite-root-helper");
	
	/* Create the GList of arguments */
	tmp = g_strdup (dir);
	args = g_list_append (NULL, "-lart");
	args = g_list_append (args, tmp);

	/* Start and run the eazel-helper */
	if (trilobite_root_helper_start (helper) == TRILOBITE_ROOT_HELPER_BAD_PASSWORD) {
		g_warning ("Incorrect password");
		return;
	} else {
		if (trilobite_root_helper_run (helper, TRILOBITE_ROOT_HELPER_RUN_LS, args, &fd) != TRILOBITE_ROOT_HELPER_SUCCESS) {
			g_warning ("trilobite_root_helper failed");
		}
	}
	/* Clean up */
	g_list_free (args);
	g_free (tmp);

	f = fdopen (fd, "r");
	fflush (f);
	tmp = g_new0 (char, 1024);
	while (!feof (f)) {
		fgets (tmp, 1023, f);
		if (feof (f)) {
			break;
		}
		fprintf (stdout, "GNE: %s", tmp);
	}
	fclose (f);
}

/*
  This creates the epv for the object.
  Basically you just have to alloc a structure of the
  appropriate type (POA_Trilobite_Eazel_Sample__epv in 
  this case), and set the pointers for the method implementations.
 */
POA_Trilobite_Eazel_Sample__epv* 
sample_service_get_epv() 
{
	POA_Trilobite_Eazel_Sample__epv *epv;

	epv = g_new0 (POA_Trilobite_Eazel_Sample__epv, 1);

	epv->remember         = (gpointer) &impl_Trilobite_Eazel_Sample_remember;
	epv->say_it           = (gpointer) &impl_Trilobite_Eazel_Sample_say_it;
	epv->list_it          = (gpointer) &impl_Trilobite_Eazel_Sample_list_it;
		
	return epv;
};

/*****************************************
  GTK+ object stuff
*****************************************/

/* This is the object destroyer. It should clean up any
 data allocated by the object, and if possible, call 
the parent destroyer */
void
sample_service_destroy (GtkObject *object)
{
	SampleService *service;

	g_return_if_fail (object != NULL);
	g_return_if_fail (SAMPLE_SERVICE (object));

	service = SAMPLE_SERVICE (object);

	/* Free the objects own crap */
	if (service->my_string) {
		g_free (service->my_string);
	}

	/* Call parents destroy */
	if (GTK_OBJECT_CLASS (sample_service_parent_class)->destroy) {
		GTK_OBJECT_CLASS (sample_service_parent_class)->destroy (object);
	}
}

/*
  This is the sample_service class initializer, see
  GGAD (http://developer.gnome.org/doc/GGAD/sec-classinit.html)
  for more on these
 */
static void
sample_service_class_initialize (SampleServiceClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->destroy = (void(*)(GtkObject*))sample_service_destroy;

	sample_service_parent_class = gtk_type_class (bonobo_object_get_type ());

	/* Here I get allocate and set up the vepv. This ensures that the
	   servant_vepv will hold the proper bindings for the corba object for
	   the sample_service */
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

/*
  _corba_object does all the greasy corba building and whatnot.
 */
static Trilobite_Eazel_Sample
sample_service_create_corba_object (BonoboObject *service) {
	impl_POA_Sample_Service *servant;
	CORBA_Environment ev;

	g_assert (service != NULL);
	
	CORBA_exception_init (&ev);
	
	/* Allocate the POA structure, using our extended version*/
	servant = (impl_POA_Sample_Service*)g_new0 (PortableServer_Servant,1);

	/* Set the vepv to the vepv build in sample_service_class_initialize */
	((POA_Trilobite_Eazel_Sample*) servant)->vepv = SAMPLE_SERVICE_CLASS ( GTK_OBJECT (service)->klass)->servant_vepv;

	/* Call the __init method. This is generated by the IDL compiler and
	   the name of the method depends on the name of your corba object */
	POA_Trilobite_Eazel_Sample__init (servant, &ev);
	
	/* Magic */
	ORBIT_OBJECT_KEY (((POA_Trilobite_Eazel_Sample*)servant)->_private)->object = NULL;

	/* Check to see if things went well */
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot instantiate Trilobite_Eazel_Sample corba object");
		g_free (servant);
		CORBA_exception_free (&ev);		
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);		

	/* Return the bonobo activation of the servant */
	return (Trilobite_Eazel_Sample) bonobo_object_activate_servant (service, servant);
}

/*
  This is the SampleService instance initializer.
  Its responsibility is to create the corba object and 
  build the bonobo_object structures using the corba object.
 */
static void
sample_service_initialize (SampleService *service) {
	Trilobite_Eazel_Sample corba_service;

	g_assert (service != NULL);
	g_assert (SAMPLE_IS_SERVICE (service));

	/* This builds the corba object */
	corba_service = sample_service_create_corba_object (BONOBO_OBJECT (service));

	/* This sets the bonobo structures in service using the corba object */
	if (!bonobo_object_construct (BONOBO_OBJECT (service), corba_service)) {
		g_warning ("bonobo_object_construct failed");
	}	
}

/*
  The  GtkType generator. Again, see GGAD for more 
 */
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
			(GtkObjectInitFunc) sample_service_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		/* Get a unique GtkType */
		trilobite_service_type = gtk_type_unique (bonobo_object_get_type (), &trilobite_service_info);
	}

	return trilobite_service_type;
}

/*
  The _new method simply builds the service
  using gtk_object_new
*/
SampleService*
sample_service_new()
{
	SampleService *service;

	g_message ("in sample_service_new");
	
	service = SAMPLE_SERVICE (gtk_object_new (SAMPLE_TYPE_SERVICE, NULL));
	
	return service;
}

/**************************************************/
/* Signal receivers                               */
/**************************************************/

/* Does stuff ... */
void 
sample_service_remember (SampleService *service, 
			 const char *something) 
{
	if (service->my_string) g_free (service->my_string);
	service->my_string = g_strdup (something);
}

/* Does stuff ... */
void 
sample_service_say_it   (SampleService *service)
{
	if (service->my_string) {
		g_message (service->my_string);
	} else {
		g_message ("call remember first");
	}
}
