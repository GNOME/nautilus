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

#include "nautilus-directory-metafile.h"
#include "nautilus-directory-private.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-private.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-link.h"
#include "nautilus-string.h"
#include <gtk/gtkmain.h>
#include <parser.h>
#include <stdlib.h>
#include <xmlmemory.h>

#define METAFILE_PERMISSIONS (GNOME_VFS_PERM_USER_READ | GNOME_VFS_PERM_USER_WRITE \
			      | GNOME_VFS_PERM_GROUP_READ | GNOME_VFS_PERM_GROUP_WRITE \
			      | GNOME_VFS_PERM_OTHER_READ | GNOME_VFS_PERM_OTHER_WRITE)

#define DIRECTORY_LOAD_ITEMS_PER_CALLBACK 32

struct MetafileReadState {
	gboolean use_public_metafile;
	NautilusReadFileHandle *handle;
	GnomeVFSAsyncHandle *get_file_info_handle;
};

struct MetafileWriteState {
	gboolean use_public_metafile;
	GnomeVFSAsyncHandle *handle;
	xmlChar *buffer;
	int size;
	gboolean write_again;
};

struct TopLeftTextReadState {
	NautilusFile *file;
	NautilusReadFileHandle *handle;
};

struct ActivationURIReadState {
	NautilusFile *file;
	NautilusReadFileHandle *handle;
};

/* A request for information about one or more files. */
typedef struct {
	gboolean metafile;
	gboolean file_list; /* always FALSE if file != NULL */
	gboolean file_info;
	gboolean directory_count;
	gboolean deep_count;
	gboolean top_left_text;
	gboolean activation_uri;
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
static void     deep_count_load      (NautilusDirectory *directory,
				      const char        *uri);
static void 	metafile_read_start  (NautilusDirectory *directory);
static gboolean request_is_satisfied (NautilusDirectory *directory,
		      		      NautilusFile  	*file,
		      		      Request 		*request);

static void
cancel_directory_counts (NautilusDirectory *directory)
{
	if (directory->details->count_in_progress != NULL) {
		gnome_vfs_async_cancel (directory->details->count_in_progress);
		directory->details->count_file = NULL;
		directory->details->count_in_progress = NULL;
	}
}

static void
cancel_deep_count (NautilusDirectory *directory)
{
	if (directory->details->deep_count_in_progress != NULL) {
		g_assert (NAUTILUS_IS_FILE (directory->details->deep_count_file));

		gnome_vfs_async_cancel (directory->details->deep_count_in_progress);

		directory->details->deep_count_file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;

		directory->details->deep_count_file = NULL;
		directory->details->deep_count_in_progress = NULL;
		g_free (directory->details->deep_count_uri);
		directory->details->deep_count_uri = NULL;
		nautilus_g_list_free_deep (directory->details->deep_count_subdirectories);
		directory->details->deep_count_subdirectories = NULL;
	}
}

static void
cancel_top_left_read (NautilusDirectory *directory)
{
	if (directory->details->top_left_read_state != NULL) {
		nautilus_read_file_cancel (directory->details->top_left_read_state->handle);
		g_free (directory->details->top_left_read_state);
		directory->details->top_left_read_state = NULL;
	}
}

static void
cancel_get_activation_uri (NautilusDirectory *directory)
{
	if (directory->details->activation_uri_read_state != NULL) {
		nautilus_read_file_cancel (directory->details->activation_uri_read_state->handle);
		g_free (directory->details->activation_uri_read_state);
		directory->details->activation_uri_read_state = NULL;
	}
}

static void
cancel_get_info (NautilusDirectory *directory)
{
	if (directory->details->get_info_in_progress != NULL) {
		gnome_vfs_async_cancel (directory->details->get_info_in_progress);
		directory->details->get_info_file = NULL;
		directory->details->get_info_in_progress = NULL;
	}
}

static void
cancel_metafile_read (NautilusDirectory *directory)
{
	if (directory->details->metafile_read_state != NULL) {
		if (directory->details->metafile_read_state->handle != NULL) {
			nautilus_read_file_cancel (directory->details->metafile_read_state->handle);
		}
		if (directory->details->metafile_read_state->get_file_info_handle != NULL) {
			gnome_vfs_async_cancel (directory->details->metafile_read_state->get_file_info_handle);
		}
		g_free (directory->details->metafile_read_state);
		directory->details->metafile_read_state = NULL;
	}
}

void
nautilus_directory_cancel (NautilusDirectory *directory)
{
	cancel_deep_count (directory);
	cancel_directory_counts (directory);
	cancel_get_info (directory);
	cancel_metafile_read (directory);
	cancel_top_left_read (directory);
	cancel_get_activation_uri (directory);
}

static gboolean
can_use_public_metafile (NautilusDirectory *directory)
{
	NautilusSpeedTradeoffValue preference_value;
	
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);

	preference_value = nautilus_preferences_get_enum
		(NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA,
		 NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY);

	if (preference_value == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		return TRUE;
	}
	
	if (preference_value == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	}

	g_assert (preference_value == NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY);
	return nautilus_directory_is_local (directory);
}

static void
metafile_read_done (NautilusDirectory *directory)
{
	g_free (directory->details->metafile_read_state);
	directory->details->metafile_read_state = NULL;	

	directory->details->metafile_read = TRUE;

	/* Move over the changes to the metafile that were in the hash table. */
	nautilus_directory_metafile_apply_pending_changes (directory);

	/* Tell change-watchers that we have update information. */
	nautilus_directory_emit_metadata_changed (directory);

	/* Let the callers that were waiting for the metafile know. */
	nautilus_directory_async_state_changed (directory);
}

