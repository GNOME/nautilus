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
#include <bonobo.h>

#include "trilobite-service.h"
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
	GET_ICON,
	LAST_SIGNAL
};

enum {
	ARG_0,
	ARG_NAME,
	ARG_VERSION,
	ARG_VENDOR_NAME,
	ARG_VENDOR_URL,
	ARG_URL,
	ARG_ICON
};

/* The signal array  and prototypes for default handlers */

static guint trilobite_service_signals[LAST_SIGNAL] = { 0 };

static BonoboObjectClass *trilobite_service_parent_class;

/*****************************************
  Corba stuff
*****************************************/

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

typedef struct {
	POA_Trilobite_Service servant;
	BonoboObject *bonobo_object;
} impl_POA_Trilobite_Service;

static CORBA_char*
impl_Trilobite_Service_get_name(impl_POA_Trilobite_Service *trilobite,
				CORBA_Environment *ev) 
{
	char *result;

	/* g_message ("in impl_Trilobite_Service_get_name"); */
	gtk_signal_emit (GTK_OBJECT (trilobite->bonobo_object), trilobite_service_signals[GET_NAME], &result);
	return CORBA_string_dup (result);
};

static CORBA_char*
impl_Trilobite_Service_get_version(impl_POA_Trilobite_Service *trilobite,
				   CORBA_Environment *ev) 
{
	char *result;

	/* g_message ("in impl_Trilobite_Service_get_version"); */
	gtk_signal_emit (GTK_OBJECT (trilobite->bonobo_object), trilobite_service_signals[GET_VERSION], &result);
	return CORBA_string_dup (result);
};

static CORBA_char*
impl_Trilobite_Service_get_vendor_name(impl_POA_Trilobite_Service *trilobite,
				       CORBA_Environment *ev) 
{
	char *result;

	/* g_message ("in impl_Trilobite_Service_get_vendor_name"); */
	gtk_signal_emit (GTK_OBJECT (trilobite->bonobo_object), trilobite_service_signals[GET_VENDOR_NAME], &result);
	return CORBA_string_dup (result);
};

static CORBA_char*
impl_Trilobite_Service_get_vendor_url(impl_POA_Trilobite_Service *trilobite,
				CORBA_Environment *ev) 
{
	char *result;

	/* g_message ("in impl_Trilobite_Service_get_vendor_url"); */
	gtk_signal_emit (GTK_OBJECT (trilobite->bonobo_object), trilobite_service_signals[GET_VENDOR_URL], &result);
	return CORBA_string_dup (result);
};

static CORBA_char*
impl_Trilobite_Service_get_url(impl_POA_Trilobite_Service *trilobite,
				CORBA_Environment *ev) 
{
	char *result;

	/* g_message ("in impl_Trilobite_Service_get_url"); */
	gtk_signal_emit (GTK_OBJECT (trilobite->bonobo_object), trilobite_service_signals[GET_URL], &result);
	return CORBA_string_dup (result);
};

static CORBA_char*
impl_Trilobite_Service_get_icon(impl_POA_Trilobite_Service *trilobite,
				CORBA_Environment *ev) 
{
	char *result;

	/* g_message ("in impl_Trilobite_Service_get_icon"); */
	gtk_signal_emit (GTK_OBJECT (trilobite->bonobo_object), trilobite_service_signals[GET_ICON], &result);
	return CORBA_string_dup (result);
};

