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
#include "libnautilus.h"

#include <stdlib.h>

#include <gtk/gtkmain.h>

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>

#include <gnome-xml/parser.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/xmlmemory.h>

#include "nautilus-gtk-macros.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-string.h"

#define METAFILE_NAME ".nautilus-metafile.xml"
#define METAFILE_PERMISSIONS (GNOME_VFS_PERM_USER_READ | GNOME_VFS_PERM_USER_WRITE \
			      | GNOME_VFS_PERM_GROUP_READ | GNOME_VFS_PERM_GROUP_WRITE \
			      | GNOME_VFS_PERM_OTHER_READ | GNOME_VFS_PERM_OTHER_WRITE)

#define NAUTILUS_DIRECTORY_NAME ".nautilus"
#define METAFILES_DIRECTORY_NAME "metafiles"
#define METAFILES_DIRECTORY_PERMISSIONS (GNOME_VFS_PERM_USER_ALL \
					 | GNOME_VFS_PERM_GROUP_ALL \
					 | GNOME_VFS_PERM_OTHER_ALL)
#define METAFILE_SUFFIX ".xml"

#define METAFILE_XML_VERSION "1.0"

static void nautilus_directory_initialize_class (gpointer klass);
static void nautilus_directory_initialize (gpointer object, gpointer klass);
static void nautilus_directory_finalize (GtkObject *object);

static NautilusDirectory *nautilus_directory_new (const char* uri);

static void nautilus_directory_read_metafile (NautilusDirectory *directory);
static void nautilus_directory_write_metafile (NautilusDirectory *directory);
static void nautilus_directory_request_write_metafile (NautilusDirectory *directory);
static void nautilus_directory_remove_write_metafile_idle (NautilusDirectory *directory);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusDirectory, nautilus_directory, GTK_TYPE_OBJECT)

struct _NautilusDirectoryDetails
{
	char *uri_text;
	GnomeVFSURI *uri;

	GnomeVFSURI *metafile_uri;
	GnomeVFSURI *alternate_metafile_uri;
	gboolean use_alternate_metafile;

	xmlDoc *metafile_tree;
	int write_metafile_idle_id;

	NautilusFileList *files;
};

struct _NautilusFile
{
	guint ref_count;

	NautilusDirectory *directory;
	GnomeVFSFileInfo *info;
};

static GHashTable* directory_objects;

static void
nautilus_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
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

	if (directory->details->write_metafile_idle_id != 0)
		nautilus_directory_write_metafile (directory);

	g_hash_table_remove (directory_objects, directory->details->uri_text);

	/* Unref all the files. */
	while (directory->details->files != NULL) {
		NautilusFile *file;
		
		file = directory->details->files->data;

		/* Detach the file from this directory. */
		g_assert (file->directory == directory);
		file->directory = NULL;
		
		/* Let the reference go. */
		nautilus_file_unref (file);

		directory->details->files = g_list_remove_link (directory->details->files, directory->details->files);
	}

	g_free (directory->details->uri_text);
	if (directory->details->uri != NULL)
		gnome_vfs_uri_unref (directory->details->uri);
	if (directory->details->metafile_uri != NULL)
		gnome_vfs_uri_unref (directory->details->metafile_uri);
	if (directory->details->alternate_metafile_uri != NULL)
		gnome_vfs_uri_unref (directory->details->alternate_metafile_uri);
	xmlFreeDoc (directory->details->metafile_tree);

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

char *
nautilus_directory_get_uri (NautilusDirectory *directory)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	return g_strdup (directory->details->uri_text);
}

