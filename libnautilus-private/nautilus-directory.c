/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory.c: Nautilus directory model.
 
   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include "nautilus-directory-private.h"

#include "nautilus-directory-metafile.h"
#include "nautilus-directory-notify.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-metadata.h"
#include "nautilus-metafile.h"
#include "nautilus-trash-directory.h"
#include "nautilus-vfs-directory.h"
#include <ctype.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

enum {
	FILES_ADDED,
	FILES_CHANGED,
	DONE_LOADING,
	LOAD_ERROR,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Specifications for parallel-directory metafile. */
#define METAFILES_DIRECTORY_NAME "metafiles"
#define METAFILE_SUFFIX ".xml"
#define METAFILES_DIRECTORY_PERMISSIONS \
	(GNOME_VFS_PERM_USER_ALL \
         | GNOME_VFS_PERM_GROUP_ALL \
	 | GNOME_VFS_PERM_OTHER_ALL)

static GHashTable *directories;

static void               nautilus_directory_destroy          (GtkObject              *object);
static void               nautilus_directory_initialize       (gpointer                object,
							       gpointer                klass);
static void               nautilus_directory_initialize_class (NautilusDirectoryClass *klass);
static NautilusDirectory *nautilus_directory_new              (const char             *uri);
static char *             real_get_name_for_self_as_new_file  (NautilusDirectory      *directory);
static void               set_directory_uri                   (NautilusDirectory      *directory,
							       const char             *new_uri);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusDirectory,
				   nautilus_directory,
				   GTK_TYPE_OBJECT)

static void
nautilus_directory_initialize_class (NautilusDirectoryClass *klass)
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
	signals[DONE_LOADING] =
		gtk_signal_new ("done_loading",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDirectoryClass, done_loading),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[LOAD_ERROR] =
		gtk_signal_new ("load_error",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDirectoryClass, load_error),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	klass->get_name_for_self_as_new_file = real_get_name_for_self_as_new_file;
}

static void
nautilus_directory_initialize (gpointer object, gpointer klass)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY(object);

	directory->details = g_new0 (NautilusDirectoryDetails, 1);
	directory->details->file_hash = g_hash_table_new (g_str_hash, g_str_equal);
	directory->details->high_priority_queue = nautilus_file_queue_new ();
	directory->details->low_priority_queue = nautilus_file_queue_new ();
	directory->details->idle_queue = nautilus_idle_queue_new ();
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

	nautilus_directory_cancel (directory);
	g_assert (directory->details->count_in_progress == NULL);
	g_assert (directory->details->top_left_read_state == NULL);

	if (directory->details->monitor_list != NULL) {
		g_warning ("destroying a NautilusDirectory while it's being monitored");
		eel_g_list_free_deep (directory->details->monitor_list);
	}

	if (directory->details->monitor != NULL) {
		nautilus_monitor_cancel (directory->details->monitor);
	}

	if (directory->details->metafile_monitor != NULL) {
		nautilus_directory_unregister_metadata_monitor (directory);
	}

	if (directory->details->metafile_corba_object != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (directory->details->metafile_corba_object, NULL);
	}

	g_hash_table_remove (directories, directory->details->uri);

	if (directory->details->dequeue_pending_idle_id != 0) {
		gtk_idle_remove (directory->details->dequeue_pending_idle_id);
	}
 
	g_free (directory->details->uri);
	if (directory->details->vfs_uri != NULL) {
		gnome_vfs_uri_unref (directory->details->vfs_uri);
	}
	g_assert (directory->details->file_list == NULL);
	g_hash_table_destroy (directory->details->file_hash);
	nautilus_file_queue_destroy (directory->details->high_priority_queue);
	nautilus_file_queue_destroy (directory->details->low_priority_queue);
	nautilus_idle_queue_destroy (directory->details->idle_queue);
	g_assert (directory->details->directory_load_in_progress == NULL);
	g_assert (directory->details->count_in_progress == NULL);
	g_assert (directory->details->dequeue_pending_idle_id == 0);
	gnome_vfs_file_info_list_unref (directory->details->pending_file_info);

	g_free (directory->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
invalidate_one_count (gpointer key, gpointer value, gpointer user_data)
{
	NautilusDirectory *directory;

	g_assert (key != NULL);
	g_assert (NAUTILUS_IS_DIRECTORY (value));
	g_assert (user_data == NULL);

	directory = NAUTILUS_DIRECTORY (value);
	
	nautilus_directory_invalidate_count_and_mime_list (directory);
}

static void
filtering_changed_callback (gpointer callback_data)
{
	g_assert (callback_data == NULL);

	/* Preference about which items to show has changed, so we
	 * can't trust any of our precomputed directory counts.
	 */
	g_hash_table_foreach (directories, invalidate_one_count, NULL);
}

void
emit_change_signals_for_all_files (NautilusDirectory *directory)
{
	GList *files;

	files = eel_g_list_copy (directory->details->file_list);
	if (directory->details->as_file != NULL) {
		files = g_list_prepend (files, directory->details->as_file);
	}

	nautilus_directory_emit_change_signals (directory, files);

	g_list_free (files);
}

static void
async_state_changed_one (gpointer key, gpointer value, gpointer user_data)
{
	NautilusDirectory *directory;

	g_assert (key != NULL);
	g_assert (NAUTILUS_IS_DIRECTORY (value));
	g_assert (user_data == NULL);

	directory = NAUTILUS_DIRECTORY (value);
	
	nautilus_directory_async_state_changed (directory);
	emit_change_signals_for_all_files (directory);
}

static void
async_data_preference_changed_callback (gpointer callback_data)
{
	g_assert (callback_data == NULL);

	/* Preference involving fetched async data has changed, so
	 * we have to kick off refetching all async data, and tell
	 * each file that it (might have) changed.
	 */
	g_hash_table_foreach (directories, async_state_changed_one, NULL);
}

static void
add_preferences_callbacks (void)
{
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
				      filtering_changed_callback,
				      NULL);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
				      filtering_changed_callback,
				      NULL);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
				      async_data_preference_changed_callback,
				      NULL);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
				      async_data_preference_changed_callback,
				      NULL);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
				      async_data_preference_changed_callback,
				      NULL);
}

