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

#include <parser.h>
#include <xmlmemory.h>

#include "nautilus-file-attributes.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-file-private.h"

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

#define DIRECTORY_LOAD_ITEMS_PER_CALLBACK 1

enum 
{
	FILES_ADDED,
	FILES_CHANGED,
	METADATA_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct MetafileReadState {
	GnomeVFSAsyncHandle *handle;
	gpointer buffer;
	size_t bytes_read;
};

struct MetafileWriteState {
	GnomeVFSAsyncHandle *handle;
	xmlChar *buffer;
	int size;
	gboolean write_again;
};

#define READ_CHUNK_SIZE (4 * 1024)

static int                           compare_file_with_name                              (gconstpointer                  a,
											  gconstpointer                  b);
static int                           compare_queued_callbacks                            (gconstpointer                  a,
											  gconstpointer                  b);
static GnomeVFSURI *                 construct_alternate_metafile_uri                    (GnomeVFSURI                   *uri);
static xmlNode *                     create_metafile_root                                (NautilusDirectory             *directory);
static gboolean                      dequeue_pending_idle_callback                       (gpointer                       callback_data);
static char *                        get_metadata_from_node                              (xmlNode                       *node,
											  const char                    *key,
											  const char                    *default_metadata);
static gboolean			     is_file_list_monitored 				 (NautilusDirectory 		*directory);
static void                          metafile_close_callback                             (GnomeVFSAsyncHandle           *handle,
											  GnomeVFSResult                 result,
											  gpointer                       callback_data);
static void                          metafile_read_cancel                                (NautilusDirectory             *directory);
static void                          metafile_read_callback                              (GnomeVFSAsyncHandle           *handle,
											  GnomeVFSResult                 result,
											  gpointer                       buffer,
											  GnomeVFSFileSize               bytes_requested,
											  GnomeVFSFileSize               bytes_read,
											  gpointer                       callback_data);
static void                          metafile_read_complete                              (NautilusDirectory             *directory);
static void                          metafile_read_done                                  (NautilusDirectory             *directory);
static void                          metafile_read_failed                                (NautilusDirectory             *directory,
											  GnomeVFSResult                 result);
static void                          metafile_read_open_callback                         (GnomeVFSAsyncHandle           *handle,
											  GnomeVFSResult                 result,
											  gpointer                       callback_data);
static void                          metafile_read_some                                  (NautilusDirectory             *directory);
static void                          metafile_read_start                                 (NautilusDirectory             *directory);
static void                          metafile_write                                      (NautilusDirectory             *directory);
static void                          metafile_write_callback                             (GnomeVFSAsyncHandle           *handle,
											  GnomeVFSResult                 result,
											  gconstpointer                  buffer,
											  GnomeVFSFileSize               bytes_requested,
											  GnomeVFSFileSize               bytes_read,
											  gpointer                       callback_data);
static void                          metafile_write_complete                             (NautilusDirectory             *directory);
static gboolean                      metafile_write_idle_callback                        (gpointer                       callback_data);
static void                          metafile_write_failed                               (NautilusDirectory             *directory,
											  GnomeVFSResult                 result);
static void                          metafile_write_start                                (NautilusDirectory             *directory);
static void                          nautilus_directory_destroy                          (GtkObject                     *object);
static void                          nautilus_directory_initialize                       (gpointer                       object,
											  gpointer                       klass);
static void                          nautilus_directory_initialize_class                 (gpointer                       klass);
static void                          nautilus_directory_load_callback                    (GnomeVFSAsyncHandle           *handle,
											  GnomeVFSResult                 result,
											  GnomeVFSDirectoryList         *list,
											  guint                          entries_read,
											  gpointer                       callback_data);
static void                          nautilus_directory_load_done                        (NautilusDirectory             *directory,
											  GnomeVFSResult                 result);
static void                          nautilus_directory_load_one                         (NautilusDirectory             *directory,
											  GnomeVFSFileInfo              *info);
static GnomeVFSResult                nautilus_make_directory_and_parents                 (GnomeVFSURI                   *uri,
											  guint                          permissions);
static NautilusDirectory *           nautilus_directory_new                              (const char                    *uri);
static void                          nautilus_directory_request_read_metafile            (NautilusDirectory             *directory);
static GnomeVFSDirectoryListPosition nautilus_gnome_vfs_directory_list_get_next_position (GnomeVFSDirectoryList         *list,
											  GnomeVFSDirectoryListPosition  position);
static void                          nautilus_gnome_vfs_file_info_list_free              (GList                         *list);
static void                          nautilus_gnome_vfs_file_info_list_unref             (GList                         *list);
static void                          process_pending_file_attribute_requests             (NautilusDirectory             *directory);
static void                          schedule_dequeue_pending                            (NautilusDirectory             *directory);
static void                          stop_monitoring_file_list                           (NautilusDirectory             *directory);

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

static void
nautilus_gnome_vfs_file_info_list_unref (GList *list)
{
	g_list_foreach (list, (GFunc) gnome_vfs_file_info_unref, NULL);
}

static void
nautilus_gnome_vfs_file_info_list_free (GList *list)
{
	nautilus_gnome_vfs_file_info_list_unref (list);
	g_list_free (list);
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
	metafile_read_cancel (directory);

	if (is_file_list_monitored (directory)) {
		stop_monitoring_file_list (directory);
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

	/* FIXME: This currently ignores the issue of two uris that are not identical but point
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

static void
metafile_read_done (NautilusDirectory *directory)
{
	GList *p;
	QueuedCallback *callback;

	/* Tell the callers that were waiting for metadata that it's here.
	 */
	for (p = directory->details->metafile_callbacks; p != NULL; p = p->next) {
		callback = p->data;

		if (callback->file != NULL) {
			g_assert (callback->file->details->directory == directory);
			(* callback->callback.file) (callback->file,
						     callback->callback_data);
			nautilus_file_unref (callback->file);
		} else {
			(* callback->callback.directory) (directory,
							  callback->callback_data);
		}
	}
	nautilus_g_list_free_deep (directory->details->metafile_callbacks);
	directory->details->metafile_callbacks = NULL;
}

static void
metafile_read_cancel (NautilusDirectory *directory)
{
	if (directory->details->read_state == NULL) {
		return;
	}

	gnome_vfs_async_cancel (directory->details->read_state->handle);
	g_free (directory->details->read_state);
	directory->details->read_state = NULL;
}

static void
metafile_read_failed (NautilusDirectory *directory,
		      GnomeVFSResult result)
{
	g_free (directory->details->read_state->buffer);

	if (directory->details->use_alternate_metafile) {
		directory->details->read_state->buffer = NULL;
		directory->details->read_state->bytes_read = 0;

		directory->details->use_alternate_metafile = FALSE;
		metafile_read_start (directory);
		return;
	}

	g_free (directory->details->read_state);

	directory->details->metafile_read = TRUE;
	directory->details->read_state = NULL;

	/* Let the callers that were waiting for the metafile know. */
	metafile_read_done (directory);
}

static void
metafile_read_complete (NautilusDirectory *directory)
{
	char *buffer;
	int size;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (directory->details->metafile == NULL);
	
	/* The gnome-xml parser requires a zero-terminated array.
	 * Also, we don't want to allocate an empty buffer
	 * because it would be NULL and gnome-xml won't parse
	 * NULL properly.
	 */
	size = directory->details->read_state->bytes_read;
	buffer = g_realloc (directory->details->read_state->buffer, size + 1);
	buffer[size] = '\0';
	directory->details->metafile = xmlParseMemory (buffer, size);
	g_free (buffer);

	g_free (directory->details->read_state);

	directory->details->metafile_read = TRUE;
	directory->details->read_state = NULL;

	/* Tell that the directory metadata has changed. */
	gtk_signal_emit (GTK_OBJECT (directory),
			 signals[METADATA_CHANGED]);

	/* Say that all the files have changed.
	 * We could optimize this to only mention files that
	 * have metadata, but this is a fine rough cut for now.
	 */
	nautilus_directory_files_changed (directory,
					  directory->details->files);

	/* Let the callers that were waiting for the metafile know. */
	metafile_read_done (directory);
}

static void
metafile_close_callback (GnomeVFSAsyncHandle *handle,
			 GnomeVFSResult result,
			 gpointer callback_data)
{
	/* Do nothing. */
}

static void
metafile_read_callback (GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			gpointer buffer,
			GnomeVFSFileSize bytes_requested,
			GnomeVFSFileSize bytes_read,
			gpointer callback_data)
{
	NautilusDirectory *directory;

	g_assert (bytes_requested == READ_CHUNK_SIZE);

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->read_state->handle == handle);
	g_assert ((char *) directory->details->read_state->buffer
		  + directory->details->read_state->bytes_read == buffer);

	if (result != GNOME_VFS_OK
	    && result != GNOME_VFS_ERROR_EOF) {
		metafile_read_failed (directory, result);
		return;
	}

	directory->details->read_state->bytes_read += bytes_read;

	/* FIXME: Is the call allowed to return fewer bytes than I requested
	 * when I'm not at the end of the file? If so, then I need to fix this
	 * check. I don't want to stop until the end of the file.
	 */
	if (bytes_read == bytes_requested && result == GNOME_VFS_OK) {
		metafile_read_some (directory);
		return;
	}

	gnome_vfs_async_close (directory->details->read_state->handle,
			       metafile_close_callback,
			       directory);

	metafile_read_complete (directory);
}

static void
metafile_read_some (NautilusDirectory *directory)
{
	directory->details->read_state->buffer = g_realloc
		(directory->details->read_state->buffer,
		 directory->details->read_state->bytes_read + READ_CHUNK_SIZE);

	gnome_vfs_async_read (directory->details->read_state->handle,
			      (char *) directory->details->read_state->buffer
			      + directory->details->read_state->bytes_read,
			      READ_CHUNK_SIZE,
			      metafile_read_callback,
			      directory);
}

static void
metafile_read_open_callback (GnomeVFSAsyncHandle *handle,
			     GnomeVFSResult result,
			     gpointer callback_data)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->read_state->handle == handle);

	if (result != GNOME_VFS_OK) {
		metafile_read_failed (directory, result);
		return;
	}

	metafile_read_some (directory);
}

