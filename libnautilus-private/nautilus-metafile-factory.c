/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/* nautilus-metafile.c - server side of Nautilus::MetafileFactory
 *
 * Copyright (C) 2001 Eazel, Inc.
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
#include "nautilus-metafile-factory.h"
#include "nautilus-metafile.h"

#include <eel/eel-gtk-macros.h>
#include "nautilus-bonobo-extensions.h"
#include <libnautilus/nautilus-bonobo-workarounds.h>

struct NautilusMetafileFactoryDetails {
	char dummy; /* ANSI C does not allow empty structs */
};

static void nautilus_metafile_factory_init       (NautilusMetafileFactory      *factory);
static void nautilus_metafile_factory_class_init (NautilusMetafileFactoryClass *klass);

static void destroy (GtkObject *factory);

static Nautilus_Metafile corba_open (PortableServer_Servant  servant,
				     const Nautilus_URI      directory,
				     CORBA_Environment      *ev);

NAUTILUS_BONOBO_X_BOILERPLATE (NautilusMetafileFactory, Nautilus_MetafileFactory, BONOBO_X_OBJECT_TYPE, nautilus_metafile_factory)

static void
nautilus_metafile_factory_class_init (NautilusMetafileFactoryClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = destroy;

	klass->epv.open = corba_open;
}

static void
nautilus_metafile_factory_init (NautilusMetafileFactory *factory)
{
	factory->details = g_new0 (NautilusMetafileFactoryDetails, 1);
}

static void
destroy (GtkObject *object)
{
	NautilusMetafileFactory *factory;

	factory = NAUTILUS_METAFILE_FACTORY (object);
	g_free (factory->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static NautilusMetafileFactory *
nautilus_metafile_factory_new (void)
{
	NautilusMetafileFactory *metafile_factory;
	metafile_factory = NAUTILUS_METAFILE_FACTORY (gtk_object_new (NAUTILUS_TYPE_METAFILE_FACTORY, NULL));
	return metafile_factory;
}

static NautilusMetafileFactory *the_factory;

static void
free_factory_instance (void)
{
	bonobo_object_unref (BONOBO_OBJECT (the_factory));
	the_factory = NULL;
}

NautilusMetafileFactory *
nautilus_metafile_factory_get_instance (void)
{
	if (the_factory == NULL) {
		the_factory = nautilus_metafile_factory_new ();
		g_atexit (free_factory_instance);
	}
	
	bonobo_object_ref (BONOBO_OBJECT (the_factory));
	
	return the_factory;
}

static Nautilus_Metafile
corba_open (PortableServer_Servant  servant,
	    const Nautilus_URI      directory,
	    CORBA_Environment      *ev)
{
	BonoboObject *object;
	object = BONOBO_OBJECT (nautilus_metafile_get (directory));
	return CORBA_Object_duplicate (bonobo_object_corba_objref (object), ev);
}