char *
nautilus_directory_make_uri_canonical (const char *uri)
{
	char *canonical_maybe_trailing_slash;
	char *canonical;
	char *with_slashes;
	size_t length;

	canonical_maybe_trailing_slash = eel_make_uri_canonical (uri);

	/* To NautilusDirectory, a uri with or without a trailing
	 * / is equivalent. This is necessary to prevent separate
	 * NautilusDirectories for the same location from being
	 * created. (See bugzilla.gnome.org 43322 for an example.)
	 */
	canonical = eel_str_strip_trailing_chr (canonical_maybe_trailing_slash, '/');
	if (strcmp (canonical, canonical_maybe_trailing_slash) != 0 &&
	    strcmp (canonical, "favorites:") != 0) {
		/* If some trailing '/' were stripped, there's the possibility,
		 * that we stripped away all the '/' from a uri that has only
		 * '/' characters. If you change this code, check to make sure
		 * that "file:///" still works as a URI.
		 */
		length = strlen (canonical);
		if (length == 0 || canonical[length - 1] == ':') {
			with_slashes = g_strconcat (canonical, "///", NULL);
			g_free (canonical);
			canonical = with_slashes;
		}
	}

	g_free (canonical_maybe_trailing_slash);
	
	return canonical;
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
nautilus_directory_get_internal (const char *uri, gboolean create)
{
	char *canonical_uri;
	NautilusDirectory *directory;

	if (uri == NULL) {
    		return NULL;
	}

	canonical_uri = nautilus_directory_make_uri_canonical (uri);

	/* Create the hash table first time through. */
	if (directories == NULL) {
		directories = eel_g_hash_table_new_free_at_exit
			(g_str_hash, g_str_equal, "nautilus-directory.c: directories");

		add_preferences_callbacks ();
	}

	/* If the object is already in the hash table, look it up. */

	directory = g_hash_table_lookup (directories,
					 canonical_uri);
	if (directory != NULL) {
		nautilus_directory_ref (directory);
	} else if (create) {
		/* Create a new directory object instead. */
		directory = nautilus_directory_new (canonical_uri);
		if (directory == NULL) {
			return NULL;
		}

		g_assert (strcmp (directory->details->uri, canonical_uri) == 0);

		/* Put it in the hash table. */
		g_hash_table_insert (directories,
				     directory->details->uri,
				     directory);
	}

	g_free (canonical_uri);

	return directory;
}

NautilusDirectory *
nautilus_directory_get (const char *uri)
{
	return nautilus_directory_get_internal (uri, TRUE);
}

NautilusDirectory *
nautilus_directory_get_existing (const char *uri)
{
	return nautilus_directory_get_internal (uri, FALSE);
}

/* Returns a reffed NautilusFile object for this directory.
 */
NautilusFile *
nautilus_directory_get_corresponding_file (NautilusDirectory *directory)
{
	NautilusFile *file;

	file = nautilus_directory_get_existing_corresponding_file (directory);
	if (file == NULL) {
		file = nautilus_file_get (directory->details->uri);
	}

	return file;
}

/* Returns a reffed NautilusFile object for this directory, but only if the
 * NautilusFile object has already been created.
 */
NautilusFile *
nautilus_directory_get_existing_corresponding_file (NautilusDirectory *directory)
{
	NautilusFile *file;
	
	file = directory->details->as_file;
	if (file != NULL) {
		nautilus_file_ref (file);
		return file;
	}

	return nautilus_file_get_existing (directory->details->uri);
}

/* nautilus_directory_get_name_for_self_as_new_file:
 * 
 * Get a name to display for the file representing this
 * directory. This is called only when there's no VFS
 * directory for this NautilusDirectory.
 */
char *
nautilus_directory_get_name_for_self_as_new_file (NautilusDirectory *directory)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	
	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_DIRECTORY_CLASS, directory,
		 get_name_for_self_as_new_file, (directory));
}

static char *
real_get_name_for_self_as_new_file (NautilusDirectory *directory)
{
	const char *directory_uri;
	char *name, *colon;
	
	directory_uri = directory->details->uri;

	colon = strchr (directory_uri, ':');
	if (colon == NULL || colon == directory_uri) {
		name = g_strdup (directory_uri);
	} else {
		name = g_strndup (directory_uri, colon - directory_uri);
	}

	return name;
}

char *
nautilus_directory_get_uri (NautilusDirectory *directory)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	return g_strdup (directory->details->uri);
}


static NautilusDirectory *
nautilus_directory_new (const char *uri)
{
	NautilusDirectory *directory;

	g_assert (uri != NULL);

	if (eel_uri_is_trash (uri)) {
		directory = NAUTILUS_DIRECTORY (gtk_object_new (NAUTILUS_TYPE_TRASH_DIRECTORY, NULL));
	} else {
		directory = NAUTILUS_DIRECTORY (gtk_object_new (NAUTILUS_TYPE_VFS_DIRECTORY, NULL));
	}
	gtk_object_ref (GTK_OBJECT (directory));
	gtk_object_sink (GTK_OBJECT (directory));

	set_directory_uri (directory, uri);

	return directory;
}

gboolean
nautilus_directory_is_local (NautilusDirectory *directory)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	
	if (directory->details->vfs_uri == NULL) {
		return TRUE;
	}
	return gnome_vfs_uri_is_local (directory->details->vfs_uri);
}

