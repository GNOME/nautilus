/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

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
#include <bonobo/bonobo-macros.h>
#include <eel/eel-debug.h>

static NautilusMetafileFactory *the_factory;

BONOBO_CLASS_BOILERPLATE_FULL (NautilusMetafileFactory, nautilus_metafile_factory,
			       Nautilus_MetafileFactory,
			       BonoboObject, BONOBO_OBJECT_TYPE)

static void
nautilus_metafile_factory_instance_init (NautilusMetafileFactory *factory)
{
}

static NautilusMetafileFactory *
nautilus_metafile_factory_new (void)
{
	return NAUTILUS_METAFILE_FACTORY
		(g_object_new (NAUTILUS_TYPE_METAFILE_FACTORY, NULL));
}

static Nautilus_Metafile
corba_open (PortableServer_Servant servant,
	    const CORBA_char *directory,
	    CORBA_Environment *ev)
{
	NautilusMetafile *metafile;
	Nautilus_Metafile objref;

	metafile = nautilus_metafile_get (directory);
	objref = bonobo_object_dup_ref (BONOBO_OBJREF (metafile), NULL);
	bonobo_object_unref (metafile);
	return objref;
}

static void
nautilus_metafile_factory_class_init (NautilusMetafileFactoryClass *class)
{
	class->epv.open = corba_open;
}

static void
free_factory_instance (void)
{
	bonobo_object_unref (the_factory);
	the_factory = NULL;
}

NautilusMetafileFactory *
nautilus_metafile_factory_get_instance (void)
{
	if (the_factory == NULL) {
		the_factory = nautilus_metafile_factory_new ();
		eel_debug_call_at_shutdown (free_factory_instance);
	}

	return the_factory;
}
