/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-trash-directory.c: Subclass of NautilusDirectory to implement the
   virtual trash directory.
 
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

#include <config.h>
#include "nautilus-trash-directory.h"

#include "nautilus-gtk-macros.h"

struct NautilusTrashDirectoryDetails {
};

static void nautilus_trash_directory_destroy          (GtkObject *object);
static void nautilus_trash_directory_initialize       (gpointer   object,
						       gpointer   klass);
static void nautilus_trash_directory_initialize_class (gpointer   klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTrashDirectory,
				   nautilus_trash_directory,
				   NAUTILUS_TYPE_DIRECTORY)

static void
nautilus_trash_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_trash_directory_destroy;
}

static void
nautilus_trash_directory_initialize (gpointer object, gpointer klass)
{
	NautilusTrashDirectory *directory;

	directory = NAUTILUS_TRASH_DIRECTORY (object);

	directory->details = g_new0 (NautilusTrashDirectoryDetails, 1);
}

static void
nautilus_trash_directory_destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}
