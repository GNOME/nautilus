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
#include "trilobite-service-passwordquery-public.h"
#include "trilobite-service-passwordquery-private.h"


enum {
	ARG_0,
	ARG_PROMPT
};

enum {
	LAST_SIGNAL
};

static char* trilobite_passwordquery_get_password (TrilobiteRootHelper *roothelper, 
						   TrilobitePasswordQuery *trilobite);

/* static guint trilobite_passwordquery_signals[LAST_SIGNAL] = { 0 }; */

static BonoboObjectClass *trilobite_passwordquery_parent_class;

/*****************************************
  Corba stuff
*****************************************/

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

typedef struct {
	POA_Trilobite_PasswordQuery servant;
	BonoboObject *bonobo_object;
} impl_POA_Trilobite_PasswordQuery;

static void 
impl_Trilobite_PasswordQuery_set_query_client(impl_POA_Trilobite_PasswordQuery *service,
					      const Trilobite_PasswordQueryClient client,
					      CORBA_Environment * ev)
{
	TrilobitePasswordQuery *trilobite;

	trilobite = TRILOBITE_PASSWORDQUERY (service->bonobo_object);
	if (trilobite->private->client != CORBA_OBJECT_NIL) {
		CORBA_Object_release (trilobite->private->client, ev);
	}
	trilobite->private->client = CORBA_Object_duplicate (client, ev);
}


POA_Trilobite_PasswordQuery__epv* 
trilobite_passwordquery_get_epv(void) 
{
	POA_Trilobite_PasswordQuery__epv *epv;

	epv = g_new0 (POA_Trilobite_PasswordQuery__epv, 1);

	epv->set_query_client = (gpointer)&impl_Trilobite_PasswordQuery_set_query_client;

	return epv;
};
					  
/*****************************************
  GTK+ object stuff
*****************************************/

void
trilobite_passwordquery_destroy (GtkObject *object)
{
	TrilobitePasswordQuery *trilobite;
	CORBA_Environment ev;
	/* g_message ("in trilobite_passwordquery_destroy"); */

	CORBA_exception_init (&ev);
	g_return_if_fail (object != NULL);
	g_return_if_fail (TRILOBITE_IS_PASSWORDQUERY (object));
	
	trilobite = TRILOBITE_PASSWORDQUERY (object);

	if (trilobite->private != NULL) {
		trilobite_root_helper_destroy (GTK_OBJECT (trilobite->private->root_helper));
		g_free (trilobite->private->prompt);
		if (trilobite->private->client != CORBA_OBJECT_NIL) {
			CORBA_Object_release (trilobite->private->client, &ev);
		}
		g_free (trilobite->private);
	}

	CORBA_exception_free (&ev);

	if (GTK_OBJECT_CLASS (trilobite_passwordquery_parent_class)->destroy) {
		/* g_message ("calling trilobite-passwordquery-parent->destroy ()"); */
		GTK_OBJECT_CLASS (trilobite_passwordquery_parent_class)->destroy (object);
	}
}

static void
trilobite_passwordquery_set_arg (GtkObject *object,
				 GtkArg    *arg,
				 guint      arg_id)
{
	TrilobitePasswordQuery *trilobite;

	g_assert (object != NULL);
	g_assert (TRILOBITE_IS_PASSWORDQUERY (object));

	trilobite = TRILOBITE_PASSWORDQUERY (object);

	switch (arg_id) {
	case ARG_PROMPT:
		trilobite_passwordquery_set_prompt (trilobite, (char*)GTK_VALUE_OBJECT (*arg));
		break;
		
	}
}

static void
trilobite_passwordquery_class_initialize (TrilobitePasswordQueryClass *klass)
{
	GtkObjectClass *object_class;

	/* g_message ("in trilobite_passwordquery_class_initialize"); */

	object_class = (GtkObjectClass*) klass;
	object_class->destroy = (void(*)(GtkObject*))trilobite_passwordquery_destroy;
	object_class->set_arg = trilobite_passwordquery_set_arg;

	trilobite_passwordquery_parent_class = gtk_type_class (bonobo_object_get_type ());

	klass->servant_init = POA_Trilobite_PasswordQuery__init;
	klass->servant_fini = POA_Trilobite_PasswordQuery__fini;

	klass->servant_vepv = g_new0 (POA_Trilobite_PasswordQuery__vepv,1);
	((POA_Trilobite_PasswordQuery__vepv*)klass->servant_vepv)->_base_epv = &base_epv; 
	((POA_Trilobite_PasswordQuery__vepv*)klass->servant_vepv)->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	((POA_Trilobite_PasswordQuery__vepv*)klass->servant_vepv)->Trilobite_PasswordQuery_epv = trilobite_passwordquery_get_epv ();
	
	gtk_object_add_arg_type ("TrilobitePasswordQuery::prompt",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_PROMPT);

};

gboolean
trilobite_passwordquery_construct (TrilobitePasswordQuery *trilobite,
				   Trilobite_PasswordQuery corba_trilobite)
{
	g_assert (trilobite != NULL);
	g_assert (TRILOBITE_IS_PASSWORDQUERY (trilobite));
	g_return_val_if_fail (corba_trilobite != CORBA_OBJECT_NIL, FALSE);

	if (!bonobo_object_construct (BONOBO_OBJECT (trilobite), (CORBA_Object) corba_trilobite)) {
		return FALSE;
	}

	return TRUE;
}

