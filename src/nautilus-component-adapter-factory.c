/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* nautilus-component-adapter-factory.c - client wrapper for the
 * special adapter component, which wraps Bonobo components as
 * Nautilus Views and in the process keeps evil syncrhonous I/O out of
 * the Nautilus process itself.
 */

#include <config.h>
#include "nautilus-component-adapter-factory.h"

#include <bonobo/bonobo-exception.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus-adapter/nautilus-adapter-factory.h>

#define NAUTILUS_COMPONENT_ADAPTER_FACTORY_IID "OAFIID:nautilus_adapter_factory:fd24ecfc-0a6e-47ab-bc53-69d7487c6ad4"

struct NautilusComponentAdapterFactoryDetails {
	Nautilus_ComponentAdapterFactory corba_factory;
};

static NautilusComponentAdapterFactory *global_component_adapter_factory = NULL;

static void nautilus_component_adapter_factory_initialize_class (NautilusComponentAdapterFactoryClass *klass);
static void nautilus_component_adapter_factory_initialize       (NautilusComponentAdapterFactory      *factory);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusComponentAdapterFactory,
			      nautilus_component_adapter_factory,
			      GTK_TYPE_OBJECT)

static void
activate_factory (NautilusComponentAdapterFactory *factory)
{
	BonoboObjectClient *object_client;

	object_client = bonobo_object_activate (NAUTILUS_COMPONENT_ADAPTER_FACTORY_IID, 0);
	if (object_client == NULL) {
		return;
	}

	factory->details->corba_factory = bonobo_object_query_interface 
		(BONOBO_OBJECT (object_client), "IDL:Nautilus/ComponentAdapterFactory:1.0");
	bonobo_object_unref (BONOBO_OBJECT (object_client));
}

static void
unref_factory (NautilusComponentAdapterFactory *factory)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	Bonobo_Unknown_unref (factory->details->corba_factory, &ev);
	CORBA_exception_free (&ev);
}

static void
release_factory (NautilusComponentAdapterFactory *factory)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	CORBA_Object_release (factory->details->corba_factory, &ev);
	factory->details->corba_factory = CORBA_OBJECT_NIL;
	CORBA_exception_free (&ev);
}

static Nautilus_ComponentAdapterFactory
get_corba_factory (NautilusComponentAdapterFactory *factory)
{
	CORBA_Environment ev;
	Nautilus_ComponentAdapterFactory result;
	gboolean need_unref;

	CORBA_exception_init (&ev);

	need_unref = FALSE;
	if (CORBA_Object_is_nil (factory->details->corba_factory, &ev)
	    || BONOBO_EX (&ev)
	    || CORBA_Object_non_existent (factory->details->corba_factory, &ev)
	    || BONOBO_EX (&ev)) {
		release_factory (factory);
		activate_factory (factory);
		need_unref = TRUE;
	}
	
	CORBA_exception_free (&ev);

	result = bonobo_object_dup_ref (factory->details->corba_factory, NULL);
	if (need_unref) {
		unref_factory (factory);
	}
	return result;
}

static void
nautilus_component_adapter_factory_initialize (NautilusComponentAdapterFactory *factory)
{
	factory->details = g_new0 (NautilusComponentAdapterFactoryDetails, 1);
}

static void
nautilus_component_adapter_factory_destroy (GtkObject *object)
{
	NautilusComponentAdapterFactory *factory;

	factory = NAUTILUS_COMPONENT_ADAPTER_FACTORY (object);

	release_factory (factory);
	g_free (factory->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_component_adapter_factory_initialize_class  (NautilusComponentAdapterFactoryClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) klass;
	object_class->destroy = nautilus_component_adapter_factory_destroy;
}

static void
component_adapter_factory_at_exit_destructor (void)
{
	if (global_component_adapter_factory != NULL) {
		gtk_object_unref (GTK_OBJECT (global_component_adapter_factory));
	}
}

NautilusComponentAdapterFactory *
nautilus_component_adapter_factory_get (void)
{
	NautilusComponentAdapterFactory *factory;

	if (global_component_adapter_factory == NULL) {
		factory = NAUTILUS_COMPONENT_ADAPTER_FACTORY
			(gtk_object_new (NAUTILUS_TYPE_COMPONENT_ADAPTER_FACTORY, NULL));
		
		gtk_object_ref (GTK_OBJECT (factory));
		gtk_object_sink (GTK_OBJECT (factory));
		
		global_component_adapter_factory = factory;
		g_atexit (component_adapter_factory_at_exit_destructor);
	}

	return global_component_adapter_factory;
}

Nautilus_View
nautilus_component_adapter_factory_create_adapter (NautilusComponentAdapterFactory *factory,
						   BonoboObjectClient *component)
{
	Nautilus_View nautilus_view;
	Bonobo_Control bonobo_control;
	Nautilus_ComponentAdapterFactory corba_factory;
	CORBA_Environment ev;

	nautilus_view = bonobo_object_client_query_interface 
		(component, "IDL:Nautilus/View:1.0", NULL);

	CORBA_exception_init (&ev);

	if (!CORBA_Object_is_nil (nautilus_view, &ev)) {
		/* Object has the View interface, great! We might not
		 * need to adapt it.
		 */
		bonobo_control = bonobo_object_client_query_interface 
			(component, "IDL:Bonobo/Control:1.0", NULL); 
		if (!CORBA_Object_is_nil (bonobo_control, &ev)) {
			/* It has the control interface too, so all is peachy. */
			bonobo_object_release_unref (bonobo_control, NULL);
		} else {
			/* No control interface; we have no way to
			 * support a View that doesn't also support
			 * the Control interface, so fail.
			 */
			bonobo_object_release_unref (nautilus_view, NULL);
			nautilus_view = CORBA_OBJECT_NIL;
		}
	} else {
		/* No View interface, we must adapt the object */

		corba_factory = get_corba_factory (factory);
		nautilus_view = Nautilus_ComponentAdapterFactory_create_adapter 
			(corba_factory,
			 bonobo_object_corba_objref (BONOBO_OBJECT (component)),
			 &ev);
		if (BONOBO_EX (&ev)) {
			nautilus_view = CORBA_OBJECT_NIL;
		}
		bonobo_object_release_unref (corba_factory, NULL);
	}

	CORBA_exception_free (&ev);
	
	return nautilus_view;
}
