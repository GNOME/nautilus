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
#include <liboaf/liboaf.h>

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
	GET_ICON_URI,
	DONE,
	LAST_SIGNAL
};

/* The signal array  and prototypes for default handlers*/

static guint trilobite_service_signals[LAST_SIGNAL] = { 0 };

char* trilobite_service_default_get_name            (TrilobiteService *trilobite);
char* trilobite_service_default_get_version         (TrilobiteService *trilobite);
char* trilobite_service_default_get_vendor_name     (TrilobiteService *trilobite);
char* trilobite_service_default_get_vendor_url      (TrilobiteService *trilobite);
char* trilobite_service_default_get_url             (TrilobiteService *trilobite);
char* trilobite_service_default_get_icon_uri        (TrilobiteService *trilobite);
void  trilobite_service_default_done                (TrilobiteService *trilobite);

/*****************************************
  Corba stuff
*****************************************/

static CORBA_char*
impl_Trilobite_Service_get_name(PortableServer_Servant pos,
				CORBA_Environment *ev) 
{
	char *result;
	TrilobiteService *trilobite;

	trilobite = TRILOBITE_SERVICE (POA_GTK_TRILOBITE_SERVICE_PTR (pos)->gtk);
	g_message ("in impl_Trilobite_Service_get_name");
	gtk_signal_emit (GTK_OBJECT (trilobite), trilobite_service_signals[GET_NAME], &result);
	return CORBA_string_dup (result);
};

static CORBA_char*
impl_Trilobite_Service_get_version(PortableServer_Servant pos,
				   CORBA_Environment *ev) 
{
	char *result;
	TrilobiteService *trilobite;

	trilobite = TRILOBITE_SERVICE (POA_GTK_TRILOBITE_SERVICE_PTR (pos)->gtk);
	g_message ("in impl_Trilobite_Service_get_version");
	gtk_signal_emit (GTK_OBJECT (trilobite), trilobite_service_signals[GET_VERSION], &result);
	return CORBA_string_dup (result);
};

static CORBA_char*
impl_Trilobite_Service_get_vendor_name(PortableServer_Servant pos,
				       CORBA_Environment *ev) 
{
	char *result;
	TrilobiteService *trilobite;

	trilobite = TRILOBITE_SERVICE (POA_GTK_TRILOBITE_SERVICE_PTR (pos)->gtk);
	g_message ("in impl_Trilobite_Service_get_vendor_name");
	gtk_signal_emit (GTK_OBJECT (trilobite), trilobite_service_signals[GET_VENDOR_NAME], &result);
	return CORBA_string_dup (result);
};

static CORBA_char*
impl_Trilobite_Service_get_vendor_url(PortableServer_Servant pos,
				CORBA_Environment *ev) 
{
	char *result;
	TrilobiteService *trilobite;

	trilobite = TRILOBITE_SERVICE (POA_GTK_TRILOBITE_SERVICE_PTR (pos)->gtk);
	g_message ("in impl_Trilobite_Service_get_vendor_url");
	gtk_signal_emit (GTK_OBJECT (trilobite), trilobite_service_signals[GET_VENDOR_URL], &result);
	return CORBA_string_dup (result);
};

static CORBA_char*
impl_Trilobite_Service_get_url(PortableServer_Servant pos,
				CORBA_Environment *ev) 
{
	char *result;
	TrilobiteService *trilobite;

	trilobite = TRILOBITE_SERVICE (POA_GTK_TRILOBITE_SERVICE_PTR (pos)->gtk);
	g_message ("in impl_Trilobite_Service_get_url");
	gtk_signal_emit (GTK_OBJECT (trilobite), trilobite_service_signals[GET_URL], &result);
	return CORBA_string_dup (result);
};

static CORBA_char*
impl_Trilobite_Service_get_icon_uri(PortableServer_Servant pos,
				CORBA_Environment *ev) 
{
	char *result;
	TrilobiteService *trilobite;

	trilobite = TRILOBITE_SERVICE (POA_GTK_TRILOBITE_SERVICE_PTR (pos)->gtk);
	g_message ("in impl_Trilobite_Service_get_icon_uri");
	gtk_signal_emit (GTK_OBJECT (trilobite), trilobite_service_signals[GET_ICON_URI], &result);
	return CORBA_string_dup (result);
};

