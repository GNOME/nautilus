/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory.c: Mautilus directory model.
 
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

#include "nautilus-directory.h"

#include <stdlib.h>

#include <gtk/gtkmain.h>

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>

#include <gnome-xml/parser.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/xmlmemory.h>

#include "nautilus-gtk-macros.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-string.h"

#define METAFILE_NAME ".nautilus.xml"
#define METAFILE_XML_VERSION "1.0"

static void nautilus_directory_initialize_class (gpointer klass);
static void nautilus_directory_initialize (gpointer object, gpointer klass);
static void nautilus_directory_finalize (GtkObject *object);

static NautilusDirectory *nautilus_directory_new (const char* uri);

static void nautilus_directory_read_metafile (NautilusDirectory *directory);
static void nautilus_directory_write_metafile (NautilusDirectory *directory);
static void nautilus_directory_request_write_metafile (NautilusDirectory *directory);
static void nautilus_directory_remove_write_metafile_idle (NautilusDirectory *directory);

NAUTILUS_DEFINE_GET_TYPE_FUNCTION (NautilusDirectory, nautilus_directory, GTK_TYPE_OBJECT)

static GtkObjectClass *parent_class;

struct _NautilusDirectoryDetails
{
	char *uri_text;
	GnomeVFSURI *uri;

	GnomeVFSURI *metafile_uri;
	xmlDoc *metafile_tree;
	int write_metafile_idle_id;

	NautilusFileList *files;
};

struct _NautilusFile
{
};

static GHashTable* directory_objects;

static void
nautilus_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class (GTK_TYPE_OBJECT);
	
	object_class->finalize = nautilus_directory_finalize;
}

static void
nautilus_directory_initialize (gpointer object, gpointer klass)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY(object);

	directory->details = g_new0 (NautilusDirectoryDetails, 1);
}

static void
nautilus_directory_finalize (GtkObject *object)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (object);

	g_hash_table_remove (directory_objects, directory->details->uri_text);

	g_free (directory->details->uri_text);
	if (directory->details->uri)
		gnome_vfs_uri_unref (directory->details->uri);
	if (directory->details->metafile_uri)
		gnome_vfs_uri_unref (directory->details->metafile_uri);
	xmlFreeDoc (directory->details->metafile_tree);
	nautilus_directory_remove_write_metafile_idle (directory);

	g_free (directory->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, finalize, (object));
}

/**
 * nautilus_directory_get:
 * @uri: URI of directory to get.
 *
 * Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
NautilusDirectory *
nautilus_directory_get (const char *uri)
{
	NautilusDirectory *directory;

	g_return_val_if_fail (uri != NULL, NULL);

	/* FIXME: This currently ignores the issue of two uris that are not identical but point
	   to the same data.
	*/

	/* Create the hash table first time through. */
	if (!directory_objects)
		directory_objects = g_hash_table_new (g_str_hash, g_str_equal);

	/* If the object is already in the hash table, look it up. */
	directory = g_hash_table_lookup (directory_objects, uri);
	if (directory != NULL) {
		g_assert (NAUTILUS_IS_DIRECTORY (directory));
		gtk_object_ref (GTK_OBJECT (directory));
	} else {
		/* Create a new directory object instead. */
		directory = NAUTILUS_DIRECTORY (nautilus_directory_new (uri));
		g_assert (strcmp (directory->details->uri_text, uri) == 0);

		/* Put it in the hash table. */
		gtk_object_ref (GTK_OBJECT (directory));
		gtk_object_sink (GTK_OBJECT (directory));
		g_hash_table_insert (directory_objects, directory->details->uri_text, directory);
	}

	return directory;
}

