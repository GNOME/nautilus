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

/* nautilus-component-adapter-factory.h - client wrapper for the
 * special adapter component, which wraps Bonobo components as
 * Nautilus Views and in the process keeps evil syncrhonous I/O out of
 * the Nautilus process itself.
 */

#ifndef NAUTILUS_COMPONENT_ADAPTER_FACTORY_H
#define NAUTILUS_COMPONENT_ADAPTER_FACTORY_H

#include <gtk/gtkobject.h>
#include <bonobo/bonobo-object-client.h>
#include <libnautilus/nautilus-view-component.h>

typedef struct NautilusComponentAdapterFactory NautilusComponentAdapterFactory;
typedef struct NautilusComponentAdapterFactoryClass NautilusComponentAdapterFactoryClass;

typedef struct NautilusComponentAdapterFactoryDetails NautilusComponentAdapterFactoryDetails;

#define NAUTILUS_TYPE_COMPONENT_ADAPTER_FACTORY \
	(nautilus_component_adapter_factory_get_type ())
#define NAUTILUS_COMPONENT_ADAPTER_FACTORY(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_COMPONENT_ADAPTER_FACTORY, NautilusComponentAdapterFactory))
#define NAUTILUS_COMPONENT_ADAPTER_FACTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_COMPONENT_ADAPTER_FACTORY, NautilusComponentAdapterFactoryClass))
#define NAUTILUS_IS_COMPONENT_ADAPTER_FACTORY(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_COMPONENT_ADAPTER_FACTORY))
#define NAUTILUS_IS_COMPONENT_ADAPTER_FACTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_COMPONENT_ADAPTER_FACTORY))

struct NautilusComponentAdapterFactory
{
	GtkObject object;
	NautilusComponentAdapterFactoryDetails *details;
};

struct NautilusComponentAdapterFactoryClass
{
	GtkObjectClass parent_class;
};


GtkType                          nautilus_component_adapter_factory_get_type  (void);

NautilusComponentAdapterFactory *nautilus_component_adapter_factory_get       (void);

Nautilus_View nautilus_component_adapter_factory_create_adapter               (NautilusComponentAdapterFactory *factory,
									       BonoboObjectClient              *component);


#endif /* NAUTILUS_COMPONENT_ADAPTER_FACTORY_H */


