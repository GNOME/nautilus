/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory.c: Nautilus directory model.
 
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
#include "nautilus-directory-private.h"

#include <stdlib.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>

#include <xmlmemory.h>

#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-file-private.h"

enum 
{
	FILES_ADDED,
	FILES_CHANGED,
	METADATA_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

#define METAFILE_NAME ".nautilus-metafile.xml"

#define NAUTILUS_DIRECTORY_NAME ".nautilus"
#define METAFILES_DIRECTORY_NAME "metafiles"
#define METAFILE_SUFFIX ".xml"
#define METAFILES_DIRECTORY_PERMISSIONS (GNOME_VFS_PERM_USER_ALL \
					 | GNOME_VFS_PERM_GROUP_ALL \
					 | GNOME_VFS_PERM_OTHER_ALL)

#define METAFILE_XML_VERSION "1.0"

static GnomeVFSURI *      construct_alternate_metafile_uri    (GnomeVFSURI       *uri);
static xmlNode *          create_metafile_root                (NautilusDirectory *directory);
static char *             get_metadata_from_node              (xmlNode           *node,
							       const char        *key,
							       const char        *default_metadata);
static void               nautilus_directory_destroy          (GtkObject         *object);
static void               nautilus_directory_initialize       (gpointer           object,
							       gpointer           klass);
static void               nautilus_directory_initialize_class (gpointer           klass);
static GnomeVFSResult     nautilus_make_directory_and_parents (GnomeVFSURI       *uri,
							       guint              permissions);
static NautilusDirectory *nautilus_directory_new              (const char        *uri);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusDirectory, nautilus_directory, GTK_TYPE_OBJECT)

static GHashTable* directory_objects;

