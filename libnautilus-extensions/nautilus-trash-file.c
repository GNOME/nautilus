/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-trash-file.c: Subclass of NautilusFile to help implement the
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
#include "nautilus-trash-file.h"

#include "nautilus-gtk-macros.h"

struct NautilusTrashFileDetails {
};

static void nautilus_trash_file_initialize       (gpointer   object,
						  gpointer   klass);
static void nautilus_trash_file_initialize_class (gpointer   klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTrashFile,
				   nautilus_trash_file,
				   NAUTILUS_TYPE_FILE)

static void
nautilus_trash_file_initialize (gpointer object, gpointer klass)
{
	NautilusTrashFile *file;

	file = NAUTILUS_TRASH_FILE (object);

	file->details = g_new0 (NautilusTrashFileDetails, 1);
}

static void
trash_destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_trash_file_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	NautilusFileClass *file_class;

	object_class = GTK_OBJECT_CLASS (klass);
	file_class = NAUTILUS_FILE_CLASS (klass);
	
	object_class->destroy = trash_destroy;

	/* file_class-> */
}