gboolean
nautilus_directory_are_all_files_seen (NautilusDirectory *directory)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	
	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_DIRECTORY_CLASS, directory,
		 are_all_files_seen, (directory));
}

static void
add_to_hash_table (NautilusDirectory *directory, NautilusFile *file, GList *node)
{
	g_assert (node != NULL);
	g_assert (g_hash_table_lookup (directory->details->file_hash,
				       file->details->relative_uri) == NULL);
	g_hash_table_insert (directory->details->file_hash,
			     file->details->relative_uri, node);
}

static GList *
extract_from_hash_table (NautilusDirectory *directory, NautilusFile *file)
{
	char *relative_uri;
	GList *node;

	relative_uri = file->details->relative_uri;
	if (relative_uri == NULL) {
		return NULL;
	}

	/* Find the list node in the hash table. */
	node = g_hash_table_lookup (directory->details->file_hash, relative_uri);
	g_hash_table_remove (directory->details->file_hash, relative_uri);

	return node;
}

void
nautilus_directory_add_file (NautilusDirectory *directory, NautilusFile *file)
{
	GList *node;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (file->details->relative_uri != NULL);

	/* Add to list. */
	node = g_list_prepend (directory->details->file_list, file);
	directory->details->file_list = node;

	/* Add to hash table. */
	add_to_hash_table (directory, file, node);

	directory->details->confirmed_file_count++;

	/* Ref if we are monitoring. */
	if (nautilus_directory_is_file_list_monitored (directory)) {
		nautilus_file_ref (file);
		nautilus_directory_add_file_to_work_queue (directory, file);
	}
}

void
nautilus_directory_remove_file (NautilusDirectory *directory, NautilusFile *file)
{
	GList *node;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (file->details->relative_uri != NULL);

	/* Find the list node in the hash table. */
	node = extract_from_hash_table (directory, file);
	g_assert (node != NULL);
	g_assert (node->data == file);

	/* Remove the item from the list. */
	directory->details->file_list = g_list_remove_link
		(directory->details->file_list, node);
	g_list_free_1 (node);

	nautilus_directory_remove_file_from_work_queue (directory, file);

	if (!file->details->unconfirmed) {
		directory->details->confirmed_file_count--;
	}

	/* Unref if we are monitoring. */
	if (nautilus_directory_is_file_list_monitored (directory)) {
		nautilus_file_unref (file);
	}
}

gboolean
nautilus_directory_file_list_length_reached (NautilusDirectory *directory)
{
	return directory->details->confirmed_file_count >= NAUTILUS_DIRECTORY_FILE_LIST_HARD_LIMIT;
}

GList *
nautilus_directory_begin_file_name_change (NautilusDirectory *directory,
					   NautilusFile *file)
{
	/* Find the list node in the hash table. */
	return extract_from_hash_table (directory, file);
}

void
nautilus_directory_end_file_name_change (NautilusDirectory *directory,
					 NautilusFile *file,
					 GList *node)
{
	/* Add the list node to the hash table. */
	if (node != NULL) {
		add_to_hash_table (directory, file, node);
	}
}

NautilusFile *
nautilus_directory_find_file_by_name (NautilusDirectory *directory,
				      const char *name)
{
	char *relative_uri;
	NautilusFile *file;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	relative_uri = gnome_vfs_escape_string (name);
	file = nautilus_directory_find_file_by_relative_uri
		(directory, relative_uri);
	g_free (relative_uri);
	return file;
}

NautilusFile *
nautilus_directory_find_file_by_relative_uri (NautilusDirectory *directory,
					      const char *relative_uri)
{
	GList *node;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (relative_uri != NULL, NULL);

	node = g_hash_table_lookup (directory->details->file_hash,
				    relative_uri);
	return node == NULL ? NULL : NAUTILUS_FILE (node->data);
}

NautilusFile *
nautilus_directory_find_file_by_internal_uri (NautilusDirectory *directory,
					      const char *relative_uri)
{
	NautilusFile *result;

	if (eel_strcmp (relative_uri, ".") == 0) {
		result = nautilus_directory_get_existing_corresponding_file (directory);
		if (result != NULL) {
			nautilus_file_unref (result);
			g_return_val_if_fail (!GTK_OBJECT_DESTROYED (result), NULL);
		}
	} else {
		result = nautilus_directory_find_file_by_relative_uri (directory, relative_uri);
	}

	return result;
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
	if (changed_files != NULL) {
		gtk_signal_emit (GTK_OBJECT (directory),
				 signals[FILES_CHANGED],
				 changed_files);
	}
}

void
nautilus_directory_emit_change_signals (NautilusDirectory *directory,
					     GList *changed_files)
{
	GList *p;

	for (p = changed_files; p != NULL; p = p->next) {
		nautilus_file_emit_changed (p->data);
	}
	nautilus_directory_emit_files_changed (directory, changed_files);
}

void
nautilus_directory_emit_done_loading (NautilusDirectory *directory)
{
	gtk_signal_emit (GTK_OBJECT (directory),
			 signals[DONE_LOADING]);
}

