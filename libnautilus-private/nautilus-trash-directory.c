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
	GList *directories;
};

typedef struct {
	NautilusDirectory *directory;
} RealTrashDirectory;

static void nautilus_trash_directory_initialize       (gpointer   object,
						       gpointer   klass);
static void nautilus_trash_directory_initialize_class (gpointer   klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTrashDirectory,
				   nautilus_trash_directory,
				   NAUTILUS_TYPE_DIRECTORY)


static void
nautilus_trash_directory_initialize (gpointer object, gpointer klass)
{
	NautilusTrashDirectory *directory;

	directory = NAUTILUS_TRASH_DIRECTORY (object);

	directory->details = g_new0 (NautilusTrashDirectoryDetails, 1);
}

static void
trash_destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static gboolean
trash_contains_file (NautilusDirectory *directory,
		     NautilusFile *file)
{
	NautilusTrashDirectory *trash;
	GList *p;
	RealTrashDirectory *trash_directory;

	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (directory));

	trash = NAUTILUS_TRASH_DIRECTORY (trash);
	for (p = trash->details->directories; p != NULL; p = p->next) {
		trash_directory = p->data;

		if (nautilus_directory_contains_file (trash_directory->directory, file)) {
			return TRUE;
		}
	}
	return FALSE;
}

static void
trash_call_when_ready (NautilusDirectory *directory,
		       GList *file_attributes,
		       gboolean wait_for_metadata,
		       NautilusDirectoryCallback callback,
		       gpointer callback_data)
{
	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (directory));

	/* FIXME: Not implemented. */
	g_warning ("NautilusTrashDirectory: call_when_ready not implemented");
}

static void
trash_cancel_callback (NautilusDirectory *directory,
		       NautilusDirectoryCallback callback,
		       gpointer callback_data)
{
	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (directory));

	/* FIXME: Not implemented. */
	g_warning ("NautilusTrashDirectory: cancel_callback not implemented");
}

static void
trash_file_monitor_add (NautilusDirectory *directory,
			gconstpointer client,
			GList *file_attributes,
			gboolean monitor_metadata,
			gboolean force_reload,
			NautilusDirectoryCallback callback,
			gpointer callback_data)
{
	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (directory));

	/* FIXME: Not implemented. */
	g_warning ("NautilusTrashDirectory: file_monitor_add not implemented");
}

static void
trash_file_monitor_remove (NautilusDirectory *directory,
			   gconstpointer client)
{
	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (directory));

	/* FIXME: Not implemented. */
	g_warning ("NautilusTrashDirectory: file_monitor_remove not implemented");
}

static gboolean
trash_are_all_files_seen (NautilusDirectory *directory)
{
	NautilusTrashDirectory *trash;
	GList *p;
	RealTrashDirectory *trash_directory;

	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (directory));

	trash = NAUTILUS_TRASH_DIRECTORY (trash);
	for (p = trash->details->directories; p != NULL; p = p->next) {
		trash_directory = p->data;

		if (!nautilus_directory_are_all_files_seen (trash_directory->directory)) {
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
trash_is_not_empty (NautilusDirectory *directory)
{
	NautilusTrashDirectory *trash;
	GList *p;
	RealTrashDirectory *trash_directory;

	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (directory));

	trash = NAUTILUS_TRASH_DIRECTORY (trash);
	for (p = trash->details->directories; p != NULL; p = p->next) {
		trash_directory = p->data;

		if (nautilus_directory_is_not_empty (trash_directory->directory)) {
			return TRUE;
		}
	}
	return FALSE;
}

static void
nautilus_trash_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	NautilusDirectoryClass *directory_class;

	object_class = GTK_OBJECT_CLASS (klass);
	directory_class = NAUTILUS_DIRECTORY_CLASS (klass);
	
	object_class->destroy = trash_destroy;

	directory_class->contains_file = trash_contains_file;
	directory_class->call_when_ready = trash_call_when_ready;
	directory_class->cancel_callback = trash_cancel_callback;
	directory_class->file_monitor_add = trash_file_monitor_add;
	directory_class->file_monitor_remove = trash_file_monitor_remove;
	directory_class->are_all_files_seen = trash_are_all_files_seen;
	directory_class->is_not_empty = trash_is_not_empty;
}
