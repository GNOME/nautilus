/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusUndoContext - Used internally by undo machinery.
 *                       Not public.
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
#include "nautilus-undo-context.h"

#include <eel/eel-gtk-macros.h>
#include <bonobo/bonobo-main.h>
#include <gtk/gtksignal.h>
#include <libnautilus/nautilus-bonobo-workarounds.h>

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

EEL_DEFINE_CLASS_BOILERPLATE (NautilusUndoContext, nautilus_undo_context, BONOBO_OBJECT_TYPE)

POA_Nautilus_Undo_Context__epv libnautilus_Nautilus_Undo_Context_epv =
{
	NULL, /* _private */
	&impl_Nautilus_Undo_Context__get_undo_manager,
};

static PortableServer_ServantBase__epv base_epv;
static POA_Nautilus_Undo_Context__vepv vepv =
{
	&base_epv,
	NULL,
	&libnautilus_Nautilus_Undo_Context_epv
};

static void
impl_Nautilus_Undo_Context__destroy (BonoboObject *object,
				     PortableServer_Servant servant)
{
	PortableServer_ObjectId *object_id;
	CORBA_Environment ev;

  	CORBA_exception_init (&ev);

  	object_id = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
  	PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);
  	CORBA_free (object_id);
  	object->servant = NULL;

	POA_Nautilus_Undo_Context__fini (servant, &ev);
  	g_free (servant);

  	CORBA_exception_free (&ev);
}

static Nautilus_Undo_Context
impl_Nautilus_Undo_Context__create (NautilusUndoContext *bonobo_object,
				    CORBA_Environment *ev)
{
	impl_POA_Nautilus_Undo_Context *servant;

	servant = g_new0 (impl_POA_Nautilus_Undo_Context, 1);
	servant->servant.vepv = &vepv;
	vepv.Bonobo_Unknown_epv = nautilus_bonobo_object_get_epv ();
	POA_Nautilus_Undo_Context__init ((PortableServer_Servant) servant, ev);

  	gtk_signal_connect (GTK_OBJECT (bonobo_object), "destroy",
			    GTK_SIGNAL_FUNC (impl_Nautilus_Undo_Context__destroy),
			    servant);

  	servant->bonobo_object = bonobo_object;
  	return bonobo_object_activate_servant (BONOBO_OBJECT (bonobo_object), servant);
}

static Nautilus_Undo_Manager
impl_Nautilus_Undo_Context__get_undo_manager (PortableServer_Servant servant,
					      CORBA_Environment *ev)
{
	NautilusUndoContext *context;
	
	context = ((impl_POA_Nautilus_Undo_Context *) servant)->bonobo_object;
	g_assert (NAUTILUS_IS_UNDO_CONTEXT (context));
	return CORBA_Object_duplicate (context->undo_manager, ev);
}

NautilusUndoContext *
nautilus_undo_context_new (Nautilus_Undo_Manager undo_manager)
{
	CORBA_Environment ev;	
	NautilusUndoContext *context;
	
	CORBA_exception_init (&ev);

	context = NAUTILUS_UNDO_CONTEXT (gtk_object_new (nautilus_undo_context_get_type (), NULL));
	context->undo_manager = CORBA_Object_duplicate (undo_manager, &ev);
	
  	CORBA_exception_free (&ev);

	return context;
}

static void 
nautilus_undo_context_initialize (NautilusUndoContext *context)
{
	CORBA_Environment ev;	
	
	CORBA_exception_init (&ev);

	bonobo_object_construct (BONOBO_OBJECT (context),
				 impl_Nautilus_Undo_Context__create (context, &ev));

  	CORBA_exception_free (&ev);
}

static void
destroy (GtkObject *object)
{
	CORBA_Environment ev;	
	NautilusUndoContext *context;

	CORBA_exception_init (&ev);

	context = NAUTILUS_UNDO_CONTEXT (object);
	CORBA_Object_release (context->undo_manager, &ev);
	
 	CORBA_exception_free (&ev);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_undo_context_initialize_class (NautilusUndoContextClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = destroy;
}
