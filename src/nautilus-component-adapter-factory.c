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

#include <libnautilus-adapter/nautilus-adapter-factory.h>

#include <eel/eel-gtk-macros.h>



#define NAUTILUS_COMPONENT_ADAPTER_FACTORY_IID "OAFIID:nautilus_adapter_factory:fd24ecfc-0a6e-47ab-bc53-69d7487c6ad4"

struct NautilusComponentAdapterFactoryDetails {
	Nautilus_ComponentAdapterFactory corba_factory;
};



static void nautilus_component_adapter_factory_initialize_class (NautilusComponentAdapterFactoryClass *klass);
static void nautilus_component_adapter_factory_initialize       (NautilusComponentAdapterFactory      *factory);
static void nautilus_component_adapter_factory_destroy          (GtkObject                            *object);


EEL_DEFINE_CLASS_BOILERPLATE (NautilusComponentAdapterFactory, nautilus_component_adapter_factory, GTK_TYPE_OBJECT)

static void
nautilus_component_adapter_factory_initialize_class  (NautilusComponentAdapterFactoryClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass*) klass;
	object_class->destroy = nautilus_component_adapter_factory_destroy;
}

static void
nautilus_component_adapter_factory_initialize (NautilusComponentAdapterFactory *factory)
{
	BonoboObjectClient *object_client;

	factory->details = g_new0 (NautilusComponentAdapterFactoryDetails, 1);

	object_client = bonobo_object_activate (NAUTILUS_COMPONENT_ADAPTER_FACTORY_IID, 0);
	if (object_client != NULL) {
		factory->details->corba_factory = bonobo_object_query_interface 
			(BONOBO_OBJECT (object_client), "IDL:Nautilus/ComponentAdapterFactory:1.0");
		bonobo_object_unref (BONOBO_OBJECT (object_client)); 
	}
}


static void
nautilus_component_adapter_factory_destroy (GtkObject *object)
{
	NautilusComponentAdapterFactory *factory;

	factory = NAUTILUS_COMPONENT_ADAPTER_FACTORY (object);

	if (factory->details->corba_factory != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (factory->details->corba_factory, NULL);
	}

	g_free (factory->details);
}



static NautilusComponentAdapterFactory *global_component_adapter_factory = NULL;

static void
component_adapter_factory_at_exit_destructor (void)
{
	if (global_component_adapter_factory != NULL) {
		gtk_object_unref (GTK_OBJECT (global_component_adapter_factory));
	}
	
	global_component_adapter_factory = NULL;
}


NautilusComponentAdapterFactory *
nautilus_component_adapter_factory_get (void)
{
	if (global_component_adapter_factory == NULL) {
		global_component_adapter_factory = NAUTILUS_COMPONENT_ADAPTER_FACTORY
			(gtk_object_new (NAUTILUS_TYPE_COMPONENT_ADAPTER_FACTORY, NULL));
		gtk_object_ref (GTK_OBJECT (global_component_adapter_factory));
		gtk_object_sink (GTK_OBJECT (global_component_adapter_factory));

		g_atexit (component_adapter_factory_at_exit_destructor);
	}


	if (global_component_adapter_factory->details->corba_factory == CORBA_OBJECT_NIL) {
		gtk_object_unref (GTK_OBJECT (global_component_adapter_factory));
		global_component_adapter_factory = NULL;
	}

	return global_component_adapter_factory;
}

Nautilus_View
nautilus_component_adapter_factory_create_adapter (NautilusComponentAdapterFactory *factory,
						   BonoboObjectClient              *component)
{
	Nautilus_View nautilus_view;
	Bonobo_Control bonobo_control;
	CORBA_Environment ev;

	nautilus_view = bonobo_object_client_query_interface 
		(component, "IDL:Nautilus/View:1.0", NULL);

	CORBA_exception_init (&ev);

	if (!CORBA_Object_is_nil (nautilus_view, &ev)) {
		/* Object has the View interface, great! We might not
                   need to adapt it. */

		bonobo_control = bonobo_object_client_query_interface 
			(component, "IDL:Bonobo/Control:1.0", NULL); 

		if (!CORBA_Object_is_nil (bonobo_control, &ev)) {
			/* It has the control interface too, so all is peachy. */

			bonobo_object_release_unref (bonobo_control, &ev);
			CORBA_exception_free (&ev);

			return nautilus_view;
		} else {
			/* No control interface; we have no way to
                           support a View that doesn't also support
                           the Control interface, so fail. */

			bonobo_object_release_unref (nautilus_view, &ev);
			CORBA_exception_free (&ev);

			return CORBA_OBJECT_NIL;
		}
	} else {
		/* No View interface, we must adapt the object */

		CORBA_exception_free (&ev);
		CORBA_exception_init (&ev);
		
		nautilus_view = Nautilus_ComponentAdapterFactory_create_adapter 
			(factory->details->corba_factory,
			 bonobo_object_corba_objref (BONOBO_OBJECT (component)),
			 &ev);

		if (ev._major != CORBA_NO_EXCEPTION) {
			nautilus_view = CORBA_OBJECT_NIL;
		}

		CORBA_exception_free (&ev);

		return nautilus_view;
	}
}



