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
#include <libgnomeui/gnome-stock-icons.h>
#include <libnautilus-adapter/nautilus-adapter-factory.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus/nautilus-bonobo-ui.h>

static Nautilus_View
impl_Nautilus_ComponentAdapterFactory_create_adapter (PortableServer_Servant           servant,
						      const Bonobo_Unknown             component,
						      CORBA_Environment               *ev);

static void nautilus_adapter_factory_server_class_init (NautilusAdapterFactoryServerClass *klass);
static void nautilus_adapter_factory_server_init       (NautilusAdapterFactoryServer      *server);
static void nautilus_adapter_factory_server_finalize   (GObject                           *object);

static BonoboObjectClass *parent_class;

     
static void
nautilus_adapter_factory_server_class_init (NautilusAdapterFactoryServerClass *klass)
{
	GObjectClass *object_class;
	POA_Nautilus_ComponentAdapterFactory__epv *epv = &klass->epv;

	g_assert (NAUTILUS_IS_ADAPTER_FACTORY_SERVER_CLASS (klass));

	parent_class = g_type_class_peek_parent (klass);

	epv->create_adapter = impl_Nautilus_ComponentAdapterFactory_create_adapter;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = nautilus_adapter_factory_server_finalize;
}

static void
nautilus_adapter_factory_server_init (NautilusAdapterFactoryServer *server)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	g_assert (NAUTILUS_IS_ADAPTER_FACTORY_SERVER (server));

	CORBA_exception_free (&ev);
}

static void
nautilus_adapter_factory_server_finalize (GObject *object)
{
	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
adapter_object_weak_notify (gpointer server,
			    GObject *adapter)
{
	bonobo_object_unref (BONOBO_OBJECT (server));
}


static Nautilus_View
impl_Nautilus_ComponentAdapterFactory_create_adapter (PortableServer_Servant  servant,
						      const Bonobo_Unknown    component,
						      CORBA_Environment      *ev)
{
	NautilusAdapterFactoryServer *factory_servant;
	NautilusAdapter *adapter;
	NautilusView *adapter_view;

	factory_servant = NAUTILUS_ADAPTER_FACTORY_SERVER (bonobo_object (servant));
	adapter = nautilus_adapter_new (component);

	if (adapter == NULL) {
		return CORBA_OBJECT_NIL;
	} else {
		bonobo_object_ref (BONOBO_OBJECT (factory_servant));

		adapter_view = nautilus_adapter_get_nautilus_view (adapter);
		g_object_weak_ref (G_OBJECT (adapter_view), 
				   adapter_object_weak_notify,
				   factory_servant);
		
		return CORBA_Object_duplicate (BONOBO_OBJREF (adapter_view),
					       ev);
	}
}

GType
nautilus_adapter_factory_server_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		GTypeInfo info = {
			sizeof (NautilusAdapterFactoryServerClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc)nautilus_adapter_factory_server_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (NautilusAdapterFactoryServer),
			0, /* n_preallocs */
			(GInstanceInitFunc)nautilus_adapter_factory_server_init
		};
		
		type = bonobo_type_unique 
			(BONOBO_OBJECT_TYPE,
			 POA_Nautilus_ComponentAdapterFactory__init, NULL,
			 G_STRUCT_OFFSET (NautilusAdapterFactoryServerClass, 
					  epv),
			 &info, "NautilusAdapterFactoryServer");
	}
	
	return type; 
}


