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

#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>

#include <gnome-xml/parser.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/xmlmemory.h>

#include "nautilus-alloc.h"
#include "nautilus-glib-extensions.h"
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

#define DIRECTORY_LOAD_ITEMS_PER_CB 1

enum 
{
	FILES_ADDED,
	LAST_SIGNAL
};

static guint nautilus_directory_signals[LAST_SIGNAL];

static void nautilus_directory_initialize_class (gpointer klass);
static void nautilus_directory_initialize (gpointer object, gpointer klass);
static void nautilus_directory_finalize (GtkObject *object);

static NautilusDirectory *nautilus_directory_new (const char* uri);

static void nautilus_directory_read_metafile (NautilusDirectory *directory);
static void nautilus_directory_write_metafile (NautilusDirectory *directory);
static void nautilus_directory_request_write_metafile (NautilusDirectory *directory);
static void nautilus_directory_remove_write_metafile_idle (NautilusDirectory *directory);

static void nautilus_file_detach (NautilusFile *file);
static int  nautilus_file_compare_for_sort_internal (NautilusFile *file_1,
					 	     NautilusFile *file_2,
					 	     NautilusFileSortType sort_type,
					 	     gboolean reversed);

static void nautilus_directory_load_cb (GnomeVFSAsyncHandle *handle,
					GnomeVFSResult result,
					GnomeVFSDirectoryList *list,
					guint entries_read,
					gpointer callback_data);
static NautilusFile *nautilus_directory_new_file (NautilusDirectory *directory,
						  GnomeVFSFileInfo *info);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusDirectory, nautilus_directory, GTK_TYPE_OBJECT)

struct _NautilusDirectoryDetails
{
	char *uri_text;
	GnomeVFSURI *uri;

	GnomeVFSURI *metafile_uri;
	GnomeVFSURI *alternate_metafile_uri;
	gboolean use_alternate_metafile;

	xmlDoc *metafile_tree;
	guint write_metafile_idle_id;

	GnomeVFSAsyncHandle *directory_load_in_progress;
	GnomeVFSDirectoryListPosition directory_load_list_last_handled;

	GList *pending_file_info;
        guint dequeue_pending_idle_id;

	gboolean directory_loaded;

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

	nautilus_directory_signals[FILES_ADDED] =
		gtk_signal_new ("files_added",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusDirectoryClass, files_added),
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
nautilus_directory_finalize (GtkObject *object)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (object);

	if (directory->details->write_metafile_idle_id != 0)
		nautilus_directory_write_metafile (directory);

	g_hash_table_remove (directory_objects, directory->details->uri_text);

