/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusUndoContext
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Gene Z. Ragan <gzr@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-control.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

#include "nautilus-undo.h"

#include "nautilus-undo-context.h"

typedef struct {
	POA_Nautilus_Undo_Context servant;
	NautilusUndoContext *bonobo_object;
} impl_POA_Nautilus_Undo_Context;

/* GtkObject */
static void                  nautilus_undo_context_initialize_class       (NautilusUndoContextClass *class);
static void                  nautilus_undo_context_initialize             (NautilusUndoContext      *item);
static void                  destroy                                      (GtkObject                *object);

/* CORBA/Bonobo */
static Nautilus_Undo_Manager impl_Nautilus_Undo_Context__get_undo_manager (PortableServer_Servant    servant,
									   CORBA_Environment        *ev);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusUndoContext, nautilus_undo_context, BONOBO_OBJECT_TYPE)

POA_Nautilus_Undo_Context__epv libnautilus_Nautilus_Undo_Context_epv =
{
	NULL, /* _private */
	&impl_Nautilus_Undo_Context__get_undo_manager,
};

static PortableServer_ServantBase__epv base_epv;
static POA_Nautilus_Undo_Context__vepv impl_Nautilus_Undo_Context_vepv =
{
	&base_epv,
	NULL,
	&libnautilus_Nautilus_Undo_Context_epv
};

static void
impl_Nautilus_Undo_Context__destroy (BonoboObject *obj, impl_POA_Nautilus_Undo_Context *servant)
{
	PortableServer_ObjectId *objid;
	CORBA_Environment ev;
	void (*servant_destroy_func) (PortableServer_Servant servant, CORBA_Environment *ev);

  	CORBA_exception_init (&ev);
  	servant_destroy_func = NAUTILUS_UNDO_CONTEXT_CLASS
		(GTK_OBJECT (servant->bonobo_object)->klass)->servant_destroy_func;

  	objid = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
  	PortableServer_POA_deactivate_object (bonobo_poa (), objid, &ev);
  	CORBA_free (objid);
  	obj->servant = NULL;

  	(* servant_destroy_func) ((PortableServer_Servant) servant, &ev);

  	g_free (servant);
  	CORBA_exception_free (&ev);
}

static Nautilus_Undo_Context
impl_Nautilus_Undo_Context__create (NautilusUndoContext *context, CORBA_Environment * ev)
{
	Nautilus_Undo_Context retval;
	impl_POA_Nautilus_Undo_Context *servant;
	void (*servant_init_func) (PortableServer_Servant servant, CORBA_Environment *ev);
	NautilusUndoContextClass *context_class;

	context_class = NAUTILUS_UNDO_CONTEXT_CLASS (GTK_OBJECT (context)->klass);

	servant_init_func = context_class->servant_init_func;
	servant = g_new0 (impl_POA_Nautilus_Undo_Context, 1);
	servant->servant.vepv = context_class->vepv;
	if (servant->servant.vepv->Bonobo_Unknown_epv == NULL) {
		servant->servant.vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	}
  	(* servant_init_func) ((PortableServer_Servant) servant, ev);

  	servant->bonobo_object = context;

  	retval = bonobo_object_activate_servant (BONOBO_OBJECT (context), servant);

  	gtk_signal_connect (GTK_OBJECT (context), "destroy",
			    GTK_SIGNAL_FUNC (impl_Nautilus_Undo_Context__destroy),
			    servant);

  	return retval;
}

static Nautilus_Undo_Manager
impl_Nautilus_Undo_Context__get_undo_manager (PortableServer_Servant servant,
					      CORBA_Environment *ev)
{
	NautilusUndoContext *context;
	
	//g_assert (NAUTILUS_IS_UNDO_CONTEXT (servant->bonobo_object));
	//context = NAUTILUS_UNDO_CONTEXT (servant->gtk_object);
	context = ((impl_POA_Nautilus_Undo_Context *) servant)->bonobo_object;

	g_assert (NAUTILUS_IS_UNDO_CONTEXT (context));
	return CORBA_Object_duplicate (context->undo_manager, ev);
}

/* nautilus_undo_manager_new */
NautilusUndoContext *
nautilus_undo_context_new (Nautilus_Undo_Manager undo_manager)
{
	NautilusUndoContext *context;
	
	context = gtk_type_new (nautilus_undo_context_get_type ());
	context->undo_manager = undo_manager;
	return context;
}

/* Object initialization function for the NautilusUndoContext */
static void 
nautilus_undo_context_initialize (NautilusUndoContext *context)
{
	CORBA_Environment ev;	
	
	CORBA_exception_init (&ev);

	bonobo_object_construct (BONOBO_OBJECT (context),
				 impl_Nautilus_Undo_Context__create (context, &ev));

  	CORBA_exception_free (&ev);
}

/* Class initialization function for the NautilusUndoManager. */
static void
nautilus_undo_context_initialize_class (NautilusUndoContextClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = destroy;

	klass->servant_init_func = POA_Nautilus_Undo_Context__init;
	klass->servant_destroy_func = POA_Nautilus_Undo_Context__fini;
	klass->vepv = &impl_Nautilus_Undo_Context_vepv;
}

/* destroy */
static void
destroy (GtkObject *object)
{
	NautilusUndoContext *context;

	context = NAUTILUS_UNDO_CONTEXT (object);
		
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}
