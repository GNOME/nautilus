/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   fm-directory.c: GNOME file manager directory model.
 
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

#include "fm-directory.h"

#include "fm-directory-protected.h"
#include "fm-vfs-directory.h"
#include <gtk/gtksignal.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include "../nautilus-self-check-functions.h"

static void fm_directory_destroy (GtkObject *object);
static void fm_directory_finalize (GtkObject *object);

enum {
	GET_FILES,
	LAST_SIGNAL
};

static GtkObjectClass *parent_class;
static guint signals[LAST_SIGNAL];

static GHashTable* directory_objects;

static void
fm_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class (GTK_TYPE_OBJECT);
	
	signals[GET_FILES] =
		gtk_signal_new ("get_files",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (FMDirectoryClass, get_files),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE,
				2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	object_class->destroy = fm_directory_destroy;
	object_class->finalize = fm_directory_finalize;
}

static void
fm_directory_initialize (gpointer object, gpointer klass)
{
	FMDirectory *directory;

	directory = FM_DIRECTORY(object);

	directory->details = g_new0 (FMDirectoryDetails, 1);
}

static void
fm_directory_destroy (GtkObject *object)
{
	FMDirectory *directory;

	directory = FM_DIRECTORY (object);
	g_hash_table_remove (directory_objects, directory->details->hash_table_key);

	NAUTILUS_CALL_PARENT_CLASS(GTK_OBJECT_CLASS, destroy, (object));
}

static void
fm_directory_finalize (GtkObject *object)
{
	FMDirectory *directory;

	directory = FM_DIRECTORY (object);
	g_free (directory->details->hash_table_key);
	g_free (directory->details);

	NAUTILUS_CALL_PARENT_CLASS(GTK_OBJECT_CLASS, finalize, (object));
}

NAUTILUS_DEFINE_GET_TYPE_FUNCTION(FMDirectory, fm_directory, GTK_TYPE_OBJECT)

/**
 * fm_directory_get:
 * @uri: URI of directory to get.
 *
 * Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
FMDirectory *
fm_directory_get(const char *uri)
{
	FMDirectory *directory;

	g_return_val_if_fail(uri != NULL, NULL);

	/* FIXME: This currently ignores the issue of two uris that are not identical but point
	   to the same data.
	*/

	/* Create the hash table first time through. */
	if (!directory_objects)
		directory_objects = g_hash_table_new(g_str_hash, g_str_equal);

	/* If the object is already in the hash table, look it up. */
	directory = g_hash_table_lookup(directory_objects, uri);
	if (directory != NULL) {
		g_assert(FM_IS_DIRECTORY(directory));
		gtk_object_ref(GTK_OBJECT(directory));
	} else {
		/* Create a new directory object instead. */
		directory = FM_DIRECTORY(fm_vfs_directory_new(uri));
		g_assert(strcmp(directory->details->hash_table_key, uri) == 0);

		/* Put it in the hash table. */
		gtk_object_ref(GTK_OBJECT(directory));
		gtk_object_sink(GTK_OBJECT(directory));
		g_hash_table_insert(directory_objects, directory->details->hash_table_key, directory);
	}

	return directory;
}

void
fm_directory_get_files(FMDirectory *directory,
		       FMFileListCallback callback,
		       gpointer callback_data)
{
	g_return_if_fail(FM_IS_DIRECTORY(directory));
	g_return_if_fail(callback);

	gtk_signal_emit(GTK_OBJECT(directory), signals[GET_FILES], callback, callback_data);
}

/* self check code */

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

static int data_dummy;
static guint file_count;

static void
get_files_cb(FMDirectory *directory, FMFileList *files, gpointer data)
{
	g_assert(FM_IS_DIRECTORY(directory));
	g_assert(files);
	g_assert(data == &data_dummy);

	file_count += g_list_length(files);
}

void
nautilus_self_check_fm_directory(void)
{
	FMDirectory *directory;

	directory = fm_directory_get("file:///etc");

	g_assert(g_hash_table_size(directory_objects) == 1);

	file_count = 0;
	fm_directory_get_files(directory, get_files_cb, &data_dummy);

	gtk_object_unref(GTK_OBJECT(directory));

	g_assert(g_hash_table_size(directory_objects) == 0);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
