/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
   
   fm-vfs-directory.c: GNOME file manager directory model, VFS implementation.
 
   Copyright (C) 1999 Eazel, Inc.
  
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

struct _FMVFSDirectoryDetails {
	GList *files;

  /*	GnomeVFSAsyncHandle *async_handle; */
};

static void fm_vfs_directory_destroy (GtkObject *object);
static void fm_vfs_directory_finalize (GtkObject *object);

static GtkObjectClass *parent_class;

static void
fm_vfs_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class (FM_TYPE_DIRECTORY);

	object_class->destroy = fm_vfs_directory_destroy;
	object_class->finalize = fm_vfs_directory_finalize;
}

static void
fm_vfs_directory_initialize (gpointer object, gpointer klass)
{
	FMVFSDirectory *directory;

	directory = FM_VFS_DIRECTORY(object);

	directory->details = g_new0 (FMVFSDirectoryDetails, 1);
}

static void
fm_vfs_directory_destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS(GTK_OBJECT_CLASS, destroy, (object));
}

static void
fm_vfs_directory_finalize (GtkObject *object)
{
	FMVFSDirectory *directory;

	directory = FM_VFS_DIRECTORY (object);
	g_free (directory->details);
	
	NAUTILUS_CALL_PARENT_CLASS(GTK_OBJECT_CLASS, finalize, (object));
}

NAUTILUS_DEFINE_GET_TYPE_FUNCTION(FMVFSDirectory, fm_vfs_directory, FM_TYPE_DIRECTORY)

FMVFSDirectory *
fm_vfs_directory_new (const char* uri)
{
	FMVFSDirectory *directory;

	directory = gtk_type_new (FM_TYPE_VFS_DIRECTORY);

	FM_DIRECTORY(directory)->details->hash_table_key = g_strdup(uri);

	return directory;
}