static void
metafile_read_try_public_metafile (NautilusDirectory *directory)
{
	directory->details->metafile_read_state->use_public_metafile = TRUE;
	metafile_read_start (directory);
}

static void
metafile_read_check_for_directory_callback (GnomeVFSAsyncHandle *handle,
					    GList *results,
					    gpointer callback_data)
{
	NautilusDirectory *directory;
	GnomeVFSGetFileInfoResult *result;

	directory = NAUTILUS_DIRECTORY (callback_data);

	g_assert (directory->details->metafile_read_state->get_file_info_handle == handle);
	g_assert (nautilus_g_list_exactly_one_item (results));

	directory->details->metafile_read_state->get_file_info_handle = NULL;

	result = results->data;

	if (result->result == GNOME_VFS_OK
	    && ((result->file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) != 0)
	    && result->file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		/* Is a directory. */
		metafile_read_try_public_metafile (directory);
	} else {
		/* Not a directory. */
		metafile_read_done (directory);
	}
}

static void
metafile_read_check_for_directory (NautilusDirectory *directory)
{
	GList fake_list;

	/* We have to do a get_info call to check if this a directory. */
	fake_list.data = directory->details->uri;
	fake_list.next = NULL;
	fake_list.prev = NULL;
	gnome_vfs_async_get_file_info
		(&directory->details->metafile_read_state->get_file_info_handle,
		 &fake_list,
		 GNOME_VFS_FILE_INFO_DEFAULT,
		 metafile_read_check_for_directory_callback,
		 directory);
}

static void
metafile_read_failed (NautilusDirectory *directory)
{
	NautilusFile *file;
	gboolean need_directory_check, is_directory;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (directory->details->metafile == NULL);

	directory->details->metafile_read_state->handle = NULL;

	if (!directory->details->metafile_read_state->use_public_metafile
	    && can_use_public_metafile (directory)) {
		/* The goal here is to read the real metafile, but
		 * only if the directory is actually a directory.
		 */

		/* First, check if we already know if it a directory. */
		file = nautilus_file_get (directory->details->uri_text);
		if (file == NULL || file->details->is_gone) {
			need_directory_check = FALSE;
			is_directory = FALSE;
		} else if (file->details->info == NULL) {
			need_directory_check = TRUE;
			is_directory = TRUE;
		} else {
			need_directory_check = FALSE;
			is_directory = nautilus_file_is_directory (file);
		}
		nautilus_file_unref (file);

		/* Do the directory check if we don't know. */
		if (need_directory_check) {
			metafile_read_check_for_directory (directory);
			return;
		}

		/* Try for the public metafile if it is a directory. */
		if (is_directory) {
			metafile_read_try_public_metafile (directory);
			return;
		}
	}

	metafile_read_done (directory);
}

static void
metafile_read_done_callback (GnomeVFSResult result,
			     GnomeVFSFileSize file_size,
			     char *file_contents,
			     gpointer callback_data)
{
	NautilusDirectory *directory;
	int size;
	char *buffer;

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->metafile == NULL);

	if (result != GNOME_VFS_OK) {
		g_assert (file_contents == NULL);
		metafile_read_failed (directory);
		return;
	}
	
	size = file_size;
	if ((GnomeVFSFileSize) size != file_size) {
		g_free (file_contents);
		metafile_read_failed (directory);
		return;
	}

	/* The gnome-xml parser requires a zero-terminated array. */
	buffer = g_realloc (file_contents, size + 1);
	buffer[size] = '\0';
	directory->details->metafile = xmlParseMemory (buffer, size);
	g_free (buffer);

	metafile_read_done (directory);
}

static void
metafile_read_start (NautilusDirectory *directory)
{
	char *text_uri;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	text_uri = gnome_vfs_uri_to_string
		(directory->details->metafile_read_state->use_public_metafile
		 ? directory->details->public_metafile_uri
		 : directory->details->private_metafile_uri,
		 GNOME_VFS_URI_HIDE_NONE);

	directory->details->metafile_read_state->handle = nautilus_read_entire_file_async
		(text_uri, metafile_read_done_callback, directory);

	g_free (text_uri);
}

static gboolean
allow_metafile (NautilusDirectory *directory)
{
	const char *scheme;

	/* Note that this inhibits both reading and writing metadata
	 * completely. In the future we may want to inhibit writing to
	 * the real directory while allowing parallel-directory
	 * metadata.
	 */

	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	/* For now, hard-code these schemes. Perhaps we should
	 * hardcode the schemes that are good for metadata instead of
	 * the schemes that are bad for it.
	 */
	/* FIXME: We need to handle this in a better way. Perhaps a
	 * better way can wait until we have support for metadata
	 * access inside gnome-vfs.
	 */
	scheme = gnome_vfs_uri_get_scheme (directory->details->uri);
	if (nautilus_strcasecmp (scheme, "info") == 0
	    || nautilus_strcasecmp (scheme, "help") == 0
	    || nautilus_strcasecmp (scheme, "man") == 0
	    || nautilus_strcasecmp (scheme, "pipe") == 0
	    || nautilus_strcasecmp (scheme, "search") == 0
	    || nautilus_strcasecmp (scheme, "gnome-search") == 0) {
		return FALSE;
	}
	
	return TRUE;
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

	if (!allow_metafile (directory)) {
		metafile_read_done (directory);
	} else {
		directory->details->metafile_read_state = g_new0 (MetafileReadState, 1);
		metafile_read_start (directory);
	}
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
	if (directory->details->metafile_write_state->use_public_metafile) {
		directory->details->metafile_write_state->use_public_metafile = FALSE;
		nautilus_metafile_write_start (directory);
		return;
	}

	metafile_write_done (directory);
}