static void
nautilus_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_directory_destroy;

	signals[FILES_ADDED] =
		gtk_signal_new ("files_added",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDirectoryClass, files_added),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[FILES_CHANGED] =
		gtk_signal_new ("files_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDirectoryClass, files_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[METADATA_CHANGED] =
		gtk_signal_new ("metadata_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDirectoryClass, metadata_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_directory_initialize (gpointer object, gpointer klass)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY(object);

	directory->details = g_new0 (NautilusDirectoryDetails, 1);
}

void
nautilus_directory_ref (NautilusDirectory *directory)
{
	if (directory == NULL) {
		return;
	}

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	gtk_object_ref (GTK_OBJECT (directory));
}

void
nautilus_directory_unref (NautilusDirectory *directory)
{
	if (directory == NULL) {
		return;
	}

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	gtk_object_unref (GTK_OBJECT (directory));
}

static void
nautilus_directory_destroy (GtkObject *object)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (object);

	g_assert (directory->details->write_state == NULL);
	nautilus_metafile_read_cancel (directory);

	if (nautilus_directory_is_file_list_monitored (directory)) {
		nautilus_directory_stop_monitoring_file_list (directory);
	}

	if (directory->details->file_monitors != NULL) {
		g_warning ("destroying a NautilusDirectory while it's being monitored");
		nautilus_g_list_free_deep (directory->details->file_monitors);
	}

	g_hash_table_remove (directory_objects, directory->details->uri_text);

	if (directory->details->dequeue_pending_idle_id != 0) {
		gtk_idle_remove (directory->details->dequeue_pending_idle_id);
	}

	g_free (directory->details->uri_text);
	if (directory->details->uri != NULL) {
		gnome_vfs_uri_unref (directory->details->uri);
	}
	if (directory->details->metafile_uri != NULL) {
		gnome_vfs_uri_unref (directory->details->metafile_uri);
	}
	if (directory->details->alternate_metafile_uri != NULL) {
		gnome_vfs_uri_unref (directory->details->alternate_metafile_uri);
	}
	g_assert (directory->details->files == NULL);
	xmlFreeDoc (directory->details->metafile);
	g_assert (directory->details->directory_load_in_progress == NULL);
	g_assert (directory->details->count_in_progress == NULL);
	g_assert (directory->details->dequeue_pending_idle_id == 0);
	nautilus_gnome_vfs_file_info_list_unref (directory->details->pending_file_info);
	g_assert (directory->details->write_metafile_idle_id == 0);

	g_free (directory->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

#ifndef G_DISABLE_ASSERT

static gboolean
is_canonical_uri (const char *uri)
{
	if (uri == NULL) {
		return FALSE;
	}
	if (nautilus_str_has_suffix (uri, "/")) {
		if (nautilus_str_has_suffix (uri, ":///")) {
			return TRUE;
		}
		return FALSE;
	}
	return TRUE;
}

#endif /* !G_DISABLE_ASSERT */

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
	char *canonical_uri, *with_slashes;
	size_t length;
	NautilusDirectory *directory;

	g_return_val_if_fail (uri != NULL, NULL);

	/* FIXME bugzilla.eazel.com 648: 
	 * This currently ignores the issue of two uris that are not identical but point
	 * to the same data except for the specific case of trailing '/' characters.
	 */
	canonical_uri = nautilus_str_strip_trailing_chr (uri, '/');
	if (strcmp (canonical_uri, uri) != 0) {
		/* If some trailing '/' were stripped, there's the possibility,
		 * that we stripped away all the '/' from a uri that has only
		 * '/' characters. If you change this code, check to make sure
		 * that "file:///" still works as a URI.
		 */
		length = strlen (canonical_uri);
		if (length == 0 || canonical_uri[length - 1] == ':') {
			with_slashes = g_strconcat (canonical_uri, "///", NULL);
			g_free (canonical_uri);
			canonical_uri = with_slashes;
		}
	}
	g_assert (is_canonical_uri (canonical_uri));

	/* Create the hash table first time through. */
	if (directory_objects == NULL) {
		directory_objects = g_hash_table_new (g_str_hash, g_str_equal);
	}

	/* If the object is already in the hash table, look it up. */
	directory = g_hash_table_lookup (directory_objects,
					 canonical_uri);
	if (directory != NULL) {
		nautilus_directory_ref (directory);
	} else {
		/* Create a new directory object instead. */
		directory = nautilus_directory_new (canonical_uri);
		if (directory == NULL) {
			return NULL;
		}

		g_assert (strcmp (directory->details->uri_text, canonical_uri) == 0);

		/* Put it in the hash table. */
		nautilus_directory_ref (directory);
		gtk_object_sink (GTK_OBJECT (directory));
		g_hash_table_insert (directory_objects,
				     directory->details->uri_text,
				     directory);
	}

	g_free (canonical_uri);

	return directory;
}

char *
nautilus_directory_get_uri (NautilusDirectory *directory)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	return g_strdup (directory->details->uri_text);
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
	if (result != GNOME_VFS_ERROR_NOTFOUND) {
		return result;
	}

	/* If we can't get a parent, we are done. */
	parent_uri = gnome_vfs_uri_get_parent (uri);
	if (parent_uri == NULL) {
		return result;
	}

	/* If we can get a parent, use a recursive call to create
	   the parent and its parents.
	*/
	result = nautilus_make_directory_and_parents (parent_uri, permissions);
	gnome_vfs_uri_unref (parent_uri);
	if (result != GNOME_VFS_OK) {
		return result;
	}

	/* A second try at making the directory after the parents
	   have all been created.
	*/
	result = gnome_vfs_make_directory_for_uri (uri, permissions);
	return result;
}

static GnomeVFSURI *
construct_alternate_metafile_uri (GnomeVFSURI *uri)
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
	uri_as_string = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	escaped_uri = nautilus_str_escape_slashes (uri_as_string);
	g_free (uri_as_string);
	file_name = g_strconcat (escaped_uri, ".xml", NULL);
	g_free (escaped_uri);

	/* Construct a URI for something in the "metafiles" directory. */
	alternate_uri = gnome_vfs_uri_append_path (metafiles_directory_uri, file_name);
	gnome_vfs_uri_unref (metafiles_directory_uri);
	g_free (file_name);

	return alternate_uri;
}
      