static void
metafile_read_start (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	gnome_vfs_async_open_uri (&directory->details->read_state->handle,
				  directory->details->use_alternate_metafile
				  ? directory->details->alternate_metafile_uri
				  : directory->details->metafile_uri,
				  GNOME_VFS_OPEN_READ,
				  metafile_read_open_callback,
				  directory);
}

static void
nautilus_directory_request_read_metafile (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	if (directory->details->metafile_read
	    || directory->details->read_state != NULL) {
		return;
	}

	g_assert (directory->details->metafile == NULL);

	directory->details->read_state = g_new0 (MetafileReadState, 1);
	directory->details->use_alternate_metafile = TRUE;
	metafile_read_start (directory);
}

static void
metafile_write_done (NautilusDirectory *directory)
{
	if (directory->details->write_state->write_again) {
		metafile_write_start (directory);
		return;
	}

	xmlFree (directory->details->write_state->buffer);
	g_free (directory->details->write_state);
	directory->details->write_state = NULL;
	nautilus_directory_unref (directory);
}

static void
metafile_write_failed (NautilusDirectory *directory,
		       GnomeVFSResult result)
{
	if (!directory->details->use_alternate_metafile) {
		directory->details->use_alternate_metafile = TRUE;
		metafile_write_start (directory);
		return;
	}

	metafile_write_done (directory);
}