static Trilobite_PasswordQuery
trilobite_passwordquery_create_corba_object (BonoboObject *trilobite)
{
	impl_POA_Trilobite_PasswordQuery *servant;
	void (*servant_init) (PortableServer_Servant servant, CORBA_Environment *ev);
	CORBA_Environment ev;

	/* g_message ("in trilobite_passwordquery_create_corba_object"); */

	g_assert (trilobite != NULL);

	CORBA_exception_init(&ev);
	
	servant = (impl_POA_Trilobite_PasswordQuery*) g_new0 (BonoboObjectServant, 1);
	((POA_Trilobite_PasswordQuery*) servant)->vepv = TRILOBITE_PASSWORDQUERY_CLASS ( GTK_OBJECT (trilobite)->klass)->servant_vepv;
	servant->bonobo_object = trilobite;

	servant_init = TRILOBITE_PASSWORDQUERY_CLASS ( GTK_OBJECT (trilobite)->klass)->servant_init;
	servant_init ((PortableServer_Servant)servant, &ev);
	ORBIT_OBJECT_KEY (((POA_Trilobite_PasswordQuery*) servant)->_private)->object = NULL;	

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot instantiate Trilobite_PasswordQuery corba object"); 
		g_free (servant);
		CORBA_exception_free (&ev);		
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);		

	return (Trilobite_PasswordQuery) bonobo_object_activate_servant (trilobite, servant);
}

static void
trilobite_passwordquery_initialize (TrilobitePasswordQuery *trilobite)
{
	Trilobite_PasswordQuery corba_trilobite;

	/* g_message ("in trilobite_passwordquery_initialize"); */

	g_return_if_fail (trilobite != NULL);
	g_return_if_fail (TRILOBITE_IS_PASSWORDQUERY (trilobite));

	corba_trilobite = trilobite_passwordquery_create_corba_object (BONOBO_OBJECT (trilobite));
	
	if (trilobite_passwordquery_construct (trilobite, corba_trilobite) == FALSE) {
		trilobite_passwordquery_destroy (GTK_OBJECT (trilobite));
		trilobite = NULL;
	} 

	trilobite->private = g_new0 (TrilobitePasswordQueryPrivate,1);

	trilobite->private->destroyed = FALSE;
	trilobite->private->alive = TRUE;
	trilobite->private->client = CORBA_OBJECT_NIL;
	trilobite->private->prompt = g_strdup ("");

	trilobite->private->root_helper = trilobite_root_helper_new ();
	gtk_signal_connect (GTK_OBJECT (trilobite->private->root_helper), 
			    "need_password", 
			    (GFunc)trilobite_passwordquery_get_password, 
			    trilobite);
}

GtkType
trilobite_passwordquery_get_type (void)
{
	static GtkType trilobite_passwordquery_type = 0;

	/* g_message ("into trilobite_passwordquery_get_type"); */

	/* First time it's called ? */
	if (!trilobite_passwordquery_type)
	{
		static const GtkTypeInfo trilobite_passwordquery_info =
		{
			"TrilobitePasswordQuery",
			sizeof (TrilobitePasswordQuery),
			sizeof (TrilobitePasswordQueryClass),
			(GtkClassInitFunc) trilobite_passwordquery_class_initialize,
			(GtkObjectInitFunc) trilobite_passwordquery_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		/* Get a unique GtkType */
		trilobite_passwordquery_type = gtk_type_unique (bonobo_object_get_type (), &trilobite_passwordquery_info);
	}

	return trilobite_passwordquery_type;
}

TrilobitePasswordQuery*
trilobite_passwordquery_new() 
{
	TrilobitePasswordQuery *trilobite;

	/* g_message ("in trilobite_passwordquery_new"); */

	trilobite = TRILOBITE_PASSWORDQUERY (gtk_type_new (TRILOBITE_TYPE_PASSWORDQUERY));

	if (trilobite == NULL) {
		g_warning ("Could not create a TrilobitePasswordQuery*");
	}

	trilobite_passwordquery_initialize (trilobite);

	return trilobite;
}

void trilobite_passwordquery_add_interface (TrilobitePasswordQuery *trilobite, 
					    BonoboObject *service)
{
	g_assert (trilobite!=NULL);
	g_assert (TRILOBITE_IS_PASSWORDQUERY (trilobite));
	g_assert (service!=NULL);
	g_assert (BONOBO_IS_OBJECT (service));

	gtk_object_set_data (GTK_OBJECT (service), "trilobite-root-helper", trilobite->private->root_helper);

	bonobo_object_add_interface (BONOBO_OBJECT (trilobite), service);
}

static char*
trilobite_passwordquery_get_password (TrilobiteRootHelper *roothelper, 
				      TrilobitePasswordQuery *trilobite)
{
	if (trilobite->private->client == CORBA_OBJECT_NIL) {
		/* you lose. */
		return NULL;
	} else {
		CORBA_Environment ev;
		CORBA_char *tmp;
		
		CORBA_exception_init (&ev);
		tmp = Trilobite_PasswordQueryClient_get_password (trilobite->private->client, 
								  trilobite->private->prompt, 
								  &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("Error during query of password from client: %s",
				   CORBA_exception_id (&ev));
		}
		CORBA_exception_free (&ev);
		
		return g_strdup (tmp);
	}
}

/* Sets the prompt */
void 
trilobite_passwordquery_set_prompt (TrilobitePasswordQuery *trilobite, 
				    const char *prompt)
{
	if (trilobite->private->prompt != NULL) {
		g_free (trilobite->private->prompt);
	}
	trilobite->private->prompt = g_strdup (prompt);
}