/* This reads the metafile synchronously. This must go eventually.
   To do this asynchronously we'd need a way to read an entire file
   with async. calls; currently you can only get the file length with
   a synchronous call.
*/
static void
nautilus_directory_read_metafile (NautilusDirectory *directory)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo metafile_info;
	GnomeVFSHandle *metafile_handle;
	char *buffer;
	GnomeVFSFileSize size, actual_size;

	g_assert (directory->details->metafile_tree == NULL);

	result = gnome_vfs_get_file_info_uri (directory->details->metafile_uri,
					      &metafile_info,
					      GNOME_VFS_FILE_INFO_DEFAULT,
					      NULL);
	if (result == GNOME_VFS_ERROR_NOTFOUND)
		return;
	if (result != GNOME_VFS_OK)
		g_warning ("nautilus_directory_read_metafile: gnome_vfs_get_file_info_uri failed");

	metafile_handle = NULL;
	if (result == GNOME_VFS_OK) {
		result = gnome_vfs_open_uri (&metafile_handle,
					     directory->details->metafile_uri,
					     GNOME_VFS_OPEN_READ);
		if (result != GNOME_VFS_OK)
			g_warning ("nautilus_directory_read_metafile: gnome_vfs_open_uri failed");
	}

	if (result == GNOME_VFS_OK) {
		size = metafile_info.size;
		if (size != metafile_info.size) {
			g_warning ("nautilus_directory_read_metafile: metafile too large");
			result = GNOME_VFS_ERROR_TOOBIG;
		}
	}

	if (result == GNOME_VFS_OK) {
		buffer = g_malloc (size);
		
		result = gnome_vfs_read (metafile_handle, buffer, size, &actual_size);
		
		if (result != GNOME_VFS_OK)
			g_warning ("nautilus_directory_read_metafile: gnome_vfs_read failed");
		else if (actual_size != size)
			g_warning ("nautilus_directory_read_metafile: size changed between get_info and read");
	}

	if (metafile_handle != NULL)
		gnome_vfs_close (metafile_handle);

	if (result == GNOME_VFS_OK)
		directory->details->metafile_tree = xmlParseMemory (buffer, actual_size);

	g_free (buffer);
}

static void
nautilus_directory_remove_write_metafile_idle (NautilusDirectory *directory)
{
	if (directory->details->write_metafile_idle_id != 0) {
		gtk_idle_remove (directory->details->write_metafile_idle_id);
		directory->details->write_metafile_idle_id = 0;
	}
}

/* This writes the metafile synchronously. This must go eventually. */
static void
nautilus_directory_write_metafile (NautilusDirectory *directory)
{
	xmlChar *buffer;
	int buffer_size;
	GnomeVFSResult result;
	GnomeVFSHandle *metafile_handle;
	GnomeVFSFileSize actual_size;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	/* We are about the write the metafile, so we can cancel the pending
	   request to do it. */
	nautilus_directory_remove_write_metafile_idle (directory);

	/* Don't write anything if there's nothing to write.
	   At some point, we might want to change this to actually delete
	   the metafile in this case.
	*/
	if (directory->details->metafile_tree == NULL)
		return;

	xmlDocDumpMemory (directory->details->metafile_tree, &buffer, &buffer_size);

	metafile_handle = NULL;
	result = gnome_vfs_create_uri (&metafile_handle,
				       directory->details->metafile_uri,
				       GNOME_VFS_OPEN_WRITE,
				       FALSE,
				       GNOME_VFS_PERM_USER_ALL | GNOME_VFS_PERM_GROUP_ALL | GNOME_VFS_PERM_OTHER_ALL);
	if (result != GNOME_VFS_OK)
		g_warning ("nautilus_directory_write_metafile: gnome_vfs_create_uri failed");

	if (result == GNOME_VFS_OK) {
		result = gnome_vfs_write (metafile_handle, buffer, buffer_size, &actual_size);
		
		if (result != GNOME_VFS_OK)
			g_warning ("nautilus_directory_write_metafile: gnome_vfs_write failed");
		else if (actual_size != buffer_size)
			g_warning ("nautilus_directory_read_metafile: unable to write all");
	}

	if (metafile_handle != NULL)
		gnome_vfs_close (metafile_handle);

	xmlFree (buffer);
}

static gboolean
nautilus_directory_write_metafile_on_idle (gpointer data)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (data), FALSE);
	nautilus_directory_write_metafile (data);
	return FALSE;
}

static void
nautilus_directory_request_write_metafile (NautilusDirectory *directory)
{
	/* Set up an idle task that will write the metafile. */
	if (directory->details->write_metafile_idle_id == 0)
		directory->details->write_metafile_idle_id =
			gtk_idle_add (nautilus_directory_write_metafile_on_idle,
				      directory);
}
      
