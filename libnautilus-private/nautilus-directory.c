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

#define DIRECTORY_LOAD_ITEMS_PER_CB 1

enum 
{
	FILES_ADDED,
	FILES_REMOVED,
	FILES_CHANGED,
	LAST_SIGNAL
};

static guint nautilus_directory_signals[LAST_SIGNAL];

static void               nautilus_directory_initialize_class           (gpointer               klass);
static void               nautilus_directory_initialize                 (gpointer               object,
									 gpointer               klass);
static void               nautilus_directory_destroy                    (GtkObject             *object);
static NautilusDirectory *nautilus_directory_new                        (const char            *uri);
static void               nautilus_directory_read_metafile              (NautilusDirectory     *directory);
static void               nautilus_directory_write_metafile             (NautilusDirectory     *directory);
static void               nautilus_directory_load_cb                    (GnomeVFSAsyncHandle   *handle,
									 GnomeVFSResult         result,
									 GnomeVFSDirectoryList *list,
									 guint                  entries_read,
									 gpointer               callback_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusDirectory, nautilus_directory, GTK_TYPE_OBJECT)

static GHashTable* directory_objects;

static void
nautilus_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_directory_destroy;

	nautilus_directory_signals[FILES_ADDED] =
		gtk_signal_new ("files_added",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDirectoryClass, files_added),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	nautilus_directory_signals[FILES_REMOVED] =
		gtk_signal_new ("files_removed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDirectoryClass, files_removed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	nautilus_directory_signals[FILES_CHANGED] =
		gtk_signal_new ("files_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDirectoryClass, files_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, nautilus_directory_signals, LAST_SIGNAL);
}

static void
nautilus_directory_initialize (gpointer object, gpointer klass)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY(object);

	directory->details = g_new0 (NautilusDirectoryDetails, 1);
}

static void
nautilus_directory_destroy (GtkObject *object)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (object);

	if (directory->details->monitor_files_ref_count != 0) {
		g_warning ("destroying a NautilusDirectory while it's being monitored");
	}

	g_assert (directory->details->write_metafile_idle_id == 0);

	g_hash_table_remove (directory_objects, directory->details->uri_text);

	/* Let go of all the files. */
	g_list_foreach (directory->details->files, (GFunc) nautilus_file_free, NULL);
	g_list_free (directory->details->files);

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
	xmlFreeDoc (directory->details->metafile);

	g_free (directory->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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

	/* Create the hash table first time through. */
	if (directory_objects == NULL) {
		directory_objects = g_hash_table_new (g_str_hash, g_str_equal);
	}

	/* If the object is already in the hash table, look it up. */
	directory = g_hash_table_lookup (directory_objects,
					 canonical_uri);
	if (directory != NULL) {
		g_assert (NAUTILUS_IS_DIRECTORY (directory));
		gtk_object_ref (GTK_OBJECT (directory));
	} else {
		/* Create a new directory object instead. */
		directory = nautilus_directory_new (canonical_uri);
		if (directory == NULL) {
			return NULL;
		}

		g_assert (strcmp (directory->details->uri_text, canonical_uri) == 0);

		/* Put it in the hash table. */
		gtk_object_ref (GTK_OBJECT (directory));
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
	size_t size; /* not GnomeVFSFileSize, since it's passed to g_malloc */
	GnomeVFSFileSize actual_size;
	char *buffer;
	
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), GNOME_VFS_ERROR_GENERIC);
	g_return_val_if_fail (directory->details->metafile == NULL, GNOME_VFS_ERROR_GENERIC);

	result = gnome_vfs_get_file_info_uri (metafile_uri,
					      &metafile_info,
					      GNOME_VFS_FILE_INFO_DEFAULT,
					      NULL);

	if (result == GNOME_VFS_OK) {
		/* Check for the case where the info doesn't give the file size. */
		if ((metafile_info.valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) == 0) {
			result = GNOME_VFS_ERROR_GENERIC;
		}
	}

	if (result == GNOME_VFS_OK) {
		/* Check for a size that won't fit into a size_t. */
		size = metafile_info.size;
		if (size != metafile_info.size) {
			result = GNOME_VFS_ERROR_TOOBIG;
		}
	}

	metafile_handle = NULL;
	if (result == GNOME_VFS_OK) {
		result = gnome_vfs_open_uri (&metafile_handle,
					     metafile_uri,
					     GNOME_VFS_OPEN_READ);
	}

	if (result == GNOME_VFS_OK) {
		/* The gnome-xml parser requires a zero-terminated array.
		 * Also, we don't want to allocate an empty buffer
		 * because it would be NULL and gnome-xml won't parse
		 * NULL properly.
		 */
		buffer = g_malloc (size + 1);
		result = gnome_vfs_read (metafile_handle, buffer, size, &actual_size);
		buffer[actual_size] = '\0';
		directory->details->metafile = xmlParseMemory (buffer, actual_size);
		g_free (buffer);
	}

	if (metafile_handle != NULL) {
		gnome_vfs_close (metafile_handle);
	}

	return result;
}