/* This reads the metafile synchronously. This must go eventually.
   To do this asynchronously we'd need a way to read an entire file
   with async. calls; currently you can only get the file length with
   a synchronous call.
*/
static GnomeVFSResult
nautilus_directory_try_to_read_metafile (NautilusDirectory *directory, GnomeVFSURI *metafile_uri)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo metafile_info;
	GnomeVFSHandle *metafile_handle;
	GnomeVFSFileSize size, actual_size;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), GNOME_VFS_ERROR_GENERIC);
	g_return_val_if_fail (directory->details->metafile_tree == NULL, GNOME_VFS_ERROR_GENERIC);

	result = gnome_vfs_get_file_info_uri (metafile_uri,
					      &metafile_info,
					      GNOME_VFS_FILE_INFO_DEFAULT,
					      NULL);

	metafile_handle = NULL;
	if (result == GNOME_VFS_OK)
		result = gnome_vfs_open_uri (&metafile_handle,
					     metafile_uri,
					     GNOME_VFS_OPEN_READ);

	if (result == GNOME_VFS_OK) {
		size = metafile_info.size;
		if (size != metafile_info.size)
			result = GNOME_VFS_ERROR_TOOBIG;
	}

	if (result == GNOME_VFS_OK) {
		char *buffer = g_alloca(size+1);

		result = gnome_vfs_read (metafile_handle, buffer, size, &actual_size);
		buffer[size] = '\0';
		directory->details->metafile_tree = xmlParseMemory (buffer, actual_size);
	}

	if (metafile_handle != NULL)
		gnome_vfs_close (metafile_handle);

	return result;
}

static void
nautilus_directory_read_metafile (NautilusDirectory *directory)
{
	GnomeVFSResult result;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	/* Check for the alternate metafile first.
	   If we read from it, then write to it later.
	*/
	directory->details->use_alternate_metafile = FALSE;
	result = nautilus_directory_try_to_read_metafile (directory, directory->details->alternate_metafile_uri);
	if (result == GNOME_VFS_OK)
		directory->details->use_alternate_metafile = TRUE;
	else
		result = nautilus_directory_try_to_read_metafile (directory, directory->details->metafile_uri);

	/* Check for errors. Later this must be reported to the user, not spit out as a warning. */
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_NOTFOUND)
		g_warning ("nautilus_directory_read_metafile failed to read metafile - we should report this to the user");
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
static GnomeVFSResult
nautilus_directory_try_to_write_metafile (NautilusDirectory *directory, GnomeVFSURI *metafile_uri)
{
	xmlChar *buffer;
	int buffer_size;
	GnomeVFSResult result;
	GnomeVFSHandle *metafile_handle;
	GnomeVFSFileSize actual_size;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), GNOME_VFS_ERROR_GENERIC);
	g_return_val_if_fail (directory->details != NULL, GNOME_VFS_ERROR_GENERIC);
	g_return_val_if_fail (directory->details->metafile_tree != NULL, GNOME_VFS_ERROR_GENERIC);

	metafile_handle = NULL;
	result = gnome_vfs_create_uri (&metafile_handle,
				       metafile_uri,
				       GNOME_VFS_OPEN_WRITE,
				       FALSE,
				       METAFILE_PERMISSIONS);

	buffer = NULL;
	if (result == GNOME_VFS_OK) {
		xmlDocDumpMemory (directory->details->metafile_tree, &buffer, &buffer_size);
		result = gnome_vfs_write (metafile_handle, buffer, buffer_size, &actual_size);
		if (buffer_size != actual_size)
			result = GNOME_VFS_ERROR_GENERIC;
	}

	if (metafile_handle != NULL)
		gnome_vfs_close (metafile_handle);

	xmlFree (buffer);

	return result;
}

