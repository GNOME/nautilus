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

#include "nautilus-directory-metafile.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-string.h"
#include <ctype.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

enum {
	FILES_ADDED,
	FILES_CHANGED,
	METADATA_CHANGED,
	DONE_LOADING,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Specifications for in-directory metafile. */
#define METAFILE_NAME ".nautilus-metafile.xml"

/* Specifications for parallel-directory metafile. */
#define METAFILES_DIRECTORY_NAME "metafiles"
#define METAFILE_SUFFIX ".xml"
#define METAFILES_DIRECTORY_PERMISSIONS \
	(GNOME_VFS_PERM_USER_ALL \
         | GNOME_VFS_PERM_GROUP_ALL \
	 | GNOME_VFS_PERM_OTHER_ALL)

static GnomeVFSURI *      construct_private_metafile_uri      (GnomeVFSURI *uri);
static void               nautilus_directory_destroy          (GtkObject   *object);
static void               nautilus_directory_initialize       (gpointer     object,
							       gpointer     klass);
static void               nautilus_directory_initialize_class (gpointer     klass);
static GnomeVFSResult     nautilus_make_directory_and_parents (GnomeVFSURI *uri,
							       guint        permissions);
static NautilusDirectory *nautilus_directory_new              (const char  *uri);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusDirectory, nautilus_directory, GTK_TYPE_OBJECT)