POA_Trilobite_Service__epv* 
trilobite_service_get_epv(void) 
{
	POA_Trilobite_Service__epv *epv;

	epv = g_new0 (POA_Trilobite_Service__epv, 1);

	epv->get_name         = (gpointer) &impl_Trilobite_Service_get_name,
	epv->get_version      = (gpointer) &impl_Trilobite_Service_get_version;
	epv->get_vendor_name  = (gpointer) &impl_Trilobite_Service_get_vendor_name;
	epv->get_vendor_url   = (gpointer) &impl_Trilobite_Service_get_vendor_url;
	epv->get_url          = (gpointer) &impl_Trilobite_Service_get_url;
	epv->get_icon         = (gpointer) &impl_Trilobite_Service_get_icon;
		
	return epv;
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

void
trilobite_service_destroy (GtkObject *object)
{
	TrilobiteService *trilobite;
	/* g_message ("in trilobite_service_destroy"); */

	g_return_if_fail (object != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (object));
	
	trilobite = TRILOBITE_SERVICE (object);

	if (trilobite->private != NULL) {
		g_free (trilobite->private->service_name);
		g_free (trilobite->private->service_version);
		g_free (trilobite->private->service_vendor_name);
		g_free (trilobite->private->service_vendor_url);
		g_free (trilobite->private->service_url);
		g_free (trilobite->private->service_icon);
		g_free (trilobite->private);
	}

	if (GTK_OBJECT_CLASS (trilobite_service_parent_class)->destroy) {
		/* g_message ("calling trilobite-service-parent->destroy ()"); */
		GTK_OBJECT_CLASS (trilobite_service_parent_class)->destroy (object);
	}
}

static void
trilobite_service_set_arg (GtkObject *object,
			   GtkArg    *arg,
			   guint      arg_id)
{
	TrilobiteService *trilobite;

	g_assert (object != NULL);
	g_assert (TRILOBITE_IS_SERVICE (object));

	trilobite = TRILOBITE_SERVICE (object);

	switch (arg_id) {
	case ARG_NAME:
		trilobite_service_set_name (trilobite, (char*)GTK_VALUE_OBJECT(*arg));
		break;
	case ARG_VERSION:
		trilobite_service_set_version (trilobite, (char*)GTK_VALUE_OBJECT(*arg));
		break;
	case ARG_VENDOR_NAME:
		trilobite_service_set_vendor_name (trilobite, (char*)GTK_VALUE_OBJECT(*arg));
		break;
	case ARG_VENDOR_URL:
		trilobite_service_set_vendor_url (trilobite, (char*)GTK_VALUE_OBJECT(*arg));
		break;
	case ARG_URL:
		trilobite_service_set_url (trilobite, (char*)GTK_VALUE_OBJECT(*arg));
		break;
	case ARG_ICON:
		trilobite_service_set_icon (trilobite, (char*)GTK_VALUE_OBJECT(*arg));
		break;
	}
}

static void
trilobite_service_class_initialize (TrilobiteServiceClass *klass)
{
	GtkObjectClass *object_class;

	/* g_message ("in trilobite_service_class_initialize"); */

	object_class = (GtkObjectClass*) klass;
	object_class->destroy = (void(*)(GtkObject*))trilobite_service_destroy;
	object_class->set_arg = trilobite_service_set_arg;

	trilobite_service_parent_class = gtk_type_class (bonobo_object_get_type ());

	klass->servant_init = POA_Trilobite_Service__init;
	klass->servant_fini = POA_Trilobite_Service__fini;

	klass->servant_vepv = g_new0 (POA_Trilobite_Service__vepv,1);
	((POA_Trilobite_Service__vepv*)klass->servant_vepv)->_base_epv = &base_epv; 
	((POA_Trilobite_Service__vepv*)klass->servant_vepv)->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	((POA_Trilobite_Service__vepv*)klass->servant_vepv)->Trilobite_Service_epv = trilobite_service_get_epv ();

	trilobite_service_signals[GET_NAME] = 
		gtk_signal_new ("get_name",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (TrilobiteServiceClass,get_name),
				gtk_marshal_POINTER__NONE,
				GTK_TYPE_POINTER,0);
	trilobite_service_signals[GET_VERSION] = 
		gtk_signal_new ("get_version",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (TrilobiteServiceClass,get_version),
				gtk_marshal_POINTER__NONE,
				GTK_TYPE_POINTER,0);
	trilobite_service_signals[GET_VENDOR_NAME] = 
		gtk_signal_new ("get_vendor_name",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (TrilobiteServiceClass,get_vendor_name),
				gtk_marshal_POINTER__NONE,
				GTK_TYPE_POINTER,0);
	trilobite_service_signals[GET_VENDOR_URL] = 
		gtk_signal_new ("get_vendor_url",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (TrilobiteServiceClass,get_vendor_url),
				gtk_marshal_POINTER__NONE,
				GTK_TYPE_POINTER,0);
	trilobite_service_signals[GET_URL] = 
		gtk_signal_new ("get_url",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (TrilobiteServiceClass,get_url),
				gtk_marshal_POINTER__NONE,
				GTK_TYPE_POINTER,0);
	trilobite_service_signals[GET_ICON] = 
		gtk_signal_new ("get_icon",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (TrilobiteServiceClass,get_icon),
				gtk_marshal_POINTER__NONE,
				GTK_TYPE_POINTER,0);
	
	gtk_object_class_add_signals (object_class, trilobite_service_signals,LAST_SIGNAL);

	gtk_object_add_arg_type ("TrilobiteService::name",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_NAME);
	gtk_object_add_arg_type ("TrilobiteService::version",
				 GTK_TYPE_POINTER,
				 GTK_ARG_WRITABLE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_VERSION);
	gtk_object_add_arg_type ("TrilobiteService::vendor_name",
				 GTK_TYPE_POINTER,
				 GTK_ARG_WRITABLE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_VENDOR_NAME);
	gtk_object_add_arg_type ("TrilobiteService::vendor_url",
				 GTK_TYPE_POINTER,
				 GTK_ARG_WRITABLE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_VENDOR_URL);
	gtk_object_add_arg_type ("TrilobiteService::url",
				 GTK_TYPE_POINTER,
				 GTK_ARG_WRITABLE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_URL);
	gtk_object_add_arg_type ("TrilobiteService::icon",
				 GTK_TYPE_POINTER,
				 GTK_ARG_WRITABLE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_ICON);
				 
	klass->get_name = trilobite_service_get_name;
	klass->get_version = trilobite_service_get_version;
	klass->get_vendor_name = trilobite_service_get_vendor_name;
	klass->get_vendor_url = trilobite_service_get_vendor_url;
	klass->get_url = trilobite_service_get_url;
	klass->get_icon = trilobite_service_get_icon;

	klass->set_name = trilobite_service_set_name;
	klass->set_version = trilobite_service_set_version;
	klass->set_vendor_name = trilobite_service_set_vendor_name;
	klass->set_vendor_url = trilobite_service_set_vendor_url;
	klass->set_url = trilobite_service_set_url;
	klass->set_icon = trilobite_service_set_icon;
};

void               
trilobite_service_add_interface (TrilobiteService *trilobite, 
				 BonoboObject *service)
{
	g_assert (trilobite!=NULL);
	g_assert (TRILOBITE_IS_SERVICE (trilobite));
	g_assert (service!=NULL);
	g_assert (BONOBO_IS_OBJECT (service));

	bonobo_object_add_interface (BONOBO_OBJECT (trilobite), service);
}


gboolean
trilobite_service_construct (TrilobiteService *trilobite,
			     Trilobite_Service corba_trilobite)
{
	g_assert (trilobite != NULL);
	g_assert (TRILOBITE_IS_SERVICE (trilobite));
	g_return_val_if_fail (corba_trilobite != CORBA_OBJECT_NIL, FALSE);

	if (!bonobo_object_construct (BONOBO_OBJECT (trilobite), (CORBA_Object) corba_trilobite)) {
		return FALSE;
	}

	return TRUE;
}

static Trilobite_Service
trilobite_service_create_corba_object (BonoboObject *trilobite)
{
	impl_POA_Trilobite_Service *servant;
	void (*servant_init) (PortableServer_Servant servant, CORBA_Environment *ev);
	CORBA_Environment ev;

	/* g_message ("in trilobite_service_create_corba_object"); */

	g_assert (trilobite != NULL);

	CORBA_exception_init(&ev);
	
	servant = (impl_POA_Trilobite_Service*) g_new0 (BonoboObjectServant, 1);
	((POA_Trilobite_Service*) servant)->vepv = TRILOBITE_SERVICE_CLASS ( GTK_OBJECT (trilobite)->klass)->servant_vepv;
	servant->bonobo_object = trilobite;

	servant_init = TRILOBITE_SERVICE_CLASS ( GTK_OBJECT (trilobite)->klass)->servant_init;
	servant_init ((PortableServer_Servant)servant, &ev);
	ORBIT_OBJECT_KEY (((POA_Trilobite_Service*) servant)->_private)->object = NULL;	

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot instantiate Trilobite_Service corba object"); 
		g_free (servant);
		CORBA_exception_free (&ev);		
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);		

	return (Trilobite_Service) bonobo_object_activate_servant (trilobite, servant);
}