static void
nautilus_directory_write_metafile (NautilusDirectory *directory)
{
	GnomeVFSResult result;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	/* We are about the write the metafile, so we can cancel the pending
	   request to do so.
	*/
	nautilus_directory_remove_write_metafile_idle (directory);

	/* Don't write anything if there's nothing to write.
	   At some point, we might want to change this to actually delete
	   the metafile in this case.
	*/
	if (directory->details->metafile_tree == NULL)
		return;

	/* Try the main URI, unless we have already been instructed to use the alternate URI. */
	if (directory->details->use_alternate_metafile)
		result = GNOME_VFS_ERROR_ACCESSDENIED;
	else
		result = nautilus_directory_try_to_write_metafile (directory, directory->details->metafile_uri);

	/* Try the alternate URI if the main one failed. */
	if (result != GNOME_VFS_OK)
		result = nautilus_directory_try_to_write_metafile (directory, directory->details->alternate_metafile_uri);

	/* Check for errors. Later this must be reported to the user, not spit out as a warning. */
	if (result != GNOME_VFS_OK)
		g_warning ("nautilus_directory_write_metafile failed to write metafile - we should report this to the user");
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

/* To use a directory name as a file name, we need to escape any slashes.
   This means that "/" is replaced by "%2F" and "%" is replaced by "%25".
   Later we might share the escaping code with some more generic escaping
   function, but this should do for now.
*/
static char *
nautilus_directory_escape_slashes (const char *path)
{
	char c;
	const char *in;
	guint length;
	char *result;
	char *out;

	/* Figure out how long the result needs to be. */
	in = path;
	length = 0;
	while ((c = *in++) != '\0')
		switch (c) {
		case '/':
		case '%':
			length += 3;
			break;
		default:
			length += 1;
		}

	/* Create the result string. */
	result = g_malloc (length + 1);
	in = path;
	out = result;	
	while ((c = *in++) != '\0')
		switch (c) {
		case '/':
			*out++ = '%';
			*out++ = '2';
			*out++ = 'F';
			break;
		case '%':
			*out++ = '%';
			*out++ = '2';
			*out++ = '5';
			break;
		default:
			*out++ = c;
		}
	g_assert (out == result + length);
	*out = '\0';

	return result;
}

static GnomeVFSResult
nautilus_make_directory_and_parents (GnomeVFSURI *uri, guint permissions)
{
	GnomeVFSResult result;
	GnomeVFSURI *parent_uri;

	/* Make the directory, and return right away unless there's
	   a possible problem with the parent.
	*/
	result = gnome_vfs_make_directory_for_uri (uri, permissions);
	if (result != GNOME_VFS_ERROR_NOTFOUND)
		return result;

	/* If we can't get a parent, we are done. */
	parent_uri = gnome_vfs_uri_get_parent (uri);
	if (parent_uri == NULL)
		return result;

	/* If we can get a parent, use a recursive call to create
	   the parent and its parents.
	*/
	result = nautilus_make_directory_and_parents (parent_uri, permissions);
	gnome_vfs_uri_unref (parent_uri);
	if (result != GNOME_VFS_OK)
		return result;

	/* A second try at making the directory after the parents
	   have all been created.
	*/
	result = gnome_vfs_make_directory_for_uri (uri, permissions);
	return result;
}

static GnomeVFSURI *
nautilus_directory_construct_alternate_metafile_uri (GnomeVFSURI *metafile_uri)
{
	GnomeVFSResult result;
	GnomeVFSURI *home_uri, *nautilus_directory_uri, *metafiles_directory_uri, *alternate_uri;
	char *uri_as_string, *escaped_uri, *file_name;

	/* Ensure that the metafiles directory exists. */
	home_uri = gnome_vfs_uri_new (g_get_home_dir ());
	nautilus_directory_uri = gnome_vfs_uri_append_path (home_uri, NAUTILUS_DIRECTORY_NAME);
	gnome_vfs_uri_unref (home_uri);
	metafiles_directory_uri = gnome_vfs_uri_append_path (nautilus_directory_uri, METAFILES_DIRECTORY_NAME);
	gnome_vfs_uri_unref (nautilus_directory_uri);
	result = nautilus_make_directory_and_parents (metafiles_directory_uri, METAFILES_DIRECTORY_PERMISSIONS);
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_FILEEXISTS) {
		gnome_vfs_uri_unref (metafiles_directory_uri);
		return NULL;
	}

	/* Construct a file name from the URI. */
	uri_as_string = gnome_vfs_uri_to_string (metafile_uri,
						 GNOME_VFS_URI_HIDE_NONE);
	escaped_uri = nautilus_directory_escape_slashes (uri_as_string);
	g_free (uri_as_string);
	file_name = g_strconcat (escaped_uri, ".xml", NULL);
	g_free (escaped_uri);

	/* Construct a URI for something in the "metafiles" directory. */
	alternate_uri = gnome_vfs_uri_append_path (metafiles_directory_uri, file_name);
	gnome_vfs_uri_unref (metafiles_directory_uri);
	g_free (file_name);

	return alternate_uri;
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
	GnomeVFSURI *alternate_metafile_uri;

	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL)
		return NULL;

	metafile_uri = gnome_vfs_uri_append_path (vfs_uri, METAFILE_NAME);
	alternate_metafile_uri = nautilus_directory_construct_alternate_metafile_uri (metafile_uri);

	directory = gtk_type_new (NAUTILUS_TYPE_DIRECTORY);

	directory->details->uri_text = g_strdup(uri);
	directory->details->uri = vfs_uri;
	directory->details->metafile_uri = metafile_uri;
	directory->details->alternate_metafile_uri = alternate_metafile_uri;

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

