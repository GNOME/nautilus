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

/* GtkObject */
static void                  nautilus_undo_context_class_init       (NautilusUndoContextClass *class);
static void                  nautilus_undo_context_init             (NautilusUndoContext      *item);

EEL_BONOBO_BOILERPLATE_FULL (NautilusUndoContext,
			       Nautilus_Undo_Context,
			       nautilus_undo_context,
			       BONOBO_OBJECT_TYPE)

static Nautilus_Undo_Manager
impl_Nautilus_Undo_Context__get_undo_manager (PortableServer_Servant servant,
					      CORBA_Environment *ev)
{
	NautilusUndoContext *context;
	
	context = NAUTILUS_UNDO_CONTEXT (bonobo_object_from_servant (servant));
	return CORBA_Object_duplicate (context->undo_manager, ev);
}

NautilusUndoContext *
nautilus_undo_context_new (Nautilus_Undo_Manager undo_manager)
{
	CORBA_Environment ev;	
	NautilusUndoContext *context;
	
	CORBA_exception_init (&ev);

	context = NAUTILUS_UNDO_CONTEXT (g_object_new (nautilus_undo_context_get_type (), NULL));
	context->undo_manager = CORBA_Object_duplicate (undo_manager, &ev);
	
  	CORBA_exception_free (&ev);

	return context;
}

static void 
nautilus_undo_context_init (NautilusUndoContext *context)
{
}

static void
finalize (GObject *object)
{
	CORBA_Environment ev;	
	NautilusUndoContext *context;

	CORBA_exception_init (&ev);

	context = NAUTILUS_UNDO_CONTEXT (object);
	CORBA_Object_release (context->undo_manager, &ev);
	
 	CORBA_exception_free (&ev);
	
	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_undo_context_class_init (NautilusUndoContextClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = finalize;
	
	klass->epv._get_undo_manager = impl_Nautilus_Undo_Context__get_undo_manager;
}