#if NAUTILUS_DIRECTORY_ASYNC

static void
nautilus_directory_opened_metafile (GnomeVFSAsyncHandle *handle,
				    GnomeVFSResult result,
				    gpointer callback_data)
{
}

	result = gnome_vfs_async_open_uri (&metafile_handle, metafile_uri, GNOME_VFS_OPEN_READ,
					   nautilus_directory_opened_metafile, directory);
#endif

static NautilusDirectory *
nautilus_directory_new (const char* uri)
{
	NautilusDirectory *directory;
	GnomeVFSURI *vfs_uri;
	GnomeVFSURI *metafile_uri;

	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL)
		return NULL;

	metafile_uri = gnome_vfs_uri_append_path (vfs_uri, METAFILE_NAME);
	if (metafile_uri == NULL)
		return NULL;

	directory = gtk_type_new (NAUTILUS_TYPE_DIRECTORY);

	directory->details->uri_text = g_strdup (uri);
	directory->details->uri = vfs_uri;
	directory->details->metafile_uri = metafile_uri;

	nautilus_directory_read_metafile (directory);

	return directory;
}

void
nautilus_directory_get_files (NautilusDirectory *directory,
			      NautilusFileListCallback callback,
			      gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (callback != NULL);

	if (directory->details->files != NULL)
		(* callback) (directory,
			      directory->details->files,
			      callback_data);
}

char *
nautilus_directory_get_metadata (NautilusDirectory *directory,
				 const char *tag,
				 const char *default_metadata)
{
	xmlNode *root;
	xmlChar *property;
	char *result;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (tag, NULL);
	g_return_val_if_fail (tag[0], NULL);

	root = xmlDocGetRootElement (directory->details->metafile_tree);
	property = xmlGetProp (root, tag);
	if (property == NULL)
		result = g_strdup (default_metadata);
	else
		result = g_strdup (property);
	g_free (property);

	return result;
}

void
nautilus_directory_set_metadata (NautilusDirectory *directory,
				 const char *tag,
				 const char *default_metadata,
				 const char *metadata)
{
	char *old_metadata;
	gboolean old_metadata_matches;
	xmlNode *root;
	const char *value;
	xmlAttr *property_node;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (tag);
	g_return_if_fail (tag[0]);

	/* If the data in the metafile is already correct, do nothing. */
	old_metadata = nautilus_directory_get_metadata (directory, tag, default_metadata);
	old_metadata_matches = nautilus_strcmp (old_metadata, metadata) == 0;
	g_free (old_metadata);
	if (old_metadata_matches)
		return;

	/* Data that matches the default is represented in the tree by
	   the lack of an attribute.
	*/
	if (nautilus_strcmp (default_metadata, metadata) == 0)
		value = NULL;
	else
		value = metadata;

	/* Get at the tree. */
	if (directory->details->metafile_tree == NULL)
		directory->details->metafile_tree = xmlNewDoc (METAFILE_XML_VERSION);
	root = xmlDocGetRootElement (directory->details->metafile_tree);
	if (root == NULL) {
		root = xmlNewDocNode (directory->details->metafile_tree, NULL, "DIRECTORY", NULL);
		xmlDocSetRootElement (directory->details->metafile_tree, root);
	}

	/* Add or remove an attribute node. */
	property_node = xmlSetProp (root, tag, value);
	if (value == NULL)
		xmlRemoveProp (property_node);

	/* Since we changed the tree, arrange for it to be written. */
	nautilus_directory_request_write_metafile (directory);
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

static int data_dummy;
static guint file_count;

static void
get_files_cb (NautilusDirectory *directory, NautilusFileList *files, gpointer data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (files);
	g_assert (data == &data_dummy);

	file_count += g_list_length (files);
}

void
nautilus_self_check_directory (void)
{
	NautilusDirectory *directory;

	directory = nautilus_directory_get ("file:///etc");

	g_assert (g_hash_table_size (directory_objects) == 1);

	file_count = 0;
	nautilus_directory_get_files (directory, get_files_cb, &data_dummy);

	gtk_object_unref (GTK_OBJECT (directory));

	g_assert (g_hash_table_size (directory_objects) == 0);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