static char *
nautilus_directory_get_metadata_from_node (xmlNode *node,
					   const char *tag,
					   const char *default_metadata)
{
	xmlChar *property;
	char *result;

	g_return_val_if_fail (tag, NULL);
	g_return_val_if_fail (tag[0], NULL);

	property = xmlGetProp (node, tag);
	if (property == NULL)
		result = g_strdup (default_metadata);
	else
		result = g_strdup (property);
	xmlFree (property);

	return result;
}

static xmlNode *
nautilus_directory_create_metafile_tree_root (NautilusDirectory *directory)
{
	xmlNode *root;

	if (directory->details->metafile_tree == NULL)
		directory->details->metafile_tree = xmlNewDoc (METAFILE_XML_VERSION);
	root = xmlDocGetRootElement (directory->details->metafile_tree);
	if (root == NULL) {
		root = xmlNewDocNode (directory->details->metafile_tree, NULL, "DIRECTORY", NULL);
		xmlDocSetRootElement (directory->details->metafile_tree, root);
	}

	return root;
}

char *
nautilus_directory_get_metadata (NautilusDirectory *directory,
				 const char *tag,
				 const char *default_metadata)
{
	/* It's legal to call this on a NULL directory. */
	if (directory == NULL)
		return NULL;
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	/* The root itself represents the directory. */
	return nautilus_directory_get_metadata_from_node
		(xmlDocGetRootElement (directory->details->metafile_tree),
		 tag, default_metadata);
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
	root = nautilus_directory_create_metafile_tree_root (directory);

	/* Add or remove an attribute node. */
	property_node = xmlSetProp (root, tag, value);
	if (value == NULL)
		xmlRemoveProp (property_node);

	/* Since we changed the tree, arrange for it to be written. */
	nautilus_directory_request_write_metafile (directory);
}

static char *
nautilus_directory_get_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *tag,
				      const char *default_metadata)
{
	xmlNode *root, *child;
	xmlChar *property;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	/* The root itself represents the directory. */
	root = xmlDocGetRootElement (directory->details->metafile_tree);
	if (root == NULL)
		return g_strdup (default_metadata);

	/* The children represent the files.
	   This linear search is temporary.
	   Eventually, we'll have a pointer from the NautilusFile right to
	   the corresponding XML node, or we won't have the XML tree
	   in memory at all.
	*/
	for (child = root->childs; child != NULL; child = child->next)
		if (strcmp (child->name, "FILE") == 0) {
			property = xmlGetProp (child, "NAME");
			if (nautilus_eat_strcmp (property, file_name) == 0)
				break;
		}

	/* If we found a child, we can get the data from it. */
	return nautilus_directory_get_metadata_from_node
		(child, tag, default_metadata);
}

static void
nautilus_directory_set_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *tag,
				      const char *default_metadata,
				      const char *metadata)
{
	char *old_metadata;
	gboolean old_metadata_matches;
	xmlNode *root, *child;
	const char *value;
	xmlChar *property;
	xmlAttr *property_node;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (tag);
	g_return_if_fail (tag[0]);

	/* If the data in the metafile is already correct, do nothing. */
	old_metadata = nautilus_directory_get_file_metadata
		(directory, file_name, tag, default_metadata);
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

	/* The root itself represents the directory. */
	root = nautilus_directory_create_metafile_tree_root (directory);
	
	/* The children represent the files.
	   This linear search is temporary.
	   Eventually, we'll have a pointer from the NautilusFile right to
	   the corresponding XML node, or we won't have the XML tree
	   in memory at all.
	*/
	for (child = root->childs; child != NULL; child = child->next)
		if (strcmp (child->name, "FILE") == 0) {
			property = xmlGetProp (child, "NAME");
			if (nautilus_eat_strcmp (property, file_name) == 0)
				break;
		}
	if (child == NULL) {
		g_assert (value != NULL);
		child = xmlNewChild (root, NULL, "FILE", NULL);
		xmlSetProp (child, "NAME", file_name);
	}

	/* Add or remove an attribute node. */
	property_node = xmlSetProp (child, tag, value);
	if (value == NULL)
		xmlRemoveProp (property_node);
	
	/* Since we changed the tree, arrange for it to be written. */
	nautilus_directory_request_write_metafile (directory);
}