static NautilusDirectory *
nautilus_directory_new (const char* uri)
{
	NautilusDirectory *directory;
	GnomeVFSURI *vfs_uri;
	GnomeVFSURI *metafile_uri;
	GnomeVFSURI *alternate_metafile_uri;

	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL) {
		return NULL;
	}

	metafile_uri = gnome_vfs_uri_append_path (vfs_uri, METAFILE_NAME);
	alternate_metafile_uri = construct_alternate_metafile_uri (vfs_uri);

	directory = gtk_type_new (NAUTILUS_TYPE_DIRECTORY);

	directory->details->uri_text = g_strdup(uri);
	directory->details->uri = vfs_uri;
	directory->details->metafile_uri = metafile_uri;
	directory->details->alternate_metafile_uri = alternate_metafile_uri;

	return directory;
}

gboolean
nautilus_directory_are_all_files_seen (NautilusDirectory *directory)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	
	return directory->details->directory_loaded;
}

static char *
get_metadata_from_node (xmlNode *node,
			const char *key,
			const char *default_metadata)
{
	xmlChar *property;
	char *result;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (key[0] != '\0', NULL);

	property = xmlGetProp (node, key);
	if (property == NULL) {
		result = g_strdup (default_metadata);
	} else {
		result = g_strdup (property);
	}
	xmlFree (property);

	return result;
}

static xmlNode *
create_metafile_root (NautilusDirectory *directory)
{
	xmlNode *root;

	if (directory->details->metafile == NULL) {
		directory->details->metafile = xmlNewDoc (METAFILE_XML_VERSION);
	}
	root = xmlDocGetRootElement (directory->details->metafile);
	if (root == NULL) {
		root = xmlNewDocNode (directory->details->metafile, NULL, "DIRECTORY", NULL);
		xmlDocSetRootElement (directory->details->metafile, root);
	}

	return root;
}

char *
nautilus_directory_get_metadata (NautilusDirectory *directory,
				 const char *key,
				 const char *default_metadata)
{
	/* It's legal to call this on a NULL directory. */
	if (directory == NULL) {
		return g_strdup (default_metadata);
	}

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	nautilus_directory_request_read_metafile (directory);

	/* The root itself represents the directory. */
	return get_metadata_from_node
		(xmlDocGetRootElement (directory->details->metafile),
		 key, default_metadata);
}

void
nautilus_directory_set_metadata (NautilusDirectory *directory,
				 const char *key,
				 const char *default_metadata,
				 const char *metadata)
{
	char *old_metadata;
	gboolean old_metadata_matches;
	xmlNode *root;
	const char *value;
	xmlAttr *property_node;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	/* If the data in the metafile is already correct, do nothing. */
	old_metadata = nautilus_directory_get_metadata (directory, key, default_metadata);
	old_metadata_matches = nautilus_strcmp (old_metadata, metadata) == 0;
	g_free (old_metadata);
	if (old_metadata_matches) {
		return;
	}

	/* Data that matches the default is represented in the tree by
	   the lack of an attribute.
	*/
	if (nautilus_strcmp (default_metadata, metadata) == 0) {
		value = NULL;
	} else {
		value = metadata;
	}

	/* Get at the tree. */
	root = create_metafile_root (directory);

	/* Add or remove an attribute node. */
	property_node = xmlSetProp (root, key, value);
	if (value == NULL) {
		xmlRemoveProp (property_node);
	}

	/* Since we changed the tree, arrange for it to be written. */
	nautilus_directory_request_write_metafile (directory);
	gtk_signal_emit (GTK_OBJECT (directory),
			 signals[METADATA_CHANGED]);
}