void
nautilus_directory_emit_load_error (NautilusDirectory *directory,
				    GnomeVFSResult error_result)
{
	gtk_signal_emit (GTK_OBJECT (directory),
			 signals[LOAD_ERROR],
			 error_result);
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
	directory = nautilus_directory_get_existing (directory_uri);
	g_free (directory_uri);
	return directory;
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
call_files_changed_common (NautilusDirectory *directory, GList *file_list)
{
	GList *node;

	for (node = file_list; node != NULL; node = node->next) {
		nautilus_directory_add_file_to_work_queue (directory, node->data);
	}
	nautilus_directory_async_state_changed (directory);
	nautilus_directory_emit_change_signals (directory, file_list);
}

static void
call_files_changed_free_list (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (value != NULL);
	g_assert (user_data == NULL);

	call_files_changed_common (NAUTILUS_DIRECTORY (key), value);
	g_list_free (value);
}

static void
call_files_changed_unref_free_list (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (value != NULL);
	g_assert (user_data == NULL);

	call_files_changed_common (NAUTILUS_DIRECTORY (key), value);
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

static void
invalidate_count_and_unref (gpointer key, gpointer value, gpointer user_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (key));
	g_assert (value == key);
	g_assert (user_data == NULL);

	nautilus_directory_invalidate_count_and_mime_list (key);
	nautilus_directory_unref (key);
}

static void
collect_parent_directories (GHashTable *hash_table, NautilusDirectory *directory)
{
	g_assert (hash_table != NULL);
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	if (g_hash_table_lookup (hash_table, directory) == NULL) {
		nautilus_directory_ref (directory);
		g_hash_table_insert  (hash_table, directory, directory);
	}
}

void
nautilus_directory_notify_files_added (GList *uris)
{
	GHashTable *added_lists;
	GList *p;
	NautilusDirectory *directory;
	GHashTable *parent_directories;
	const char *uri;
	const char *directory_uri;
	GnomeVFSURI *vfs_uri;
	NautilusFile *file;

	/* Make a list of added files in each directory. */
	added_lists = g_hash_table_new (NULL, NULL);

	/* Make a list of parent directories that will need their counts updated. */
	parent_directories = g_hash_table_new (NULL, NULL);

	for (p = uris; p != NULL; p = p->next) {
		uri = (const char *) p->data;

		/* See if the directory is already known. */
		directory = get_parent_directory_if_exists (uri);
		if (directory == NULL) {
			/* In case the directory is not being
			 * monitored, but the corresponding file is,
			 * we must invalidate it's item count.
			 */
			
			directory_uri = uri_get_directory_part (uri);
			file = nautilus_file_get_existing (directory_uri);

			if (file != NULL) {
				nautilus_file_invalidate_count_and_mime_list (file);
				nautilus_file_unref (file);
			}

			continue;
		}

		collect_parent_directories (parent_directories, directory);

		/* If no one is monitoring files in the directory, nothing to do. */
		if (!nautilus_directory_is_file_list_monitored (directory)) {
			nautilus_directory_unref (directory);
			continue;
		}

		/* Collect the URIs to use. */
		vfs_uri = gnome_vfs_uri_new (uri);
		if (vfs_uri == NULL) {
			nautilus_directory_unref (directory);
			g_warning ("bad uri %s", uri);
			continue;
		}
		hash_table_list_prepend (added_lists, directory, vfs_uri);
		nautilus_directory_unref (directory);
	}


	/* Now get file info for the new files. This creates NautilusFile
	 * objects for the new files, and sends out a files_added signal. 
	 */
	g_hash_table_foreach (added_lists, call_get_file_info_free_list, NULL);
	g_hash_table_destroy (added_lists);

	/* Invalidate count for each parent directory. */
	g_hash_table_foreach (parent_directories, invalidate_count_and_unref, NULL);
	g_hash_table_destroy (parent_directories);
}

void
nautilus_directory_notify_files_changed (GList *uris)
{
	GHashTable *changed_lists;
	GList *node;
	const char *uri;
	NautilusFile *file;

	/* Make a list of changed files in each directory. */
	changed_lists = g_hash_table_new (NULL, NULL);

	/* Go through all the notifications. */
	for (node = uris; node != NULL; node = node->next) {
		uri = (const char *) node->data;

		/* Find the file. */
		file = nautilus_file_get_existing (uri);
		if (file != NULL) {
			/* Tell it to re-get info now, and later emit
			 * a changed signal.
			 */
			file->details->file_info_is_up_to_date = FALSE;
			file->details->top_left_text_is_up_to_date = FALSE;
			file->details->link_info_is_up_to_date = FALSE;

			hash_table_list_prepend (changed_lists,
						 file->details->directory,
						 file);
		}
	}

	/* Now send out the changed signals. */
	g_hash_table_foreach (changed_lists, call_files_changed_unref_free_list, NULL);
	g_hash_table_destroy (changed_lists);
}

void
nautilus_directory_notify_files_removed (GList *uris)
{
	GHashTable *changed_lists;
	GList *p;
	NautilusDirectory *directory;
	GHashTable *parent_directories;
	const char *uri;
	NautilusFile *file;

	/* Make a list of changed files in each directory. */
	changed_lists = g_hash_table_new (NULL, NULL);

	/* Make a list of parent directories that will need their counts updated. */
	parent_directories = g_hash_table_new (NULL, NULL);

	/* Go through all the notifications. */
	for (p = uris; p != NULL; p = p->next) {
		uri = (const char *) p->data;

		/* Update file count for parent directory if anyone might care. */
		directory = get_parent_directory_if_exists (uri);
		if (directory != NULL) {
			collect_parent_directories (parent_directories, directory);
			nautilus_directory_unref (directory);
		}

		/* Find the file. */
		file = nautilus_file_get_existing (uri);
		if (file != NULL) {
			/* Mark it gone and prepare to send the changed signal. */
			nautilus_file_mark_gone (file);
			hash_table_list_prepend (changed_lists,
						 file->details->directory,
						 file);
		}
	}

	/* Now send out the changed signals. */
	g_hash_table_foreach (changed_lists, call_files_changed_unref_free_list, NULL);
	g_hash_table_destroy (changed_lists);

	/* Invalidate count for each parent directory. */
	g_hash_table_foreach (parent_directories, invalidate_count_and_unref, NULL);
	g_hash_table_destroy (parent_directories);
}

static void
set_directory_uri (NautilusDirectory *directory,
		   const char *new_uri)
{
	GnomeVFSURI *new_vfs_uri;

	new_vfs_uri = gnome_vfs_uri_new (new_uri);

	g_free (directory->details->uri);
	directory->details->uri = g_strdup (new_uri);
	
	if (directory->details->vfs_uri != NULL) {
		gnome_vfs_uri_unref (directory->details->vfs_uri);
	}
	directory->details->vfs_uri = new_vfs_uri;
}

static void
change_directory_uri (NautilusDirectory *directory,
		      const char *new_uri)
{
	/* I believe it's impossible for a self-owned file/directory
	 * to be moved. But if that did somehow happen, this function
	 * wouldn't do enough to handle it.
	 */
	g_return_if_fail (directory->details->as_file == NULL);

	g_hash_table_remove (directories,
			     directory->details->uri);

	set_directory_uri (directory, new_uri);

	g_hash_table_insert (directories,
			     directory->details->uri,
			     directory);

	nautilus_directory_rename_directory_metadata (directory, new_uri);
}

typedef struct {
	char *uri_prefix;
	GList *directories;
} CollectData;

static void
collect_directories_by_prefix (gpointer key, gpointer value, gpointer callback_data)
{
	const char *uri, *uri_suffix;
	NautilusDirectory *directory;
	CollectData *collect_data;

	uri = (const char *) key;
	directory = NAUTILUS_DIRECTORY (value);
	collect_data = (CollectData *) callback_data;
	
	if (eel_str_has_prefix (uri, collect_data->uri_prefix)) {
		uri_suffix = &uri[strlen (collect_data->uri_prefix)];
		switch (uri_suffix[0]) {
		case '\0':
		case '/':
			nautilus_directory_ref (directory);
			collect_data->directories =
				g_list_prepend (collect_data->directories,
						directory);
			break;
		}
	}
}

static char *
str_replace_prefix (const char *str,
		    const char *old_prefix,
		    const char *new_prefix)
{
	const char *old_suffix;

	g_return_val_if_fail (eel_str_has_prefix (str, old_prefix),
			      g_strdup (str));

	old_suffix = &str [strlen (old_prefix)];
	return g_strconcat (new_prefix, old_suffix, NULL);
}

static GList *
nautilus_directory_moved_internal (const char *old_uri,
				   const char *new_uri)
{
	char *canonical_old_uri, *canonical_new_uri;
	CollectData collection;
	NautilusDirectory *directory;
	char *new_directory_uri;
	GList *node, *affected_files;

	canonical_old_uri = nautilus_directory_make_uri_canonical (old_uri);
	canonical_new_uri = nautilus_directory_make_uri_canonical (new_uri);

	collection.uri_prefix = canonical_old_uri;
	collection.directories = NULL;

	g_hash_table_foreach (directories,
			      collect_directories_by_prefix,
			      &collection);

	affected_files = NULL;

	for (node = collection.directories; node != NULL; node = node->next) {
		directory = NAUTILUS_DIRECTORY (node->data);

		/* Change the URI in the directory object. */
		new_directory_uri = str_replace_prefix (directory->details->uri,
							canonical_old_uri,
							canonical_new_uri);
		change_directory_uri (directory,
				      new_directory_uri);
		g_free (new_directory_uri);

		/* Collect affected files. */
		if (directory->details->as_file != NULL) {
			affected_files = g_list_prepend
				(affected_files,
				 nautilus_file_ref (directory->details->as_file));
		}
		affected_files = g_list_concat
			(affected_files,
			 nautilus_file_list_copy (directory->details->file_list));
		
		nautilus_directory_unref (directory);
	}

	g_list_free (collection.directories);

	g_free (canonical_old_uri);
	g_free (canonical_new_uri);

	return affected_files;
}

void
nautilus_directory_moved (const char *old_uri,
			  const char *new_uri)
{
	GList *list, *node;
	GHashTable *hash;
	NautilusFile *file;

	hash = g_hash_table_new (NULL, NULL);

	list = nautilus_directory_moved_internal (old_uri, new_uri);
	for (node = list; node != NULL; node = node->next) {
		file = NAUTILUS_FILE (node->data);
		hash_table_list_prepend (hash,
					 file->details->directory,
					 nautilus_file_ref (file));
	}
	nautilus_file_list_free (list);

	g_hash_table_foreach (hash, call_files_changed_unref_free_list, NULL);
	g_hash_table_destroy (hash);
}

void
nautilus_directory_notify_files_moved (GList *uri_pairs)
{
	GList *p, *affected_files, *node;
	URIPair *pair;
	NautilusFile *file;
	NautilusDirectory *old_directory, *new_directory;
	GHashTable *parent_directories;
	GList *new_files_list, *unref_list;
	GHashTable *added_lists, *changed_lists;
	char *name;

	/* Make a list of added and changed files in each directory. */
	new_files_list = NULL;
	added_lists = g_hash_table_new (NULL, NULL);
	changed_lists = g_hash_table_new (NULL, NULL);
	unref_list = NULL;

	/* Make a list of parent directories that will need their counts updated. */
	parent_directories = g_hash_table_new (NULL, NULL);

	for (p = uri_pairs; p != NULL; p = p->next) {
		pair = p->data;

		/* Handle overwriting a file. */
		file = nautilus_file_get_existing (pair->to_uri);
		if (file != NULL) {
			/* Mark it gone and prepare to send the changed signal. */
			nautilus_file_mark_gone (file);
			new_directory = file->details->directory;
			hash_table_list_prepend (changed_lists,
						 new_directory,
						 file);
			collect_parent_directories (parent_directories,
						    new_directory);
		}

		/* Update any directory objects that are affected. */
		affected_files = nautilus_directory_moved_internal (pair->from_uri,
								    pair->to_uri);
		for (node = affected_files; node != NULL; node = node->next) {
			file = NAUTILUS_FILE (node->data);
			hash_table_list_prepend (changed_lists,
						 file->details->directory,
						 file);
		}
		unref_list = g_list_concat (unref_list, affected_files);

		/* Move an existing file. */
		file = nautilus_file_get_existing (pair->from_uri);
		if (file == NULL) {
			/* Handle this as if it was a new file. */
			new_files_list = g_list_prepend (new_files_list,
							 pair->to_uri);
		} else {
			/* Handle notification in the old directory. */
			old_directory = file->details->directory;
			collect_parent_directories (parent_directories, old_directory);

			/* Locate the new directory. */
			new_directory = get_parent_directory (pair->to_uri);
			collect_parent_directories (parent_directories, new_directory);
			/* We can unref now -- new_directory is in the
			 * parent directories list so it will be
			 * around until the end of this function
			 * anyway.
			 */
			nautilus_directory_unref (new_directory);

			/* Update the file's name. */
			name = eel_uri_get_basename (pair->to_uri);
			nautilus_file_update_name (file, name);
			g_free (name);

			/* Update the file's directory. */
			nautilus_file_set_directory (file, new_directory);
			
			hash_table_list_prepend (changed_lists,
						 old_directory,
						 file);
			if (old_directory != new_directory) {
				hash_table_list_prepend	(added_lists,
							 new_directory,
							 file);
			}

			/* Unref each file once to balance out nautilus_file_get. */
			unref_list = g_list_prepend (unref_list, file);
		}
	}

	/* Now send out the changed and added signals for existing file objects. */
	g_hash_table_foreach (changed_lists, call_files_changed_free_list, NULL);
	g_hash_table_destroy (changed_lists);
	g_hash_table_foreach (added_lists, call_files_added_free_list, NULL);
	g_hash_table_destroy (added_lists);

	/* Let the file objects go. */
	nautilus_file_list_free (unref_list);

	/* Invalidate count for each parent directory. */
	g_hash_table_foreach (parent_directories, invalidate_count_and_unref, NULL);
	g_hash_table_destroy (parent_directories);

	/* Separate handling for brand new file objects. */
	nautilus_directory_notify_files_added (new_files_list);
	g_list_free (new_files_list);
}

void 
nautilus_directory_schedule_metadata_copy (GList *uri_pairs)
{
	GList *p;
	URIPair *pair;
	NautilusDirectory *source_directory, *destination_directory;
	const char *source_relative_uri, *destination_relative_uri;

	for (p = uri_pairs; p != NULL; p = p->next) {
		pair = (URIPair *) p->data;

		source_directory = get_parent_directory (pair->from_uri);
		destination_directory = get_parent_directory (pair->to_uri);
		
		source_relative_uri = g_basename (pair->from_uri);
		destination_relative_uri = g_basename (pair->to_uri);
		
		nautilus_directory_copy_file_metadata (source_directory,
						       source_relative_uri,
						       destination_directory,
						       destination_relative_uri);
		
		nautilus_directory_unref (source_directory);
		nautilus_directory_unref (destination_directory);
	}
}

void 
nautilus_directory_schedule_metadata_move (GList *uri_pairs)
{
	GList *p;
	URIPair *pair;
	NautilusDirectory *source_directory, *destination_directory;
	const char *source_relative_uri, *destination_relative_uri;

	for (p = uri_pairs; p != NULL; p = p->next) {
		pair = (URIPair *) p->data;

		source_directory = get_parent_directory (pair->from_uri);
		destination_directory = get_parent_directory (pair->to_uri);
		
		source_relative_uri = g_basename (pair->from_uri);
		destination_relative_uri = g_basename (pair->to_uri);
		
		nautilus_directory_copy_file_metadata (source_directory,
						       source_relative_uri,
						       destination_directory,
						       destination_relative_uri);
		nautilus_directory_remove_file_metadata (source_directory,
							 source_relative_uri);
		
		nautilus_directory_unref (source_directory);
		nautilus_directory_unref (destination_directory);
	}
}

void 
nautilus_directory_schedule_metadata_remove (GList *uris)
{
	GList *p;
	const char *uri;
	NautilusDirectory *directory;
	const char *relative_uri;

	for (p = uris; p != NULL; p = p->next) {
		uri = (const char *) p->data;

		directory = get_parent_directory (uri);
		relative_uri = g_basename (uri);
		
		nautilus_directory_remove_file_metadata (directory,
							 relative_uri);
		
		nautilus_directory_unref (directory);
	}
}

void
nautilus_directory_schedule_position_set (GList *position_setting_list)
{
	GList *p;
	const NautilusFileChangesQueuePosition *item;
	NautilusFile *file;
	char *position_string;

	for (p = position_setting_list; p != NULL; p = p->next) {
		item = (NautilusFileChangesQueuePosition *) p->data;

		file = nautilus_file_get (item->uri);
		
		if (item->set) {
			position_string = g_strdup_printf ("%d,%d",
							   item->point.x, item->point.y);
		} else {
			position_string = NULL;
		}
		nautilus_file_set_metadata
			(file,
			 NAUTILUS_METADATA_KEY_ICON_POSITION,
			 NULL,
			 position_string);
		g_free (position_string);
		
		nautilus_file_unref (file);
	}
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

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_DIRECTORY_CLASS, directory,
		 contains_file, (directory, file));
}

