/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
   
   fm-vfs-directory.c: GNOME file manager directory model, VFS implementation.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@eazel.com>
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-vfs-directory.h"

#include "fm-directory-protected.h"
#include <libnautilus/nautilus-gtk-macros.h>
#include <libgnomevfs/gnome-vfs.h>

struct _FMVFSDirectoryDetails {
	GnomeVFSURI *uri;

	FMFileList *files;
};

static void fm_vfs_directory_destroy (GtkObject *object);
static void fm_vfs_directory_finalize (GtkObject *object);
static void fm_vfs_directory_get_files (FMDirectory *directory,
					FMFileListCallback callback,
					gpointer callback_data);

static GtkObjectClass *parent_class;

#define METAFILE_NAME ".gnomad.xml"

/* The process of reading a directory:

     1) Read and parse the metafile.
     2) Read the directory to notice changes.
*/

static void
fm_vfs_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	FMDirectoryClass *abstract_directory_class;

	object_class = GTK_OBJECT_CLASS (klass);
	abstract_directory_class = FM_DIRECTORY_CLASS (klass);
	parent_class = gtk_type_class (FM_TYPE_DIRECTORY);

	object_class->destroy = fm_vfs_directory_destroy;
	object_class->finalize = fm_vfs_directory_finalize;
	
	abstract_directory_class->get_files = fm_vfs_directory_get_files;
}

static void
fm_vfs_directory_initialize (gpointer object, gpointer klass)
{
	FMVFSDirectory *directory;

	directory = FM_VFS_DIRECTORY (object);

	directory->details = g_new0 (FMVFSDirectoryDetails, 1);
}

static void
fm_vfs_directory_destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
fm_vfs_directory_finalize (GtkObject *object)
{
	FMVFSDirectory *directory;

	directory = FM_VFS_DIRECTORY (object);
	g_free (directory->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, finalize, (object));
}

NAUTILUS_DEFINE_GET_TYPE_FUNCTION(FMVFSDirectory, fm_vfs_directory, FM_TYPE_DIRECTORY)

static void
fm_vfs_opened_metafile (GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			gpointer callback_data)
{
}

FMVFSDirectory *
fm_vfs_directory_new (const char* uri)
{
	FMVFSDirectory *directory;
	GnomeVFSURI *vfs_uri;
	GnomeVFSURI *metafile_uri;
	GnomeVFSAsyncHandle *metafile_handle;
	GnomeVFSResult result;

	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL)
		return NULL;

	metafile_uri = gnome_vfs_uri_append_path (vfs_uri, METAFILE_NAME);
	if (metafile_uri == NULL)
		return NULL;

	directory = gtk_type_new (FM_TYPE_VFS_DIRECTORY);

	FM_DIRECTORY (directory)->details->hash_table_key = g_strdup (uri);

	directory->details->uri = vfs_uri;

	result = gnome_vfs_async_open_uri (&metafile_handle, metafile_uri, GNOME_VFS_OPEN_READ,
					   fm_vfs_opened_metafile, directory);

	return directory;
}

static void
fm_vfs_directory_get_files (FMDirectory *abstract_directory,
			    FMFileListCallback callback,
			    gpointer callback_data)
{
	FMVFSDirectory *directory;

	g_return_if_fail (FM_IS_VFS_DIRECTORY (abstract_directory));
	g_return_if_fail (callback != NULL);

	directory = FM_VFS_DIRECTORY (abstract_directory);
	if (directory->details->files != NULL)
		(* callback) (abstract_directory,
			      directory->details->files,
			      callback_data);
}
