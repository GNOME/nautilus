/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   fm-directory.c: GNOME file manager directory model.
 
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

#include "fm-directory.h"

#include "fm-directory-protected.h"
#include "fm-vfs-directory.h"
#include <libnautilus/nautilus-gtk-macros.h>
#include "../nautilus-self-check-functions.h"

static void fm_directory_destroy (GtkObject *object);
static void fm_directory_finalize (GtkObject *object);

static GtkObjectClass *parent_class;
static GHashTable* directory_objects;

static void
fm_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

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
FMDirectory *fm_directory_get(const char *uri)
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

/* self check code */

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void self_check_fm_directory(void)
{
	FMDirectory *directory;

	directory = fm_directory_get("file:///etc");
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
