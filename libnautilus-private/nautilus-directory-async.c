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
#include "nautilus-file-private.h"
#include "nautilus-file-attributes.h"

#include <gtk/gtkmain.h>

#include <parser.h>
#include <xmlmemory.h>

#include "nautilus-string.h"
#include "nautilus-glib-extensions.h"

#define METAFILE_PERMISSIONS (GNOME_VFS_PERM_USER_READ | GNOME_VFS_PERM_USER_WRITE \
			      | GNOME_VFS_PERM_GROUP_READ | GNOME_VFS_PERM_GROUP_WRITE \
			      | GNOME_VFS_PERM_OTHER_READ | GNOME_VFS_PERM_OTHER_WRITE)

#define DIRECTORY_LOAD_ITEMS_PER_CALLBACK 32

struct MetafileReadState {
	GnomeVFSAsyncHandle *handle;
	gboolean is_open;
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

static int                           compare_queued_callbacks                            (gconstpointer                  a,
											  gconstpointer                  b);
static gboolean                      dequeue_pending_idle_callback                       (gpointer                       callback_data);
static void                          directory_load_callback                             (GnomeVFSAsyncHandle           *handle,
											  GnomeVFSResult                 result,
											  GnomeVFSDirectoryList         *list,
											  guint                          entries_read,
											  gpointer                       callback_data);
static void                          directory_load_done                                 (NautilusDirectory             *directory,
											  GnomeVFSResult                 result);
static void                          directory_load_one                                  (NautilusDirectory             *directory,
											  GnomeVFSFileInfo              *info);
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
static void                          metafile_write_callback                             (GnomeVFSAsyncHandle           *handle,
											  GnomeVFSResult                 result,
											  gconstpointer                  buffer,
											  GnomeVFSFileSize               bytes_requested,
											  GnomeVFSFileSize               bytes_read,
											  gpointer                       callback_data);
static void                          metafile_write_complete                             (NautilusDirectory             *directory);
static void                          metafile_write_failed                               (NautilusDirectory             *directory,
											  GnomeVFSResult                 result);
static GnomeVFSDirectoryListPosition nautilus_gnome_vfs_directory_list_get_next_position (GnomeVFSDirectoryList         *list,
											  GnomeVFSDirectoryListPosition  position);
static void                          process_pending_file_attribute_requests             (NautilusDirectory             *directory);

static void
empty_close_callback (GnomeVFSAsyncHandle *handle,
		      GnomeVFSResult result,
		      gpointer callback_data)
{
	/* Do nothing. */
}

static void
metafile_read_close (NautilusDirectory *directory)
{
	g_assert (directory->details->read_state->handle != NULL);
	if (directory->details->read_state->is_open) {
		gnome_vfs_async_close (directory->details->read_state->handle,
				       empty_close_callback,
				       directory);
		directory->details->read_state->is_open = TRUE;
	}
	directory->details->read_state->handle = NULL;
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

void
nautilus_metafile_read_cancel (NautilusDirectory *directory)
{
	if (directory->details->read_state == NULL) {
		return;
	}

	g_assert (directory->details->read_state->handle != NULL);
	gnome_vfs_async_cancel (directory->details->read_state->handle);
	metafile_read_close (directory);
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
	
	/* FIXME bugzilla.eazel.com 720: 
	 * The following assertion shouldn't be disabled, but
	 * it gets in the way when you set metadata before the
	 * metafile is completely read. Currently, the old metadata
	 * in the file will be lost. One way to test this is to
	 * remove the metafile from your home directory and the
	 * ~/Nautilus directory and then start the program.
	 */
	/* g_assert (directory->details->metafile == NULL); */
	
	/* The gnome-xml parser requires a zero-terminated array. */
	size = directory->details->read_state->bytes_read;
	buffer = g_realloc (directory->details->read_state->buffer, size + 1);
	buffer[size] = '\0';
	directory->details->metafile = xmlParseMemory (buffer, size);
	g_free (buffer);

	g_free (directory->details->read_state);

	directory->details->metafile_read = TRUE;
	directory->details->read_state = NULL;

	/* Let the callers that were waiting for the metafile know. */
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

	g_assert (bytes_requested == READ_CHUNK_SIZE);

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->read_state->handle == handle);
	g_assert ((char *) directory->details->read_state->buffer
		  + directory->details->read_state->bytes_read == buffer);

	if (result != GNOME_VFS_OK
	    && result != GNOME_VFS_ERROR_EOF) {
		metafile_read_close (directory);
		metafile_read_failed (directory, result);
		return;
	}

	directory->details->read_state->bytes_read += bytes_read;

	/* FIXME bugzilla.eazel.com 719: 
	 * Is the call allowed to return fewer bytes than I requested
	 * when I'm not at the end of the file? If so, then I need to fix this
	 * check. I don't want to stop until the end of the file.
	 */
	if (bytes_read == bytes_requested && result == GNOME_VFS_OK) {
		metafile_read_some (directory);
		return;
	}

	metafile_read_close (directory);
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

	directory->details->read_state->is_open = TRUE;
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

void
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
		nautilus_metafile_write_start (directory);
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
		nautilus_metafile_write_start (directory);
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

	g_assert (directory->details->write_state->handle != NULL);
	gnome_vfs_async_close (directory->details->write_state->handle,
			       empty_close_callback,
			       directory);
	directory->details->write_state->handle = NULL;

	if (result != GNOME_VFS_OK) {
		metafile_write_failed (directory, result);
		return;
	}

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

void
nautilus_metafile_write_start (NautilusDirectory *directory)
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
nautilus_directory_remove_file_monitor_link (NautilusDirectory *directory,
					     GList *link)
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
	nautilus_directory_remove_file_monitor_link
		(directory, find_file_monitor (directory, file, client));
}

gboolean
nautilus_directory_is_file_list_monitored (NautilusDirectory *directory) 
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

	was_monitoring_file_list = nautilus_directory_is_file_list_monitored (directory);
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
	 * nautilus_directory_stop_monitoring_file_list.
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
		 directory_load_callback,                         /* callback */
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
	g_return_if_fail (callback != NULL);

	nautilus_directory_file_monitor_add_internal (directory,
						      NULL,
						      client,
						      attributes,
						      metadata_keys,
						      callback,
						      callback_data);
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

	/* If we stopped monitoring, then throw away these. */
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
	nautilus_directory_schedule_dequeue_pending (directory);

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
	while ((p = nautilus_gnome_vfs_directory_list_get_next_position (list, p))
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
nautilus_directory_stop_monitoring_file_list (NautilusDirectory *directory)
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

	was_monitoring_file_list = nautilus_directory_is_file_list_monitored (directory);

	remove_file_monitor (directory, file, client);
	cancel_unneeded_file_attribute_requests (directory);

	if (was_monitoring_file_list && !nautilus_directory_is_file_list_monitored (directory)) {
		nautilus_directory_stop_monitoring_file_list (directory);
	}
}

void
nautilus_directory_file_monitor_remove (NautilusDirectory *directory,
					gconstpointer client)
{
	nautilus_directory_file_monitor_remove_internal (directory, NULL, client);
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
	}

	/* Send file-changed even if count failed, so interested parties can
	 * distinguish between unknowable and not-yet-known cases.
	 */
	nautilus_file_changed (directory->details->count_file);

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