static GHashTable *directory_objects;

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
	signals[DONE_LOADING] =
		gtk_signal_new ("done_loading",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDirectoryClass, done_loading),
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

	g_assert (directory->details->metafile_write_state == NULL);
	nautilus_directory_cancel (directory);
	g_assert (directory->details->metafile_read_state == NULL);
	g_assert (directory->details->count_in_progress == NULL);
	g_assert (directory->details->top_left_read_state == NULL);

	nautilus_directory_stop_monitoring_file_list (directory);

	if (directory->details->monitor_list != NULL) {
		g_warning ("destroying a NautilusDirectory while it's being monitored");
		nautilus_g_list_free_deep (directory->details->monitor_list);
	}

	g_hash_table_remove (directory_objects, directory->details->uri_text);

	if (directory->details->dequeue_pending_idle_id != 0) {
		gtk_idle_remove (directory->details->dequeue_pending_idle_id);
	}
 
	g_free (directory->details->uri_text);
	gnome_vfs_uri_unref (directory->details->uri);
	gnome_vfs_uri_unref (directory->details->private_metafile_uri);
	gnome_vfs_uri_unref (directory->details->public_metafile_uri);
	g_assert (directory->details->files == NULL);
	nautilus_directory_metafile_destroy (directory);
	g_assert (directory->details->directory_load_in_progress == NULL);
	g_assert (directory->details->count_in_progress == NULL);
	g_assert (directory->details->dequeue_pending_idle_id == 0);
	gnome_vfs_file_info_list_unref (directory->details->pending_file_info);
	g_assert (directory->details->write_metafile_idle_id == 0);

	g_free (directory->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static char *
make_uri_canonical (const char *uri)
{
	size_t length;
	char *canonical_uri, *old_uri, *with_slashes, *p;

	/* FIXME bugzilla.eazel.com 648: 
	 * This currently ignores the issue of two uris that are not identical but point
	 * to the same data except for the specific cases of trailing '/' characters,
	 * file:/ and file:///, and "lack of file:".
	 */

	/* Strip the trailing "/" characters. */
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

	/* Add file: if there is no scheme. */
	if (strchr (canonical_uri, ':') == NULL) {
		old_uri = canonical_uri;
		canonical_uri = g_strconcat ("file:", old_uri, NULL);
		g_free (old_uri);
	}

	/* Lower-case the scheme. */
	for (p = canonical_uri; *p != ':'; p++) {
		g_assert (*p != '\0');
		if (isupper (*p)) {
			*p = tolower (*p);
		}
	}

	/* Convert file:/ to file:/// */
	if (nautilus_str_has_prefix (canonical_uri, "file:/")
	    && !nautilus_str_has_prefix (canonical_uri, "file:///")) {
		old_uri = canonical_uri;
		canonical_uri = g_strconcat ("file://", old_uri + 5, NULL);
		g_free (old_uri);
	}

	return canonical_uri;
}

#ifndef G_DISABLE_ASSERT

static gboolean
is_canonical_uri (const char *uri)
{
	const char *p;

	if (uri == NULL) {
		return FALSE;
	}
	if (nautilus_str_has_suffix (uri, "/")) {
		if (nautilus_str_has_suffix (uri, ":///")) {
			return TRUE;
		}
		return FALSE;
	}
	if (strchr (uri, ':') == NULL) {
		return FALSE;
	}
	for (p = uri; *p != ':'; p++) {
		if (isupper (*p)) {
			return FALSE;
		}
	}
	if (nautilus_str_has_prefix (uri, "file:/")
	    && !nautilus_str_has_prefix (uri, "file:///")) {
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
	char *canonical_uri;
	NautilusDirectory *directory;

	if (uri == NULL) {
    		return NULL;
	}

	canonical_uri = make_uri_canonical (uri);

	/* Create the hash table first time through. */
	if (directory_objects == NULL) {
		directory_objects = g_hash_table_new (g_str_hash, g_str_equal);
	}

	/* If the object is already in the hash table, look it up. */

	g_assert (is_canonical_uri (canonical_uri));
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
	if (result != GNOME_VFS_ERROR_NOT_FOUND) {
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
construct_private_metafile_uri (GnomeVFSURI *uri)
{
	GnomeVFSResult result;
	GnomeVFSURI *nautilus_directory_uri, *metafiles_directory_uri, *alternate_uri;
	char *uri_as_string, *escaped_uri, *file_name;
	char *user_directory;

	/* Ensure that the metafiles directory exists. */
	user_directory = nautilus_get_user_directory ();
	nautilus_directory_uri = gnome_vfs_uri_new (user_directory);
	g_free (user_directory);

	metafiles_directory_uri = gnome_vfs_uri_append_file_name (nautilus_directory_uri,
								  METAFILES_DIRECTORY_NAME);
	gnome_vfs_uri_unref (nautilus_directory_uri);
	result = nautilus_make_directory_and_parents (metafiles_directory_uri,
						      METAFILES_DIRECTORY_PERMISSIONS);
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_FILE_EXISTS) {
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
	alternate_uri = gnome_vfs_uri_append_file_name (metafiles_directory_uri, file_name);
	gnome_vfs_uri_unref (metafiles_directory_uri);
	g_free (file_name);

	return alternate_uri;
}

static NautilusDirectory *
nautilus_directory_new (const char* uri)
{
	NautilusDirectory *directory;
	GnomeVFSURI *vfs_uri;
	GnomeVFSURI *private_metafile_uri;
	GnomeVFSURI *public_metafile_uri;

	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL) {
		return NULL;
	}

	private_metafile_uri = construct_private_metafile_uri (vfs_uri);
	public_metafile_uri = gnome_vfs_uri_append_file_name (vfs_uri, METAFILE_NAME);

	directory = gtk_type_new (NAUTILUS_TYPE_DIRECTORY);

	directory->details->uri_text = g_strdup (uri);
	directory->details->uri = vfs_uri;
	directory->details->private_metafile_uri = private_metafile_uri;
	directory->details->public_metafile_uri = public_metafile_uri;

	return directory;
}

gboolean
nautilus_directory_is_local (NautilusDirectory *directory)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	
	return gnome_vfs_uri_is_local (directory->details->uri);
}

/* FIXME: I think this should be named _is_virtual_directory. */
gboolean
nautilus_directory_is_search_directory (NautilusDirectory *directory)
{
	if (directory == NULL) {
		return FALSE;
	}

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);

	/* Two hard-coded schemes for now. */
	/* FIXME: Later we gnome-vfs will tell us somehow that this is
	 * a virtual directory.
	 */
	return nautilus_istr_has_prefix (directory->details->uri_text, "search:")
		|| nautilus_istr_has_prefix (directory->details->uri_text, "gnome-search:");

#if 0
	GnomeVFSFileInfo *info;
	gboolean is_search_directory;
	GnomeVFSResult result;

	info = gnome_vfs_file_info_new ();

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	
	/* FIXME bugzilla.eazel.com 1263: We can't just do sync. I/O
	 * here! The model is that get_ functions in
	 * NautilusDirectory/File are supposed to do no I/O. The
	 * aforementioned bug talks about this a bit.
	 */
	/* FIXME bugzilla.eazel.com 1263: Should make use of some sort
	 * of NautilusDirectory cover for getting the mime type.
	 */
	result = gnome_vfs_get_file_info_uri (directory->details->uri, 
					      info,
					      GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
	is_search_directory = result == GNOME_VFS_OK &&
		nautilus_strcasecmp (info->mime_type, "x-special/virtual-directory") == 0;
	
	gnome_vfs_file_info_unref (info);

	return is_search_directory;
#endif
}

gboolean
nautilus_directory_are_all_files_seen (NautilusDirectory *directory)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	
	return directory->details->directory_loaded;
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

void
nautilus_directory_emit_done_loading (NautilusDirectory *directory)
{
	gtk_signal_emit (GTK_OBJECT (directory),
			 signals[DONE_LOADING]);
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
	char *escaped_name, *name;

	/* Make VFS version of URI. */
	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL) {
		return NULL;
	}

	/* Extract name part. */
	escaped_name = gnome_vfs_uri_extract_short_path_name (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);
	name = gnome_vfs_unescape_string (escaped_name, NULL);
	g_free (escaped_name);

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
	char *directory_uri, *canonical_uri;
	NautilusDirectory *directory;

	/* Make text version of directory URI. */
	directory_uri = uri_get_directory_part (uri);
	canonical_uri = make_uri_canonical (directory_uri);
	g_free (directory_uri);

	/* Get directory from hash table. */
	if (directory_objects == NULL) {
		directory = NULL;
	} else {
		g_assert (is_canonical_uri (canonical_uri));
		directory = g_hash_table_lookup (directory_objects,
						 canonical_uri);
	}
	g_free (canonical_uri);
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

static void
hash_table_list_prepend (GHashTable *table, gconstpointer key, gpointer data)
{
	GList *list;

	list = g_hash_table_lookup (table, key);
	list = g_list_prepend (list, data);
	g_hash_table_insert (table, (gpointer) key, list);
}

static void
call_files_added_free_list (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (key));
	g_assert (value != NULL);
	g_assert (user_data == NULL);

	gtk_signal_emit (GTK_OBJECT (key),
			 signals[FILES_ADDED],
			 value);
	g_list_free (value);
}

static void
call_files_changed_free_list (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (key));
	g_assert (value != NULL);
	g_assert (user_data == NULL);

	nautilus_directory_emit_files_changed (key, value);
	g_list_free (value);
}