char *
nautilus_directory_get_file_uri (NautilusDirectory *directory,
				 const char *file_name)
{
	GnomeVFSURI *directory_uri, *file_uri;
	char *result;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (file_name != NULL, NULL);

	result = NULL;

	directory_uri = gnome_vfs_uri_new (directory->details->uri);

	g_assert (directory_uri != NULL);

	file_uri = gnome_vfs_uri_append_string (directory_uri, file_name);
	gnome_vfs_uri_unref (directory_uri);

	if (file_uri != NULL) {
		result = gnome_vfs_uri_to_string (file_uri, GNOME_VFS_URI_HIDE_NONE);
		gnome_vfs_uri_unref (file_uri);
	}

	return result;
}

void
nautilus_directory_call_when_ready (NautilusDirectory *directory,
				    GList *file_attributes,
				    gboolean wait_for_all_files,
				    NautilusDirectoryCallback callback,
				    gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (callback != NULL);

	EEL_CALL_METHOD
		(NAUTILUS_DIRECTORY_CLASS, directory,
		 call_when_ready, (directory, file_attributes, wait_for_all_files,
				   callback, callback_data));
}

void
nautilus_directory_cancel_callback (NautilusDirectory *directory,
				    NautilusDirectoryCallback callback,
				    gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (callback != NULL);

	EEL_CALL_METHOD
		(NAUTILUS_DIRECTORY_CLASS, directory,
		 cancel_callback, (directory, callback, callback_data));
}