gboolean 
nautilus_directory_get_boolean_metadata (NautilusDirectory *directory,
					 const char *key,
					 gboolean default_metadata)
{
	char *result_as_string;
	gboolean result;

	result_as_string = nautilus_directory_get_metadata
		(directory,
		 key,
		 default_metadata ? "TRUE" : "FALSE");
	
	/* FIXME bugzilla.eazel.com 649: Allow "true" and "false"? */
	if (strcmp (result_as_string, "TRUE") == 0) {
		result = TRUE;
	} else if (strcmp (result_as_string, "FALSE") == 0) {
		result = FALSE;
	} else {
		if (result_as_string != NULL) {
			g_warning ("boolean metadata with value other than TRUE or FALSE");
		}
		result = default_metadata;
	}

	g_free (result_as_string);
	return result;

}

void               
nautilus_directory_set_boolean_metadata (NautilusDirectory *directory,
					 const char *key,
					 gboolean default_metadata,
					 gboolean metadata)
{
	nautilus_directory_set_metadata (directory,
					 key,
					 default_metadata ? "TRUE" : "FALSE",
					 metadata ? "TRUE" : "FALSE");
}

int 
nautilus_directory_get_integer_metadata (NautilusDirectory *directory,
					 const char *key,
					 int default_metadata)
{
	char *result_as_string;
	char *default_as_string;
	int result;

	default_as_string = g_strdup_printf ("%d", default_metadata);
	result_as_string = nautilus_directory_get_metadata
		(directory, key, default_as_string);
	
	/* Handle oddball case of non-existent directory */
	if (result_as_string == NULL) {
		result = default_metadata;
	} else {
		result = atoi (result_as_string);
		g_free (result_as_string);
	}

	g_free (default_as_string);
	return result;

}

void               
nautilus_directory_set_integer_metadata (NautilusDirectory *directory,
					 const char *key,
					 int default_metadata,
					 int metadata)
{
	char *value_as_string;
	char *default_as_string;

	value_as_string = g_strdup_printf ("%d", metadata);
	default_as_string = g_strdup_printf ("%d", default_metadata);

	nautilus_directory_set_metadata (directory,
					 key,
					 default_as_string,
					 value_as_string);

	g_free (value_as_string);
	g_free (default_as_string);
}

xmlNode *
nautilus_directory_get_file_metadata_node (NautilusDirectory *directory,
					   const char *file_name,
					   gboolean create)
{
	xmlNode *root, *child;
	
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	
	/* The root itself represents the directory.
	 * The children represent the files.
	 * FIXME bugzilla.eazel.com 650: 
	 * This linear search may not be fast enough.
	 * Eventually, we'll have a pointer from the NautilusFile right to
	 * the corresponding XML node, or we won't have the XML tree
	 * in memory at all.
	 */
	child = nautilus_xml_get_root_child_by_name_and_property
		(directory->details->metafile,
		 "FILE", METADATA_NODE_NAME_FOR_FILE_NAME, file_name);
	if (child != NULL) {
		return child;
	}
	
	/* Create if necessary. */
	if (create) {
		root = create_metafile_root (directory);
		child = xmlNewChild (root, NULL, "FILE", NULL);
		xmlSetProp (child, METADATA_NODE_NAME_FOR_FILE_NAME, file_name);
		return child;
	}
	
	return NULL;
}

char *
nautilus_directory_get_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *key,
				      const char *default_metadata)
{
	nautilus_directory_request_read_metafile (directory);

	return get_metadata_from_node
		(nautilus_directory_get_file_metadata_node (directory, file_name, FALSE),
		 key, default_metadata);
}