static void
metafile_write_complete (NautilusDirectory *directory)
{
	metafile_write_done (directory);
}

static void
metafile_write_callback (GnomeVFSAsyncHandle *handle,
			 GnomeVFSResult result,
			 gconstpointer buffer,
			 GnomeVFSFileSize bytes_requested,
			 GnomeVFSFileSize bytes_read,
			 gpointer callback_data)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->write_state->handle == handle);
	g_assert (directory->details->write_state->buffer == buffer);
	g_assert (directory->details->write_state->size == bytes_requested);

	if (result != GNOME_VFS_OK) {
		metafile_write_failed (directory, result);
		return;
	}

	gnome_vfs_async_close (directory->details->write_state->handle,
			       metafile_close_callback,
			       directory);

	metafile_write_complete (directory);
}

static void
metafile_write_create_callback (GnomeVFSAsyncHandle *handle,
				GnomeVFSResult result,
				gpointer callback_data)
{
	NautilusDirectory *directory;
	
	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->write_state->handle == handle);
	
	if (result != GNOME_VFS_OK) {
		metafile_write_failed (directory, result);
		return;
	}

	gnome_vfs_async_write (directory->details->write_state->handle,
			       directory->details->write_state->buffer,
			       directory->details->write_state->size,
			       metafile_write_callback,
			       directory);
}

static void
metafile_write_start (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	directory->details->write_state->write_again = FALSE;

	/* Open the file. */
	gnome_vfs_async_create_uri (&directory->details->write_state->handle,
				    directory->details->use_alternate_metafile
				    ? directory->details->alternate_metafile_uri
				    : directory->details->metafile_uri,
				    GNOME_VFS_OPEN_WRITE,
				    FALSE,
				    METAFILE_PERMISSIONS,
				    metafile_write_create_callback,
				    directory);
}

static void
metafile_write (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	nautilus_directory_ref (directory);

	/* If we are already writing, then just remember to do it again. */
	if (directory->details->write_state != NULL) {
		nautilus_directory_unref (directory);
		directory->details->write_state->write_again = TRUE;
		return;
	}

	/* Don't write anything if there's nothing to write.
	 * At some point, we might want to change this to actually delete
	 * the metafile in this case.
	 */
	if (directory->details->metafile == NULL) {
		nautilus_directory_unref (directory);
		return;
	}

	/* Create the write state. */
	directory->details->write_state = g_new0 (MetafileWriteState, 1);
	xmlDocDumpMemory (directory->details->metafile,
			  &directory->details->write_state->buffer,
			  &directory->details->write_state->size);

	metafile_write_start (directory);
}

