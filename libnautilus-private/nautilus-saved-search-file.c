/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-saved-search-file.h: Subclass of NautilusVFSFile to implement the
   the case of a Saved Search file.
 
   Copyright (C) 2005 Red Hat, Inc
  
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
  
   Author: Alexander Larsson
*/
#include <config.h>
#include "nautilus-saved-search-file.h"
#include <eel/eel-gtk-macros.h>

static void nautilus_saved_search_file_init (gpointer object, gpointer klass);
static void nautilus_saved_search_file_class_init (gpointer klass);

EEL_CLASS_BOILERPLATE (NautilusSavedSearchFile,
		       nautilus_saved_search_file,
		       NAUTILUS_TYPE_VFS_FILE)



static void
nautilus_saved_search_file_init (gpointer object, gpointer klass)
{
	NautilusVFSFile *file;

	file = NAUTILUS_VFS_FILE (object);

}

static GnomeVFSFileType
saved_search_get_file_type (NautilusFile *file)
{
	return GNOME_VFS_FILE_TYPE_DIRECTORY;
}

static void
nautilus_saved_search_file_class_init (gpointer klass)
{
	NautilusFileClass *file_class;

	file_class = NAUTILUS_FILE_CLASS (klass);

	file_class->get_file_type = saved_search_get_file_type;
}
  
