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

typedef struct {
	POA_Nautilus_Undo_Context servant;
	NautilusUndoContext *bonobo_object;
} impl_POA_Nautilus_Undo_Context;

/* GtkObject */
static void                  nautilus_undo_context_class_init       (NautilusUndoContextClass *class);
static void                  nautilus_undo_context_init             (NautilusUndoContext      *item);
static void                  destroy                                      (GtkObject                *object);

/* CORBA/Bonobo */
static Nautilus_Undo_Manager impl_Nautilus_Undo_Context__get_undo_manager (PortableServer_Servant    servant,
									   CORBA_Environment        *ev);

EEL_DEFINE_BONOBO_BOILERPLATE (NautilusUndoContext, Nautilus_Undo_Context, nautilus_undo_context, BONOBO_OBJECT_TYPE)

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
nautilus_undo_context_init (NautilusUndoContext *context)
{
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
nautilus_undo_context_class_init (NautilusUndoContextClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = destroy;
	
	klass->epv._get_undo_manager = impl_Nautilus_Undo_Context__get_undo_manager;
}
