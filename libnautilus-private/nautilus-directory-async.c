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
#include "nautilus-file-attributes.h"
#include "nautilus-global-preferences.h"

#include <gtk/gtkmain.h>

#include <parser.h>
#include <xmlmemory.h>
#include <stdlib.h>

#include "nautilus-string.h"
#include "nautilus-glib-extensions.h"

#define METAFILE_PERMISSIONS (GNOME_VFS_PERM_USER_READ | GNOME_VFS_PERM_USER_WRITE \
			      | GNOME_VFS_PERM_GROUP_READ | GNOME_VFS_PERM_GROUP_WRITE \
			      | GNOME_VFS_PERM_OTHER_READ | GNOME_VFS_PERM_OTHER_WRITE)

#define DIRECTORY_LOAD_ITEMS_PER_CALLBACK 32

#define METAFILE_READ_CHUNK_SIZE (4 * 1024)
#define TOP_LEFT_TEXT_READ_CHUNK_SIZE (4 * 1024)

struct MetafileReadState {
	GnomeVFSAsyncHandle *handle;
	gboolean is_open;
	char *buffer;
	int bytes_read;
};

struct MetafileWriteState {
	GnomeVFSAsyncHandle *handle;
	xmlChar *buffer;
	int size;
	gboolean write_again;
};

struct TopLeftTextReadState {
	GnomeVFSAsyncHandle *handle;
	NautilusFile *file;
	gboolean is_open;
	char *buffer;
	int bytes_read;
};

/* A request for information about one or more files. */
typedef struct {
	gboolean metafile;
	gboolean file_list; /* always FALSE if file != NULL */
	gboolean directory_count;
	gboolean top_left_text;
} Request;

typedef struct {
	NautilusFile *file; /* Which file, NULL means all. */
	union {
		NautilusDirectoryCallback directory;
		NautilusFileCallback file;
	} callback;
	gpointer callback_data;
	Request request;
} ReadyCallback;

typedef struct {
	NautilusFile *file; /* Which file, NULL means all. */
	gconstpointer client;
	Request request;
} Monitor;

typedef gboolean (* RequestCheck) (const Request *);
typedef gboolean (* FileCheck) (NautilusFile *);

/* Forward declarations for functions that need them. */
static void metafile_read_some  (NautilusDirectory *directory);
static void metafile_read_start (NautilusDirectory *directory);
static void state_changed       (NautilusDirectory *directory);
static void top_left_read_some  (NautilusDirectory *directory);

static void
empty_close_callback (GnomeVFSAsyncHandle *handle,
		      GnomeVFSResult result,
		      gpointer callback_data)
{
	if (result != GNOME_VFS_OK) {
		g_warning ("close failed");
	}
	/* Do nothing. */
}

static void
metafile_read_close (NautilusDirectory *directory)
{
	g_assert (directory->details->metafile_read_state->handle != NULL);
	if (directory->details->metafile_read_state->is_open) {
		gnome_vfs_async_close (directory->details->metafile_read_state->handle,
				       empty_close_callback,
				       directory);
		directory->details->metafile_read_state->is_open = FALSE;
	}
	directory->details->metafile_read_state->handle = NULL;
}

void
nautilus_metafile_read_cancel (NautilusDirectory *directory)
{
	if (directory->details->metafile_read_state == NULL) {
		return;
	}

	g_assert (directory->details->metafile_read_state->handle != NULL);
	gnome_vfs_async_cancel (directory->details->metafile_read_state->handle);
	metafile_read_close (directory);
	g_free (directory->details->metafile_read_state);
	directory->details->metafile_read_state = NULL;
}

static void
metafile_read_done (NautilusDirectory *directory)
{
	g_free (directory->details->metafile_read_state);

	directory->details->metafile_read = TRUE;
	directory->details->metafile_read_state = NULL;

	/* Move over the changes to the metafile that were in the hash table. */
	nautilus_directory_metafile_apply_pending_changes (directory);

	/* Let the callers that were waiting for the metafile know. */
	state_changed (directory);
}

static void
metafile_read_failed (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (directory->details->metafile == NULL);

	g_free (directory->details->metafile_read_state->buffer);

	if (directory->details->use_alternate_metafile) {
		directory->details->metafile_read_state->buffer = NULL;
		directory->details->metafile_read_state->bytes_read = 0;

		directory->details->use_alternate_metafile = FALSE;
		metafile_read_start (directory);
		return;
	}

	metafile_read_done (directory);
}

