/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* dfos-corba.c - Implementation of the GNOME::Desktop::FileOperationService
   CORBA server.

   Copyright (C) 1999, 2000 Free Software Foundation

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Ettore Perazzoli <ettore@gnu.org> */

#include <config.h>

#include <liboaf/liboaf.h>

#include "dfos.h"

#include <libgnome/gnome-i18n.h>


struct _FileOperationServiceServant {
	POA_GNOME_Desktop_FileOperationService servant;
	PortableServer_POA poa;
	DFOS *dfos;
};
typedef struct _FileOperationServiceServant FileOperationServiceServant;

static PortableServer_ServantBase__epv FileOperationService_base_epv;
static POA_GNOME_Desktop_FileOperationService__epv FileOperationService_epv;
static POA_GNOME_Desktop_FileOperationService__vepv FileOperationService_vepv;


/* Utility functions.  */

static GList *
file_name_list_to_g_list (const GNOME_Desktop_FileOperationService_FileNameList
			          *file_list)
{
	GList *new;
	guint i;

	new = NULL;

	i = file_list->_length;
	while (i > 0) {
		i--;
		new = g_list_prepend (new, file_list->_buffer[i]);
	}

	return new;
}


/* CORBA -> VFS parameter conversion routines.  */

static GnomeVFSXferOptions
convert_options (GNOME_Desktop_FileOperationService_XferOptions options)
{
	GnomeVFSXferOptions returned_options;

	returned_options = 0;

#if 0
	if (options & GNOME_Desktop_FileOperationService_XferOptionPreserve) 
		returned_options |= GNOME_VFS_XFER_PRESERVE;
	if (options & GNOME_Desktop_FileOperationService_XferOptionFollowLinks) 
		returned_options |= GNOME_VFS_XFER_FOLLOW_LINKS;
	if (options & GNOME_Desktop_FileOperationService_XferOptionWithParents) 
		returned_options |= GNOME_VFS_XFER_WITHPARENTS;
	if (options & GNOME_Desktop_FileOperationService_XferOptionRecursive) 
		returned_options |= GNOME_VFS_XFER_RECURSIVE;
	if (options & GNOME_Desktop_FileOperationService_XferOptionSameFS) 
		returned_options |= GNOME_VFS_XFER_SAMEFS;
	if (options & GNOME_Desktop_FileOperationService_XferOptionSparseAlways) 
		returned_options |= GNOME_VFS_XFER_SPARSE_ALWAYS;
	if (options & GNOME_Desktop_FileOperationService_XferOptionSparseNever) 
		returned_options |= GNOME_VFS_XFER_SPARSE_NEVER;
	if (options & GNOME_Desktop_FileOperationService_XferOptionUpdateMode) 
		returned_options |= GNOME_VFS_XFER_UPDATEMODE;
	if (options & GNOME_Desktop_FileOperationService_XferOptionRemoveSource) 
		returned_options |= GNOME_VFS_XFER_REMOVESOURCE;
#endif

	return returned_options;
}

static GnomeVFSXferOverwriteMode
convert_overwrite_mode (GNOME_Desktop_FileOperationService_XferOverwriteMode mode)
{
	switch (mode) {
	case GNOME_Desktop_FileOperationService_XferOverwriteAbort:
		return GNOME_VFS_XFER_OVERWRITE_MODE_ABORT;
	case GNOME_Desktop_FileOperationService_XferOverwriteQuery:
		return GNOME_VFS_XFER_OVERWRITE_MODE_QUERY;
	case GNOME_Desktop_FileOperationService_XferOverwriteReplace:
		return GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE;
	case GNOME_Desktop_FileOperationService_XferOverwriteSkip:
		return GNOME_VFS_XFER_OVERWRITE_MODE_SKIP;
	default:
		g_warning (_("Unknown XferOverwriteMode %d"), mode);
		return GNOME_VFS_XFER_OVERWRITE_MODE_ABORT;
	}
}

static GNOME_Desktop_FileOperationService_XferErrorMode
convert_error_mode (GnomeVFSXferErrorMode mode)
{
	switch (mode) {
	case GNOME_Desktop_FileOperationService_XferErrorAbort:
		return GNOME_VFS_XFER_ERROR_MODE_ABORT;
	case GNOME_Desktop_FileOperationService_XferErrorQuery:
		return GNOME_VFS_XFER_ERROR_MODE_QUERY;
	default:
		g_warning (_("Unknown XferErrorMode %d"), mode);
		return GNOME_VFS_XFER_ERROR_MODE_ABORT;
	}
}