static void
trilobite_service_initialize (TrilobiteService *trilobite)
{
	Trilobite_Service corba_trilobite;

	/* g_message ("in trilobite_service_initialize"); */

	g_return_if_fail (trilobite != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (trilobite));

	corba_trilobite = trilobite_service_create_corba_object (BONOBO_OBJECT (trilobite));
	
	if (trilobite_service_construct (trilobite, corba_trilobite) == FALSE) {
		trilobite_service_destroy (GTK_OBJECT (trilobite));
		trilobite = NULL;
	} 

	trilobite->private = g_new0 (TrilobiteServicePrivate,1);

	/* By defaulting to "" instead of NULL, we avoid
	   accidently passing a NULL through CORBA */
	trilobite->private->service_name = g_strdup ("");
	trilobite->private->service_version = g_strdup ("");
	trilobite->private->service_vendor_name = g_strdup ("");
	trilobite->private->service_vendor_url = g_strdup ("");
	trilobite->private->service_url = g_strdup ("");
	trilobite->private->service_icon = g_strdup ("");

	trilobite->private->destroyed = FALSE;
	trilobite->private->alive = TRUE;
}

GtkType
trilobite_service_get_type (void)
{
	static GtkType trilobite_service_type = 0;

	/* g_message ("into trilobite_service_get_type"); */

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
		trilobite_service_type = gtk_type_unique (bonobo_object_get_type (), &trilobite_service_info);
	}

	return trilobite_service_type;
}