static void
metafile_read_complete (NautilusDirectory *directory)
{
	char *buffer;
	int size;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (directory->details->metafile == NULL);
	
	/* The gnome-xml parser requires a zero-terminated array. */
	size = directory->details->metafile_read_state->bytes_read;
	buffer = g_realloc (directory->details->metafile_read_state->buffer, size + 1);
	buffer[size] = '\0';
	directory->details->metafile = xmlParseMemory (buffer, size);
	g_free (buffer);

	metafile_read_done (directory);
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

	g_assert (bytes_requested == METAFILE_READ_CHUNK_SIZE);

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->metafile_read_state->handle == handle);
	g_assert (directory->details->metafile_read_state->buffer
		  + directory->details->metafile_read_state->bytes_read == buffer);

	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		metafile_read_close (directory);
		metafile_read_failed (directory);
		return;
	}

	directory->details->metafile_read_state->bytes_read += bytes_read;

	if (bytes_read != 0 && result == GNOME_VFS_OK) {
		metafile_read_some (directory);
		return;
	}

	metafile_read_close (directory);
	metafile_read_complete (directory);
}

static void
metafile_read_some (NautilusDirectory *directory)
{
	directory->details->metafile_read_state->buffer = g_realloc
		(directory->details->metafile_read_state->buffer,
		 directory->details->metafile_read_state->bytes_read + METAFILE_READ_CHUNK_SIZE);

	gnome_vfs_async_read (directory->details->metafile_read_state->handle,
			      directory->details->metafile_read_state->buffer
			      + directory->details->metafile_read_state->bytes_read,
			      METAFILE_READ_CHUNK_SIZE,
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
	g_assert (directory->details->metafile_read_state->handle == handle);

	if (result != GNOME_VFS_OK) {
		metafile_read_failed (directory);
		return;
	}

	directory->details->metafile_read_state->is_open = TRUE;
	metafile_read_some (directory);
}

static void
metafile_read_start (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	gnome_vfs_async_open_uri (&directory->details->metafile_read_state->handle,
				  directory->details->use_alternate_metafile
				  ? directory->details->alternate_metafile_uri
				  : directory->details->metafile_uri,
				  GNOME_VFS_OPEN_READ,
				  metafile_read_open_callback,
				  directory);
}

void
nautilus_directory_request_read_metafile (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	if (directory->details->metafile_read
	    || directory->details->metafile_read_state != NULL) {
		return;
	}

	g_assert (directory->details->metafile == NULL);

	directory->details->metafile_read_state = g_new0 (MetafileReadState, 1);
	directory->details->use_alternate_metafile = TRUE;
	metafile_read_start (directory);
}


static void
metafile_write_done (NautilusDirectory *directory)
{
	if (directory->details->metafile_write_state->write_again) {
		nautilus_metafile_write_start (directory);
		return;
	}

	xmlFree (directory->details->metafile_write_state->buffer);
	g_free (directory->details->metafile_write_state);
	directory->details->metafile_write_state = NULL;
	nautilus_directory_unref (directory);
}

static void
metafile_write_failed (NautilusDirectory *directory)
{
	if (!directory->details->use_alternate_metafile) {
		directory->details->use_alternate_metafile = TRUE;
		nautilus_metafile_write_start (directory);
		return;
	}

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
	g_assert (directory->details->metafile_write_state->handle == handle);
	g_assert (directory->details->metafile_write_state->buffer == buffer);
	g_assert (directory->details->metafile_write_state->size == bytes_requested);

	g_assert (directory->details->metafile_write_state->handle != NULL);
	gnome_vfs_async_close (directory->details->metafile_write_state->handle,
			       empty_close_callback,
			       directory);
	directory->details->metafile_write_state->handle = NULL;

	if (result != GNOME_VFS_OK) {
		metafile_write_failed (directory);
		return;
	}

	metafile_write_done (directory);
}

static void
metafile_write_create_callback (GnomeVFSAsyncHandle *handle,
				GnomeVFSResult result,
				gpointer callback_data)
{
	NautilusDirectory *directory;
	
	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->metafile_write_state->handle == handle);
	
	if (result != GNOME_VFS_OK) {
		metafile_write_failed (directory);
		return;
	}

	gnome_vfs_async_write (directory->details->metafile_write_state->handle,
			       directory->details->metafile_write_state->buffer,
			       directory->details->metafile_write_state->size,
			       metafile_write_callback,
			       directory);
}