	/* Let go of all the files. */
	while (directory->details->files != NULL) {
		nautilus_file_detach (directory->details->files->data);

		directory->details->files = g_list_remove_link
			(directory->details->files, directory->details->files);
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
		if(!directory)
			return NULL;

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
		char *buffer;

		buffer = g_alloca(size);
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
nautilus_directory_write_metafile_idle_cb (gpointer callback_data)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (callback_data);

	directory->details->write_metafile_idle_id = 0;
	nautilus_directory_write_metafile (directory);

	return FALSE;
}

static void
nautilus_directory_request_write_metafile (NautilusDirectory *directory)
{
	/* Set up an idle task that will write the metafile. */
	if (directory->details->write_metafile_idle_id == 0)
		directory->details->write_metafile_idle_id =
			gtk_idle_add (nautilus_directory_write_metafile_idle_cb,
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
nautilus_directory_start_monitoring (NautilusDirectory *directory,
				     NautilusFileListCallback callback,
				     gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (callback != NULL);

	if (directory->details->files != NULL)
		(* callback) (directory,
			      directory->details->files,
			      callback_data);

	if (directory->details->directory_load_in_progress == NULL) {
		GnomeVFSResult result;

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
}

static gboolean
dequeue_pending_idle_cb (gpointer callback_data)
{
	NautilusDirectory *directory;
	GList *pending_file_info;
	GList *p;
	NautilusFile *file;
	NautilusFileList *pending_files;

	directory = NAUTILUS_DIRECTORY (callback_data);

	directory->details->dequeue_pending_idle_id = 0;

	pending_files = NULL;

	/* Build a list of NautilusFile objects. */
	pending_file_info = directory->details->pending_file_info;
	directory->details->pending_file_info = NULL;
	for (p = pending_file_info; p != NULL; p = p->next) {
		file = nautilus_directory_new_file (directory, p->data);
		pending_files = g_list_prepend (pending_files, file);
		gnome_vfs_file_info_unref (p->data);
	}
	g_list_free (pending_file_info);

	if (pending_files == NULL)
		return FALSE;

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
	if (directory->details->dequeue_pending_idle_id == 0)
		directory->details->dequeue_pending_idle_id
			= gtk_idle_add (dequeue_pending_idle_cb, directory);
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
	directory->details->directory_load_in_progress = NULL;
	directory->details->directory_loaded = TRUE;
	schedule_dequeue_pending (directory);
}

static GnomeVFSDirectoryListPosition
nautilus_gnome_vfs_directory_list_get_next_position (GnomeVFSDirectoryList *list,
						     GnomeVFSDirectoryListPosition position)
{
	if (position != GNOME_VFS_DIRECTORY_LIST_POSITION_NONE)
		return gnome_vfs_directory_list_position_next (position);
	if (list == NULL)
		return GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;
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

	if (result != GNOME_VFS_OK)
		nautilus_directory_load_done (directory, result);
}

void
nautilus_directory_stop_monitoring (NautilusDirectory *directory)
{
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

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
	if (result_as_string == NULL)
		return default_metadata;

	if (strcmp (result_as_string, "TRUE") == 0)
	{
		result = TRUE;
	} 
	else if (strcmp (result_as_string, "FALSE") == 0)
	{
		result = FALSE;
	}
	else
	{
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
	if (result_as_string == NULL)
	{
		result = default_metadata;
	}
	else
	{
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

static NautilusFile *
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

/**
 * nautilus_file_get:
 * @uri: URI of file to get.
 *
 * Get a file given a uri.
 * Returns a referenced object. Unref when finished.
 * If two windows are viewing the same uri, the file object is shared.
 */
NautilusFile *
nautilus_file_get (const char *uri)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *file_info;
	GnomeVFSURI *vfs_uri, *directory_vfs_uri;
	char *directory_uri;
	NautilusDirectory *directory;
	NautilusFile *file;

	/* Get info on the file. */
	file_info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (uri, file_info,
					  GNOME_VFS_FILE_INFO_GETMIMETYPE
					  | GNOME_VFS_FILE_INFO_FASTMIMETYPE
		  			  | GNOME_VFS_FILE_INFO_FOLLOWLINKS, NULL);
	if (result != GNOME_VFS_OK)
		return NULL;

	/* Make VFS version of URI. */
	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL)
		return NULL;

	/* Make VFS version of directory URI. */
	directory_vfs_uri = gnome_vfs_uri_get_parent (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);
	if (directory_vfs_uri == NULL)
		return NULL;

	/* Make text version of directory URI. */
	directory_uri = gnome_vfs_uri_to_string (directory_vfs_uri,
						 GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (directory_vfs_uri);

	/* Get object that represents the directory. */
	directory = nautilus_directory_get (directory_uri);
	g_free (directory_uri);
	if (directory == NULL)
		return NULL;

	file = nautilus_directory_new_file (directory, file_info);

	gnome_vfs_file_info_unref (file_info);
	nautilus_file_ref (file);
	gtk_object_unref (GTK_OBJECT (directory));
	
	return file;
}

void
nautilus_file_ref (NautilusFile *file)
{
	g_return_if_fail (file != NULL);

	g_assert (file->ref_count < UINT_MAX);
	g_assert (file->directory != NULL);

	/* Increment the ref count. */
	if (file->ref_count++ != 0)
		return;

	/* As soon as someone other than the directory holds a ref, 
	 * we need to hold the directory too. */
	gtk_object_ref (GTK_OBJECT (file->directory));
}

void
nautilus_file_unref (NautilusFile *file)
{
	g_return_if_fail (file != NULL);

	g_assert (file->ref_count != 0);
	g_assert (file->directory != NULL);

	/* Decrement the ref count. */
	if (--file->ref_count != 0)
		return;

	/* No references left, so it's time to release our hold on the directory. */
	gtk_object_unref (GTK_OBJECT (file->directory));
}

static void
nautilus_file_detach (NautilusFile *file)
{
	g_assert (file->ref_count == 0);

	/* Destroy the file object. */
	gnome_vfs_file_info_unref (file->info);
}

static int
nautilus_file_compare_for_sort_internal (NautilusFile *file_1,
					 NautilusFile *file_2,
					 NautilusFileSortType sort_type,
					 gboolean reversed)
{
	GnomeVFSDirectorySortRule *rules;

	g_return_val_if_fail (file_1 != NULL, 0);
	g_return_val_if_fail (file_2 != NULL, 0);
	g_return_val_if_fail (sort_type != NAUTILUS_FILE_SORT_NONE, 0);

#define ALLOC_RULES(n) alloca ((n) * sizeof (GnomeVFSDirectorySortRule))

	switch (sort_type) {
	case NAUTILUS_FILE_SORT_BY_NAME:
		rules = ALLOC_RULES (2);
		/* Note: This used to put directories first. I
		 * thought that was counterintuitive and removed it,
		 * but I can imagine discussing this further.
		 * John Sullivan <sullivan@eazel.com>
		 */
		rules[0] = GNOME_VFS_DIRECTORY_SORT_BYNAME_IGNORECASE;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case NAUTILUS_FILE_SORT_BY_SIZE:
		rules = ALLOC_RULES (4);
		rules[0] = GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYSIZE;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_BYNAME_IGNORECASE;
		rules[3] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case NAUTILUS_FILE_SORT_BY_TYPE:
		rules = ALLOC_RULES (4);
		rules[0] = GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYMIMETYPE;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_BYNAME_IGNORECASE;
		rules[3] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case NAUTILUS_FILE_SORT_BY_MTIME:
		rules = ALLOC_RULES (3);
		rules[0] = GNOME_VFS_DIRECTORY_SORT_BYMTIME;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYNAME_IGNORECASE;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	default:
		g_assert_not_reached ();
		return 0;
	}

	if (reversed)
		return gnome_vfs_file_info_compare_for_sort_reversed (file_1->info,
								      file_2->info,
								      rules);
	else
		return gnome_vfs_file_info_compare_for_sort (file_1->info,
							     file_2->info,
							     rules);

#undef ALLOC_RULES
}

/**
 * nautilus_file_compare_for_sort:
 * @file_1: A file object
 * @file_2: Another file object
 * @sort_type: Sort criterion
 * 
 * Return value: int < 0 if @file_1 should come before file_2 in a smallest-to-largest
 * sorted list; int > 0 if @file_2 should come before file_1 in a smallest-to-largest
 * sorted list; 0 if @file_1 and @file_2 are equal for this sort criterion. Note
 * that each named sort type may actually break ties several ways, with the name
 * of the sort criterion being the primary but not only differentiator.
 **/
int
nautilus_file_compare_for_sort (NautilusFile *file_1,
				NautilusFile *file_2,
				NautilusFileSortType sort_type)
{
	return nautilus_file_compare_for_sort_internal (file_1, file_2, sort_type, FALSE);
}

/**
 * nautilus_file_compare_for_sort_reversed:
 * @file_1: A file object
 * @file_2: Another file object
 * @sort_type: Sort criterion
 * 
 * Return value: The opposite of nautilus_file_compare_for_sort: int > 0 if @file_1 
 * should come before file_2 in a smallest-to-largest sorted list; int < 0 if @file_2 
 * should come before file_1 in a smallest-to-largest sorted list; 0 if @file_1 
 * and @file_2 are equal for this sort criterion. Note that each named sort type 
 * may actually break ties several ways, with the name of the sort criterion 
 * being the primary but not only differentiator.
 **/
int
nautilus_file_compare_for_sort_reversed (NautilusFile *file_1,
					 NautilusFile *file_2,
					 NautilusFileSortType sort_type)
{
	return nautilus_file_compare_for_sort_internal (file_1, file_2, sort_type, TRUE);
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
	/* Note: This uses modified time. There's also accessed time and 
	 * changed time. Accessed time doesn't seem worth showing to the user.
	 * Changed time is only subtly different from modified time
	 * (changed time includes "metadata" changes like file permissions).
	 * We should not display both, but we might change our minds as to
	 * which one is better.
	 */
	 
	struct tm *file_time;
	const char *format;
	GDate *today;
	GDate *file_date;
	guint32 file_date_age;

	g_return_val_if_fail (file != NULL, NULL);

	file_time = localtime(&file->info->mtime);
	file_date = nautilus_g_date_new_tm (file_time);
	
	today = g_date_new ();
	g_date_set_time (today, time (NULL));

	/* Overflow results in a large number; fine for our purposes. */
	file_date_age = g_date_julian (today) - g_date_julian (file_date);

	g_date_free (file_date);
	g_date_free (today);

	/* Format varies depending on how old the date is. This minimizes
	 * the length (and thus clutter & complication) of typical dates
	 * while providing sufficient detail for recent dates to make
	 * them maximally understandable at a glance. Keep all format
	 * strings separate rather than combining bits & pieces for
	 * internationalization's sake.
	 */

	if (file_date_age == 0)
	{
		/* today, use special word */
		format = _("today %-I:%M %p");
	}
	else if (file_date_age == 1)
	{
		/* yesterday, use special word */
		format = _("yesterday %-I:%M %p");
	}
	else if (file_date_age < 7)
	{
		/* current week, include day of week */
		format = _("%A %-m/%-d/%y %-I:%M %p");
	}
	else
	{
		format = _("%-m/%-d/%y %-I:%M %p");
	}

	return nautilus_strdup_strftime (format, file_time);
}

/**
 * nautilus_file_get_size
 * 
 * Get the file size.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Size in bytes.
 * 
 **/
GnomeVFSFileSize
nautilus_file_get_size (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, 0);

	return file->info->size;
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

/**
 * nautilus_file_get_type
 * 
 * Return this file's type.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: The type.
 * 
 **/
GnomeVFSFileType
nautilus_file_get_type (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, FALSE);

	return file->info->type;
}

/**
 * nautilus_file_get_mime_type
 * 
 * Return this file's mime type.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: The mime type.
 * 
 **/
const char *
nautilus_file_get_mime_type (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, FALSE);

	return file->info->mime_type;
}

/**
 * nautilus_file_is_symbolic_link
 * 
 * Check if this file is a symbolic link.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: True if the file is a symbolic link.
 * 
 **/
gboolean
nautilus_file_is_symbolic_link (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, FALSE);

	return GNOME_VFS_FILE_INFO_SYMLINK (file->info);
}

/**
 * nautilus_file_is_executable
 * 
 * Check if this file is executable at all.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: True if any of the execute bits are set.
 * 
 **/
gboolean
nautilus_file_is_executable (NautilusFile *file)
{
	g_return_val_if_fail (file != NULL, FALSE);

	return (file->info->flags & (GNOME_VFS_PERM_USER_EXEC
				     | GNOME_VFS_PERM_GROUP_EXEC
				     | GNOME_VFS_PERM_OTHER_EXEC)) != 0;
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
	NautilusFile *file_1;
	NautilusFile *file_2;

	directory = nautilus_directory_get ("file:///etc");

	NAUTILUS_CHECK_INTEGER_RESULT (g_hash_table_size (directory_objects), 1);

	file_count = 0;
	nautilus_directory_start_monitoring (directory, get_files_cb, &data_dummy);

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

	gtk_object_unref (GTK_OBJECT (directory));

	NAUTILUS_CHECK_INTEGER_RESULT (g_hash_table_size (directory_objects), 0);

	directory = nautilus_directory_get ("file:///etc");

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

	/* sorting */
	file_1 = nautilus_file_get ("file:///etc");
	file_2 = nautilus_file_get ("file:///usr");

	NAUTILUS_CHECK_INTEGER_RESULT (file_1->ref_count, 1);
	NAUTILUS_CHECK_INTEGER_RESULT (file_2->ref_count, 1);

	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_2, NAUTILUS_FILE_SORT_BY_NAME) < 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort_reversed (file_1, file_2, NAUTILUS_FILE_SORT_BY_NAME) > 0, TRUE);
	NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_1, NAUTILUS_FILE_SORT_BY_NAME) == 0, TRUE);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