static void
nautilus_directory_read_metafile (NautilusDirectory *directory)
{
	GnomeVFSResult result;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	/* Check for the alternate metafile first.
	 * If we read from it, then write to it later.
	 */
	directory->details->use_alternate_metafile = FALSE;
	result = nautilus_directory_try_to_read_metafile (directory,
							  directory->details->alternate_metafile_uri);
	if (result == GNOME_VFS_OK) {
		directory->details->use_alternate_metafile = TRUE;
	} else {
		result = nautilus_directory_try_to_read_metafile (directory,
								  directory->details->metafile_uri);
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
	g_return_val_if_fail (directory->details->metafile != NULL, GNOME_VFS_ERROR_GENERIC);

	metafile_handle = NULL;
	result = gnome_vfs_create_uri (&metafile_handle,
				       metafile_uri,
				       GNOME_VFS_OPEN_WRITE,
				       FALSE,
				       METAFILE_PERMISSIONS);

	buffer = NULL;
	if (result == GNOME_VFS_OK) {
		xmlDocDumpMemory (directory->details->metafile, &buffer, &buffer_size);
		result = gnome_vfs_write (metafile_handle, buffer, buffer_size, &actual_size);
		if (buffer_size != actual_size) {
			result = GNOME_VFS_ERROR_GENERIC;
		}
	}

	if (metafile_handle != NULL) {
		gnome_vfs_close (metafile_handle);
	}

	xmlFree (buffer);

	return result;
}

static void
nautilus_directory_write_metafile (NautilusDirectory *directory)
{
	GnomeVFSResult result;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	gtk_object_ref (GTK_OBJECT (directory));

	/* We are about the write the metafile, so we can cancel the pending
	   request to do so.
	*/
	if (directory->details->write_metafile_idle_id != 0) {
		gtk_idle_remove (directory->details->write_metafile_idle_id);
		directory->details->write_metafile_idle_id = 0;
		gtk_object_unref (GTK_OBJECT (directory));
	}

	/* Don't write anything if there's nothing to write.
	   At some point, we might want to change this to actually delete
	   the metafile in this case.
	*/
	if (directory->details->metafile != NULL) {
		
		/* Try the main URI, unless we have already been instructed to use the alternate URI. */
		if (directory->details->use_alternate_metafile) {
			result = GNOME_VFS_ERROR_ACCESSDENIED;
		} else {
			result = nautilus_directory_try_to_write_metafile (directory,
									   directory->details->metafile_uri);
		}
		
		/* Try the alternate URI if the main one failed. */
		if (result != GNOME_VFS_OK) {
			result = nautilus_directory_try_to_write_metafile (directory,
									   directory->details->alternate_metafile_uri);
		}
		
		/* Check for errors. FIXME: Later this must be reported to the user, not spit out as a warning. */
		if (result != GNOME_VFS_OK) {
			g_warning ("nautilus_directory_write_metafile failed to write metafile - we should report this to the user");
		}

	}

	gtk_object_unref (GTK_OBJECT (directory));
}

static gboolean
nautilus_directory_write_metafile_idle_cb (gpointer callback_data)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (callback_data);

	directory->details->write_metafile_idle_id = 0;
	nautilus_directory_write_metafile (directory);

	gtk_object_unref (GTK_OBJECT (directory));

	return FALSE;
}

void
nautilus_directory_request_write_metafile (NautilusDirectory *directory)
{
	/* Set up an idle task that will write the metafile. */
	if (directory->details->write_metafile_idle_id == 0) {
		gtk_object_ref (GTK_OBJECT (directory));
		directory->details->write_metafile_idle_id =
			gtk_idle_add (nautilus_directory_write_metafile_idle_cb,
				      directory);
	}
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
nautilus_directory_construct_alternate_metafile_uri (GnomeVFSURI *uri)
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
	if (vfs_uri == NULL) {
		return NULL;
	}

	metafile_uri = gnome_vfs_uri_append_path (vfs_uri, METAFILE_NAME);
	alternate_metafile_uri = nautilus_directory_construct_alternate_metafile_uri (vfs_uri);

	directory = gtk_type_new (NAUTILUS_TYPE_DIRECTORY);

	directory->details->uri_text = g_strdup(uri);
	directory->details->uri = vfs_uri;
	directory->details->metafile_uri = metafile_uri;
	directory->details->alternate_metafile_uri = alternate_metafile_uri;

	nautilus_directory_read_metafile (directory);

	return directory;
}