void
nautilus_metafile_write_start (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	directory->details->metafile_write_state->write_again = FALSE;

	/* Open the file. */
	gnome_vfs_async_create_uri (&directory->details->metafile_write_state->handle,
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
	if (directory->details->metafile_write_state != NULL) {
		nautilus_directory_unref (directory);
		directory->details->metafile_write_state->write_again = TRUE;
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
	directory->details->metafile_write_state = g_new0 (MetafileWriteState, 1);
	xmlDocDumpMemory (directory->details->metafile,
			  &directory->details->metafile_write_state->buffer,
			  &directory->details->metafile_write_state->size);

	nautilus_metafile_write_start (directory);
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

static int
monitor_key_compare (gconstpointer a,
		     gconstpointer data)
{
	const Monitor *monitor;
	const Monitor *compare_monitor;

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
find_monitor (NautilusDirectory *directory,
	      NautilusFile *file,
	      gconstpointer client)
{
	GList *result;
	Monitor *monitor;

	monitor = g_new (Monitor, 1);
	monitor->client = client;
	monitor->file = file;

	result = g_list_find_custom (directory->details->monitor_list,
				     monitor,
				     monitor_key_compare);

	g_free (monitor);
	
	return result;
}

static void
remove_monitor_link (NautilusDirectory *directory,
			  GList *link)
{
	directory->details->monitor_list =
		g_list_remove_link (directory->details->monitor_list, link);
	if (link != NULL) {
		g_free (link->data);
	}
	g_list_free (link);
}

static void
remove_monitor (NautilusDirectory *directory,
		NautilusFile *file,
		gconstpointer client)
{
	remove_monitor_link (directory, find_monitor (directory, file, client));
}

static void
set_up_request_by_file_attributes (Request *request,
				   GList *file_attributes)
{
	request->directory_count = g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT,
		 nautilus_str_compare) != NULL;
	request->top_left_text = g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_TOP_LEFT_TEXT,
		 nautilus_str_compare) != NULL;
}

void
nautilus_directory_monitor_add_internal (NautilusDirectory *directory,
					 NautilusFile *file,
					 gconstpointer client,
					 GList *file_attributes,
					 gboolean monitor_metadata,
					 NautilusDirectoryCallback callback,
					 gpointer callback_data)
{
	Monitor *monitor;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	/* If monitoring everything (file == NULL) then there must be
	 * a callback. If not, there must not be.
	 */
	g_assert (file != NULL || callback != NULL);
	g_assert (file == NULL || callback == NULL);

	/* Replace any current monitor for this client/file pair. */
	remove_monitor (directory, file, client);

	/* Add the new monitor. */
	monitor = g_new (Monitor, 1);
	monitor->file = file;
	monitor->client = client;
	monitor->request.metafile = monitor_metadata;
	monitor->request.file_list = file == NULL;
	set_up_request_by_file_attributes (&monitor->request, file_attributes);
	directory->details->monitor_list =
		g_list_prepend (directory->details->monitor_list, monitor);

	/* Tell the new monitor-er about the current set of
	 * files, which may or may not be all of them.
	 */
	if (directory->details->files != NULL && file == NULL) {
		(* callback) (directory,
			      directory->details->files,
			      callback_data);
	}

	/* Kick off I/O. */
	state_changed (directory);
}

int
nautilus_compare_file_with_name (gconstpointer a, gconstpointer b)
{
	return strcmp (NAUTILUS_FILE (a)->details->info->name,
		       (const char *) b);
}

static gboolean
update_file_info_in_list_if_needed (GList *list, 
				    GnomeVFSFileInfo *file_info)
{
	GList *list_entry;

	list_entry = g_list_find_custom (list,
					 (gpointer) file_info->name,
					 nautilus_compare_file_with_name);
	if (list_entry == NULL) {
		/* the file is not in the list yet */
		return FALSE;
	}

	/* the file is in the list already update the file info if needed */
	nautilus_file_update (NAUTILUS_FILE (list_entry->data), file_info);

	return TRUE;
}

static gboolean
dequeue_pending_idle_callback (gpointer callback_data)
{
	NautilusDirectory *directory;
	GList *pending_file_info;
	GList *p;
	NautilusFile *file;
	GList *pending_files;
	GList *changed_files;

	directory = NAUTILUS_DIRECTORY (callback_data);

	directory->details->dequeue_pending_idle_id = 0;

	pending_file_info = directory->details->pending_file_info;
	directory->details->pending_file_info = NULL;

	/* Don't emit a signal if there are no new files. */
	if (pending_file_info == NULL) {
		return FALSE;
	}

	/* If we are no longer monitoring, then throw away these. */
	if (!nautilus_directory_is_file_list_monitored (directory)) {
		gnome_vfs_file_info_list_free (pending_file_info);
		return FALSE;
	}

	pending_files = NULL;
	changed_files = NULL;

	/* Build a list of NautilusFile objects. */
	for (p = pending_file_info; p != NULL; p = p->next) {
		/* check if the file already exists */
		file = nautilus_directory_find_file (directory, 
						     ((const GnomeVFSFileInfo *)p->data)->name);
		if (file != NULL) {
			/* file already exists, check if it changed */
			if (nautilus_file_update (file, p->data)) {
				/* File changed, notify about the change. */
				changed_files = g_list_prepend (changed_files, file);
			}
		} else if (!update_file_info_in_list_if_needed (pending_files, p->data)) {
			/* new file, create a nautilus file object and add it to the list */
			file = nautilus_file_new (directory, p->data);
			pending_files = g_list_prepend (pending_files, file);
		}
	}
	gnome_vfs_file_info_list_free (pending_file_info);

	/* Tell the objects that are monitoring about these new files. */
	nautilus_directory_emit_files_added (directory, pending_files);

	/* Tell the objects that are monitoring about changed files. */
	nautilus_directory_emit_files_changed (directory, changed_files);
	
	/* Remember them for later. */
	directory->details->files = g_list_concat
		(directory->details->files, pending_files);

	return FALSE;
}

void
nautilus_directory_schedule_dequeue_pending (NautilusDirectory *directory)
{
	if (directory->details->dequeue_pending_idle_id == 0) {
		directory->details->dequeue_pending_idle_id
			= gtk_idle_add (dequeue_pending_idle_callback, directory);
	}
}

static void
directory_load_one (NautilusDirectory *directory,
		    GnomeVFSFileInfo *info)
{
	if (info == NULL) {
		return;
	}
	gnome_vfs_file_info_ref (info);
        directory->details->pending_file_info
		= g_list_prepend (directory->details->pending_file_info, info);
	nautilus_directory_schedule_dequeue_pending (directory);
}

static void
directory_load_done (NautilusDirectory *directory,
		     GnomeVFSResult result)
{
	if (directory->details->directory_load_in_progress != NULL) {
		gnome_vfs_async_cancel (directory->details->directory_load_in_progress);
		directory->details->directory_load_in_progress = NULL;
	}
	directory->details->directory_loaded = TRUE;

	/* Call the idle function right away. */
	if (directory->details->dequeue_pending_idle_id != 0) {
		gtk_idle_remove (directory->details->dequeue_pending_idle_id);
	}
	dequeue_pending_idle_callback (directory);

	state_changed (directory);
}

static GnomeVFSDirectoryListPosition
directory_list_get_next_position (GnomeVFSDirectoryList *list,
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
directory_load_callback (GnomeVFSAsyncHandle *handle,
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
	while ((p = directory_list_get_next_position (list, p))
	       != GNOME_VFS_DIRECTORY_LIST_POSITION_NONE) {
		directory_load_one
			(directory, gnome_vfs_directory_list_get (list, p));
		last_handled = p;
	}
	directory->details->directory_load_list_last_handled = last_handled;

	if (result != GNOME_VFS_OK) {
		directory_load_done (directory, result);
	}
}

void
nautilus_directory_monitor_remove_internal (NautilusDirectory *directory,
					    NautilusFile *file,
					    gconstpointer client)
{
	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (file == NULL || NAUTILUS_IS_FILE (file));
	g_assert (client != NULL);

	remove_monitor (directory, file, client);
	state_changed (directory);
}

static int
ready_callback_key_compare (gconstpointer a, gconstpointer b)
{
	const ReadyCallback *callback_a, *callback_b;

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

static void
ready_callback_call (NautilusDirectory *directory,
		     const ReadyCallback *callback)
{
	GList *file_list;

	/* Call the callback. */
	if (callback->file != NULL) {
		(* callback->callback.file) (callback->file,
					     callback->callback_data);
	} else {
		if (directory == NULL || !callback->request.file_list) {
			file_list = NULL;
		} else {
			file_list = directory->details->files;
		}

		/* Pass back the file list if the user was waiting for it. */
		(* callback->callback.directory) (directory,
						  file_list,
						  callback->callback_data);
	}
}

void
nautilus_directory_call_when_ready_internal (NautilusDirectory *directory,
					     NautilusFile *file,
					     GList *file_attributes,
					     gboolean wait_for_metadata,
					     NautilusDirectoryCallback directory_callback,
					     NautilusFileCallback file_callback,
					     gpointer callback_data)
{
	ReadyCallback callback;

	g_assert (directory == NULL || NAUTILUS_IS_DIRECTORY (directory));
	g_assert (file == NULL || NAUTILUS_IS_FILE (file));
	g_assert (file != NULL || directory_callback != NULL);
	g_assert (file == NULL || file_callback != NULL);

	/* Construct a callback object. */
	callback.file = file;
	if (file == NULL) {
		callback.callback.directory = directory_callback;
	} else {
		callback.callback.file = file_callback;
	}
	callback.callback_data = callback_data;
	callback.request.metafile = wait_for_metadata;
	callback.request.file_list = file == NULL && file_attributes != NULL;
	set_up_request_by_file_attributes (&callback.request, file_attributes);
	
	/* Handle the NULL case. */
	if (directory == NULL) {
		ready_callback_call (NULL, &callback);
		return;
	}

	/* Check if the callback is already there. */
	if (g_list_find_custom (directory->details->call_when_ready_list,
				&callback,
				ready_callback_key_compare) != NULL) {
		g_warning ("tried to add a new callback while an old one was pending");
		return;
	}

	/* Add the new callback to the list. */
	directory->details->call_when_ready_list = g_list_prepend
		(directory->details->call_when_ready_list,
		 g_memdup (&callback,
			   sizeof (callback)));

	state_changed (directory);
}

static void
remove_callback_link_keep_data (NautilusDirectory *directory,
				GList *link)
{
	directory->details->call_when_ready_list = g_list_remove_link
		(directory->details->call_when_ready_list, link);
	g_list_free (link);
}

static void
remove_callback_link (NautilusDirectory *directory,
		      GList *link)
{
	directory->details->call_when_ready_list = g_list_remove_link
		(directory->details->call_when_ready_list, link);
	g_free (link->data);
	g_list_free (link);
}

void
nautilus_directory_cancel_callback_internal (NautilusDirectory *directory,
					     NautilusFile *file,
					     NautilusDirectoryCallback directory_callback,
					     NautilusFileCallback file_callback,
					     gpointer callback_data)
{
	ReadyCallback callback;
	GList *p;

	if (directory == NULL) {
		return;
	}

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (file == NULL || NAUTILUS_IS_FILE (file));
	g_assert (file != NULL || directory_callback != NULL);
	g_assert (file == NULL || file_callback != NULL);

	/* Construct a callback object. */
	callback.file = file;
	if (file == NULL) {
		callback.callback.directory = directory_callback;
	} else {
		callback.callback.file = file_callback;
	}
	callback.callback_data = callback_data;

	/* Remove queued callback from the list. */
	p = g_list_find_custom (directory->details->call_when_ready_list,
				&callback,
				ready_callback_key_compare);
	if (p != NULL) {
		remove_callback_link (directory, p);
		state_changed (directory);
	}
}

static void
directory_count_callback (GnomeVFSAsyncHandle *handle,
			  GnomeVFSResult result,
			  GnomeVFSDirectoryList *list,
			  guint entries_read,
			  gpointer callback_data)
{
	NautilusDirectory *directory;
	NautilusFile *count_file;

	directory = NAUTILUS_DIRECTORY (callback_data);

	g_assert (directory->details->count_in_progress == handle);
	count_file = directory->details->count_file;
	g_assert (NAUTILUS_IS_FILE (count_file));

	if (result == GNOME_VFS_OK) {
		return;
	}

	/* Record either a failure or success. */
	if (result != GNOME_VFS_ERROR_EOF) {
		directory->details->count_file->details->directory_count_failed = TRUE;
	} else {
		directory->details->count_file->details->directory_count = entries_read;
		directory->details->count_file->details->got_directory_count = TRUE;
	}
	directory->details->count_file = NULL;
	directory->details->count_in_progress = NULL;

	/* Send file-changed even if count failed, so interested parties can
	 * distinguish between unknowable and not-yet-known cases.
	 */
	nautilus_file_changed (count_file);

	/* Start up the next one. */
	state_changed (directory);
}

static void
new_files_callback (GnomeVFSAsyncHandle *handle,
		    GList *results,
		    gpointer callback_data)
{
	GList **handles, *p;
	NautilusDirectory *directory;
	GnomeVFSGetFileInfoResult *result;

	directory = NAUTILUS_DIRECTORY (callback_data);
	handles = &directory->details->get_file_infos_in_progress;
	g_assert (handle == NULL || g_list_find (*handles, handle) != NULL);
	
	/* Note that this call is done. */
	*handles = g_list_remove (*handles, handle);

	/* Queue up the new files. */
	for (p = results; p != NULL; p = p->next) {
		result = p->data;
		directory_load_one (directory, result->file_info);
	}
}

void
nautilus_directory_get_info_for_new_files (NautilusDirectory *directory,
					   GList *vfs_uri_list)
{
	GnomeVFSAsyncHandle *handle;

	gnome_vfs_async_get_file_info
		(&handle,
		 vfs_uri_list,
		 (GNOME_VFS_FILE_INFO_GETMIMETYPE
		  | GNOME_VFS_FILE_INFO_FASTMIMETYPE
		  | GNOME_VFS_FILE_INFO_FOLLOWLINKS),
		 NULL,
		 new_files_callback,
		 directory);

	directory->details->get_file_infos_in_progress
		= g_list_prepend (directory->details->get_file_infos_in_progress,
				  handle);
}

void
nautilus_async_destroying_file (NautilusFile *file)
{
	NautilusDirectory *directory;
	gboolean changed;
	GList *p, *next;
	ReadyCallback *callback;
	Monitor *monitor;

	directory = file->details->directory;
	changed = FALSE;

	/* Check for callbacks. */
	for (p = directory->details->call_when_ready_list; p != NULL; p = next) {
		next = p->next;
		callback = p->data;

		if (callback->file == file) {
			/* Client should have cancelled callback. */
			g_warning ("destroyed file has call_when_ready pending");
			remove_callback_link (directory, p);
			changed = TRUE;
		}
	}

	/* Check for monitors. */
	for (p = directory->details->monitor_list; p != NULL; p = next) {
		next = p->next;
		monitor = p->data;

		if (monitor->file == file) {
			/* Client should have removed monitor earlier. */
			g_warning ("destroyed file still being monitored");
			remove_monitor_link (directory, p);
			changed = TRUE;
		}
	}

	/* Check if it's the file that's currently being worked on for
	 * counts or for get_file_info. If so, make that NULL so it gets
	 * canceled right away.
	 */
	if (directory->details->count_file == file) {
		directory->details->count_file = NULL;
		changed = TRUE;
	}
	if (directory->details->top_left_read_state != NULL
	    && directory->details->top_left_read_state->file == file) {
		directory->details->top_left_read_state->file = NULL;
		changed = TRUE;
	}

	/* Let the directory take care of the rest. */
	if (changed) {
		state_changed (directory);
	}
}

static gboolean
lacks_directory_count (NautilusFile *file)
{
	return nautilus_file_is_directory (file)
		&& !file->details->got_directory_count
		&& !file->details->directory_count_failed;
}

static gboolean
wants_directory_count (const Request *request)
{
	return request->directory_count;
}

static gboolean
lacks_top_left (NautilusFile *file)
{
	return nautilus_file_contains_text (file)
		&& !file->details->got_top_left_text;
}

static gboolean
wants_top_left (const Request *request)
{
	return request->top_left_text;
}

static gboolean
has_problem (NautilusDirectory *directory, NautilusFile *file, FileCheck problem)
{
	GList *p;

	if (file != NULL) {
		return (* problem) (file);
	}

	for (p = directory->details->files; p != NULL; p = p->next) {
		if ((* problem) (p->data)) {
			return TRUE;
		}
	}

	return FALSE;
	
}

static gboolean
ready_callback_is_satisfied (NautilusDirectory *directory,
			     ReadyCallback *callback)
{
	if (callback->request.metafile && !directory->details->metafile_read) {
		return FALSE;
	}

	if (callback->request.file_list && !directory->details->directory_loaded) {
		return FALSE;
	}

	if (callback->request.directory_count) {
		if (has_problem (directory, callback->file, lacks_directory_count)) {
			return FALSE;
		}
	}

	return TRUE;
}

static void
call_ready_callbacks (NautilusDirectory *directory)
{
	GList *p, *next;
	ReadyCallback *callback;

	while (1) {
		/* Check if any callbacks are satisifed and call them if they are. */
		for (p = directory->details->call_when_ready_list; p != NULL; p = next) {
			next = p->next;
			callback = p->data;
			
			if (ready_callback_is_satisfied (directory, callback)) {
				break;
			}
		}
		if (p == NULL) {
			return;
		}
		
		/* Callbacks are one-shots, so remove it now. */
		remove_callback_link_keep_data (directory, p);
		
		/* Call the callback. */
		ready_callback_call (directory, callback);
		g_free (callback);
	}
}

/* This checks if there's a request for monitoring the file list. */
static gboolean
is_anyone_monitoring_file_list (NautilusDirectory *directory)
{
	GList *p;
	ReadyCallback *callback;
	Monitor *monitor;

	for (p = directory->details->call_when_ready_list; p != NULL; p = p->next) {
		callback = p->data;
		if (callback->request.file_list) {
			return TRUE;
		}
	}

	for (p = directory->details->monitor_list; p != NULL; p = p->next) {
		monitor = p->data;
		if (monitor->request.file_list) {
			return TRUE;
		}
	}

	return FALSE;
}

/* This checks if there's a request for the metafile contents. */
static gboolean
is_anyone_waiting_for_metafile (NautilusDirectory *directory)
{
	GList *p;
	ReadyCallback *callback;

	for (p = directory->details->call_when_ready_list; p != NULL; p = p->next) {
		callback = p->data;
		if (callback->request.metafile) {
			return TRUE;
		}
	}

	return FALSE;
}

/* This checks if the file list being monitored. */
gboolean
nautilus_directory_is_file_list_monitored (NautilusDirectory *directory) 
{
	return directory->details->file_list_monitored;
}

/* Start monitoring the file list if it isn't already. */
static void
start_monitoring_file_list (NautilusDirectory *directory)
{
	if (directory->details->file_list_monitored) {
		return;
	}

	g_assert (directory->details->directory_load_in_progress == NULL);

	directory->details->file_list_monitored = TRUE;

	nautilus_file_list_ref (directory->details->files);

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
		 directory_load_callback,                         /* callback */
		 directory);
}

/* Stop monitoring the file list if it is being monitored. */
void
nautilus_directory_stop_monitoring_file_list (NautilusDirectory *directory)
{
	if (!directory->details->file_list_monitored) {
		g_assert (directory->details->directory_load_in_progress == NULL);
		return;
	}

	if (directory->details->directory_load_in_progress != NULL) {
		gnome_vfs_async_cancel (directory->details->directory_load_in_progress);
		directory->details->directory_load_in_progress = NULL;
	}

	nautilus_file_list_unref (directory->details->files);
	
	directory->details->file_list_monitored = FALSE;
}

static gboolean
is_wanted (NautilusFile *file, RequestCheck check_wanted)
{
	GList *p;
	ReadyCallback *callback;
	Monitor *monitor;

	g_assert (NAUTILUS_IS_FILE (file));

	for (p = file->details->directory->details->call_when_ready_list; p != NULL; p = p->next) {
		callback = p->data;
		if ((* check_wanted) (&callback->request)
		    && (callback->file == NULL || callback->file == file)) {
			return TRUE;
		}
	}
	for (p = file->details->directory->details->monitor_list; p != NULL; p = p->next) {
		monitor = p->data;
		if ((* check_wanted) (&monitor->request)
		    && (monitor->file == NULL || monitor->file == file)) {
			return TRUE;
		}
	}
	return FALSE;
}

static NautilusFile *
select_needy_file (NautilusDirectory *directory,
		   FileCheck check_missing,
		   RequestCheck check_wanted)
{
	GList *p, *p2;
	ReadyCallback *callback;
	Monitor *monitor;
	NautilusFile *file;

	/* Quick out if no one is interested. */
	for (p = directory->details->call_when_ready_list; p != NULL; p = p->next) {
		callback = p->data;
		if ((* check_wanted) (&callback->request)) {
			break;
		}
	}
	if (p == NULL) {
		for (p = directory->details->monitor_list; p != NULL; p = p->next) {
			monitor = p->data;
			if ((* check_wanted) (&monitor->request)) {
				break;
			}
		}
		if (p == NULL) {
			return NULL;
		}
	}

	/* Search for a file that has an unfulfilled request. */
	for (p = directory->details->files; p != NULL; p = p->next) {
		file = p->data;
		if ((* check_missing) (file)) {
		    	/* Make sure that someone cares about this particular directory's count. */
		    	for (p2 = directory->details->call_when_ready_list; p2 != NULL; p2 = p2->next) {
				callback = p2->data;
				if ((* check_wanted) (&callback->request)
				    && (callback->file == NULL || callback->file == file)) {
					break;
				}
		    	}
			if (p2 != NULL) {
				return file;
			}
			for (p2 = directory->details->monitor_list; p2 != NULL; p2 = p2->next) {
				monitor = p2->data;
				if ((* check_wanted) (&monitor->request)
				    && (monitor->file == NULL || monitor->file == file)) {
					break;
				}
			}
			if (p2 != NULL) {
				return file;
			}
		}
	}
	return NULL;
}

static void
start_getting_directory_counts (NautilusDirectory *directory)
{
	NautilusFile *file;
	char *uri;

	/* If there's already a count in progress, check to be sure
	 * it's still wanted.
	 */
	if (directory->details->count_in_progress != NULL) {
		if (directory->details->count_file != NULL) {
			g_assert (NAUTILUS_IS_FILE (directory->details->count_file));
			g_assert (directory->details->count_file->details->directory == directory);
			if (is_wanted (directory->details->count_file,
				       wants_directory_count)) {
				return;
			}
		}

		/* The count is not wanted, so stop it. */
		gnome_vfs_async_cancel (directory->details->count_in_progress);
		directory->details->count_file = NULL;
		directory->details->count_in_progress = NULL;
	}

	/* Figure out which file to get a count for. */
	file = select_needy_file (directory,
				  lacks_directory_count,
				  wants_directory_count);
	if (file == NULL) {
		return;
	}

	/* Start counting. */
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

static int
count_lines (const char *text, int length)
{
	int count, i;

	count = 0;
	for (i = 0; i < length; i++) {
		count += *text++ == '\n';
	}
	return count;
}

static void
top_left_read_done (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_FILE (directory->details->top_left_read_state->file));

	directory->details->top_left_read_state->file->details->got_top_left_text = TRUE;

	g_free (directory->details->top_left_read_state->buffer);
	g_free (directory->details->top_left_read_state);
	directory->details->top_left_read_state = NULL;

	state_changed (directory);
}

static void
top_left_read_failed (NautilusDirectory *directory)
{
	top_left_read_done (directory);
}

static void
top_left_read_complete (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_FILE (directory->details->top_left_read_state->file));

	g_free (directory->details->top_left_read_state->file->details->top_left_text);
	directory->details->top_left_read_state->file->details->top_left_text =
		nautilus_extract_top_left_text (directory->details->top_left_read_state->buffer,
						directory->details->top_left_read_state->bytes_read);

	nautilus_file_changed (directory->details->top_left_read_state->file);

	top_left_read_done (directory);
}

static void
top_left_read_close (NautilusDirectory *directory)
{
	g_assert (directory->details->top_left_read_state->handle != NULL);
	if (directory->details->top_left_read_state->is_open) {
		gnome_vfs_async_close (directory->details->top_left_read_state->handle,
				       empty_close_callback,
				       directory);
		directory->details->top_left_read_state->is_open = FALSE;
	}
	directory->details->top_left_read_state->handle = NULL;
}

static void
top_left_read_callback (GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			gpointer buffer,
			GnomeVFSFileSize bytes_requested,
			GnomeVFSFileSize bytes_read,
			gpointer callback_data)
{
	NautilusDirectory *directory;

	g_assert (bytes_requested == TOP_LEFT_TEXT_READ_CHUNK_SIZE);

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->top_left_read_state->handle == handle);
	g_assert (directory->details->top_left_read_state->buffer
		  + directory->details->top_left_read_state->bytes_read == buffer);

	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		top_left_read_close (directory);
		top_left_read_failed (directory);
		return;
	}

	directory->details->top_left_read_state->bytes_read += bytes_read;

	/* If we haven't read the whole file and we don't have enough lines,
	 * keep reading.
	 */
	if (bytes_read != 0 && result == GNOME_VFS_OK
	    && count_lines (directory->details->top_left_read_state->buffer,
			    directory->details->top_left_read_state->bytes_read)
	    <= NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_LINES) {
		top_left_read_some (directory);
		return;
	}

	/* We are done reading. */
	top_left_read_close (directory);
	top_left_read_complete (directory);
}