TrilobiteService*
trilobite_service_new() 
{
	TrilobiteService *trilobite;

	/* g_message ("in trilobite_service_new"); */

	trilobite = TRILOBITE_SERVICE (gtk_type_new (TRILOBITE_TYPE_SERVICE));

	if (trilobite == NULL) {
		g_warning ("Could not create a TrilobiteService*");
	}

	trilobite_service_initialize (trilobite);

	return trilobite;
}

/**************************************************/
/* Class methods                                  */
/**************************************************/

char*
trilobite_service_get_name(TrilobiteService *trilobite)
{
	g_return_val_if_fail (trilobite != NULL, NULL);
	g_return_val_if_fail (TRILOBITE_IS_SERVICE (trilobite), NULL);
	g_assert (trilobite->private != NULL);

	return trilobite->private->service_name;
}

char*
trilobite_service_get_version(TrilobiteService *trilobite)
{
	g_return_val_if_fail (trilobite != NULL, NULL);
	g_return_val_if_fail (TRILOBITE_IS_SERVICE (trilobite), NULL);
	g_assert (trilobite->private != NULL);

	return trilobite->private->service_version;
}

char*
trilobite_service_get_vendor_name(TrilobiteService *trilobite)
{
	g_return_val_if_fail (trilobite != NULL, NULL);
	g_return_val_if_fail (TRILOBITE_IS_SERVICE (trilobite), NULL);
	g_assert (trilobite->private != NULL);

	return trilobite->private->service_vendor_name;
}

char*
trilobite_service_get_vendor_url(TrilobiteService *trilobite)
{
	g_return_val_if_fail (trilobite != NULL, NULL);
	g_return_val_if_fail (TRILOBITE_IS_SERVICE (trilobite), NULL);
	g_assert (trilobite->private != NULL);

	return trilobite->private->service_vendor_url;
}

char*
trilobite_service_get_url(TrilobiteService *trilobite)
{
	g_return_val_if_fail (trilobite != NULL, NULL);
	g_return_val_if_fail (TRILOBITE_IS_SERVICE (trilobite), NULL);
	g_assert (trilobite->private != NULL);

	return trilobite->private->service_url;
}

char*
trilobite_service_get_icon(TrilobiteService *trilobite)
{
	g_return_val_if_fail (trilobite != NULL, NULL);
	g_return_val_if_fail (TRILOBITE_IS_SERVICE (trilobite), NULL);
	g_assert (trilobite->private != NULL);

	return trilobite->private->service_icon;
}

/*
  This is a helper function, since the code in the
  _set_x calls is so identical. I have not moved the g_returns
  and g_asserts into the helper, as I still want the 
  warnings and assertions to happen in the correct function.
 */
static void 
trilobite_service_set_helper (const char *value,
			      char **element) 
{
	g_free (*element);
	if (value == NULL) {
		(*element) = g_strdup ("");
	} else {
		(*element) = g_strdup (value);
	}
}

void 
trilobite_service_set_name (TrilobiteService *trilobite, 
			    const char *value) 
{
	g_return_if_fail (trilobite != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (trilobite));
	g_assert (trilobite->private != NULL);

	trilobite_service_set_helper (value, &(trilobite->private->service_name));
}

void 
trilobite_service_set_version (TrilobiteService *trilobite, 
			       const char *value)
{
	g_return_if_fail (trilobite != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (trilobite));
	g_assert (trilobite->private != NULL);

	trilobite_service_set_helper (value, &(trilobite->private->service_version));
}

void 
trilobite_service_set_vendor_name (TrilobiteService *trilobite, 
				   const char *value)
{
	g_return_if_fail (trilobite != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (trilobite));
	g_assert (trilobite->private != NULL);

	trilobite_service_set_helper (value, &(trilobite->private->service_vendor_name));
}

void 
trilobite_service_set_vendor_url (TrilobiteService *trilobite, 
				  const char *value)
{
	g_return_if_fail (trilobite != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (trilobite));
	g_assert (trilobite->private != NULL);

	trilobite_service_set_helper (value, &(trilobite->private->service_vendor_url));
}

void 
trilobite_service_set_url (TrilobiteService *trilobite, 
			   const char *value)
{
	g_return_if_fail (trilobite != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (trilobite));
	g_assert (trilobite->private != NULL);

	trilobite_service_set_helper (value, &(trilobite->private->service_url));
}

void 
trilobite_service_set_icon (TrilobiteService *trilobite, 
				const char *value)
{
	g_return_if_fail (trilobite != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (trilobite));
	g_assert (trilobite->private != NULL);

	trilobite_service_set_helper (value, &(trilobite->private->service_icon));
}