/* GNOME::Desktop::FileOperationService method implementations.  */

static void
impl_FileOperationService_xfer (PortableServer_Servant servant,
				const CORBA_char *source_directory_uri,
				const GNOME_Desktop_FileOperationService_FileNameList *source_file_names,
				const CORBA_char * target_directory_uri,
				const GNOME_Desktop_FileOperationService_FileNameList *target_file_names,
				const GNOME_Desktop_FileOperationService_XferOptions options,
				const GNOME_Desktop_FileOperationService_XferErrorMode error_mode,
				const GNOME_Desktop_FileOperationService_XferOverwriteMode overwrite_mode,
				CORBA_Environment *ev)
{
	GList *source_file_name_list;
	GList *target_file_name_list;
	GnomeVFSXferOptions vfs_options;
	GnomeVFSXferOverwriteMode vfs_overwrite_mode;
	GnomeVFSXferErrorMode vfs_error_mode;
	DFOS *dfos;

	dfos = ((FileOperationServiceServant *) servant)->dfos;

	source_file_name_list = file_name_list_to_g_list (source_file_names);
	target_file_name_list = file_name_list_to_g_list (target_file_names);

	vfs_options = convert_options (options);
	vfs_error_mode = convert_error_mode (error_mode);
	vfs_overwrite_mode = convert_overwrite_mode (overwrite_mode);

	dfos_xfer (dfos,
		   source_directory_uri, source_file_name_list,
		   target_directory_uri, target_file_name_list,
		   vfs_options, vfs_error_mode, vfs_overwrite_mode);

	/* Notice that we don't have to deallocate the strings, because we have
           copied pointers from the CORBA parameters which we are not supposed
           to free.  */
	g_list_free (source_file_name_list);
	g_list_free (target_file_name_list);
}


static GNOME_Desktop_FileOperationService
create_server (DFOS *dfos,
	       PortableServer_POA poa,
	       CORBA_Environment *ev)
{
	FileOperationServiceServant *servant;

	/* Set up vtables.  */

	FileOperationService_base_epv._private = NULL;
	FileOperationService_base_epv.finalize = NULL;
	FileOperationService_base_epv.default_POA = NULL;

	FileOperationService_epv.xfer = (gpointer)impl_FileOperationService_xfer;

	FileOperationService_vepv._base_epv = &FileOperationService_base_epv;
	FileOperationService_vepv.GNOME_Desktop_FileOperationService_epv =
		&FileOperationService_epv;

	servant = g_new0 (FileOperationServiceServant, 1);
	servant->servant.vepv = &FileOperationService_vepv;
	servant->poa = poa;
	servant->dfos = dfos;

	POA_GNOME_Desktop_FileOperationService__init
		((PortableServer_Servant) servant, ev);
	if (ev->_major != CORBA_NO_EXCEPTION){
		g_free (servant);
		return CORBA_OBJECT_NIL;
	}

	CORBA_free (PortableServer_POA_activate_object (poa, servant, ev));

	return PortableServer_POA_servant_to_reference (poa, servant, ev);
}

static GNOME_Desktop_FileOperationService
init (DFOS *dfos,
      CORBA_Environment *ev)
{
	GNOME_Desktop_FileOperationService objref;
	PortableServer_POA poa;
	PortableServer_POAManager poa_manager;

	poa = (PortableServer_POA) CORBA_ORB_resolve_initial_references
		(oaf_orb_get (), "RootPOA", ev);
	if (ev->_major != CORBA_NO_EXCEPTION)
		return FALSE;

	poa_manager = PortableServer_POA__get_the_POAManager (poa, ev);
	if (ev->_major != CORBA_NO_EXCEPTION)
		return FALSE;

	PortableServer_POAManager_activate (poa_manager, ev);
	if (ev->_major != CORBA_NO_EXCEPTION)
		return FALSE;

	objref = create_server (dfos, poa, ev);
	if (ev->_major != CORBA_NO_EXCEPTION)
		return CORBA_OBJECT_NIL;

	if (! oaf_active_server_register ("IDL:GNOME:Desktop:FileOperationService:1.0", objref))
	    return objref;

	return CORBA_OBJECT_NIL;
}


GNOME_Desktop_FileOperationService
dfos_corba_init (DFOS *dfos)
{
	GNOME_Desktop_FileOperationService objref;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	objref = init (dfos, &ev);
	CORBA_exception_free (&ev);

	return objref;
}