NautilusFile *
nautilus_directory_new_file (NautilusDirectory *directory, GnomeVFSFileInfo *info)
{
	NautilusFile *file;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (info != NULL, NULL);

	gnome_vfs_file_info_ref (info);

	file = g_new (NautilusFile, 1);
	file->ref_count = 1;
	file->directory = directory;
	file->info = info;

	directory->details->files = g_list_prepend (directory->details->files, file);

	return file;
}

void
nautilus_file_unref (NautilusFile *file)
{
	g_return_if_fail (file != NULL);
	g_return_if_fail (file->ref_count != 0);

	/* Decrement the ref count. */
	if (--file->ref_count != 0)
		return;

	/* Destroy the file object. */
	g_assert (file->directory == NULL);
	gnome_vfs_file_info_unref (file->info);
}

char *
nautilus_file_get_metadata (NautilusFile *file,
			    const char *tag,
			    const char *default_metadata)
{
	g_return_val_if_fail (file != NULL, NULL);

	return nautilus_directory_get_file_metadata (file->directory, file->info->name, tag, default_metadata);
}

void
nautilus_file_set_metadata (NautilusFile *file,
			    const char *tag,
			    const char *default_metadata,
			    const char *metadata)
{
	g_return_if_fail (file != NULL);

	nautilus_directory_set_file_metadata (file->directory, file->info->name, tag, default_metadata, metadata);
}

char *
nautilus_file_get_name (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, NULL);

	g_assert (file->ref_count != 0);
	g_assert (file->directory == NULL || NAUTILUS_IS_DIRECTORY (file->directory));
	g_assert (file->info->name != NULL);
	g_assert (file->info->name[0] != '\0');

	return g_strdup (file->info->name);
}

char *
nautilus_file_get_uri (NautilusFile *file)
{
	GnomeVFSURI *uri;
	char *uri_text;

	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (file->directory != NULL, NULL);

	uri = gnome_vfs_uri_append_path (file->directory->details->uri, file->info->name);
	uri_text = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (uri);
	return uri_text;
}

GnomeVFSFileInfo *
nautilus_file_get_info (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, NULL);

	return file->info;
}

/**
 * nautilus_file_get_date_as_string:
 * 
 * Get a user-displayable string representing a file modification date. 
 * The caller is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
gchar *
nautilus_file_get_date_as_string (NautilusFile *file)
{
	/* Note: There's also accessed time and changed time.
	 * Accessed time doesn't seem worth showing to the user.
	 * Changed time is only subtly different from modified time
	 * (changed time includes "metadata" changes like file permissions).
	 * We should not display both, but we might change our minds as to
	 * which one is better.
	 */

	g_return_val_if_fail (file != NULL, NULL);

	/* Note that ctime is a funky function that returns a
	 * string that you're not supposed to free.
	 */
	return g_strdup (ctime (&file->info->mtime));
}

/**
 * nautilus_file_get_size_as_string:
 * 
 * Get a user-displayable string representing a file size. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
gchar *
nautilus_file_get_size_as_string (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, NULL);

	if (file->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
		return g_strdup (_("--"));

	return gnome_vfs_file_size_to_string (file->info->size);
}

/**
 * nautilus_file_get_type_as_string:
 * 
 * Get a user-displayable string representing a file type. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
gchar *
nautilus_file_get_type_as_string (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, NULL);

	if (file->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
		/* Special-case this so it isn't "special/directory".
		 * Should this be "folder" instead?
		 */		
		return g_strdup (_("directory"));

	return g_strdup (file->info->mime_type);
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

	nautilus_directory_set_metadata (directory, "TEST", "default", "value");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_get_metadata (directory, "TEST", "default"), "value");

	gtk_object_unref (GTK_OBJECT (directory));

	g_assert (g_hash_table_size (directory_objects) == 0);

	directory = nautilus_directory_get ("file:///etc");

	g_assert (g_hash_table_size (directory_objects) == 1);

	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_get_metadata (directory, "TEST", "default"), "value");

	/* nautilus_directory_escape_slashes */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_escape_slashes (""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_escape_slashes ("a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_escape_slashes ("/"), "%2F");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_escape_slashes ("%"), "%25");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_escape_slashes ("a/a"), "a%2Fa");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_escape_slashes ("a%a"), "a%25a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_escape_slashes ("%25"), "%2525");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_escape_slashes ("%2F"), "%252F");
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