static void
metafile_write_failure_close_callback (GnomeVFSAsyncHandle *handle,
				       GnomeVFSResult result,
				       gpointer callback_data)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (callback_data);

	metafile_write_failed (directory);
}

static void
metafile_write_success_close_callback (GnomeVFSAsyncHandle *handle,
				       GnomeVFSResult result,
				       gpointer callback_data)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->metafile_write_state->handle == NULL);

	if (result != GNOME_VFS_OK) {
		metafile_write_failed (directory);
		return;
	}

	/* Now that we have finished writing, it is time to delete the
	 * private file if we wrote the public one.
	 */
	if (directory->details->metafile_write_state->use_public_metafile) {
		/* A synchronous unlink is OK here because the private
		 * metafiles are local, so an unlink is very fast.
		 */
		gnome_vfs_unlink_from_uri (directory->details->private_metafile_uri);
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
			       result == GNOME_VFS_OK
			       ? metafile_write_success_close_callback
			       : metafile_write_failure_close_callback,
			       directory);
	directory->details->metafile_write_state->handle = NULL;
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
	gnome_vfs_async_create_uri
		(&directory->details->metafile_write_state->handle,
		 directory->details->metafile_write_state->use_public_metafile
		 ? directory->details->public_metafile_uri
		 : directory->details->private_metafile_uri,
		 GNOME_VFS_OPEN_WRITE, FALSE, METAFILE_PERMISSIONS,
		 metafile_write_create_callback, directory);
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
	directory->details->metafile_write_state->use_public_metafile
		= can_use_public_metafile (directory);
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

	if (!allow_metafile (directory)) {
		return;
	}

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
	memset (request, 0, sizeof (*request));

	request->directory_count = g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT,
		 nautilus_str_compare) != NULL;
	request->deep_count = g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS,
		 nautilus_str_compare) != NULL;
	request->top_left_text = g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_TOP_LEFT_TEXT,
		 nautilus_str_compare) != NULL;
	request->file_info = g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_FAST_MIME_TYPE,
		 nautilus_str_compare) != NULL;
	request->file_info |= g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_IS_DIRECTORY,
		 nautilus_str_compare) != NULL;
	if (g_list_find_custom (file_attributes,
				NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI,
				nautilus_str_compare) != NULL) {
		request->file_info = TRUE;
		request->activation_uri = TRUE;
	}

	/* FIXME:
	 * Some file attributes are really pieces of metadata.
	 * This is a confusing/broken design, since other metadata
	 * pieces are handled separately from file attributes. There
	 * are many ways this could be changed, ranging from making
	 * all metadata pieces have corresponding file attributes, to
	 * making a single file attribute that means "get all metadata",
	 * to making metadata keys be acceptable as file attributes
	 * directly (would need some funky char trick to prevent
	 * namespace collisions).
	 */
	if (!request->metafile) {
		request->metafile = g_list_find_custom
			(file_attributes,
			 NAUTILUS_FILE_ATTRIBUTE_CUSTOM_ICON,
			 nautilus_str_compare) != NULL;
	}
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

	/* Replace any current monitor for this client/file pair. */
	remove_monitor (directory, file, client);

	/* Add the new monitor. */
	monitor = g_new (Monitor, 1);
	monitor->file = file;
	monitor->client = client;
	set_up_request_by_file_attributes (&monitor->request, file_attributes);
	monitor->request.metafile = monitor_metadata;
	monitor->request.file_list = file == NULL;
	directory->details->monitor_list =
		g_list_prepend (directory->details->monitor_list, monitor);

	/* Tell the new monitor-er about the current set of
	 * files, which may or may not be all of them.
	 */
	if (directory->details->files != NULL && callback != NULL) {
		g_assert (file == NULL);
		(* callback) (directory,
			      directory->details->files,
			      callback_data);
	}

	/* Kick off I/O. */
	nautilus_directory_async_state_changed (directory);
}