gboolean
nautilus_directory_set_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *key,
				      const char *default_metadata,
				      const char *metadata)
{
	char *old_metadata;
	gboolean old_metadata_matches;
	xmlNode *child;
	const char *value;
	xmlAttr *property_node;

	g_return_val_if_fail (strcmp (key, METADATA_NODE_NAME_FOR_FILE_NAME) != 0, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (key[0] != '\0', FALSE);

	/* If the data in the metafile is already correct, do nothing. */
	old_metadata = nautilus_directory_get_file_metadata
		(directory, file_name, key, default_metadata);
	old_metadata_matches = nautilus_strcmp (old_metadata, metadata) == 0;
	g_free (old_metadata);
	if (old_metadata_matches) {
		return FALSE;
	}

	/* Data that matches the default is represented in the tree by
	   the lack of an attribute.
	*/
	if (nautilus_strcmp (default_metadata, metadata) == 0) {
		value = NULL;
	} else {
		value = metadata;
	}

	/* Get or create the node. */
	child = nautilus_directory_get_file_metadata_node (directory,
							   file_name,
							   value != NULL);
	/* Add or remove an attribute node. */
	if (child != NULL) {
		property_node = xmlSetProp (child, key, value);
		if (value == NULL) {
			xmlRemoveProp (property_node);
		}
	}
	
	/* Since we changed the tree, arrange for it to be written. */
	nautilus_directory_request_write_metafile (directory);

	return TRUE;
}

NautilusFile *
nautilus_directory_find_file (NautilusDirectory *directory, const char *name)
{
	GList *list_entry;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	list_entry = g_list_find_custom (directory->details->files,
					 (gpointer) name,
					 nautilus_compare_file_with_name);

	return list_entry == NULL ? NULL : list_entry->data;
}

void
nautilus_directory_emit_files_added (NautilusDirectory *directory,
				     GList *added_files)
{
	if (added_files != NULL) {
		gtk_signal_emit (GTK_OBJECT (directory),
				 signals[FILES_ADDED],
				 added_files);
	}
}

void
nautilus_directory_emit_files_changed (NautilusDirectory *directory,
				       GList *changed_files)
{
	GList *p;

	for (p = changed_files; p != NULL; p = p->next) {
		nautilus_file_emit_changed (p->data);
	}
	if (changed_files != NULL) {
		gtk_signal_emit (GTK_OBJECT (directory),
				 signals[FILES_CHANGED],
				 changed_files);
	}
}

void
nautilus_directory_emit_metadata_changed (NautilusDirectory *directory)
{
	/* Tell that the directory metadata has changed. */
	gtk_signal_emit (GTK_OBJECT (directory),
			 signals[METADATA_CHANGED]);

	/* Say that all the files have changed.
	 * We could optimize this to only mention files that
	 * have metadata, but this is a fine rough cut for now.
	 */
	nautilus_directory_emit_files_changed (directory,
					       directory->details->files);
}

static char *
uri_get_directory_part (const char *uri)
{
	GnomeVFSURI *vfs_uri, *directory_vfs_uri;
	char *directory_uri;

	/* Make VFS version of URI. */
	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL) {
		return NULL;
	}

	/* Make VFS version of directory URI. */
	directory_vfs_uri = gnome_vfs_uri_get_parent (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);
	if (directory_vfs_uri == NULL) {
		return NULL;
	}

	/* Make text version of directory URI. */
	directory_uri = gnome_vfs_uri_to_string (directory_vfs_uri,
						 GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (directory_vfs_uri);
	
	return directory_uri;
}

static char *
uri_get_basename (const char *uri)
{
	GnomeVFSURI *vfs_uri;
	char *name;

	/* Make VFS version of URI. */
	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL) {
		return NULL;
	}

	/* Extract name part. */
	name = gnome_vfs_uri_extract_short_name (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);

	return name;
}

/* Return a directory object for this one's parent. */
static NautilusDirectory *
get_parent_directory (const char *uri)
{
	char *directory_uri;
	NautilusDirectory *directory;

	directory_uri = uri_get_directory_part (uri);
	directory = nautilus_directory_get (directory_uri);
	g_free (directory_uri);
	return directory;
}

