/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-adapter-factory-server.c - Server object for a factory to
 * create NautilusAdapter objects.
 */

#include <config.h>
#include "nautilus-adapter-factory-server.h"

#include "nautilus-adapter.h"
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-main.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus-adapter/nautilus-adapter-factory.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus/nautilus-bonobo-workarounds.h>

typedef struct {
	POA_Nautilus_ComponentAdapterFactory  servant;
	NautilusAdapterFactoryServer         *bonobo_object;
} impl_POA_Nautilus_ComponentAdapterFactory;



static Nautilus_View
impl_Nautilus_ComponentAdapterFactory_create_adapter (PortableServer_Servant           servant,
						      const Bonobo_Unknown             component,
						      CORBA_Environment               *ev);

static void
impl_Nautilus_ComponentAdapterFactory__destroy       (BonoboObject                    *object, 
						      PortableServer_Servant           servant);

static Nautilus_ComponentAdapterFactory
impl_Nautilus_ComponentAdapterFactory__create        (NautilusAdapterFactoryServer    *bonobo_object,
						      CORBA_Environment               *ev);


POA_Nautilus_ComponentAdapterFactory__epv impl_Nautilus_ComponentAdapterFactory_epv =
{
	NULL,
	&impl_Nautilus_ComponentAdapterFactory_create_adapter
};


static PortableServer_ServantBase__epv base_epv;

static POA_Nautilus_ComponentAdapterFactory__vepv impl_Nautilus_ComponentAdapterFactory_vepv =
{
	&base_epv,
	NULL,
	&impl_Nautilus_ComponentAdapterFactory_epv
};



static void nautilus_adapter_factory_server_initialize_class (NautilusAdapterFactoryServerClass *klass);
static void nautilus_adapter_factory_server_initialize       (NautilusAdapterFactoryServer      *server);
static void nautilus_adapter_factory_server_destroy          (GtkObject                      *object);


EEL_DEFINE_CLASS_BOILERPLATE (NautilusAdapterFactoryServer,
				   nautilus_adapter_factory_server,
				   BONOBO_OBJECT_TYPE)


     
static void
nautilus_adapter_factory_server_initialize_class (NautilusAdapterFactoryServerClass *klass)
{
	GtkObjectClass *object_class;
	
	g_assert (NAUTILUS_IS_ADAPTER_FACTORY_SERVER_CLASS (klass));

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_adapter_factory_server_destroy;
}

static void
nautilus_adapter_factory_server_initialize (NautilusAdapterFactoryServer *server)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	g_assert (NAUTILUS_IS_ADAPTER_FACTORY_SERVER (server));

	bonobo_object_construct
		(BONOBO_OBJECT (server),
		 impl_Nautilus_ComponentAdapterFactory__create (server, &ev));
	
	CORBA_exception_free (&ev);
}

static void
nautilus_adapter_factory_server_destroy (GtkObject *object)
{
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
adapter_object_destroyed (GtkObject *adapter, NautilusAdapterFactoryServer *server)
{
	bonobo_object_unref (BONOBO_OBJECT (server));
}


static Nautilus_View
impl_Nautilus_ComponentAdapterFactory_create_adapter (PortableServer_Servant  servant,
						      const Bonobo_Unknown    component,
						      CORBA_Environment      *ev)
{
	impl_POA_Nautilus_ComponentAdapterFactory *factory_servant;
	NautilusAdapter *adapter;
	NautilusView *adapter_view;

	factory_servant = (impl_POA_Nautilus_ComponentAdapterFactory *) servant;

	adapter = nautilus_adapter_new (component);

	if (adapter == NULL) {
		return CORBA_OBJECT_NIL;
	} else {
		bonobo_object_ref (BONOBO_OBJECT (factory_servant->bonobo_object));

		adapter_view = nautilus_adapter_get_nautilus_view (adapter);
		
		gtk_signal_connect (GTK_OBJECT (adapter_view), "destroy",
				    adapter_object_destroyed, factory_servant->bonobo_object);

		return CORBA_Object_duplicate
			(bonobo_object_corba_objref (BONOBO_OBJECT (adapter_view)), ev);
	}
}

static void
impl_Nautilus_ComponentAdapterFactory__destroy (BonoboObject           *object, 
						PortableServer_Servant  servant)
{
	PortableServer_ObjectId *object_id;
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	
	object_id = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
	PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);
	CORBA_free (object_id);

	object->servant = NULL;
	
	POA_Nautilus_ComponentAdapterFactory__fini (servant, &ev);
	g_free (servant);

	CORBA_exception_free (&ev);
}

static Nautilus_ComponentAdapterFactory
impl_Nautilus_ComponentAdapterFactory__create (NautilusAdapterFactoryServer *bonobo_object,
					       CORBA_Environment            *ev)
{
	impl_POA_Nautilus_ComponentAdapterFactory *servant;
	
	impl_Nautilus_ComponentAdapterFactory_vepv.Bonobo_Unknown_epv = nautilus_bonobo_object_get_epv ();

	servant = g_new0 (impl_POA_Nautilus_ComponentAdapterFactory, 1);
	servant->servant.vepv = &impl_Nautilus_ComponentAdapterFactory_vepv;
	POA_Nautilus_ComponentAdapterFactory__init ((PortableServer_Servant) servant, ev);

	gtk_signal_connect (GTK_OBJECT (bonobo_object), "destroy",
			    GTK_SIGNAL_FUNC (impl_Nautilus_ComponentAdapterFactory__destroy), servant);
	
	servant->bonobo_object = bonobo_object;
	return bonobo_object_activate_servant (BONOBO_OBJECT (bonobo_object), servant);
}