static void
call_files_changed_unref_free_list (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (key));
	g_assert (value != NULL);
	g_assert (user_data == NULL);

	nautilus_directory_emit_files_changed (key, value);
	nautilus_file_list_free (value);
}

static void
call_get_file_info_free_list (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (key));
	g_assert (value != NULL);
	g_assert (user_data == NULL);

	nautilus_directory_get_info_for_new_files (key, value);
	gnome_vfs_uri_list_free (value);
}

void
nautilus_directory_notify_files_added (GList *uris)
{
	GHashTable *added_lists;
	GList *p;
	NautilusDirectory *directory;
	const char *uri;
	GnomeVFSURI *vfs_uri;

	/* Make a list of added files in each directory. */
	added_lists = g_hash_table_new (g_direct_hash, g_direct_equal);

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

		/* Collect the URIs to use. */
		vfs_uri = gnome_vfs_uri_new (uri);
		if (vfs_uri == NULL) {
			g_warning ("bad uri %s", uri);
			continue;
		}
		hash_table_list_prepend (added_lists, directory, vfs_uri);
	}


	/* Now send out the changed signals. */
	g_hash_table_foreach (added_lists, call_get_file_info_free_list, NULL);
	g_hash_table_destroy (added_lists);
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
		nautilus_file_ref (file);
		nautilus_file_mark_gone (file);
		hash_table_list_prepend (changed_lists,
					 file->details->directory,
					 file);
	}

	/* Now send out the changed signals. */
	g_hash_table_foreach (changed_lists, call_files_changed_unref_free_list, NULL);
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
	char *name;

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

			/* If the file is moving between directories,
			 * there is more to do.
			 */
			if (new_directory != old_directory) {
				/* Remove from old directory. */
				files = &old_directory->details->files;
				g_assert (g_list_find (*files, file) != NULL);
				*files = g_list_remove (*files, file);

				/* Update the name. */
				name = uri_get_basename (pair->to_uri);
				nautilus_file_update_name (file, name);
				g_free (name);
				
				/* Add to new directory. */
				files = &new_directory->details->files;
				g_assert (g_list_find (*files, file) == NULL);
				*files = g_list_prepend (*files, file);
				
				/* Handle notification in the new directory. */
				hash_table_list_prepend (added_lists,
							 new_directory,
							 file);
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
	g_hash_table_foreach (changed_lists, call_files_changed_free_list, NULL);
	g_hash_table_destroy (changed_lists);
	g_hash_table_foreach (added_lists, call_files_added_free_list, NULL);
	g_hash_table_destroy (added_lists);

	/* Let the file objects go. */
	nautilus_g_list_free_deep_custom (unref_list, (GFunc) nautilus_file_unref, NULL);

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

void
nautilus_directory_call_when_ready (NautilusDirectory *directory,
				    GList *file_attributes,
				    gboolean wait_for_metadata,
				    NautilusDirectoryCallback callback,
				    gpointer callback_data)
{
	g_return_if_fail (directory == NULL || NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (wait_for_metadata == FALSE || wait_for_metadata == TRUE);
	g_return_if_fail (callback != NULL);

	nautilus_directory_call_when_ready_internal
		(directory,
		 NULL,
		 file_attributes,
		 wait_for_metadata,
		 callback,
		 NULL,
		 callback_data);
}

typedef struct {
	gboolean done;
	GList *file_return;
} wait_until_ready_callback_data;

static void
nautilus_directory_wait_until_ready_callback (NautilusDirectory *directory,
					      GList             *files,
					      wait_until_ready_callback_data *data)
{
	data->done = TRUE;
	data->file_return = nautilus_file_list_copy (files);
}


GList *
nautilus_directory_wait_until_ready (NautilusDirectory *directory,
				     GList             *file_attributes,
				     gboolean           wait_for_metadata)
{
	wait_until_ready_callback_data data;

	data.done = FALSE;
	data.file_return = NULL;

	nautilus_directory_call_when_ready (directory,
					    file_attributes,
					    wait_for_metadata,
					    (NautilusDirectoryCallback) nautilus_directory_wait_until_ready_callback,
					    (gpointer) &data);
	
	while (!data.done) {
		gtk_main_iteration ();
	}

	return data.file_return;
}

void
nautilus_directory_cancel_callback (NautilusDirectory *directory,
				    NautilusDirectoryCallback callback,
				    gpointer callback_data)
{
	g_return_if_fail (directory == NULL || NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (callback != NULL);

	nautilus_directory_cancel_callback_internal
		(directory,
		 NULL,
		 callback,
		 NULL,
		 callback_data);
}

void
nautilus_directory_file_monitor_add (NautilusDirectory *directory,
				     gconstpointer client,
				     GList *file_attributes,
				     gboolean monitor_metadata,
				     gboolean force_reload,
				     NautilusDirectoryCallback callback,
				     gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (client != NULL);
	g_return_if_fail (callback != NULL);

	if (force_reload) {
		nautilus_directory_force_reload (directory);
	}

	nautilus_directory_monitor_add_internal
		(directory, NULL,
		 client,
		 file_attributes, monitor_metadata,
		 force_reload ? NULL : callback,
		 callback_data);
}

void
nautilus_directory_file_monitor_remove (NautilusDirectory *directory,
					gconstpointer client)
{
	nautilus_directory_monitor_remove_internal (directory, NULL, client);
}

static int
any_non_metafile_item (gconstpointer item, gconstpointer callback_data)
{
	/* A metafile is exactly what we are not looking for, anything else is a match. */
	return nautilus_file_matches_uri
		(NAUTILUS_FILE (item), (const char *) callback_data)
		? 1 : 0;
}

gboolean
nautilus_directory_is_not_empty (NautilusDirectory *directory)
{
	char *public_metafile_uri_string;
	gboolean not_empty;
	
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);

	/* Directory must be monitored for this call to be correct. */
	g_return_val_if_fail (nautilus_directory_is_file_list_monitored (directory), FALSE);

	public_metafile_uri_string = gnome_vfs_uri_to_string
		(directory->details->public_metafile_uri,
		 GNOME_VFS_URI_HIDE_NONE);

	/* Return TRUE if the directory contains anything besides a metafile. */
	not_empty = g_list_find_custom (directory->details->files,
					public_metafile_uri_string,
					any_non_metafile_item) != NULL;

	g_free (public_metafile_uri_string);

	return not_empty;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

#include "nautilus-debug.h"
#include "nautilus-file-attributes.h"

static int data_dummy;
static guint file_count;
static gboolean got_metadata_flag;
static gboolean got_files_flag;

static void
get_files_callback (NautilusDirectory *directory, GList *files, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (files != NULL);
	g_assert (callback_data == &data_dummy);

	file_count += g_list_length (files);
}

static void
got_metadata_callback (NautilusDirectory *directory, GList *files, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (files == NULL);
	g_assert (callback_data == &data_dummy);

	got_metadata_flag = TRUE;
}

static void
got_files_callback (NautilusDirectory *directory, GList *files, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (g_list_length (files) > 10);
	g_assert (callback_data == &data_dummy);

	got_files_flag = TRUE;
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
	GList *attributes;

	directory = nautilus_directory_get ("file:///etc");

	NAUTILUS_CHECK_INTEGER_RESULT (g_hash_table_size (directory_objects), 1);

	file_count = 0;
	nautilus_directory_file_monitor_add
		(directory, &file_count,
		 NULL, FALSE, FALSE,
		 get_files_callback, &data_dummy);

	got_metadata_flag = FALSE;
	nautilus_directory_call_when_ready (directory, NULL, TRUE,
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
	nautilus_directory_call_when_ready (directory, NULL, TRUE,
					    got_metadata_callback, &data_dummy);

	while (!got_metadata_flag) {
		gtk_main_iteration ();
	}

	NAUTILUS_CHECK_BOOLEAN_RESULT (directory->details->metafile != NULL, TRUE);

	got_files_flag = FALSE;

	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_FAST_MIME_TYPE);
	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS);
	nautilus_directory_call_when_ready (directory, attributes, FALSE,
					    got_files_callback, &data_dummy);
	g_list_free (attributes);

	while (!got_files_flag) {
		gtk_main_iteration ();
	}

	NAUTILUS_CHECK_BOOLEAN_RESULT (directory->details->files == NULL, TRUE);

	NAUTILUS_CHECK_INTEGER_RESULT (g_hash_table_size (directory_objects), 1);

	NAUTILUS_CHECK_STRING_RESULT (nautilus_directory_get_metadata (directory, "TEST", "default"), "value");

	nautilus_directory_unref (directory);

	NAUTILUS_CHECK_INTEGER_RESULT (g_hash_table_size (directory_objects), 0);

	/* escape_slashes */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes (""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("a"), "a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("/"), "%2F");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("%"), "%25");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("a/a"), "a%2Fa");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("a%a"), "a%25a");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("%25"), "%2525");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_str_escape_slashes ("%2F"), "%252F");

	/* make_uri_canonical */
	NAUTILUS_CHECK_STRING_RESULT (make_uri_canonical (""), "file:");
	NAUTILUS_CHECK_STRING_RESULT (make_uri_canonical ("file:/"), "file:///");
	NAUTILUS_CHECK_STRING_RESULT (make_uri_canonical ("file:///"), "file:///");
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