/* If a directory object exists for this one's parent, then
 * return it, otherwise return NULL.
 */
static NautilusDirectory *
get_parent_directory_if_exists (const char *uri)
{
	char *directory_uri;
	NautilusDirectory *directory;

	/* Make text version of directory URI. */
	directory_uri = uri_get_directory_part (uri);
	g_assert (is_canonical_uri (directory_uri));

	/* Get directory from hash table. */
	if (directory_objects == NULL) {
		directory = NULL;
	} else {
		directory = g_hash_table_lookup (directory_objects, directory_uri);
	}
	g_free (directory_uri);
	return directory;
}

static NautilusFile *
get_file_if_exists (const char *uri)
{
	NautilusDirectory *directory;
	char *name;
	NautilusFile *file;

	/* Get parent directory. */
	directory = get_parent_directory_if_exists (uri);
	if (directory == NULL) {
		return NULL;
	}

	/* Find the file. */
	name = uri_get_basename (uri);
	file = nautilus_directory_find_file (directory, name);
	g_free (name);

	return file;
}

void
nautilus_directory_notify_files_added (GList *uris)
{
	GList *p;
	NautilusDirectory *directory;
	GnomeVFSFileInfo *info;
	const char *uri;
	GnomeVFSResult result;

	/* FIXME bugzilla.eazel.com 465: 
	   gnome_vfs_file_info calls need to be
	   called asynchronously. We probably need a new gnome_vfs call that 
	   takes a list of URIs and generates a list of file info structures.
	 */
	for (p = uris; p != NULL; p = p->next) {
		uri = (const char *) p->data;

		/* See if the directory is already known. */
		directory = get_parent_directory_if_exists (uri);
		if (directory == NULL) {
			continue;
		}

		/* If no one is monitoring files in the directory, nothing to do. */
		if (!nautilus_directory_is_file_list_monitored (directory)) {
			continue;
		}

		info = gnome_vfs_file_info_new ();
		result = gnome_vfs_get_file_info (uri, info, GNOME_VFS_FILE_INFO_DEFAULT, NULL);
		if (result == GNOME_VFS_OK) {
			gnome_vfs_file_info_ref (info);
        		directory->details->pending_file_info
				= g_list_prepend (directory->details->pending_file_info, info);
			
			nautilus_directory_schedule_dequeue_pending (directory);
		}
		
		gnome_vfs_file_info_unref (info);
	}
}

static void
call_files_added (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (key));
	g_assert (value != NULL);
	g_assert (user_data == NULL);

	gtk_signal_emit (GTK_OBJECT (key),
			 signals[FILES_ADDED],
			 value);
}

static void
call_files_changed (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (key));
	g_assert (value != NULL);
	g_assert (user_data == NULL);

	nautilus_directory_emit_files_changed (key, value);
}

static void
hash_table_list_prepend (GHashTable *table, gconstpointer key, gpointer data)
{
	GList *list;

	list = g_hash_table_lookup (table, key);
	list = g_list_prepend (list, data);
	g_hash_table_insert (table, (gpointer) key, list);
}

void
nautilus_directory_notify_files_removed (GList *uris)
{
	GHashTable *changed_lists;
	GList *p;
	const char *uri;
	NautilusFile *file;

	/* Make a list of changed files in each directory. */
	changed_lists = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* Go through all the notifications. */
	for (p = uris; p != NULL; p = p->next) {
		uri = (const char *) p->data;

		/* Find the file. */
		file = get_file_if_exists (uri);
		if (file == NULL) {
			continue;
		}

		/* Mark it gone and prepare to send the changed signal. */
		nautilus_file_mark_gone (file);
		hash_table_list_prepend (changed_lists,
					 file->details->directory,
					 file);
	}

	/* Now send out the changed signals. */
	g_hash_table_foreach (changed_lists, call_files_changed, NULL);
	g_hash_table_destroy (changed_lists);
}