void
nautilus_directory_file_monitor_add (NautilusDirectory *directory,
				     gconstpointer client,
				     gboolean monitor_hidden_files,
				     gboolean monitor_backup_files,
				     GList *file_attributes,
				     NautilusDirectoryCallback callback,
				     gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (client != NULL);

	EEL_CALL_METHOD
		(NAUTILUS_DIRECTORY_CLASS, directory,
		 file_monitor_add, (directory, client,
				    monitor_hidden_files,
				    monitor_backup_files,
				    file_attributes,
				    callback, callback_data));
}

void
nautilus_directory_file_monitor_remove (NautilusDirectory *directory,
					gconstpointer client)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (client != NULL);

	EEL_CALL_METHOD
		(NAUTILUS_DIRECTORY_CLASS, directory,
		 file_monitor_remove, (directory, client));
}

void
nautilus_directory_force_reload (NautilusDirectory *directory)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	EEL_CALL_METHOD
		(NAUTILUS_DIRECTORY_CLASS, directory,
		 force_reload, (directory));
}

gboolean
nautilus_directory_is_not_empty (NautilusDirectory *directory)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_DIRECTORY_CLASS, directory,
		 is_not_empty, (directory));
}

static gboolean
is_tentative (gpointer data, gpointer callback_data)
{
	NautilusFile *file;

	g_assert (callback_data == NULL);

	file = NAUTILUS_FILE (data);
	return file->details->info == NULL;
}

