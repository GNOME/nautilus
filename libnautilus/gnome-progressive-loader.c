/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-progressive-loader.c
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
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
 * Author: Ettore Perazzoli
 */

/* FIXME this is just a quick hack.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "gnome-progressive-loader.h"


static GnomeObjectClass *parent_class = NULL;

POA_GNOME_ProgressiveLoader__epv gnome_progressive_loader_epv;
POA_GNOME_ProgressiveLoader__vepv gnome_progressive_loader_vepv;


static void
impl_load (PortableServer_Servant servant,
	   const CORBA_char *uri,
	   GNOME_ProgressiveDataSink pdsink,
	   CORBA_Environment *ev)
{
	GnomeObject *object;
	GnomeProgressiveLoader *loader;
	GnomeVFSResult result;
	GNOME_ProgressiveLoader_Error *exception;

	object = gnome_object_from_servant (servant);
	loader = GNOME_PROGRESSIVE_LOADER (object);

	result = (* loader->load_fn) (loader, uri, pdsink);

	if (result == GNOME_VFS_OK)
		return;

	exception = g_new (GNOME_ProgressiveLoader_Error, 1);
	exception->vfs_result = result;

	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_GNOME_ProgressiveLoader_Error,
			     exception);
}


static GNOME_ProgressiveLoader
create_GNOME_ProgressiveLoader (GnomeObject *object)
{
	POA_GNOME_ProgressiveLoader *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_ProgressiveLoader *) g_new (GnomeObjectServant, 1);
	servant->vepv = &gnome_progressive_loader_vepv;

	CORBA_exception_init (&ev);

	POA_GNOME_ProgressiveLoader__init ((PortableServer_Servant) servant,
					   &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (GNOME_ProgressiveLoader) gnome_object_activate_servant
		(object, servant);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	GnomeProgressiveLoader *loader;

	loader = GNOME_PROGRESSIVE_LOADER (object);

	/* Nothing special.  */

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
corba_class_init (void)
{
	gnome_progressive_loader_epv.load = impl_load;

	gnome_progressive_loader_vepv.GNOME_ProgressiveLoader_epv
		= &gnome_progressive_loader_epv;
}

static void
class_init (GnomeProgressiveLoaderClass *class)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (gnome_object_get_type ());

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;

	corba_class_init ();
}

static void
init (GnomeProgressiveLoader *progressive_loader)
{
}


GtkType
gnome_progressive_loader_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"IDL:GNOME/ProgressiveLoader:1.0",
			sizeof (GnomeProgressiveLoader),
			sizeof (GnomeProgressiveLoaderClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (gnome_object_get_type (), &info);
	}

	return type;
}

gboolean
gnome_progressive_loader_construct (GnomeProgressiveLoader *loader,
				    GNOME_ProgressiveLoader corba_loader,
				    GnomeProgressiveLoaderLoadFn load_fn)
{
	g_return_val_if_fail (loader != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_PROGRESSIVE_LOADER (loader), FALSE);
	g_return_val_if_fail (corba_loader != CORBA_OBJECT_NIL, FALSE);
	g_return_val_if_fail (load_fn != NULL, FALSE);

	gnome_object_construct (GNOME_OBJECT (loader), corba_loader);

	if (corba_loader == CORBA_OBJECT_NIL) {
		corba_loader = create_GNOME_ProgressiveLoader (GNOME_OBJECT (loader));
		if (corba_loader == CORBA_OBJECT_NIL)
			return FALSE;
	}

	loader->load_fn = load_fn;

	return TRUE;
}

GnomeProgressiveLoader *
gnome_progressive_loader_new (GnomeProgressiveLoaderLoadFn load_fn)
{
	GnomeProgressiveLoader *loader;

	g_return_val_if_fail (load_fn != NULL, NULL);

	loader = gtk_type_new (gnome_progressive_loader_get_type ());

	if (! gnome_progressive_loader_construct (loader, CORBA_OBJECT_NIL,
						  load_fn)) {
		gtk_object_destroy (GTK_OBJECT (loader));
		return NULL;
	}

	return loader;
}