void
nautilus_directory_notify_files_moved (GList *uri_pairs)
{
	GList *p;
	URIPair *pair;
	NautilusFile *file;
	NautilusDirectory *old_directory, *new_directory;
	GList *new_files_list, *unref_list;
	GHashTable *added_lists, *changed_lists;
	GList **files;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;

	/* Make a list of added and changed files in each directory. */
	new_files_list = NULL;
	added_lists = g_hash_table_new (g_direct_hash, g_direct_equal);
	changed_lists = g_hash_table_new (g_direct_hash, g_direct_equal);
	unref_list = NULL;

	for (p = uri_pairs; p != NULL; p = p->next) {
		pair = p->data;

		/* Move an existing file. */
		file = get_file_if_exists (pair->from_uri);
		if (file == NULL) {
			/* Handle this as it it was a new file. */
			new_files_list = g_list_prepend (new_files_list,
							 pair->to_uri);
		} else {
			/* Handle notification in the old directory. */
			old_directory = file->details->directory;
			hash_table_list_prepend (changed_lists,
						 old_directory,
						 file);

			/* Locate the new directory. */
			new_directory = get_parent_directory (pair->to_uri);
			g_assert (new_directory != NULL);

			/* Point the file at the new directory.
			 * We already have a ref to it from the get_parent_directory
			 * function so we don't have to ref. We do have to unref
			 * the old directory, but we better not do that before
			 * the code below runs.
			 */
			file->details->directory = new_directory;


			/* If the file is moving, between directories, there
			 * is more to do.
			 */
			if (new_directory != old_directory) {
				/* Remove from old directory. */
				files = &old_directory->details->files;
				g_assert (g_list_find (*files, file) != NULL);
				*files = g_list_remove (*files, file);
				
				/* FIXME bugzilla.eazel.com 465: 
				 * Need to call get info in async mode. 
				 */
				info = gnome_vfs_file_info_new ();
				result = gnome_vfs_get_file_info (pair->to_uri, info, 
								  GNOME_VFS_FILE_INFO_DEFAULT, 
								  NULL);
				if (result == GNOME_VFS_OK) {
					gnome_vfs_file_info_ref (info);
					nautilus_file_update (file, info);

					/* Add to new directory. */
					files = &new_directory->details->files;
					g_assert (g_list_find (*files, file) == NULL);
					*files = g_list_prepend (*files, file);
					
					/* Handle notification in the new directory. */
					hash_table_list_prepend (added_lists,
								 new_directory,
								 file);
				}
				gnome_vfs_file_info_unref (info);
			}

			/* If the old directory was monitoring files, then it
			 * may have been the only one with a ref to the file.
			 */
			if (nautilus_directory_is_file_list_monitored (old_directory)) {
				unref_list = g_list_prepend (unref_list, file);
			}
			
			/* Done with the old directory. */
			nautilus_directory_unref (old_directory);
		}
	}

	/* Now send out the changed and added signals for existing file objects. */
	g_hash_table_foreach (changed_lists, call_files_changed, NULL);
	g_hash_table_destroy (changed_lists);
	g_hash_table_foreach (added_lists, call_files_added, NULL);
	g_hash_table_destroy (added_lists);

	/* Let the file objects go. */
	g_list_foreach (unref_list, (GFunc) nautilus_file_unref, NULL);
	g_list_free (unref_list);

	/* Separate handling for brand new file objects. */
	nautilus_directory_notify_files_added (new_files_list);
	g_list_free (new_files_list);
}