GList *
nautilus_directory_get_file_list (NautilusDirectory *directory)
{
	GList *tentative_files, *non_tentative_files;

	tentative_files = eel_g_list_partition
		(g_list_copy (directory->details->file_list),
		 is_tentative, NULL, &non_tentative_files);
	g_list_free (tentative_files);

	nautilus_file_list_ref (non_tentative_files);
	return non_tentative_files;
}

/**
 * nautilus_directory_list_ref
 *
 * Ref all the directories in a list.
 * @list: GList of directories.
 **/
GList *
nautilus_directory_list_ref (GList *list)
{
	g_list_foreach (list, (GFunc) nautilus_directory_ref, NULL);
	return list;
}

/**
 * nautilus_directory_list_unref
 *
 * Unref all the directories in a list.
 * @list: GList of directories.
 **/
void
nautilus_directory_list_unref (GList *list)
{
	eel_g_list_safe_for_each (list, (GFunc) nautilus_directory_unref, NULL);
}

/**
 * nautilus_directory_list_free
 *
 * Free a list of directories after unrefing them.
 * @list: GList of directories.
 **/
void
nautilus_directory_list_free (GList *list)
{
	nautilus_directory_list_unref (list);
	g_list_free (list);
}

/**
 * nautilus_directory_list_copy
 *
 * Copy the list of directories, making a new ref of each,
 * @list: GList of directories.
 **/
GList *
nautilus_directory_list_copy (GList *list)
{
	return g_list_copy (nautilus_directory_list_ref (list));
}

static int
compare_by_uri (NautilusDirectory *a, NautilusDirectory *b)
{
	return strcmp (a->details->uri, b->details->uri);
}

static int
compare_by_uri_cover (gconstpointer a, gconstpointer b)
{
	return compare_by_uri (NAUTILUS_DIRECTORY (a), NAUTILUS_DIRECTORY (b));
}

/**
 * nautilus_directory_list_sort_by_uri
 * 
 * Sort the list of directories by directory uri.
 * @list: GList of directories.
 **/
GList *
nautilus_directory_list_sort_by_uri (GList *list)
{
	return g_list_sort (list, compare_by_uri_cover);
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

#include <eel/eel-debug.h>
#include "nautilus-file-attributes.h"

static int data_dummy;
static gboolean got_metadata_flag;
static gboolean got_files_flag;

static void
got_metadata_callback (NautilusDirectory *directory, GList *files, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));
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
        return directories ? g_hash_table_size (directories) : 0;
}