int
nautilus_compare_file_with_name (gconstpointer a, gconstpointer b)
{
	return strcmp (NAUTILUS_FILE (a)->details->name,
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
	nautilus_file_update_info (NAUTILUS_FILE (list_entry->data), file_info);

	return TRUE;
}

static gboolean
dequeue_pending_idle_callback (gpointer callback_data)
{
	NautilusDirectory *directory;
	GList *pending_file_info;
	GList *p, *next;
	NautilusFile *file;
	GList *pending_files, *changed_files, *saw_again_files;
	GnomeVFSFileInfo *file_info;

	directory = NAUTILUS_DIRECTORY (callback_data);

	directory->details->dequeue_pending_idle_id = 0;

	pending_file_info = directory->details->pending_file_info;
	directory->details->pending_file_info = NULL;

	/* If we are no longer monitoring, then throw away these. */
	if (!nautilus_directory_is_file_list_monitored (directory)) {
		gnome_vfs_file_info_list_free (pending_file_info);
		nautilus_directory_async_state_changed (directory);
		return FALSE;
	}

	pending_files = NULL;
	changed_files = NULL;
	saw_again_files = NULL;

	/* Build a list of NautilusFile objects. */
	for (p = pending_file_info; p != NULL; p = p->next) {
		file_info = p->data;

		/* check if the file already exists */
		file = nautilus_directory_find_file (directory, file_info->name);
		if (file != NULL) {
			/* file already exists, check if it changed */
			file->details->unconfirmed = FALSE;
			if (nautilus_file_update_info (file, file_info)) {
				/* File changed, notify about the change. */
				nautilus_file_ref (file);
				changed_files = g_list_prepend (changed_files, file);
			}
			nautilus_file_ref (file);
			saw_again_files = g_list_prepend (saw_again_files, file);
		} else if (!update_file_info_in_list_if_needed (pending_files, file_info)) {
			/* new file, create a nautilus file object and add it to the list */
			file = nautilus_file_new_from_info (directory, file_info);
			pending_files = g_list_prepend (pending_files, file);
		}
	}
	gnome_vfs_file_info_list_free (pending_file_info);

	/* If we are done loading, then we assume that any unconfirmed
         * files are gone.
	 */
	if (directory->details->directory_loaded) {
		for (p = directory->details->files; p != NULL; p = next) {
			file = p->data;
			next = p->next;

			if (file->details->unconfirmed) {
				file->details->is_gone = TRUE;
				directory->details->files = g_list_remove_link
					(directory->details->files, p);
				g_list_free (p);

				if (!nautilus_directory_is_file_list_monitored (directory)) {
					nautilus_file_ref (file);
				}
				changed_files = g_list_prepend (changed_files, file);
			}
		}
	}

	/* Tell the objects that are monitoring about changed files.
	 * Send out added signals too, for the reload case.
	 */
	nautilus_directory_emit_files_added (directory, saw_again_files);
	nautilus_directory_emit_files_changed (directory, changed_files);
	nautilus_file_list_free (saw_again_files);
	nautilus_file_list_free (changed_files);
	
	/* Tell the objects that are monitoring about these new files. */
	nautilus_directory_emit_files_added (directory, pending_files);
	directory->details->files = g_list_concat
		(directory->details->files, pending_files);

	if (directory->details->directory_loaded && 
	    !directory->details->directory_loaded_sent_notification) {
		nautilus_directory_emit_done_loading (directory);
		directory->details->directory_loaded_sent_notification = TRUE;
	}

	nautilus_directory_async_state_changed (directory);
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
cancel_directory_load (NautilusDirectory *directory)
{
	if (directory->details->directory_load_in_progress != NULL) {
		gnome_vfs_async_cancel (directory->details->directory_load_in_progress);
		directory->details->directory_load_in_progress = NULL;
	}
}

static void
directory_load_done (NautilusDirectory *directory,
		     GnomeVFSResult result)
{
	cancel_directory_load (directory);
	directory->details->directory_loaded = TRUE;
	directory->details->directory_loaded_sent_notification = FALSE;


	/* Call the idle function right away. */
	if (directory->details->dequeue_pending_idle_id != 0) {
		gtk_idle_remove (directory->details->dequeue_pending_idle_id);
	}
	dequeue_pending_idle_callback (directory);
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

	nautilus_directory_async_state_changed (directory);
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
	set_up_request_by_file_attributes (&callback.request, file_attributes);
	callback.request.metafile = wait_for_metadata;
	callback.request.file_list = file == NULL && file_attributes != NULL;
	
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
		 g_memdup (&callback, sizeof (callback)));

	nautilus_directory_async_state_changed (directory);
}

gboolean      
nautilus_directory_check_if_ready_internal (NautilusDirectory *directory,
					    NautilusFile *file,
					    GList *file_attributes)
{
	Request request;

	g_assert (directory != NULL);

	set_up_request_by_file_attributes (&request, file_attributes);

	return request_is_satisfied (directory, file, &request);
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
		nautilus_directory_async_state_changed (directory);
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
		count_file->details->directory_count_failed = TRUE;
	} else {
		count_file->details->directory_count = entries_read;
		count_file->details->got_directory_count = TRUE;
	}
	directory->details->count_file = NULL;
	directory->details->count_in_progress = NULL;

	/* Send file-changed even if count failed, so interested parties can
	 * distinguish between unknowable and not-yet-known cases.
	 */
	nautilus_file_changed (count_file);

	/* Start up the next one. */
	nautilus_directory_async_state_changed (directory);
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

		if (result->result == GNOME_VFS_OK) {
			directory_load_one (directory, result->file_info);
		}
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
		 (GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		  | GNOME_VFS_FILE_INFO_FOLLOW_LINKS),
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

	/* Check if it's a file that's currently being worked on.
	 * If so, make that NULL so it gets canceled right away.
	 */
	if (directory->details->count_file == file) {
		directory->details->count_file = NULL;
		changed = TRUE;
	}
	if (directory->details->deep_count_file == file) {
		directory->details->deep_count_file = NULL;
		changed = TRUE;
	}
	if (directory->details->get_info_file == file) {
		directory->details->get_info_file = NULL;
		changed = TRUE;
	}
	if (directory->details->top_left_read_state != NULL
	    && directory->details->top_left_read_state->file == file) {
		directory->details->top_left_read_state->file = NULL;
		changed = TRUE;
	}
	if (directory->details->activation_uri_read_state != NULL
	    && directory->details->activation_uri_read_state->file == file) {
		directory->details->activation_uri_read_state->file = NULL;
		changed = TRUE;
	}

	/* Let the directory take care of the rest. */
	if (changed) {
		nautilus_directory_async_state_changed (directory);
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
	return nautilus_file_should_get_top_left_text (file)
		&& nautilus_file_contains_text (file)
		&& !file->details->got_top_left_text;
}

static gboolean
wants_top_left (const Request *request)
{
	return request->top_left_text;
}

static gboolean
lacks_info (NautilusFile *file)
{
	return file->details->info == NULL
		&& !file->details->is_gone
		&& !file->details->get_info_failed;
}

static gboolean
wants_info (const Request *request)
{
	return request->file_info;
}

static gboolean
lacks_deep_count (NautilusFile *file)
{
	return nautilus_file_is_directory (file)
		&& file->details->deep_counts_status != NAUTILUS_REQUEST_DONE;
}

static gboolean
wants_deep_count (const Request *request)
{
	return request->deep_count;
}

static gboolean
lacks_activation_uri (NautilusFile *file)
{
	return file->details->info != NULL
		&& !file->details->got_activation_uri;
}

static gboolean
wants_activation_uri (const Request *request)
{
	return request->activation_uri;
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
request_is_satisfied (NautilusDirectory *directory,
		      NautilusFile *file,
		      Request *request)
{
	if (request->metafile && !directory->details->metafile_read) {
		return FALSE;
	}

	if (request->file_list && !directory->details->directory_loaded) {
		return FALSE;
	}

	if (request->directory_count) {
		if (has_problem (directory, file, lacks_directory_count)) {
			return FALSE;
		}
	}

	if (request->file_info) {
		if (has_problem (directory, file, lacks_info)) {
			return FALSE;
		}
	}

	if (request->deep_count) {
		if (has_problem (directory, file, lacks_deep_count)) {
			return FALSE;
		}
	}

	if (request->activation_uri) {
		if (has_problem (directory, file, lacks_activation_uri)) {
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

			if (request_is_satisfied (directory, callback->file, &callback->request)) {
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
	Monitor *monitor;	

	for (p = directory->details->call_when_ready_list; p != NULL; p = p->next) {
		callback = p->data;
		if (callback->request.metafile) {
			return TRUE;
		}
	}

	for (p = directory->details->monitor_list; p != NULL; p = p->next) {
		monitor = p->data;
		if (monitor->request.metafile) {
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

static void
mark_all_files_unconfirmed (NautilusDirectory *directory)
{
	GList *p;
	NautilusFile *file;

	for (p = directory->details->files; p != NULL; p = p->next) {
		file = p->data;

		file->details->unconfirmed = TRUE;
	}
}

/* Start monitoring the file list if it isn't already. */
static void
start_monitoring_file_list (NautilusDirectory *directory)
{
	if (!directory->details->file_list_monitored) {
		g_assert (directory->details->directory_load_in_progress == NULL);
		directory->details->file_list_monitored = TRUE;
		nautilus_file_list_ref (directory->details->files);
	}

	if (directory->details->directory_loaded
	    || directory->details->directory_load_in_progress != NULL) {
		return;
	}

	mark_all_files_unconfirmed (directory);

	g_assert (directory->details->uri_text != NULL);
	directory->details->directory_load_list_last_handled
		= GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;
	
	gnome_vfs_async_load_directory
		(&directory->details->directory_load_in_progress, /* handle */
		 directory->details->uri_text,                    /* uri */
		 (GNOME_VFS_FILE_INFO_GET_MIME_TYPE	          /* options */
		  | GNOME_VFS_FILE_INFO_FOLLOW_LINKS),
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

	directory->details->file_list_monitored = FALSE;
	cancel_directory_load (directory);
	nautilus_file_list_unref (directory->details->files);
}

static NautilusFile *
get_corresponding_file (NautilusDirectory *directory)
{
	NautilusFile *file;
	
	file = directory->details->as_file;
	if (file != NULL) {
		nautilus_file_ref (file);
		return file;
	}

	return nautilus_file_get_existing (directory->details->uri_text);
}

void
nautilus_directory_force_reload (NautilusDirectory *directory)
{
	NautilusFile *file;
	NautilusDirectory *parent_directory;

	/* Start a new directory load. */
	cancel_directory_load (directory);
	directory->details->directory_loaded = FALSE;

	/* Start a new directory count. */
	file = get_corresponding_file (directory);
	if (file != NULL) {
		parent_directory = file->details->directory;

		if (parent_directory->details->count_file == file) {
			cancel_directory_counts (parent_directory);
		}
		if (parent_directory->details->deep_count_file == file) {
			cancel_deep_count (parent_directory);
		}

		file->details->got_directory_count = FALSE;
		file->details->directory_count_failed = FALSE;
		file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;

		if (parent_directory != directory) {
			nautilus_directory_async_state_changed (parent_directory);
		}

		nautilus_file_unref (file);
	}

	nautilus_directory_async_state_changed (directory);
}


static gboolean
is_needy (NautilusFile *file,
	  FileCheck check_missing,
	  RequestCheck check_wanted)
{
	NautilusDirectory *directory;
	GList *p;
	ReadyCallback *callback;
	Monitor *monitor;

	g_assert (NAUTILUS_IS_FILE (file));

	if (!(* check_missing) (file)) {
		return FALSE;
	}

	directory = file->details->directory;
	for (p = directory->details->call_when_ready_list;
	     p != NULL; p = p->next) {
		callback = p->data;
		if ((* check_wanted) (&callback->request)) {
			if (callback->file == file) {
				return TRUE;
			}
			if (callback->file == NULL
			    && file != directory->details->as_file) {
				return TRUE;
			}
		}
	}
	for (p = directory->details->monitor_list;
	     p != NULL; p = p->next) {
		monitor = p->data;
		if ((* check_wanted) (&monitor->request)) {
			if (monitor->file == file) {
				return TRUE;
			}
			if (monitor->file == NULL
			    && file != directory->details->as_file) {
				return TRUE;
			}
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
		    	for (p2 = directory->details->call_when_ready_list; p2 != NULL; p2 = p2->next) {
				callback = p2->data;
				if ((callback->file == NULL || callback->file == file)
				    && (* check_wanted) (&callback->request)) {
					break;
				}
		    	}
			if (p2 != NULL) {
				return file;
			}
			for (p2 = directory->details->monitor_list; p2 != NULL; p2 = p2->next) {
				monitor = p2->data;
				if ((monitor->file == NULL || monitor->file == file)
				    && (* check_wanted) (&monitor->request)) {
					break;
				}
			}
			if (p2 != NULL) {
				return file;
			}
		}
	}

	/* Finally, check the file for the directory itself. */
	file = directory->details->as_file;
	if (file != NULL) {
		if ((* check_missing) (file)) {
		    	for (p2 = directory->details->call_when_ready_list; p2 != NULL; p2 = p2->next) {
				callback = p2->data;
				if (callback->file == file
				    && (* check_wanted) (&callback->request)) {
					break;
				}
		    	}
			if (p2 != NULL) {
				return file;
			}
			for (p2 = directory->details->monitor_list; p2 != NULL; p2 = p2->next) {
				monitor = p2->data;
				if (monitor->file == file
				    && (* check_wanted) (&monitor->request)) {
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
		file = directory->details->count_file;
		if (file != NULL) {
			g_assert (NAUTILUS_IS_FILE (file));
			g_assert (file->details->directory == directory);
			if (is_needy (file,
				      lacks_directory_count,
				      wants_directory_count)) {
				return;
			}
		}

		/* The count is not wanted, so stop it. */
		cancel_directory_counts (directory);
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
deep_count_one (NautilusDirectory *directory,
		GnomeVFSFileInfo *info)
{
	NautilusFile *file;
	char *escaped_name, *uri;

	file = directory->details->deep_count_file;

	if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) != 0
	    && info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		/* Count the directory. */
		file->details->deep_directory_count += 1;

		/* Record the fact that we have to descend into this directory. */
		escaped_name = gnome_vfs_escape_string (info->name);
		uri = nautilus_make_path (directory->details->deep_count_uri, escaped_name);
		g_free (escaped_name);
		directory->details->deep_count_subdirectories = g_list_prepend
			(directory->details->deep_count_subdirectories, uri);
	} else {
		/* Even non-regular files count as files. */
		file->details->deep_file_count += 1;
	}

	/* Count the size. */
	if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) != 0) {
		file->details->deep_size += info->size;
	}
}

static void
deep_count_callback (GnomeVFSAsyncHandle *handle,
		     GnomeVFSResult result,
		     GnomeVFSDirectoryList *list,
		     guint entries_read,
		     gpointer callback_data)
{
	NautilusDirectory *directory;
	NautilusFile *file;
	GnomeVFSDirectoryListPosition last_handled, p;
	char *uri;
	gboolean done;

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->deep_count_in_progress == handle);
	file = directory->details->deep_count_file;
	g_assert (NAUTILUS_IS_FILE (file));

	/* We can't do this in the most straightforward way, becuse the position
	 * for a gnome_vfs_directory_list does not have a way of representing one
	 * past the end. So we must keep a position to the last item we handled
	 * rather than keeping a position past the last item we handled.
	 */
	last_handled = directory->details->deep_count_last_handled;
        p = last_handled;
	while ((p = directory_list_get_next_position (list, p))
	       != GNOME_VFS_DIRECTORY_LIST_POSITION_NONE) {
		deep_count_one (directory, gnome_vfs_directory_list_get (list, p));
		last_handled = p;
	}
	directory->details->deep_count_last_handled = last_handled;

	done = FALSE;
	if (result != GNOME_VFS_OK) {
		if (result != GNOME_VFS_ERROR_EOF) {
			file->details->deep_unreadable_count += 1;
		}
		
		directory->details->deep_count_in_progress = NULL;
		g_free (directory->details->deep_count_uri);
		directory->details->deep_count_uri = NULL;

		if (directory->details->deep_count_subdirectories != NULL) {
			/* Work on a new directory. */
			uri = directory->details->deep_count_subdirectories->data;
			directory->details->deep_count_subdirectories = g_list_remove
				(directory->details->deep_count_subdirectories, uri);
			deep_count_load (directory, uri);
			g_free (uri);
		} else {
			file->details->deep_counts_status = NAUTILUS_REQUEST_DONE;
			directory->details->deep_count_file = NULL;
			done = TRUE;
		}
	}

	nautilus_file_changed (file);

	if (done) {
		nautilus_directory_async_state_changed (directory);
	}
}

static void
deep_count_load (NautilusDirectory *directory, const char *uri)
{
	g_assert (directory->details->deep_count_uri == NULL);
	directory->details->deep_count_uri = g_strdup (uri);
	directory->details->deep_count_last_handled
		= GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;
	gnome_vfs_async_load_directory
		(&directory->details->deep_count_in_progress,
		 uri,
		 GNOME_VFS_FILE_INFO_DEFAULT,
		 NULL,
		 FALSE,
		 GNOME_VFS_DIRECTORY_FILTER_NONE,
		 (GNOME_VFS_DIRECTORY_FILTER_NOSELFDIR
		  | GNOME_VFS_DIRECTORY_FILTER_NOPARENTDIR),
		 NULL,
		 G_MAXINT,
		 deep_count_callback,
		 directory);
}

static void
deep_count_start (NautilusDirectory *directory)
{
	NautilusFile *file;
	char *uri;

	/* If there's already a count in progress, check to be sure
	 * it's still wanted.
	 */
	if (directory->details->deep_count_in_progress != NULL) {
		file = directory->details->deep_count_file;
		if (file != NULL) {
			g_assert (NAUTILUS_IS_FILE (file));
			g_assert (file->details->directory == directory);
			if (is_needy (file,
				      lacks_deep_count,
				      wants_deep_count)) {
				return;
			}
		}

		/* The count is not wanted, so stop it. */
		cancel_deep_count (directory);
	}

	/* Figure out which file to get a count for. */
	file = select_needy_file (directory,
				  lacks_deep_count,
				  wants_deep_count);
	if (file == NULL) {
		return;
	}

	/* Start counting. */
	file->details->deep_counts_status = NAUTILUS_REQUEST_IN_PROGRESS;
	file->details->deep_directory_count = 0;
	file->details->deep_file_count = 0;
	file->details->deep_unreadable_count = 0;
	file->details->deep_size = 0;
	directory->details->deep_count_file = file;
	uri = nautilus_file_get_uri (file);
	deep_count_load (directory, uri);
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
	g_assert (directory->details->top_left_read_state->handle == NULL);
	g_assert (NAUTILUS_IS_FILE (directory->details->top_left_read_state->file));

	directory->details->top_left_read_state->file->details->got_top_left_text = TRUE;

	g_free (directory->details->top_left_read_state);
	directory->details->top_left_read_state = NULL;

	nautilus_directory_async_state_changed (directory);
}

static void
top_left_read_callback (GnomeVFSResult result,
			GnomeVFSFileSize bytes_read,
			char *file_contents,
			gpointer callback_data)
{
	NautilusDirectory *directory;

	directory = NAUTILUS_DIRECTORY (callback_data);

	directory->details->top_left_read_state->handle = NULL;

	if (result == GNOME_VFS_OK) {
		g_free (directory->details->top_left_read_state->file->details->top_left_text);
		directory->details->top_left_read_state->file->details->top_left_text =
			nautilus_extract_top_left_text (file_contents, bytes_read);
		
		nautilus_file_changed (directory->details->top_left_read_state->file);
		
		g_free (file_contents);
	}
	
	top_left_read_done (directory);
}

static gboolean
top_left_read_more_callback (GnomeVFSFileSize bytes_read,
			     const char *file_contents,
			     gpointer callback_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (callback_data));

	/* Stop reading when we have enough. */
	return bytes_read < NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_BYTES
		&& count_lines (file_contents, bytes_read) <= NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_LINES;
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
		file = directory->details->top_left_read_state->file;
		if (file != NULL) {
			g_assert (NAUTILUS_IS_FILE (file));
			g_assert (file->details->directory == directory);
			if (is_needy (file,
				      lacks_top_left,
				      wants_top_left)) {
				return;
			}
		}

		/* The top left is not wanted, so stop it. */
		cancel_top_left_read (directory);
	}

	/* Figure out which file to read the top left for. */
	file = select_needy_file (directory,
				  lacks_top_left,
				  wants_top_left);
	if (file == NULL) {
		return;
	}

	/* Start reading. */
	directory->details->top_left_read_state = g_new0 (TopLeftTextReadState, 1);
	directory->details->top_left_read_state->file = file;
	uri = nautilus_file_get_uri (file);
	directory->details->top_left_read_state->handle = nautilus_read_file_async
		(uri,
		 top_left_read_callback,
		 top_left_read_more_callback,
		 directory);
	g_free (uri);
}

static void
get_info_callback (GnomeVFSAsyncHandle *handle,
		   GList *results,
		   gpointer callback_data)
{
	NautilusDirectory *directory;
	NautilusFile *get_info_file;
	GnomeVFSGetFileInfoResult *result;

	directory = NAUTILUS_DIRECTORY (callback_data);

	g_assert (handle == NULL || handle == directory->details->get_info_in_progress);
	g_assert (nautilus_g_list_exactly_one_item (results));
	get_info_file = directory->details->get_info_file;
	g_assert (NAUTILUS_IS_FILE (get_info_file));
	
	directory->details->get_info_file = NULL;
	directory->details->get_info_in_progress = NULL;
	
	result = results->data;
	if (result->result != GNOME_VFS_OK) {
		get_info_file->details->get_info_failed = TRUE;
	} else {
		nautilus_file_update_info (get_info_file, result->file_info);
		nautilus_file_changed (get_info_file);
	}

	nautilus_directory_async_state_changed (directory);
}

static void
start_getting_file_info (NautilusDirectory *directory)
{
	NautilusFile *file;
	char *uri;
	GnomeVFSURI *vfs_uri;
	GList fake_list;

	/* If there's already a file info fetch in progress, check to
	 * be sure it's still wanted.
	 */
	if (directory->details->get_info_in_progress != NULL) {
		file = directory->details->get_info_file;
		if (file != NULL) {
			g_assert (NAUTILUS_IS_FILE (file));
			g_assert (file->details->directory == directory);
			if (is_needy (file, lacks_info, wants_info)) {
				return;
			}
		}

		/* The info is not wanted, so stop it. */
		cancel_get_info (directory);
	}

	/* Figure out which file to get file info for. */
	file = select_needy_file (directory, lacks_info, wants_info);
	if (file == NULL) {
		return;
	}

	directory->details->get_info_file = file;
	uri = nautilus_file_get_uri (file);
	vfs_uri = gnome_vfs_uri_new (uri);
	g_free (uri);
	fake_list.data = vfs_uri;
	fake_list.prev = NULL;
	fake_list.next = NULL;
	gnome_vfs_async_get_file_info
		(&directory->details->get_info_in_progress,
		 &fake_list,
		 (GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		  | GNOME_VFS_FILE_INFO_FOLLOW_LINKS),
		 get_info_callback,
		 directory);
	gnome_vfs_uri_unref (vfs_uri);
}

static void
activation_uri_done (NautilusDirectory *directory,
		     NautilusFile *file,
		     const char *uri)
{
	file->details->got_activation_uri = TRUE;
	g_free (file->details->activation_uri);
	file->details->activation_uri = g_strdup (uri);

	nautilus_directory_async_state_changed (directory);
}

static void
activation_uri_read_done (NautilusDirectory *directory,
			  const char *uri)
{
	NautilusFile *file;

	file = directory->details->activation_uri_read_state->file;

	g_free (directory->details->activation_uri_read_state);
	directory->details->activation_uri_read_state = NULL;

	activation_uri_done (directory, file, uri);
}

static void
activation_uri_nautilus_link_read_callback (GnomeVFSResult result,
					    GnomeVFSFileSize bytes_read,
					    char *file_contents,
					    gpointer callback_data)
{
	NautilusDirectory *directory;
	char *buffer, *uri;

	directory = NAUTILUS_DIRECTORY (callback_data);

	/* Handle the case where we read the Nautilus link. */
	if (result != GNOME_VFS_OK) {
		/* FIXME: We should report this error to the user. */
		g_free (file_contents);
		uri = NULL;
	} else {
		/* The gnome-xml parser requires a zero-terminated array. */
		buffer = g_realloc (file_contents, bytes_read + 1);
		buffer[bytes_read] = '\0';
		uri = nautilus_link_get_link_uri_given_file_contents (buffer, bytes_read);
		g_free (buffer);
	}

	activation_uri_read_done (directory, uri);
	g_free (uri);
}

static void
activation_uri_gmc_link_read_callback (GnomeVFSResult result,
				       GnomeVFSFileSize bytes_read,
				       char *file_contents,
				       gpointer callback_data)
{
	NautilusDirectory *directory;
	char *end_of_line, *uri;

	directory = NAUTILUS_DIRECTORY (callback_data);

	/* Handle the case where we read the GMC link. */
	if (result != GNOME_VFS_OK || !nautilus_str_has_prefix (file_contents, "URL: ")) {
		/* FIXME: We should report this error to the user. */
		uri = NULL;
	} else {
		/* Make sure we don't run off the end of the buffer. */
		end_of_line = memchr (file_contents, '\n', bytes_read);
		if (end_of_line != NULL) {
			uri = g_strndup (file_contents, end_of_line - file_contents);
		} else {
			uri = g_strndup (file_contents, bytes_read);
		}
	}

	g_free (file_contents);
	activation_uri_read_done (directory, uri);
	g_free (uri);
}

static gboolean
activation_uri_gmc_link_read_more_callback (GnomeVFSFileSize bytes_read,
					    const char *file_contents,
					    gpointer callback_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (callback_data));

	/* We need the first 512 bytes to see if something is a gmc link. */
	return bytes_read < 512;
}

static void
start_getting_activation_uris (NautilusDirectory *directory)
{
	NautilusFile *file;
	char *mime_type, *uri;
	gboolean gmc_style_link, nautilus_style_link;

	/* If there's already a activation URI read in progress, check
	 * to be sure it's still wanted.
	 */
	if (directory->details->activation_uri_read_state != NULL) {
		file = directory->details->activation_uri_read_state->file;
		if (file != NULL) {
			g_assert (NAUTILUS_IS_FILE (file));
			g_assert (file->details->directory == directory);
			if (is_needy (file, lacks_info, wants_info)) {
				return;
			}
		}

		/* The count is not wanted, so stop it. */
		cancel_get_activation_uri (directory);
	}

	/* Figure out which file to get activation_uri for. */
	file = select_needy_file (directory,
				  lacks_activation_uri,
				  wants_activation_uri);
	if (file == NULL) {
		return;
	}

	/* Figure out if it is a link. */
	mime_type = nautilus_file_get_mime_type (file);
	gmc_style_link = nautilus_strcasecmp (mime_type, "application/x-gmc-link") == 0;
	g_free (mime_type);
	nautilus_style_link = nautilus_link_is_link_file (file);

	/* If it's not a link we are done. If it is, we need to read it. */
	if (!(gmc_style_link || nautilus_style_link)) {
		activation_uri_done (directory, file, NULL);
	} else {
		directory->details->activation_uri_read_state = g_new0 (ActivationURIReadState, 1);
		directory->details->activation_uri_read_state->file = file;
		uri = nautilus_file_get_uri (file);
		if (gmc_style_link) {
			directory->details->activation_uri_read_state->handle = nautilus_read_file_async
				(uri,
				 activation_uri_gmc_link_read_callback,
				 activation_uri_gmc_link_read_more_callback,
				 directory);
		} else {
			directory->details->activation_uri_read_state->handle = nautilus_read_entire_file_async
				(uri,
				 activation_uri_nautilus_link_read_callback,
				 directory);
		}
		g_free (uri);

	}
}

/* Call this when the monitor or call when ready list changes,
 * or when some I/O is completed.
 */
void
nautilus_directory_async_state_changed (NautilusDirectory *directory)
{
	/* Check if any callbacks are satisifed and call them if they are. */
	call_ready_callbacks (directory);

	/* Start or stop getting file info. */
	start_getting_file_info (directory);

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
	deep_count_start (directory);

	/* Start or stop getting top left pieces of files. */
	start_getting_top_lefts (directory);

	start_getting_activation_uris (directory);
}