void
nautilus_directory_monitor_files_ref (NautilusDirectory *directory,
				      NautilusFileListCallback callback,
				      gpointer callback_data)
{
	GnomeVFSResult result;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (callback != NULL);

	if (directory->details->files != NULL) {
		(* callback) (directory,
			      directory->details->files,
			      callback_data);
	}

	g_assert (directory->details->monitor_files_ref_count != UINT_MAX);
	directory->details->monitor_files_ref_count++;

	if (directory->details->directory_loaded) {
		return;
	}
	if (directory->details->directory_load_in_progress != NULL) {
		return;
	}
	
	g_assert (directory->details->uri->text != NULL);
	directory->details->directory_load_list_last_handled
		= GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;
	result = gnome_vfs_async_load_directory_uri
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
		 DIRECTORY_LOAD_ITEMS_PER_CB,                     /* items_per_notification */
		 nautilus_directory_load_cb,                      /* callback */
		 directory);
}

static gboolean
dequeue_pending_idle_cb (gpointer callback_data)
{
	NautilusDirectory *directory;
	GList *pending_file_info;
	GList *p;
	NautilusFile *file;
	GList *pending_files;

	directory = NAUTILUS_DIRECTORY (callback_data);

	directory->details->dequeue_pending_idle_id = 0;

	pending_files = NULL;

	/* Build a list of NautilusFile objects. */
	pending_file_info = directory->details->pending_file_info;
	directory->details->pending_file_info = NULL;
	for (p = pending_file_info; p != NULL; p = p->next) {
		file = nautilus_directory_new_file (directory, p->data);
		g_assert (file != NULL);
		pending_files = g_list_prepend (pending_files, file);
		gnome_vfs_file_info_unref (p->data);
	}
	g_list_free (pending_file_info);

	if (pending_files == NULL) {
		return FALSE;
	}

	/* Tell the people who are monitoring about these new files. */
	gtk_signal_emit (GTK_OBJECT (directory),
			 nautilus_directory_signals[FILES_ADDED],
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
			= gtk_idle_add (dequeue_pending_idle_cb, directory);
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
nautilus_directory_load_cb (GnomeVFSAsyncHandle *handle,
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

void
nautilus_directory_monitor_files_unref (NautilusDirectory *directory)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (directory->details->monitor_files_ref_count != 0);

	if (--directory->details->monitor_files_ref_count != 0) {
		return;
	}

	if (directory->details->directory_load_in_progress != NULL) {
		gnome_vfs_async_cancel (directory->details->directory_load_in_progress);
		directory->details->directory_load_in_progress = NULL;
	}
}

gboolean
nautilus_directory_are_all_files_seen (NautilusDirectory *directory)
{
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	
	return directory->details->directory_loaded;
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
	if (property == NULL) {
		result = g_strdup (default_metadata);
	} else {
		result = g_strdup (property);
	}
	xmlFree (property);

	return result;
}

static xmlNode *
nautilus_directory_create_metafile_root (NautilusDirectory *directory)
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
				 const char *tag,
				 const char *default_metadata)
{
	/* It's legal to call this on a NULL directory. */
	if (directory == NULL) {
		return g_strdup (default_metadata);
	}

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	/* The root itself represents the directory. */
	return nautilus_directory_get_metadata_from_node
		(xmlDocGetRootElement (directory->details->metafile),
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
	root = nautilus_directory_create_metafile_root (directory);

	/* Add or remove an attribute node. */
	property_node = xmlSetProp (root, tag, value);
	if (value == NULL) {
		xmlRemoveProp (property_node);
	}

	/* Since we changed the tree, arrange for it to be written. */
	nautilus_directory_request_write_metafile (directory);
}

gboolean 
nautilus_directory_get_boolean_metadata (NautilusDirectory *directory,
					 const char *tag,
					 gboolean default_metadata)
{
	char *result_as_string;
	gboolean result;

	result_as_string = nautilus_directory_get_metadata (
				directory,
				tag,
				default_metadata ? "TRUE" : "FALSE");

	/* Handle oddball case of non-existent directory */
	if (result_as_string == NULL) {
		return default_metadata;
	}

	if (strcmp (result_as_string, "TRUE") == 0) {
		result = TRUE;
	} else if (strcmp (result_as_string, "FALSE") == 0) {
		result = FALSE;
	} else {
		g_assert_not_reached ();
		result = default_metadata;
	}

	g_free (result_as_string);
	return result;

}

void               
nautilus_directory_set_boolean_metadata (NautilusDirectory *directory,
					 const char *tag,
					 gboolean default_metadata,
					 gboolean metadata)
{
	nautilus_directory_set_metadata (directory,
					 tag,
					 default_metadata ? "TRUE" : "FALSE",
					 metadata ? "TRUE" : "FALSE");
}

int 
nautilus_directory_get_integer_metadata (NautilusDirectory *directory,
					 const char *tag,
					 int default_metadata)
{
	char *result_as_string;
	char *default_as_string;
	int result;

	default_as_string = g_strdup_printf ("%d", default_metadata);
	result_as_string = nautilus_directory_get_metadata (
				directory,
				tag,
				default_as_string);

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
					 const char *tag,
					 int default_metadata,
					 int metadata)
{
	char *value_as_string;
	char *default_as_string;

	value_as_string = g_strdup_printf ("%d", metadata);
	default_as_string = g_strdup_printf ("%d", default_metadata);

	nautilus_directory_set_metadata (directory,
					 tag,
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
	 * FIXME: This linear search is temporary.
	 * Eventually, we'll have a pointer from the NautilusFile right to
	 * the corresponding XML node, or we won't have the XML tree
	 * in memory at all.
	 */
	child = nautilus_xml_get_root_child_by_name_and_property
		(directory->details->metafile,
		 "FILE", "NAME", file_name);
	if (child != NULL) {
		return child;
	}
	
	/* Create if necessary. */
	if (create) {
		root = nautilus_directory_create_metafile_root (directory);
		child = xmlNewChild (root, NULL, "FILE", NULL);
		xmlSetProp (child, "NAME", file_name);
		return child;
	}
	
	return NULL;
}

char *
nautilus_directory_get_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *tag,
				      const char *default_metadata)
{
	return nautilus_directory_get_metadata_from_node
		(nautilus_directory_get_file_metadata_node (directory, file_name, FALSE),
		 tag, default_metadata);
}

gboolean
nautilus_directory_set_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *tag,
				      const char *default_metadata,
				      const char *metadata)
{
	char *old_metadata;
	gboolean old_metadata_matches;
	xmlNode *child;
	const char *value;
	xmlAttr *property_node;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (tag != NULL, FALSE);
	g_return_val_if_fail (tag[0] != '\0', FALSE);

	/* If the data in the metafile is already correct, do nothing. */
	old_metadata = nautilus_directory_get_file_metadata
		(directory, file_name, tag, default_metadata);
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
		property_node = xmlSetProp (child, tag, value);
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
	return strcmp (((const NautilusFile *) a)->info->name,
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

NautilusFile *
nautilus_directory_new_file (NautilusDirectory *directory, GnomeVFSFileInfo *info)
{
	NautilusFile *file;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (info != NULL, NULL);

	gnome_vfs_file_info_ref (info);

	file = g_new0 (NautilusFile, 1);
	file->directory = directory;
	file->info = info;

	return file;
}

void
nautilus_directory_files_removed (NautilusDirectory *directory,
				  GList *removed_files)
{
	gtk_signal_emit (GTK_OBJECT (directory),
			 nautilus_directory_signals[FILES_REMOVED],
			 removed_files);
}

void
nautilus_directory_files_changed (NautilusDirectory *directory,
				  GList *changed_files)
{
	gtk_signal_emit (GTK_OBJECT (directory),
			 nautilus_directory_signals[FILES_CHANGED],
			 changed_files);
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

static int data_dummy;
static guint file_count;

static void
get_files_cb (NautilusDirectory *directory, GList *files, gpointer data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (files);
	g_assert (data == &data_dummy);

	file_count += g_list_length (files);
}

/* Return the number of extant NautilusDirectories */
int
nautilus_directory_number_outstanding ()
{
        return g_hash_table_size (directory_objects);
}

void
nautilus_self_check_directory (void)
{
	NautilusDirectory *directory;

	directory = nautilus_directory_get ("file:///etc");

	NAUTILUS_CHECK_INTEGER_RESULT (g_hash_table_size (directory_objects), 1);

	file_count = 0;
	nautilus_directory_monitor_files_ref (directory, get_files_cb, &data_dummy);

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
	gtk_object_unref (GTK_OBJECT (directory));

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_directory_get ("file:///etc/") == directory, TRUE);
	gtk_object_unref (GTK_OBJECT (directory));

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_directory_get ("file:///etc////") == directory, TRUE);
	gtk_object_unref (GTK_OBJECT (directory));

	nautilus_directory_monitor_files_unref (directory);

	gtk_object_unref (GTK_OBJECT (directory));

	/* let the idle function run */
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	NAUTILUS_CHECK_INTEGER_RESULT (g_hash_table_size (directory_objects), 0);

	directory = nautilus_directory_get ("file:///etc");

	NAUTILUS_CHECK_BOOLEAN_RESULT (directory->details->metafile != NULL, TRUE);

	NAUTILUS_CHECK_INTEGER_RESULT (g_hash_table_size (directory_objects), 1);

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

	gtk_object_unref (GTK_OBJECT (directory));
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