gboolean
nautilus_directory_contains_file (NautilusDirectory *directory,
				  NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	if (nautilus_file_is_gone (file)) {
		return FALSE;
	}

	return file->details->directory == directory;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

static int data_dummy;
static guint file_count;
static gboolean got_metadata_flag;

static void
get_files_callback (NautilusDirectory *directory, GList *files, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (files != NULL);
	g_assert (callback_data == &data_dummy);

	file_count += g_list_length (files);
}

static void
got_metadata_callback (NautilusDirectory *directory, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (callback_data == &data_dummy);

	got_metadata_flag = TRUE;
}

/* Return the number of extant NautilusDirectories */
int
nautilus_directory_number_outstanding (void)
{
        return g_hash_table_size (directory_objects);
}

void
nautilus_self_check_directory (void)
{
	NautilusDirectory *directory;
	GList *list;

	list = g_list_prepend (NULL, "TEST");

	directory = nautilus_directory_get ("file:///etc");

	NAUTILUS_CHECK_INTEGER_RESULT (g_hash_table_size (directory_objects), 1);

	file_count = 0;
	nautilus_directory_file_monitor_add (directory, &file_count,
					     NULL, NULL,
					     get_files_callback, &data_dummy);

	got_metadata_flag = FALSE;
	nautilus_directory_call_when_ready (directory, list, NULL,
					    got_metadata_callback, &data_dummy);

	while (!got_metadata_flag) {
		gtk_main_iteration ();
	}

	nautilus_directory_set_metadata (directory, "TEST", "default", "value");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_get_metadata (directory, "TEST", "default"), "value");

	nautilus_directory_set_boolean_metadata (directory, "TEST_BOOLEAN", TRUE, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_directory_get_boolean_metadata (directory, "TEST_BOOLEAN", TRUE), TRUE);
	nautilus_directory_set_boolean_metadata (directory, "TEST_BOOLEAN", TRUE, FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_directory_get_boolean_metadata (directory, "TEST_BOOLEAN", TRUE), FALSE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_directory_get_boolean_metadata (NULL, "TEST_BOOLEAN", TRUE), TRUE);

	nautilus_directory_set_integer_metadata (directory, "TEST_INTEGER", 0, 17);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_get_integer_metadata (directory, "TEST_INTEGER", 0), 17);
	nautilus_directory_set_integer_metadata (directory, "TEST_INTEGER", 0, -1);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_get_integer_metadata (directory, "TEST_INTEGER", 0), -1);
	nautilus_directory_set_integer_metadata (directory, "TEST_INTEGER", 42, 42);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_get_integer_metadata (directory, "TEST_INTEGER", 42), 42);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_get_integer_metadata (NULL, "TEST_INTEGER", 42), 42);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_directory_get_integer_metadata (directory, "NONEXISTENT_KEY", 42), 42);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_directory_get ("file:///etc") == directory, TRUE);
	nautilus_directory_unref (directory);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_directory_get ("file:///etc/") == directory, TRUE);
	nautilus_directory_unref (directory);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_directory_get ("file:///etc////") == directory, TRUE);
	nautilus_directory_unref (directory);

	nautilus_directory_file_monitor_remove (directory, &file_count);

	nautilus_directory_unref (directory);

	while (g_hash_table_size (directory_objects) != 0) {
		gtk_main_iteration ();
	}

	NAUTILUS_CHECK_INTEGER_RESULT (g_hash_table_size (directory_objects), 0);

	directory = nautilus_directory_get ("file:///etc");

	got_metadata_flag = FALSE;
	nautilus_directory_call_when_ready (directory, list, NULL,
					    got_metadata_callback, &data_dummy);

	while (!got_metadata_flag) {
		gtk_main_iteration ();
	}

	NAUTILUS_CHECK_BOOLEAN_RESULT (directory->details->metafile != NULL, TRUE);

	NAUTILUS_CHECK_INTEGER_RESULT (g_hash_table_size (directory_objects), 1);

	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_get_metadata (directory, "TEST", "default"), "value");

	nautilus_directory_unref (directory);

	/* escape_slashes */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes (""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("/"), "%2F");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("%"), "%25");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("a/a"), "a%2Fa");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("a%a"), "a%25a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("%25"), "%2525");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("%2F"), "%252F");

	g_list_free (list);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