void
nautilus_self_check_directory (void)
{
	NautilusDirectory *directory;
	NautilusFile *file;
	GList *attributes;

	directory = nautilus_directory_get ("file:///etc");
	file = nautilus_file_get ("file:///etc/passwd");

	EEL_CHECK_INTEGER_RESULT (g_hash_table_size (directories), 1);

	nautilus_directory_file_monitor_add
		(directory, &data_dummy,
		 TRUE, TRUE, NULL, NULL, NULL);

	got_metadata_flag = FALSE;

	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_METADATA);
	nautilus_directory_call_when_ready (directory, attributes, TRUE,
					    got_metadata_callback, &data_dummy);
	g_list_free (attributes);

	while (!got_metadata_flag) {
		gtk_main_iteration ();
	}

	nautilus_file_set_metadata (file, "test", "default", "value");
	EEL_CHECK_STRING_RESULT (nautilus_file_get_metadata (file, "test", "default"), "value");

	nautilus_file_set_boolean_metadata (file, "test_boolean", TRUE, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_get_boolean_metadata (file, "test_boolean", TRUE), TRUE);
	nautilus_file_set_boolean_metadata (file, "test_boolean", TRUE, FALSE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_get_boolean_metadata (file, "test_boolean", TRUE), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_get_boolean_metadata (NULL, "test_boolean", TRUE), TRUE);

	nautilus_file_set_integer_metadata (file, "test_integer", 0, 17);
	EEL_CHECK_INTEGER_RESULT (nautilus_file_get_integer_metadata (file, "test_integer", 0), 17);
	nautilus_file_set_integer_metadata (file, "test_integer", 0, -1);
	EEL_CHECK_INTEGER_RESULT (nautilus_file_get_integer_metadata (file, "test_integer", 0), -1);
	nautilus_file_set_integer_metadata (file, "test_integer", 42, 42);
	EEL_CHECK_INTEGER_RESULT (nautilus_file_get_integer_metadata (file, "test_integer", 42), 42);
	EEL_CHECK_INTEGER_RESULT (nautilus_file_get_integer_metadata (NULL, "test_integer", 42), 42);
	EEL_CHECK_INTEGER_RESULT (nautilus_file_get_integer_metadata (file, "nonexistent_key", 42), 42);

	EEL_CHECK_BOOLEAN_RESULT (nautilus_directory_get ("file:///etc") == directory, TRUE);
	nautilus_directory_unref (directory);

	EEL_CHECK_BOOLEAN_RESULT (nautilus_directory_get ("file:///etc/") == directory, TRUE);
	nautilus_directory_unref (directory);

	EEL_CHECK_BOOLEAN_RESULT (nautilus_directory_get ("file:///etc////") == directory, TRUE);
	nautilus_directory_unref (directory);

	nautilus_file_unref (file);

	nautilus_directory_file_monitor_remove (directory, &data_dummy);

	nautilus_directory_unref (directory);

	while (g_hash_table_size (directories) != 0) {
		gtk_main_iteration ();
	}

	EEL_CHECK_INTEGER_RESULT (g_hash_table_size (directories), 0);

	directory = nautilus_directory_get ("file:///etc");

	got_metadata_flag = FALSE;
	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_METADATA);
	nautilus_directory_call_when_ready (directory, attributes, TRUE,
					    got_metadata_callback, &data_dummy);
	g_list_free (attributes);

	while (!got_metadata_flag) {
		gtk_main_iteration ();
	}

	EEL_CHECK_BOOLEAN_RESULT (nautilus_directory_is_metadata_read (directory), TRUE);

	got_files_flag = FALSE;

	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_MIME_TYPE);
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS);
	nautilus_directory_call_when_ready (directory, attributes, TRUE,
					    got_files_callback, &data_dummy);
	g_list_free (attributes);

	while (!got_files_flag) {
		gtk_main_iteration ();
	}

	EEL_CHECK_BOOLEAN_RESULT (directory->details->file_list == NULL, TRUE);

	EEL_CHECK_INTEGER_RESULT (g_hash_table_size (directories), 1);

	file = nautilus_file_get ("file:///etc/passwd");

	EEL_CHECK_STRING_RESULT (nautilus_file_get_metadata (file, "test", "default"), "value");
	
	nautilus_file_unref (file);

	nautilus_directory_unref (directory);

	EEL_CHECK_INTEGER_RESULT (g_hash_table_size (directories), 0);

	/* escape_slashes: code is now in gnome-vfs, but lets keep the tests here for now */
	EEL_CHECK_STRING_RESULT (gnome_vfs_escape_slashes (""), "");
	EEL_CHECK_STRING_RESULT (gnome_vfs_escape_slashes ("a"), "a");
	EEL_CHECK_STRING_RESULT (gnome_vfs_escape_slashes ("/"), "%2F");
	EEL_CHECK_STRING_RESULT (gnome_vfs_escape_slashes ("%"), "%25");
	EEL_CHECK_STRING_RESULT (gnome_vfs_escape_slashes ("a/a"), "a%2Fa");
	EEL_CHECK_STRING_RESULT (gnome_vfs_escape_slashes ("a%a"), "a%25a");
	EEL_CHECK_STRING_RESULT (gnome_vfs_escape_slashes ("%25"), "%2525");
	EEL_CHECK_STRING_RESULT (gnome_vfs_escape_slashes ("%2F"), "%252F");

	/* nautilus_directory_make_uri_canonical */
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical (""), "file:///");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("file:/"), "file:///");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("file:///"), "file:///");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("TRASH:XXX"), EEL_TRASH_URI);
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("trash:xxx"), EEL_TRASH_URI);
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("GNOME-TRASH:XXX"), EEL_TRASH_URI);
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("gnome-trash:xxx"), EEL_TRASH_URI);
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("file:///home/mathieu/"), "file:///home/mathieu");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("file:///home/mathieu"), "file:///home/mathieu");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("ftp://mathieu:password@le-hackeur.org"), "ftp://mathieu:password@le-hackeur.org");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("ftp://mathieu:password@le-hackeur.org/"), "ftp://mathieu:password@le-hackeur.org");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("http://le-hackeur.org"), "http://le-hackeur.org");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("http://le-hackeur.org/"), "http://le-hackeur.org");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("http://le-hackeur.org/dir"), "http://le-hackeur.org/dir");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("http://le-hackeur.org/dir/"), "http://le-hackeur.org/dir");
	/* FIXME bugzilla.gnome.org 45068: the "nested" URI loses some characters here. Maybe that's OK because we escape them in practice? */
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("search://[file://]file_name contains stuff"), "search://[file/]file_name contains stuff");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("eazel-services:/~turtle"), "eazel-services:///~turtle");
	EEL_CHECK_STRING_RESULT (nautilus_directory_make_uri_canonical ("eazel-services:///~turtle"), "eazel-services:///~turtle");
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
