/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

/* trilobite-service.c: Implementation for the toplevel
   interface for trilobite objects */

#include <config.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include "trilobite-service-public.h"
#include "trilobite-service-private.h"

/*
  These are enums used for the signals binding. 
  LAST_SIGNAL indicates the end.
*/
enum {
	GET_NAME,
	GET_VERSION,
	GET_VENDOR_NAME,
	GET_VENDOR_URL,
	GET_URL,
	GET_ICON_URI,
	DONE,
	LAST_SIGNAL
};

/* The signal array */
static guint trilobite_service_signals[LAST_SIGNAL] = { 0 };

/*****************************************
  Corba stuff
*****************************************/

POA_Trilobite_Service__epv Trilobite_Service_epv =
{
	NULL, /* CORBA internals */
	/* Put pointer to methods here */
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Trilobite_Service__vepv impl_Trilobite_Service_vepv =
{
  &base_epv,
  &Trilobite_Service_epv
};

/*****************************************
  The marshallers
*****************************************/

typedef gpointer (*GtkSignal_POINTER__NONE) (GtkObject * object,
					     gpointer user_data);
static void
gtk_marshal_POINTER__NONE (GtkObject * object,
			   GtkSignalFunc func,
			   gpointer func_data,
			   GtkArg * args)
{
    GtkSignal_POINTER__NONE rfunc;
    gpointer *return_val;
    return_val = GTK_RETLOC_POINTER (args[0]);
    rfunc = (GtkSignal_POINTER__NONE) func;
    *return_val = (*rfunc) (object,
                func_data);
}
					  
/*****************************************
  GTK+ object stuff
*****************************************/

static void
trilobite_service_destroy (GtkObject *object)
{
	TrilobiteService *trilobite;

	g_return_if_fail (object != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (object));

	trilobite = TRILOBITE_SERVICE (object);

	if (TRILOBITE_SERVICE_CLASS (trilobite)->parent_class->destroy != NULL) {
		TRILOBITE_SERVICE_CLASS (trilobite)->parent_class->destroy (object);
	}
}


static void
trilobite_service_class_initialize (TrilobiteServiceClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;
	object_class->destroy = (void(*)(GtkObject*))trilobite_service_destroy;
	/*
	object_class->set_arg = trilobite_service_set_arg;
	object_class->get_arg = trilobite_service_get_arg;
	*/

	klass->parent_class = gtk_type_class (gtk_object_get_type ());
	klass->poa_servant_init = POA_Trilobite_Service__init;
	klass->poa_servant_fini = POA_Trilobite_Service__fini;
	klass->poa_vepv = &impl_Trilobite_Service_vepv;


	trilobite_service_signals[GET_NAME] = 
		gtk_signal_new ("get_name",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (TrilobiteServiceClass,get_name),
				gtk_marshal_POINTER__NONE,
				GTK_TYPE_POINTER,0);
	
	gtk_object_class_add_signals (object_class, trilobite_service_signals,LAST_SIGNAL);
};

static void
trilobite_service_initialize (TrilobiteService *trilobite)
{
	g_return_if_fail (trilobite != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (trilobite));

}

/*
  Returns a GtkType for the trilobiteservice.
  At first call trilobite_service_type is 0, and then
  it is set, so next call will just return the generated
  GtkType.
 */
GtkType
trilobite_service_get_type (void)
{
	static GtkType trilobite_service_type = 0;

	/* First time it's called ? */
	if (!trilobite_service_type)
	{
		static const GtkTypeInfo trilobite_service_info =
		{
			"TrilobiteService",
			sizeof (TrilobiteService),
			sizeof (TrilobiteServiceClass),
			(GtkClassInitFunc) trilobite_service_class_initialize,
			(GtkObjectInitFunc) trilobite_service_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		/* Get a unique GtkType */
		trilobite_service_type = gtk_type_unique (gtk_object_get_type (), &trilobite_service_info);
	}

	return trilobite_service_type;
}

/*
  FIXME: bugzilla.eazel.com 854
  determine whether we want to set the name/version/whatnot in the _new(...) call
  or using _set_name(...) calls.
 */
TrilobiteService*
trilobite_service_new() 
{
	TrilobiteService *service;

	service = TRILOBITE_SERVICE (gtk_object_new (TRILOBITE_TYPE_SERVICE, NULL));

	return service;
}
