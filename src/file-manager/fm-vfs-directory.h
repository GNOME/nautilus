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

#ifndef FM_VFS_DIRECTORY_H
#define FM_VFS_DIRECTORY_H

#include "fm-directory.h"

/* FMVFSDirectory is the concrete VFS implementation of FMDirectory. */

typedef struct _FMVFSDirectory FMVFSDirectory;
typedef struct _FMVFSDirectoryClass FMVFSDirectoryClass;

#define FM_TYPE_VFS_DIRECTORY \
	(fm_vfs_directory_get_type ())
#define FM_VFS_DIRECTORY(obj) \
	(GTK_CHECK_CAST ((obj), FM_TYPE_VFS_DIRECTORY, FMVFSDirectory))
#define FM_VFS_DIRECTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_VFS_DIRECTORY, FMVFSDirectoryClass))
#define FM_IS_VFS_DIRECTORY(obj) \
	(GTK_CHECK_TYPE ((obj), FM_TYPE_VFS_DIRECTORY))
#define FM_IS_VFS_DIRECTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_VFS_DIRECTORY))

typedef struct _FMVFSDirectoryDetails FMVFSDirectoryDetails;

struct _FMVFSDirectory
{
	FMDirectory abstract_directory;

	FMVFSDirectoryDetails *details;
};

struct _FMVFSDirectoryClass
{
	FMDirectoryClass parent_class;
};

/* Basic GtkObject requirements. */
GtkType         fm_vfs_directory_get_type (void);
FMVFSDirectory *fm_vfs_directory_new      (const char *uri);

#endif /* FM_VFS_DIRECTORY_H */