static void
top_left_read_some (NautilusDirectory *directory)
{
	directory->details->top_left_read_state->buffer = g_realloc
		(directory->details->top_left_read_state->buffer,
		 directory->details->top_left_read_state->bytes_read
		 + TOP_LEFT_TEXT_READ_CHUNK_SIZE);

	gnome_vfs_async_read (directory->details->top_left_read_state->handle,
			      directory->details->top_left_read_state->buffer
			      + directory->details->top_left_read_state->bytes_read,
			      TOP_LEFT_TEXT_READ_CHUNK_SIZE,
			      top_left_read_callback,
			      directory);
}

static void
top_left_open_callback (GnomeVFSAsyncHandle *handle,
			GnomeVFSResult result,
			gpointer callback_data)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->top_left_read_state->handle == handle);

	if (result != GNOME_VFS_OK) {
		top_left_read_failed (directory);
		return;
	}

	directory->details->top_left_read_state->is_open = TRUE;
	top_left_read_some (directory);
}

static void
start_getting_top_lefts (NautilusDirectory *directory)
{
	NautilusFile *file;
	char *uri;

	/* If there's already a read in progress, check to be sure
	 * it's still wanted.
	 */
	if (directory->details->top_left_read_state != NULL) {
		if (directory->details->top_left_read_state->file != NULL) {
			g_assert (NAUTILUS_IS_FILE (directory->details->top_left_read_state->file));
			g_assert (directory->details->top_left_read_state->file->details->directory == directory);
			if (is_wanted (directory->details->top_left_read_state->file,
				       wants_top_left)) {
				return;
			}
		}

		/* The top left is not wanted, so stop it. */
		gnome_vfs_async_cancel (directory->details->top_left_read_state->handle);
		top_left_read_close (directory);
		g_free (directory->details->top_left_read_state->buffer);
		g_free (directory->details->top_left_read_state);
		directory->details->top_left_read_state = NULL;
	}

	/* Figure out which file to read the top left for. */
	file = select_needy_file (directory,
				  lacks_top_left,
				  wants_top_left);
	if (file == NULL) {
		return;
	}

	/* Start reading. */
	uri = nautilus_file_get_uri (file);
	directory->details->top_left_read_state = g_new0 (TopLeftTextReadState, 1);
	directory->details->top_left_read_state->file = file;
	gnome_vfs_async_open (&directory->details->top_left_read_state->handle,
			      uri,
			      GNOME_VFS_OPEN_READ,
			      top_left_open_callback,
			      directory);
	g_free (uri);
}

/* Call this when the monitor or call when ready list changes,
 * or when some I/O is completed.
 */
static void
state_changed (NautilusDirectory *directory)
{
	/* Check if any callbacks are satisifed and call them if they are. */
	call_ready_callbacks (directory);

	/* Start or stop reading the metafile. */
	if (is_anyone_waiting_for_metafile (directory)) {
		nautilus_directory_request_read_metafile (directory);
	}

	/* Start or stop reading files. */
	if (is_anyone_monitoring_file_list (directory)) {
		start_monitoring_file_list (directory);
	} else {
		nautilus_directory_stop_monitoring_file_list (directory);
	}

	/* Start or stop getting directory counts. */
	start_getting_directory_counts (directory);

	/* Start or stop getting top left pieces of files. */
	if(nautilus_directory_is_local(directory) || 
			nautilus_preferences_get_boolean(NAUTILUS_PREFERENCES_REMOTE_VIEWS, FALSE)) {
		start_getting_top_lefts (directory);
	}
}