static void
impl_Trilobite_Service_done(PortableServer_Servant pos,
				CORBA_Environment *ev) 
{
	TrilobiteService *trilobite;

	trilobite = TRILOBITE_SERVICE (POA_GTK_TRILOBITE_SERVICE_PTR (pos)->gtk);
	g_message ("in impl_Trilobite_Service_done");
	gtk_signal_emit (GTK_OBJECT (trilobite), trilobite_service_signals[DONE]);
	return;
};

POA_Trilobite_Service__epv Trilobite_Service_epv =
{
	NULL, /* CORBA internal */
	(gpointer) &impl_Trilobite_Service_get_name,
	(gpointer) &impl_Trilobite_Service_get_version,
	(gpointer) &impl_Trilobite_Service_get_vendor_name,
	(gpointer) &impl_Trilobite_Service_get_vendor_url,
	(gpointer) &impl_Trilobite_Service_get_url,
	(gpointer) &impl_Trilobite_Service_get_icon_uri,

	(gpointer) &impl_Trilobite_Service_done
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

void
trilobite_service_destroy (TrilobiteService *trilobite)
{
	PortableServer_ObjectId *objid;
	CORBA_Environment ev;
	PortableServer_POA poa;
	void (*poa_servant_fini) (PortableServer_Servant servant, CORBA_Environment *ev);

	g_return_if_fail (trilobite != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (trilobite));

	if (trilobite->private->destroyed==TRUE) {
		return;
	}

	CORBA_exception_init(&ev);
	poa = (PortableServer_POA)CORBA_ORB_resolve_initial_references (oaf_orb_get (), "RootPOA", &ev);
	poa_servant_fini = TRILOBITE_SERVICE_CLASS (GTK_OBJECT (trilobite)->klass)->poa_servant_fini;
	objid = PortableServer_POA_servant_to_id (poa, &trilobite->poa_gtk, &ev);
	PortableServer_POA_deactivate_object (poa, objid, &ev);
	CORBA_free (objid);

	poa_servant_fini (&trilobite->poa_gtk, &ev);

	if (GTK_OBJECT_CLASS ( TRILOBITE_SERVICE_CLASS( GTK_OBJECT (trilobite)->klass)->parent_class)->destroy != NULL) {
		 GTK_OBJECT_CLASS ( TRILOBITE_SERVICE_CLASS( GTK_OBJECT (trilobite)->klass)->parent_class)->destroy (GTK_OBJECT (trilobite));
	}

	trilobite->private->destroyed = TRUE;

	g_free (trilobite->private->service_name);
	g_free (trilobite->private->service_version);
	g_free (trilobite->private->service_vendor_name);
	g_free (trilobite->private->service_vendor_url);
	g_free (trilobite->private->service_url);
	g_free (trilobite->private->service_icon_uri);
	g_free (trilobite->private);
	g_free (trilobite);

	CORBA_exception_free(&ev);
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
	trilobite_service_signals[GET_ICON_URI] = 
		gtk_signal_new ("get_icon_uri",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (TrilobiteServiceClass,get_icon_uri),
				gtk_marshal_POINTER__NONE,
				GTK_TYPE_POINTER,0);
	trilobite_service_signals[DONE] = 
		gtk_signal_new ("service_done",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (TrilobiteServiceClass,done),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,0);
	
	gtk_object_class_add_signals (object_class, trilobite_service_signals,LAST_SIGNAL);

	klass->get_name = trilobite_service_default_get_name;
	klass->get_version = trilobite_service_default_get_version;
	klass->get_vendor_name = trilobite_service_default_get_vendor_name;
	klass->get_vendor_url = trilobite_service_default_get_vendor_url;
	klass->get_url = trilobite_service_default_get_url;
	klass->get_icon_uri = trilobite_service_default_get_icon_uri;
	klass->done = trilobite_service_default_done;
};

void
trilobite_service_activate (TrilobiteService *trilobite)
{
	PortableServer_POA poa;
	CORBA_Environment ev;
	void (*poa_servant_init) (PortableServer_Servant servant, CORBA_Environment *ev);

	g_assert (trilobite != NULL);
	g_assert (TRILOBITE_IS_SERVICE (trilobite));

	CORBA_exception_init(&ev);
	poa = (PortableServer_POA)CORBA_ORB_resolve_initial_references (oaf_orb_get (), "RootPOA", &ev);

	poa_servant_init = TRILOBITE_SERVICE_CLASS (GTK_OBJECT(trilobite)->klass)->poa_servant_init;
	poa_servant_init ((PortableServer_Servant)trilobite->poa_gtk, &ev);
	CORBA_free (PortableServer_POA_activate_object (poa,  (PortableServer_Servant)trilobite->poa_gtk, &ev));
	trilobite->corba_object = 
		(Trilobite_Service)PortableServer_POA_servant_to_reference (poa, (PortableServer_Servant)trilobite->poa_gtk, &ev);

	CORBA_exception_free (&ev);
}

static void
trilobite_service_initialize (TrilobiteService *trilobite)
{
	g_return_if_fail (trilobite != NULL);
	g_return_if_fail (TRILOBITE_IS_SERVICE (trilobite));

	/* FIXME bugzilla.eazel.com 854:
	   how to initialize the values */
	trilobite->private = g_new0 (TrilobiteServicePrivate,1);
	trilobite->private->service_name = g_strdup ("Default");
	trilobite->private->service_version = g_strdup ("0.1");
	trilobite->private->service_vendor_name = g_strdup ("Dev Null");
	trilobite->private->service_vendor_url = g_strdup ("http://www.dev.null");
	trilobite->private->service_url = g_strdup ("http://www.dev.null");
	trilobite->private->service_icon_uri = g_strdup ("file://dev/random");

	trilobite->private->destroyed = FALSE;
	trilobite->private->alive = TRUE;

	trilobite->poa_gtk = g_new0(POA_GTK_Trilobite_Service,1);
	trilobite->poa_gtk->gtk = trilobite;

	trilobite->poa_gtk->poa.vepv = 
		TRILOBITE_SERVICE_CLASS (GTK_OBJECT(trilobite)->klass)->poa_vepv;
}

GtkType
trilobite_service_get_type (void)
{
	static GtkType trilobite_service_type = 0;

	g_message("into trilobite_service_get_type");

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
  FIXME bugzilla.eazel.com 854:
  determine whether we want to set the name/version/whatnot in the _new(...) call
  or using _set_name(...) calls.
 */
TrilobiteService*
trilobite_service_new() 
{
	TrilobiteService *trilobite;

	trilobite = TRILOBITE_SERVICE (gtk_type_new (TRILOBITE_TYPE_SERVICE));

	if (trilobite == NULL) {
		g_warning ("Could not create a TrilobiteService*");
	}

	return trilobite;
}

gboolean
trilobite_service_alive (TrilobiteService *trilobite) {
	g_return_val_if_fail (trilobite!=NULL, FALSE);
	return trilobite->private->alive;
}

char*
trilobite_service_default_get_name(TrilobiteService *trilobite)
{
	return trilobite->private->service_name;
}

char*
trilobite_service_default_get_version(TrilobiteService *trilobite)
{
	return trilobite->private->service_version;
}

char*
trilobite_service_default_get_vendor_name(TrilobiteService *trilobite)
{
	return trilobite->private->service_vendor_name;
}

char*
trilobite_service_default_get_vendor_url(TrilobiteService *trilobite)
{
	return trilobite->private->service_vendor_url;
}

char*
trilobite_service_default_get_url(TrilobiteService *trilobite)
{
	return trilobite->private->service_url;
}

char*
trilobite_service_default_get_icon_uri(TrilobiteService *trilobite)
{
	return trilobite->private->service_icon_uri;
}

void
trilobite_service_default_done(TrilobiteService *trilobite)
{
	/* FIXME: bugzille.eazel.com 900
	   should this calll _destroy or should it deref and _destroy when ref == 0 */
	trilobite->private->alive = FALSE;
	return;
}