static gboolean
metafile_write_idle_callback (gpointer callback_data)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (callback_data);

	directory->details->write_metafile_idle_id = 0;
	metafile_write (directory);

	nautilus_directory_unref (directory);

	return FALSE;
}

void
nautilus_directory_request_write_metafile (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	/* Set up an idle task that will write the metafile. */
	if (directory->details->write_metafile_idle_id == 0) {
		nautilus_directory_ref (directory);
		directory->details->write_metafile_idle_id =
			gtk_idle_add (metafile_write_idle_callback,
				      directory);
	}
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

static int
compare_file_monitor_by_client_and_file (gconstpointer a,
				  	 gconstpointer data)
{
	const FileMonitor *monitor;
	const FileMonitor *compare_monitor;

	monitor = a;
	compare_monitor = data;
	
	if (monitor->client < compare_monitor->client) {
		return -1;
	}
	if (monitor->client > compare_monitor->client) {
		return +1;
	}

	if (monitor->file < compare_monitor->file) {
		return -1;
	}
	if (monitor->file > compare_monitor->file) {
		return +1;
	}
	
	return 0;
}

static GList *
find_file_monitor (NautilusDirectory *directory,
		   NautilusFile *file,
		   gconstpointer client)
{
	GList *result;
	FileMonitor *file_monitor;

	file_monitor = g_new (FileMonitor, 1);
	file_monitor->client = client;
	file_monitor->file = file;

	result = g_list_find_custom (directory->details->file_monitors,
				     (gpointer) file_monitor,
				     compare_file_monitor_by_client_and_file);

	g_free (file_monitor);
	
	return result;
}

static void
cancel_unneeded_file_attribute_requests (NautilusDirectory *directory)
{
	GList *p;
	FileMonitor *file_monitor;

	/* Cancel the directory-count request if no one cares anymore */
	if (directory->details->count_in_progress != NULL) {
		for (p = directory->details->file_monitors; p != NULL; p = p->next) {
			file_monitor = p->data;
			if (file_monitor->monitor_directory_counts) {
				break;
			}
		}
		if (p == NULL) {
			gnome_vfs_async_cancel (directory->details->count_in_progress);
			directory->details->count_in_progress = NULL;
		}
	}		
}

void
remove_file_monitor_link (NautilusDirectory *directory, GList *link)
{
	directory->details->file_monitors =
		g_list_remove_link (directory->details->file_monitors, link);
	if (link != NULL) {
		g_free (link->data);
	}
	g_list_free (link);
}

static void
remove_file_monitor (NautilusDirectory *directory,
		     NautilusFile *file,
		     gconstpointer client)
{
	remove_file_monitor_link (directory, find_file_monitor (directory, file, client));
}

static gboolean
is_file_list_monitored (NautilusDirectory *directory) 
{
	FileMonitor *file_monitor;
	GList *p;

	for (p = directory->details->file_monitors; p != NULL; p = p->next) {
		file_monitor = p->data;
		if (file_monitor->file == NULL) {
			return TRUE;
		}
	}
	
	return FALSE;
}

void
nautilus_directory_file_monitor_add_internal (NautilusDirectory *directory,
					      NautilusFile *file,
				     	      gconstpointer client,
				     	      GList *attributes,
				     	      GList *metadata_keys,
				     	      NautilusFileListCallback callback,
				     	      gpointer callback_data)
{
	gboolean was_monitoring_file_list;
	gboolean will_be_monitoring_file_list;
	gboolean rely_on_directory_load;
	FileMonitor *file_monitor;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	/* If monitoring everything (file == NULL) then there must be
	 * a callback. If not, there must not be.
	 */
	g_return_if_fail (file != NULL || callback != NULL);
	g_return_if_fail (file == NULL || callback == NULL);

	was_monitoring_file_list = is_file_list_monitored (directory);
	will_be_monitoring_file_list = was_monitoring_file_list || file == NULL;
	rely_on_directory_load = will_be_monitoring_file_list && !directory->details->directory_loaded;

	/* Replace any current monitor for this client/file pair. */
	remove_file_monitor (directory, file, client);

	file_monitor = g_new (FileMonitor, 1);
	file_monitor->client = client;
	file_monitor->file = file;
	file_monitor->monitor_directory_counts = g_list_find_custom
		(attributes,
		 NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT,
		 (GCompareFunc) nautilus_strcmp) != NULL;				     

	directory->details->file_monitors =
		g_list_prepend (directory->details->file_monitors, file_monitor);

	/* Clean up stale requests only after (optionally) removing old 
	 * monitor and adding new one. 
	 */
	cancel_unneeded_file_attribute_requests (directory);

	/* Keep a ref to all the files so they don't vanish while we're monitoring them. */
	if (file == NULL) {
		nautilus_file_list_ref (directory->details->files);
	}

	/* Tell the new monitor-er about the current set of
	 * files, which may or may not be all of them.
	 */
	if (directory->details->files != NULL && file == NULL) {
		(* callback) (directory,
			      directory->details->files,
			      callback_data);
	}

	/* Process pending requests if there's nothing (left) to load. 
	 * Otherwise this will happen after the load.
	 */
	if (!rely_on_directory_load) {
		process_pending_file_attribute_requests (directory);
	}

	/* No need to hold onto these new refs if we already have old ones
	 * from when we started monitoring. Those will be cleared up in
	 * stop_monitoring_file_list.
	 */
	if (was_monitoring_file_list && file == NULL) {
		nautilus_file_list_unref (directory->details->files);
		return;
	}

	/* If we don't need to load any more files, bail out now. */
	if (!rely_on_directory_load) {
		return;
	}

	g_assert (directory->details->directory_load_in_progress == NULL);
	
	g_assert (directory->details->uri->text != NULL);
	directory->details->directory_load_list_last_handled
		= GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;
	gnome_vfs_async_load_directory_uri
		(&directory->details->directory_load_in_progress, /* handle */
		 directory->details->uri,                         /* uri */
		 (GNOME_VFS_FILE_INFO_GETMIMETYPE	          /* options */
		  | GNOME_VFS_FILE_INFO_FASTMIMETYPE
		  | GNOME_VFS_FILE_INFO_FOLLOWLINKS),
		 NULL, 					          /* meta_keys */
		 NULL, 					          /* sort_rules */
		 FALSE, 				          /* reverse_order */
		 GNOME_VFS_DIRECTORY_FILTER_NONE,                 /* filter_type */
		 (GNOME_VFS_DIRECTORY_FILTER_NOSELFDIR            /* filter_options */
		  | GNOME_VFS_DIRECTORY_FILTER_NOPARENTDIR),
		 NULL,                                            /* filter_pattern */
		 DIRECTORY_LOAD_ITEMS_PER_CALLBACK,               /* items_per_notification */
		 nautilus_directory_load_callback,                /* callback */
		 directory);
}

void
nautilus_directory_file_monitor_add (NautilusDirectory *directory,
				     gconstpointer client,
				     GList *attributes,
				     GList *metadata_keys,
				     NautilusFileListCallback callback,
				     gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (client != NULL);
	g_return_if_fail (attributes != NULL || metadata_keys != NULL);
	g_return_if_fail (callback != NULL);

	nautilus_directory_file_monitor_add_internal (directory,
						      NULL,
						      client,
						      attributes,
						      metadata_keys,
						      callback,
						      callback_data);
}

static gboolean
dequeue_pending_idle_callback (gpointer callback_data)
{
	NautilusDirectory *directory;
	GList *pending_file_info;
	GList *p;
	NautilusFile *file;
	GList *pending_files;

	directory = NAUTILUS_DIRECTORY (callback_data);

	directory->details->dequeue_pending_idle_id = 0;

	pending_file_info = directory->details->pending_file_info;
	directory->details->pending_file_info = NULL;

	/* Don't emit a signal if there are no new files. */
	if (pending_file_info == NULL) {
		return FALSE;
	}

	/* If we stopped monitoring, then throw away these. */
	if (!is_file_list_monitored (directory)) {
		nautilus_gnome_vfs_file_info_list_free (pending_file_info);
		return FALSE;
	}

	/* Build a list of NautilusFile objects. */
	pending_files = NULL;
	for (p = pending_file_info; p != NULL; p = p->next) {
		/* FIXME: The file could already be in the files list
		 * if someone did a nautilus_file_get already on it.
		 */
		file = nautilus_file_new (directory, p->data);
		pending_files = g_list_prepend (pending_files, file);
	}
	nautilus_gnome_vfs_file_info_list_free (pending_file_info);

	/* Tell the objects that are monitoring about these new files. */
	g_assert (pending_files != NULL);
	gtk_signal_emit (GTK_OBJECT (directory),
			 signals[FILES_ADDED],
			 pending_files);

	/* Remember them for later. */
	directory->details->files = g_list_concat
		(directory->details->files, pending_files);

	return FALSE;
}

static void
schedule_dequeue_pending (NautilusDirectory *directory)
{
	if (directory->details->dequeue_pending_idle_id == 0) {
		directory->details->dequeue_pending_idle_id
			= gtk_idle_add (dequeue_pending_idle_callback, directory);
	}
}

static void
nautilus_directory_load_one (NautilusDirectory *directory,
			     GnomeVFSFileInfo *info)
{
	gnome_vfs_file_info_ref (info);
        directory->details->pending_file_info
		= g_list_prepend (directory->details->pending_file_info, info);
	schedule_dequeue_pending (directory);
}

static void
nautilus_directory_load_done (NautilusDirectory *directory,
			      GnomeVFSResult result)
{
	if (directory->details->directory_load_in_progress != NULL) {
		gnome_vfs_async_cancel (directory->details->directory_load_in_progress);
		directory->details->directory_load_in_progress = NULL;
	}
	directory->details->directory_loaded = TRUE;
	schedule_dequeue_pending (directory);

	process_pending_file_attribute_requests (directory);
}

static GnomeVFSDirectoryListPosition
nautilus_gnome_vfs_directory_list_get_next_position (GnomeVFSDirectoryList *list,
						     GnomeVFSDirectoryListPosition position)
{
	if (position != GNOME_VFS_DIRECTORY_LIST_POSITION_NONE) {
		return gnome_vfs_directory_list_position_next (position);
	}
	if (list == NULL) {
		return GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;
	}
	return gnome_vfs_directory_list_get_first_position (list);
}

static void
nautilus_directory_load_callback (GnomeVFSAsyncHandle *handle,
				  GnomeVFSResult result,
				  GnomeVFSDirectoryList *list,
				  guint entries_read,
				  gpointer callback_data)
{
	NautilusDirectory *directory;
	GnomeVFSDirectoryListPosition last_handled, p;

	directory = NAUTILUS_DIRECTORY (callback_data);

	g_assert (directory->details->directory_load_in_progress == handle);

	/* Move items from the list onto our pending queue.
	 * We can't do this in the most straightforward way, becuse the position
	 * for a gnome_vfs_directory_list does not have a way of representing one
	 * past the end. So we must keep a position to the last item we handled
	 * rather than keeping a position past the last item we handled.
	 */
	last_handled = directory->details->directory_load_list_last_handled;
        p = last_handled;
	while ((p = nautilus_gnome_vfs_directory_list_get_next_position (list, p))
	       != GNOME_VFS_DIRECTORY_LIST_POSITION_NONE) {
		nautilus_directory_load_one
			(directory, gnome_vfs_directory_list_get (list, p));
		last_handled = p;
	}
	directory->details->directory_load_list_last_handled = last_handled;

	if (result != GNOME_VFS_OK) {
		nautilus_directory_load_done (directory, result);
	}
}

static void
stop_monitoring_file_list (NautilusDirectory *directory)
{
	if (directory->details->directory_load_in_progress != NULL) {
		gnome_vfs_async_cancel (directory->details->directory_load_in_progress);
		directory->details->directory_load_in_progress = NULL;
	}

	nautilus_file_list_unref (directory->details->files);
}

void
nautilus_directory_file_monitor_remove_internal (NautilusDirectory *directory,
						 NautilusFile *file,
						 gconstpointer client)
{
	gboolean was_monitoring_file_list;
	
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	was_monitoring_file_list = is_file_list_monitored (directory);

	remove_file_monitor (directory, file, client);
	cancel_unneeded_file_attribute_requests (directory);

	if (was_monitoring_file_list && !is_file_list_monitored (directory)) {
		stop_monitoring_file_list (directory);
	}
}

void
nautilus_directory_file_monitor_remove (NautilusDirectory *directory,
					gconstpointer client)
{
	nautilus_directory_file_monitor_remove_internal (directory, NULL, client);
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
	
	/* FIXME: Allow "true" and "false"? */
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
	 * FIXME: This linear search may not be fast enough.
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

static int
compare_queued_callbacks (gconstpointer a, gconstpointer b)
{
	const QueuedCallback *callback_a, *callback_b;

	callback_a = a;
	callback_b = b;
	if (callback_a->file < callback_b->file) {
		return -1;
	}
	if (callback_a->file > callback_b->file) {
		return 1;
	}
	if (callback_a->file == NULL) {
		if (callback_a->callback.directory < callback_b->callback.directory) {
			return -1;
		}
		if (callback_a->callback.directory > callback_b->callback.directory) {
			return 1;
		}
	} else {
		if (callback_a->callback.file < callback_b->callback.file) {
			return -1;
		}
		if (callback_a->callback.file > callback_b->callback.file) {
			return 1;
		}
	}
	if (callback_a->callback_data < callback_b->callback_data) {
		return -1;
	}
	if (callback_a->callback_data > callback_b->callback_data) {
		return 1;
	}
	return 0;
}

void
nautilus_directory_call_when_ready_internal (NautilusDirectory *directory,
					     const QueuedCallback *callback)
{
	g_assert (directory == NULL || NAUTILUS_IS_DIRECTORY (directory));
	g_assert (callback != NULL);
	g_assert (callback->file == NULL || callback->file->details->directory == directory);

	/* Call back right away if it's already ready. */
	if (directory == NULL || directory->details->metafile_read) {
		if (callback->file != NULL) {
			(* callback->callback.file) (callback->file,
						     callback->callback_data);
		} else {
			(* callback->callback.directory) (directory,
							  callback->callback_data);
		}
		return;
	}

	/* Add the new callback to the list unless it's already in there. */
	if (g_list_find_custom (directory->details->metafile_callbacks,
				(QueuedCallback *) callback,
				compare_queued_callbacks) == NULL) {
		nautilus_file_ref (callback->file);
		directory->details->metafile_callbacks = g_list_prepend
			(directory->details->metafile_callbacks,
			 g_memdup (callback,
				   sizeof (*callback)));
	}

	/* Start reading the metafile. */
	nautilus_directory_request_read_metafile (directory);
}

void
nautilus_directory_call_when_ready (NautilusDirectory *directory,
				    GList *directory_metadata_keys,
				    GList *file_metadata_keys,
				    NautilusDirectoryCallback callback,
				    gpointer callback_data)
{
	QueuedCallback new_callback;

	g_return_if_fail (directory == NULL || NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (directory_metadata_keys != NULL || file_metadata_keys != NULL);
	g_return_if_fail (callback != NULL);

	new_callback.file = NULL;
	new_callback.callback.directory = callback;
	new_callback.callback_data = callback_data;

	nautilus_directory_call_when_ready_internal (directory, &new_callback);
}

void
nautilus_directory_cancel_callback_internal (NautilusDirectory *directory,
					     const QueuedCallback *callback)
{
	GList *p;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (callback != NULL);

	/* Remove queued callback from the list. */
	p = g_list_find_custom (directory->details->metafile_callbacks,
				(QueuedCallback *) callback,
				compare_queued_callbacks);
	if (p != NULL) {
		nautilus_file_unref (callback->file);
		g_free (p->data);
		directory->details->metafile_callbacks = g_list_remove_link
			(directory->details->metafile_callbacks, p);
	}
}

void
nautilus_directory_cancel_callback (NautilusDirectory *directory,
				    NautilusDirectoryCallback callback,
				    gpointer callback_data)
{
	QueuedCallback old_callback;

	g_return_if_fail (callback != NULL);

	if (directory == NULL) {
		return;
	}

	/* NULL is OK here for non-vfs protocols */
	g_return_if_fail (!directory || NAUTILUS_IS_DIRECTORY (directory));

        old_callback.file = NULL;
	old_callback.callback.directory = callback;
	old_callback.callback_data = callback_data;

	nautilus_directory_cancel_callback_internal (directory, &old_callback);
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

static int
compare_file_with_name (gconstpointer a, gconstpointer b)
{
	return strcmp (NAUTILUS_FILE (a)->details->info->name,
		       (const char *) b);
}

NautilusFile *
nautilus_directory_find_file (NautilusDirectory *directory, const char *name)
{
	GList *list_entry;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	list_entry = g_list_find_custom (directory->details->files,
					 (gpointer) name,
					 compare_file_with_name);

	return list_entry == NULL ? NULL : list_entry->data;
}

void
nautilus_directory_files_changed (NautilusDirectory *directory,
				  GList *changed_files)
{
	GList *p;

	for (p = changed_files; p != NULL; p = p->next) {
		nautilus_file_emit_changed (p->data);
	}
	gtk_signal_emit (GTK_OBJECT (directory),
			 signals[FILES_CHANGED],
			 changed_files);
}

static void
directory_count_callback (GnomeVFSAsyncHandle *handle,
			  GnomeVFSResult result,
			  GnomeVFSDirectoryList *list,
			  guint entries_read,
			  gpointer callback_data)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (callback_data);

	g_assert (directory->details->count_in_progress == handle);
	if (result == GNOME_VFS_OK) {
		return;
	}

	/* Record either a failure or success. */
	if (result != GNOME_VFS_ERROR_EOF) {
		directory->details->count_file->details->directory_count_failed = TRUE;
	} else {
		directory->details->count_file->details->directory_count = entries_read;
		directory->details->count_file->details->got_directory_count = TRUE;
		nautilus_file_changed (directory->details->count_file);
	}

	/* Let go of this request. */
	nautilus_file_unref (directory->details->count_file);
	directory->details->count_in_progress = NULL;

	/* Start up the next one. */
	process_pending_file_attribute_requests (directory);
}

static void
process_pending_file_attribute_requests (NautilusDirectory *directory)
{
	GList *p, *p2;
	FileMonitor *monitor;
	NautilusFile *file;
	char *uri;

	if (directory->details->count_in_progress != NULL) {
		return;
	}

	/* Quick out if no one wants directory counts monitored. */
	for (p = directory->details->file_monitors; p != NULL; p = p->next) {
		monitor = p->data;
		if (monitor->monitor_directory_counts) {
			break;
		}
	}
	if (p == NULL) {
		return;
	}

	/* Search for a file that's a directory that needs a count. */
	for (p = directory->details->files; p != NULL; p = p->next) {
		file = p->data;
		if (nautilus_file_is_directory (file)
		    && !file->details->got_directory_count
		    && !file->details->directory_count_failed) {
		    	/* Make sure that someone cares about this particular directory's count. */
		    	for (p2 = directory->details->file_monitors; p2 != NULL; p2 = p2->next) {
				monitor = p2->data;
				if (monitor->monitor_directory_counts
				    && (monitor->file == NULL || monitor->file == file)) {
					break;
				}
		    	}

			if (p2 != NULL) {
				break;
			}		    	
		}
	}
	if (p == NULL) {
		return;
	}

	/* Start a load on the directory to count. */
	nautilus_file_ref (file);
	directory->details->count_file = file;
	uri = nautilus_file_get_uri (file);
	gnome_vfs_async_load_directory
		(&directory->details->count_in_progress,
		 uri,
		 GNOME_VFS_FILE_INFO_DEFAULT,
		 NULL,
		 NULL,
		 FALSE,
		 GNOME_VFS_DIRECTORY_FILTER_NONE,
		 (GNOME_VFS_DIRECTORY_FILTER_NOSELFDIR
		  | GNOME_VFS_DIRECTORY_FILTER_NOPARENTDIR),
		 NULL,
		 G_MAXINT,
		 directory_count_callback,
		 directory);
	g_free (uri);
}

/* If a directory object exists for this one's parent, then
 * return it, otherwise return NULL.
 */
static NautilusDirectory *
parent_directory_if_exists (const char *uri)
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
	g_assert (is_canonical_uri (directory_uri));

	/* Get directory from hash table. */
	if (directory_objects == NULL) {
		return NULL;
	}
	return g_hash_table_lookup (directory_objects, directory_uri);
}

static NautilusFile *
file_if_exists (const char *uri)
{
	/* FIXME: Darin will implement this soon. */
	return NULL;
}

void
nautilus_directory_notify_files_added (GList *uris)
{
	GList *p;
	NautilusDirectory *directory;
	const char *uri;

	for (p = uris; p != NULL; p = p->next) {
		uri = (const char *) p->data;

#ifdef COPY_NOTIFY_TESTING
		g_message ("added %s", uri);
#endif

		/* See if the directory is already known. */
		directory = parent_directory_if_exists (uri);
		if (directory == NULL) {
			continue;
		}

		/* If no one is monitoring files in the directory, nothing to do. */
		if (!is_file_list_monitored (directory)) {
			continue;
		}

		/* FIXME: Queue up files to have get_file_info called on them.
		 * We can't just call it synchronously.
		 * Once the NautilusFile objects are created we can emit
		 * the files_added signals. Do this in a way that
		 * results in a single files_added per directory
		 * instead of many single-file calls.
		 */
	}
}

void
nautilus_directory_notify_files_removed (GList *uris)
{
	GList *p;
	const char *uri;
	NautilusFile *file;

	for (p = uris; p != NULL; p = p->next) {
		uri = (const char *) p->data;

#ifdef COPY_NOTIFY_TESTING
		g_message ("removed %s", p->data);
#endif

		file = file_if_exists (uri);

		/* FIXME: Set the is_gone flag on the file, remove it
		 * from the directory, and call
		 * nautilus_directory_files_changed.  Do this in a way
		 * that results in a single
		 * nautilus_directory_files_changed per directory
		 * instead of many single-file calls (little hash
		 * table full of lists?).
		 */
	}
}

void
nautilus_directory_notify_files_moved (GList *uri_pairs)
{
	GList *p;
	URIPair *pair;
	NautilusFile *file;
	NautilusDirectory *from_directory, *to_directory;

	for (p = uri_pairs; p != NULL; p = p->next) {
		pair = p->data;

#ifdef COPY_NOTIFY_TESTING
		g_message ("moved %s to %s", pair->from_uri, pair->to_uri);
#endif

		file = file_if_exists (pair->from_uri);
		from_directory = parent_directory_if_exists (pair->from_uri);
		to_directory = parent_directory_if_exists (pair->to_uri);

		/* FIXME: If both directories are the same, send out a
		 * files_changed for the file. If the directories are
		 * different, move the file into the new directory and
		 * send out a file added. Do this in a way that
		 * results in a single files_changed or file_added per
		 * directory instead of many single-file calls (little
		 * hash table full of lists?).
		 */
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
