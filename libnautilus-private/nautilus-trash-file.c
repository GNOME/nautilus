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

#include "nautilus-file-utilities.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-trash-directory.h"
#include <gtk/gtksignal.h>

struct NautilusTrashFileDetails {
	NautilusTrashDirectory *as_directory;
	guint add_directory_connection_id;
	guint remove_directory_connection_id;
};

static void nautilus_trash_file_initialize       (gpointer   object,
						  gpointer   klass);
static void nautilus_trash_file_initialize_class (gpointer   klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTrashFile,
				   nautilus_trash_file,
				   NAUTILUS_TYPE_FILE)

static void
add_directory_callback (NautilusTrashDirectory *trash_directory,
			NautilusDirectory *real_directory,
			NautilusTrashFile *trash_file)
{
	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (trash_directory));
	g_assert (NAUTILUS_IS_DIRECTORY (real_directory));
	g_assert (!NAUTILUS_IS_MERGED_DIRECTORY (real_directory));
	g_assert (NAUTILUS_IS_TRASH_FILE (trash_file));
	g_assert (trash_file->details->as_directory == trash_directory);
}

static void
remove_directory_callback (NautilusTrashDirectory *trash_directory,
			   NautilusDirectory *real_directory,
			   NautilusTrashFile *trash_file)
{
	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (trash_directory));
	g_assert (NAUTILUS_IS_DIRECTORY (real_directory));
	g_assert (!NAUTILUS_IS_MERGED_DIRECTORY (real_directory));
	g_assert (NAUTILUS_IS_TRASH_FILE (trash_file));
	g_assert (trash_file->details->as_directory == trash_directory);
}

static void
nautilus_trash_file_initialize (gpointer object, gpointer klass)
{
	NautilusTrashFile *trash_file;
	NautilusTrashDirectory *trash_directory;

	trash_file = NAUTILUS_TRASH_FILE (object);

	trash_directory = NAUTILUS_TRASH_DIRECTORY (nautilus_directory_get (NAUTILUS_TRASH_URI));

	trash_file->details = g_new0 (NautilusTrashFileDetails, 1);
	trash_file->details->as_directory = trash_directory;

	trash_file->details->add_directory_connection_id = gtk_signal_connect
		(GTK_OBJECT (trash_directory),
		 "add_real_directory",
		 add_directory_callback,
		 trash_file);
	trash_file->details->remove_directory_connection_id = gtk_signal_connect
		(GTK_OBJECT (trash_directory),
		 "remove_real_directory",
		 remove_directory_callback,
		 trash_file);
}

static void
trash_destroy (GtkObject *object)
{
	NautilusTrashFile *trash_file;
	NautilusTrashDirectory *trash_directory;

	trash_file = NAUTILUS_TRASH_FILE (object);
	trash_directory = trash_file->details->as_directory;

	gtk_signal_disconnect (GTK_OBJECT (trash_directory),
			       trash_file->details->add_directory_connection_id);
	gtk_signal_disconnect (GTK_OBJECT (trash_directory),
			       trash_file->details->remove_directory_connection_id);
	nautilus_directory_unref (NAUTILUS_DIRECTORY (trash_directory));
	g_free (trash_file->details);

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
