/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/* nautilus-metafile.c - server side of Nautilus::Metafile
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
#include "nautilus-metafile.h"
#include "nautilus-metafile-server.h"

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus/nautilus-bonobo-workarounds.h>

struct NautilusMetafileDetails {
	NautilusDirectory *directory;
};

static void nautilus_metafile_initialize       (NautilusMetafile      *metafile);
static void nautilus_metafile_initialize_class (NautilusMetafileClass *klass);

static void destroy (GtkObject *metafile);

static CORBA_char *corba_get		      (PortableServer_Servant  servant,
					       const CORBA_char       *file_name,
					       const CORBA_char       *key,
					       const CORBA_char       *default_value,
					       CORBA_Environment      *ev);
static Nautilus_MetadataList *corba_get_list (PortableServer_Servant  servant,
					       const CORBA_char      *file_name,
					       const CORBA_char      *list_key,
					       const CORBA_char      *list_subkey,
					       CORBA_Environment     *ev);

static CORBA_boolean corba_set      (PortableServer_Servant  servant,
				     const CORBA_char       *file_name,
				     const CORBA_char       *key,
				     const CORBA_char       *default_value,
				     const CORBA_char       *metadata,
				     CORBA_Environment      *ev);
static CORBA_boolean corba_set_list (PortableServer_Servant       servant,
				     const CORBA_char            *file_name,
				     const CORBA_char            *list_key,
				     const CORBA_char            *list_subkey,
				     const Nautilus_MetadataList *list,
				     CORBA_Environment           *ev);
					       
static void corba_copy  (PortableServer_Servant   servant,
			 const Nautilus_Metafile  source_metafile,
			 const CORBA_char        *source_file_name,
			 const CORBA_char        *destination_file_name,
			 CORBA_Environment       *ev);
static void corba_remove (PortableServer_Servant  servant,
			 const CORBA_char        *file_name,
			 CORBA_Environment       *ev);
static void corba_rename (PortableServer_Servant  servant,
			 const CORBA_char        *old_file_name,
			 const CORBA_char        *new_file_name,
			 CORBA_Environment       *ev);

static void corba_register_monitor   (PortableServer_Servant          servant,
				      const Nautilus_MetafileMonitor  monitor,
				      CORBA_Environment              *ev);
static void corba_unregister_monitor (PortableServer_Servant          servant,
				      const Nautilus_MetafileMonitor  monitor,
				      CORBA_Environment              *ev);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusMetafile, nautilus_metafile, BONOBO_OBJECT_TYPE)

static void
nautilus_metafile_initialize_class (NautilusMetafileClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = destroy;
}

static POA_Nautilus_Metafile__epv *
nautilus_metafile_get_epv (void)
{
	static POA_Nautilus_Metafile__epv epv;
	
	epv.get                = corba_get;
	epv.get_list           = corba_get_list;
	epv.set                = corba_set;
	epv.set_list           = corba_set_list;
	epv.copy               = corba_copy;
	epv.remove             = corba_remove;
	epv.rename             = corba_rename;
	epv.register_monitor   = corba_register_monitor;
	epv.unregister_monitor = corba_unregister_monitor;

	return &epv;
}

static POA_Nautilus_Metafile__vepv *
nautilus_metafile_get_vepv (void)
{
	static POA_Nautilus_Metafile__vepv vepv;

	vepv.Bonobo_Unknown_epv = nautilus_bonobo_object_get_epv ();
	vepv.Nautilus_Metafile_epv = nautilus_metafile_get_epv ();

	return &vepv;
}

static POA_Nautilus_Metafile *
nautilus_metafile_create_servant (void)
{
	POA_Nautilus_Metafile *servant;
	CORBA_Environment ev;

	servant = (POA_Nautilus_Metafile *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = nautilus_metafile_get_vepv ();
	CORBA_exception_init (&ev);
	POA_Nautilus_Metafile__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_error ("can't initialize Nautilus metafile");
	}
	CORBA_exception_free (&ev);

	return servant;
}

static void
nautilus_metafile_initialize (NautilusMetafile *metafile)
{
	Nautilus_Metafile corba_metafile;

	metafile->details = g_new0 (NautilusMetafileDetails, 1);

	corba_metafile = bonobo_object_activate_servant
		(BONOBO_OBJECT (metafile), nautilus_metafile_create_servant ());
	bonobo_object_construct (BONOBO_OBJECT (metafile), corba_metafile);
}

static void
destroy (GtkObject *object)
{
	NautilusMetafile *metafile;

	metafile = NAUTILUS_METAFILE (object);
	nautilus_directory_unref (metafile->details->directory);
	g_free (metafile->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusMetafile *
nautilus_metafile_new (const char *directory_uri)
{
	NautilusMetafile *metafile;
	metafile = NAUTILUS_METAFILE (gtk_object_new (NAUTILUS_TYPE_METAFILE, NULL));
	metafile->details->directory = nautilus_directory_get (directory_uri);
	return metafile;
}

static CORBA_char *
corba_get (PortableServer_Servant  servant,
	   const CORBA_char       *file_name,
	   const CORBA_char       *key,
	   const CORBA_char       *default_value,
	   CORBA_Environment      *ev)
{
	/* mse-evil */
	return CORBA_OBJECT_NIL;
}

static Nautilus_MetadataList *
corba_get_list (PortableServer_Servant  servant,
	        const CORBA_char       *file_name,
	        const CORBA_char       *list_key,
	        const CORBA_char       *list_subkey,
	        CORBA_Environment      *ev)
{
	/* mse-evil */
	return CORBA_OBJECT_NIL;
}

static CORBA_boolean
corba_set (PortableServer_Servant  servant,
	   const CORBA_char       *file_name,
	   const CORBA_char       *key,
	   const CORBA_char       *default_value,
	   const CORBA_char       *metadata,
	   CORBA_Environment      *ev)
{
	/* mse-evil */
	return CORBA_FALSE;
}

static CORBA_boolean
corba_set_list (PortableServer_Servant      servant,
		const CORBA_char            *file_name,
		const CORBA_char            *list_key,
		const CORBA_char            *list_subkey,
		const Nautilus_MetadataList *list,
		CORBA_Environment           *ev)
{
	/* mse-evil */
	return CORBA_FALSE;
}
					       
static void
corba_copy (PortableServer_Servant   servant,
	    const Nautilus_Metafile  source_metafile,
	    const CORBA_char        *source_file_name,
	    const CORBA_char        *destination_file_name,
	    CORBA_Environment       *ev)
{
	/* mse-evil */
}

static void
corba_remove (PortableServer_Servant  servant,
	      const CORBA_char       *file_name,
	      CORBA_Environment      *ev)
{
	/* mse-evil */
}

static void
corba_rename (PortableServer_Servant  servant,
	      const CORBA_char       *old_file_name,
	      const CORBA_char       *new_file_name,
	      CORBA_Environment      *ev)
{
	/* mse-evil */
}

static void
corba_register_monitor (PortableServer_Servant          servant,
			const Nautilus_MetafileMonitor  monitor,
			CORBA_Environment              *ev)
{
	/* mse-evil */
}

static void
corba_unregister_monitor (PortableServer_Servant          servant,
			  const Nautilus_MetafileMonitor  monitor,
			  CORBA_Environment              *ev)
{
	/* mse-evil */
}
