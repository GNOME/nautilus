/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory-async.c: Nautilus directory model state machine.
 
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

#include "nautilus-metafile.h"
#include "nautilus-directory-metafile.h"
#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-private.h"
#include <eel/eel-glib-extensions.h>
#include "nautilus-global-preferences.h"
#include "nautilus-link.h"
#include "nautilus-search-uri.h"
#include <eel/eel-string.h>
#include <ctype.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libgnome/gnome-metadata.h>
#include <libgnome/gnome-mime-info.h>
#include <gtk/gtkmain.h>
#include <stdlib.h>
#include <stdio.h>

/* turn this on to see messages about each load_directory call: */
#if 0
#define DEBUG_LOAD_DIRECTORY
#endif

/* turn this on to check if async. job calls are balanced */
#if 0
#define DEBUG_ASYNC_JOBS
#endif

/* turn this on to log things starting and stopping */
#if 0
#define DEBUG_START_STOP
#endif


#define DIRECTORY_LOAD_ITEMS_PER_CALLBACK 100

/* Keep async. jobs down to this number for all directories. */
#define MAX_ASYNC_JOBS 10

struct TopLeftTextReadState {
	NautilusFile *file;
	EelReadFileHandle *handle;
};

struct LinkInfoReadState {
	NautilusFile *file;
	EelReadFileHandle *handle;
};

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
	gboolean monitor_hidden_files; /* defines whether "all" includes hidden files */
	gboolean monitor_backup_files; /* defines whether "all" includes backup files */
	gconstpointer client;
	Request request;
} Monitor;

typedef gboolean (* RequestCheck) (const Request *);
typedef gboolean (* FileCheck) (NautilusFile *);

/* Current number of async. jobs. */
static int async_job_count;
static GHashTable *waiting_directories;
#ifdef DEBUG_ASYNC_JOBS
static GHashTable *async_jobs;
#endif

/* Forward declarations for functions that need them. */
static void     deep_count_load             (NautilusDirectory *directory,
					     const char        *uri);
static gboolean request_is_satisfied        (NautilusDirectory *directory,
					     NautilusFile      *file,
					     Request           *request);
static void     cancel_loading_attributes   (NautilusDirectory *directory,
					     GList             *file_attributes);
static void     add_all_files_to_work_queue (NautilusDirectory *directory);
static void     link_info_done              (NautilusDirectory *directory,
					     NautilusFile      *file,
					     const char        *uri,
					     const char        *name, 
					     const char        *icon);
static gboolean file_needs_high_priority_work_done (NautilusDirectory *directory,
						    NautilusFile      *file);
static gboolean file_needs_low_priority_work_done  (NautilusDirectory *directory,
						    NautilusFile      *file);
static void     move_file_to_low_priority_queue    (NautilusDirectory *directory,
						    NautilusFile      *file);



/* Some helpers for case-insensitive strings.
 * Move to nautilus-glib-extensions?
 */

static gboolean
istr_equal (gconstpointer v, gconstpointer v2)
{
	return g_strcasecmp (v, v2) == 0;
}

static guint
istr_hash (gconstpointer key)
{
	const char *p;
	guint h;

	h = 0;
	for (p = key; *p != '\0'; p++) {
		h = (h << 5) - h + tolower ((guchar) *p);
	}
	
	return h;
}

static GHashTable *
istr_set_new (void)
{
	return g_hash_table_new (istr_hash, istr_equal);
}

static void
istr_set_insert (GHashTable *table, const char *istr)
{
	char *key;

	if (g_hash_table_lookup (table, istr) == NULL) {
		key = g_strdup (istr);
		g_hash_table_insert (table, key, key);
	}
}

static void
add_istr_to_list (gpointer key, gpointer value, gpointer callback_data)
{
	GList **list;

	list = callback_data;
	*list = g_list_prepend (*list, g_strdup (key));
}

static GList *
istr_set_get_as_list (GHashTable *table)
{
	GList *list;

	list = NULL;
	g_hash_table_foreach (table, add_istr_to_list, &list);
	return list;
}

static void
istr_set_destroy (GHashTable *table)
{
	eel_g_hash_table_destroy_deep (table);
}

/* Start a job. This is really just a way of limiting the number of
 * async. requests that we issue at any given time. Without this, the
 * number of requests is unbounded.
 */
static gboolean
async_job_start (NautilusDirectory *directory,
		 const char *job)
{
#ifdef DEBUG_ASYNC_JOBS
	char *key;
#endif

#ifdef DEBUG_START_STOP
	g_message ("starting %s in %s", job, directory->details->uri);
#endif

	g_assert (async_job_count >= 0);
	g_assert (async_job_count <= MAX_ASYNC_JOBS);

	if (async_job_count >= MAX_ASYNC_JOBS) {
		if (waiting_directories == NULL) {
			waiting_directories = eel_g_hash_table_new_free_at_exit
				(NULL, NULL,
				 "nautilus-directory-async.c: waiting_directories");
		}

		g_hash_table_insert (waiting_directories,
				     directory,
				     directory);
		
		return FALSE;
	}

#ifdef DEBUG_ASYNC_JOBS
	if (async_jobs == NULL) {
		async_jobs = eel_g_hash_table_new_free_at_exit
			(g_str_hash, g_str_equal,
			 "nautilus-directory-async.c: async_jobs");
	}
	key = g_strconcat (directory->details->uri, ": ", job, NULL);
	if (g_hash_table_lookup (async_jobs, key) != NULL) {
		g_warning ("same job twice: %s in %s",
			   job,
			   directory->details->uri);
	}
	g_hash_table_insert (async_jobs, key, directory);
#endif	

	async_job_count += 1;
	return TRUE;
}

/* End a job. */
static void
async_job_end (NautilusDirectory *directory,
	       const char *job)
{
#ifdef DEBUG_ASYNC_JOBS
	char *key;
	gpointer table_key, value;
#endif

#ifdef DEBUG_START_STOP
	g_message ("stopping %s in %s", job, directory->details->uri);
#endif

	g_assert (async_job_count > 0);

#ifdef DEBUG_ASYNC_JOBS
	g_assert (async_jobs != NULL);
	key = g_strconcat (directory->details->uri, ": ", job, NULL);
	if (!g_hash_table_lookup_extended (async_jobs, key, &table_key, &value)) {
		g_warning ("ending job we didn't start: %s in %s",
			   job,
			   directory->details->uri);
	} else {
		g_hash_table_remove (async_jobs, key);
		g_free (table_key);
	}
	g_free (key);
#endif

	async_job_count -= 1;
}

/* Helper to get one value from a hash table. */
static void
get_one_value_callback (gpointer key, gpointer value, gpointer callback_data)
{
	gpointer *returned_value;

	returned_value = callback_data;
	*returned_value = value;
}

/* return a single value from a hash table. */
static gpointer
get_one_value (GHashTable *table)
{
	gpointer value;

	value = NULL;
	if (table != NULL) {
		g_hash_table_foreach (table, get_one_value_callback, &value);
	}
	return value;
}

/* Wake up directories that are "blocked" as long as there are job
 * slots available.
 */
static void
async_job_wake_up (void)
{
	static gboolean already_waking_up = FALSE;
	gpointer value;

	g_assert (async_job_count >= 0);
	g_assert (async_job_count <= MAX_ASYNC_JOBS);

	if (already_waking_up) {
		return;
	}
	
	already_waking_up = TRUE;
	while (async_job_count < MAX_ASYNC_JOBS) {
		value = get_one_value (waiting_directories);
		if (value == NULL) {
			break;
		}
		g_hash_table_remove (waiting_directories, value);
		nautilus_directory_async_state_changed
			(NAUTILUS_DIRECTORY (value));
	}
	already_waking_up = FALSE;
}

static void
directory_count_cancel (NautilusDirectory *directory)
{
	if (directory->details->count_in_progress != NULL) {
		gnome_vfs_async_cancel (directory->details->count_in_progress);
		directory->details->count_file = NULL;
		directory->details->count_in_progress = NULL;

		async_job_end (directory, "directory count");
	}
}

static void
deep_count_cancel (NautilusDirectory *directory)
{
	if (directory->details->deep_count_in_progress != NULL) {
		g_assert (NAUTILUS_IS_FILE (directory->details->deep_count_file));

		gnome_vfs_async_cancel (directory->details->deep_count_in_progress);

		directory->details->deep_count_file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;

		directory->details->deep_count_file = NULL;
		directory->details->deep_count_in_progress = NULL;
		g_free (directory->details->deep_count_uri);
		directory->details->deep_count_uri = NULL;
		eel_g_list_free_deep (directory->details->deep_count_subdirectories);
		directory->details->deep_count_subdirectories = NULL;

		async_job_end (directory, "deep count");
	}
}

static void
mime_list_cancel (NautilusDirectory *directory)
{
	if (directory->details->mime_list_in_progress != NULL) {
		g_assert (NAUTILUS_IS_FILE (directory->details->mime_list_file));

		gnome_vfs_async_cancel (directory->details->mime_list_in_progress);
		istr_set_destroy (directory->details->mime_list_hash);

		directory->details->mime_list_file = NULL;
		directory->details->mime_list_in_progress = NULL;
		directory->details->mime_list_hash = NULL;

		async_job_end (directory, "MIME list");
	}
}

static void
top_left_cancel (NautilusDirectory *directory)
{
	if (directory->details->top_left_read_state != NULL) {
		eel_read_file_cancel (directory->details->top_left_read_state->handle);
		g_free (directory->details->top_left_read_state);
		directory->details->top_left_read_state = NULL;

		async_job_end (directory, "top left");
	}
}

static void
link_info_cancel (NautilusDirectory *directory)
{
	if (directory->details->link_info_read_state != NULL) {
		eel_read_file_cancel (directory->details->link_info_read_state->handle);
		g_free (directory->details->link_info_read_state);
		directory->details->link_info_read_state = NULL;
		async_job_end (directory, "link info");
	}
}

static void
file_info_cancel (NautilusDirectory *directory)
{
	if (directory->details->get_info_in_progress != NULL) {
		gnome_vfs_async_cancel (directory->details->get_info_in_progress);
		directory->details->get_info_file = NULL;
		directory->details->get_info_in_progress = NULL;

		async_job_end (directory, "file info");
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
	Monitor monitor;

	monitor.client = client;
	monitor.file = file;

	return g_list_find_custom (directory->details->monitor_list,
				   &monitor,
				   monitor_key_compare);
}

static int
monitor_file_compare (gconstpointer a,
		      gconstpointer data)
{
	const Monitor *monitor;
	NautilusFile *file;

	monitor = a;
	file = (NautilusFile *) data;
	
	if (monitor->file < file) {
		return -1;
	}
	if (monitor->file > file) {
		return +1;
	}
	
	return 0;
}

static gboolean
find_any_monitor (NautilusDirectory *directory,
		  NautilusFile *file)
{
	return g_list_find_custom (directory->details->monitor_list,
				   file,
				   monitor_file_compare) != NULL;
}

static void
remove_monitor_link (NautilusDirectory *directory,
		     GList *link)
{
	if (link != NULL) {
		directory->details->monitor_list =
			g_list_remove_link (directory->details->monitor_list, link);
		g_free (link->data);
		g_list_free_1 (link);
	}
}

static void
remove_monitor (NautilusDirectory *directory,
		NautilusFile *file,
		gconstpointer client)
{
	remove_monitor_link (directory, find_monitor (directory, file, client));
}

void
nautilus_directory_set_up_request (Request *request,
				   GList *file_attributes)
{
	memset (request, 0, sizeof (*request));

	request->directory_count = g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT,
		 eel_strcmp_compare_func) != NULL;
	request->deep_count = g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS,
		 eel_strcmp_compare_func) != NULL;
	request->mime_list = g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES,
		 eel_strcmp_compare_func) != NULL;
	request->file_info = g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_MIME_TYPE,
		 eel_strcmp_compare_func) != NULL;
	request->file_info |= g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_IS_DIRECTORY,
		 eel_strcmp_compare_func) != NULL;
	request->file_info |= g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_CAPABILITIES,
		 eel_strcmp_compare_func) != NULL;
	request->file_info |= g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_FILE_TYPE,
		 eel_strcmp_compare_func) != NULL;
	
	if (g_list_find_custom (file_attributes,
				NAUTILUS_FILE_ATTRIBUTE_TOP_LEFT_TEXT,
				eel_strcmp_compare_func) != NULL) {
		request->top_left_text = TRUE;
		request->file_info = TRUE;
	}
	
	if (g_list_find_custom (file_attributes,
				NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI,
				eel_strcmp_compare_func) != NULL) {
		request->file_info = TRUE;
		request->link_info = TRUE;
	}

	if (g_list_find_custom (file_attributes,
				NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME,
				eel_strcmp_compare_func) != NULL) {
		request->file_info = TRUE;
		request->link_info = TRUE;
	}

	/* FIXME bugzilla.gnome.org 42435:
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
	if (g_list_find_custom (file_attributes,
				NAUTILUS_FILE_ATTRIBUTE_CUSTOM_ICON,
				eel_strcmp_compare_func) != NULL) {
		request->metafile = TRUE;
		request->file_info = TRUE;
		request->link_info = TRUE;
	}
	
	request->metafile |= g_list_find_custom
		(file_attributes,
		 NAUTILUS_FILE_ATTRIBUTE_METADATA,
		 eel_strcmp_compare_func) != NULL;

}

void
nautilus_directory_monitor_add_internal (NautilusDirectory *directory,
					 NautilusFile *file,
					 gconstpointer client,
					 gboolean monitor_hidden_files,
					 gboolean monitor_backup_files,
					 GList *file_attributes,
					 NautilusDirectoryCallback callback,
					 gpointer callback_data)
{
	Monitor *monitor;
	GList *file_list;
	char *file_uri;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	/* Replace any current monitor for this client/file pair. */
	remove_monitor (directory, file, client);

	/* Add the new monitor. */
	monitor = g_new (Monitor, 1);
	monitor->file = file;
	monitor->monitor_hidden_files = monitor_hidden_files;
	monitor->monitor_backup_files = monitor_backup_files;
	monitor->client = client;
	nautilus_directory_set_up_request (&monitor->request, file_attributes);

	monitor->request.file_list = file == NULL;
	directory->details->monitor_list =
		g_list_prepend (directory->details->monitor_list, monitor);

	if (callback != NULL) {
		file_list = nautilus_directory_get_file_list (directory);
		(* callback) (directory, file_list, callback_data);
		nautilus_file_list_free (file_list);
	}
	
	/* Start the "real" monitoring (FAM or whatever). */
	if (file == NULL) {
		if (directory->details->monitor == NULL) {
			directory->details->monitor = nautilus_monitor_directory (directory->details->uri);
		}
	} else {
		if (file->details->monitor == NULL) {
			file_uri = nautilus_file_get_uri (file);
			file->details->monitor = nautilus_monitor_file (file_uri);
			g_free (file_uri);
		}
	}
	
	/* We could just call update_metadata_monitors here, but we can be smarter
	 * since we know what monitor was just added.
	 */
	if (monitor->request.metafile && directory->details->metafile_monitor == NULL) {
		nautilus_directory_register_metadata_monitor (directory);	
	}

	/* Put the monitor file or all the files on the work queue. */
	if (file != NULL) {
		nautilus_directory_add_file_to_work_queue (directory, file);
	} else {
		add_all_files_to_work_queue (directory);
	}

	/* Kick off I/O. */
	nautilus_directory_async_state_changed (directory);
}

static void
set_file_unconfirmed (NautilusFile *file, gboolean unconfirmed)
{
	NautilusDirectory *directory;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (unconfirmed == FALSE || unconfirmed == TRUE);

	if (file->details->unconfirmed == unconfirmed) {
		return;
	}
	file->details->unconfirmed = unconfirmed;

	directory = file->details->directory;
	if (unconfirmed) {
		directory->details->confirmed_file_count--;
	} else {
		directory->details->confirmed_file_count++;
	}
}

static gboolean show_hidden_files = TRUE;
static gboolean show_backup_files = TRUE;

static void
show_hidden_files_changed_callback (gpointer callback_data)
{
	show_hidden_files = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);
}

static void
show_backup_files_changed_callback (gpointer callback_data)
{
	show_backup_files = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES);
}

static GnomeVFSDirectoryFilterOptions
get_filter_options_for_directory_count (void)
{
	static gboolean show_hidden_files_changed_callback_installed = FALSE;
	static gboolean show_backup_files_changed_callback_installed = FALSE;
	GnomeVFSDirectoryFilterOptions filter_options;
	
	filter_options = GNOME_VFS_DIRECTORY_FILTER_NOSELFDIR
		| GNOME_VFS_DIRECTORY_FILTER_NOPARENTDIR;

	/* Add the callback once for the life of our process */
	if (!show_hidden_files_changed_callback_installed) {
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					      show_hidden_files_changed_callback,
					      NULL);
		show_hidden_files_changed_callback_installed = TRUE;
		
		/* Peek for the first time */
		show_hidden_files_changed_callback (NULL);
	}

	/* Add the callback once for the life of our process */
	if (!show_backup_files_changed_callback_installed) {
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
					      show_backup_files_changed_callback,
					      NULL);
		show_backup_files_changed_callback_installed = TRUE;
		
		/* Peek for the first time */
		show_backup_files_changed_callback (NULL);
	}
	
	if (!show_hidden_files) {
		filter_options |= GNOME_VFS_DIRECTORY_FILTER_NODOTFILES;
	}
	if (!show_backup_files) {
		filter_options |= GNOME_VFS_DIRECTORY_FILTER_NOBACKUPFILES;
	}

	return filter_options;
}

static void
load_directory_state_destroy (NautilusDirectory *directory)
{
	NautilusFile *file;

	if (directory->details->load_mime_list_hash != NULL) {
		istr_set_destroy (directory->details->load_mime_list_hash);
		directory->details->load_mime_list_hash = NULL;
	}

	file = directory->details->load_directory_file;
	if (file != NULL) {
		directory->details->load_directory_file = NULL;

		file->details->loading_directory = FALSE;
		if (file->details->directory != directory) {
			nautilus_directory_async_state_changed (file->details->directory);
		}
		
		nautilus_file_unref (file);
	}

	gnome_vfs_directory_filter_destroy (directory->details->load_file_count_filter);
	directory->details->load_file_count_filter = NULL;
}

static void
load_directory_done (NautilusDirectory *directory)
{
	load_directory_state_destroy (directory);
	nautilus_directory_async_state_changed (directory);
}

static gboolean
dequeue_pending_idle_callback (gpointer callback_data)
{
	NautilusDirectory *directory;
	GList *pending_file_info;
	GList *node, *next;
	NautilusFile *file;
	GList *changed_files, *added_files;
	GnomeVFSFileInfo *file_info;

	directory = NAUTILUS_DIRECTORY (callback_data);

	nautilus_directory_ref (directory);

	directory->details->dequeue_pending_idle_id = 0;

	/* Handle the files in the order we saw them. */
	pending_file_info = g_list_reverse (directory->details->pending_file_info);
	directory->details->pending_file_info = NULL;

	/* If we are no longer monitoring, then throw away these. */
	if (!nautilus_directory_is_file_list_monitored (directory)) {
		load_directory_done (directory);
		goto drain;
	}

	added_files = NULL;
	changed_files = NULL;

	/* Build a list of NautilusFile objects. */
	for (node = pending_file_info; node != NULL; node = node->next) {
		file_info = node->data;

		/* Update the file count. */
		/* FIXME bugzilla.gnome.org 45063: This could count a
		 * file twice if we get it from both load_directory
		 * and from new_files_callback. Not too hard to fix by
		 * moving this into the actual callback instead of
		 * waiting for the idle function.
		 */
		if (gnome_vfs_directory_filter_apply (directory->details->load_file_count_filter,
						      file_info)) {
			directory->details->load_file_count += 1;
		}

		/* Add the MIME type to the set. */
		if ((file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) != 0
			&& directory->details->load_mime_list_hash != NULL) {
			istr_set_insert (directory->details->load_mime_list_hash,
					 file_info->mime_type);
		}
		
		/* check if the file already exists */
		file = nautilus_directory_find_file_by_name (directory, file_info->name);
		if (file != NULL) {
			/* file already exists, check if it changed */
			set_file_unconfirmed (file, FALSE);
			if (nautilus_file_update_info (file, file_info)) {
				/* File changed, notify about the change. */
				nautilus_file_ref (file);
				changed_files = g_list_prepend (changed_files, file);
			}
			nautilus_file_ref (file);			
		} else {
			/* new file, create a nautilus file object and add it to the list */
			file = nautilus_file_new_from_info (directory, file_info);
			nautilus_directory_add_file (directory, file);			
		}
		added_files = g_list_prepend (added_files, file);
	}

	/* If we are done loading, then we assume that any unconfirmed
         * files are gone.
	 */
	if (directory->details->directory_loaded) {
		for (node = directory->details->file_list;
		     node != NULL; node = next) {
			file = NAUTILUS_FILE (node->data);
			next = node->next;

			if (file->details->unconfirmed) {
				nautilus_file_ref (file);
				changed_files = g_list_prepend (changed_files, file);

				file->details->is_gone = TRUE;
				nautilus_directory_remove_file (directory, file);
			}
		}
	}

	/* Send the changed and added signals. */
	nautilus_directory_emit_change_signals (directory, changed_files);
	nautilus_file_list_free (changed_files);
	nautilus_directory_emit_files_added (directory, added_files);
	nautilus_file_list_free (added_files);

	if (directory->details->directory_loaded
	    && !directory->details->directory_loaded_sent_notification) {
		/* Send the done_loading signal. */
		nautilus_directory_emit_done_loading (directory);

		file = directory->details->load_directory_file;

		if (file != NULL) {
			file->details->directory_count_is_up_to_date = TRUE;
			file->details->got_directory_count = TRUE;
			file->details->directory_count = directory->details->load_file_count;

			file->details->got_mime_list = TRUE;
			file->details->mime_list_is_up_to_date = TRUE;
			file->details->mime_list = istr_set_get_as_list
				(directory->details->load_mime_list_hash);

			nautilus_file_changed (file);
		}
		
		load_directory_done (directory);

		directory->details->directory_loaded_sent_notification = TRUE;
	}

 drain:
	gnome_vfs_file_info_list_free (pending_file_info);

	/* Get the state machine running again. */
	nautilus_directory_async_state_changed (directory);

	nautilus_directory_unref (directory);
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

	/* Arrange for the "loading" part of the work. */
	gnome_vfs_file_info_ref (info);
	directory->details->pending_file_info
		= g_list_prepend (directory->details->pending_file_info, info);
	nautilus_directory_schedule_dequeue_pending (directory);
}

static void
directory_load_cancel (NautilusDirectory *directory)
{
	if (directory->details->directory_load_in_progress != NULL) {
		gnome_vfs_async_cancel (directory->details->directory_load_in_progress);
		directory->details->directory_load_in_progress = NULL;
		async_job_end (directory, "file list");
	}
}

static void
file_list_cancel (NautilusDirectory *directory)
{
	directory_load_cancel (directory);
	
	if (directory->details->dequeue_pending_idle_id != 0) {
		gtk_idle_remove (directory->details->dequeue_pending_idle_id);
		directory->details->dequeue_pending_idle_id = 0;
	}

	if (directory->details->pending_file_info != NULL) {
		gnome_vfs_file_info_list_free (directory->details->pending_file_info);
		directory->details->pending_file_info = NULL;
	}

	load_directory_state_destroy (directory);
}

static void
directory_load_done (NautilusDirectory *directory,
		     GnomeVFSResult result)
{
	GList *node;

	directory_load_cancel (directory);
	directory->details->directory_loaded = TRUE;
	directory->details->directory_loaded_sent_notification = FALSE;

	/* Note that GNOME_VFS_OK can make it this far when the file-list
	 * length limit has been reached. In that case, don't treat it as
	 * an error.
	 */
	if (result != GNOME_VFS_ERROR_EOF && result != GNOME_VFS_OK) {
		/* The load did not complete successfully. This means
		 * we don't know the status of the files in this directory.
		 * We clear the unconfirmed bit on each file here so that
		 * they won't be marked "gone" later -- we don't know enough
		 * about them to know whether they are really gone.
		 */
		for (node = directory->details->file_list;
		     node != NULL; node = node->next) {
			set_file_unconfirmed (NAUTILUS_FILE (node->data), FALSE);
		}

		nautilus_directory_emit_load_error (directory,
						    result);
	}

	/* Call the idle function right away. */
	if (directory->details->dequeue_pending_idle_id != 0) {
		gtk_idle_remove (directory->details->dequeue_pending_idle_id);
	}
	dequeue_pending_idle_callback (directory);
}

static void
directory_load_callback (GnomeVFSAsyncHandle *handle,
			 GnomeVFSResult result,
			 GList *list,
			 guint entries_read,
			 gpointer callback_data)
{
	NautilusDirectory *directory;
	GList *element;

	directory = NAUTILUS_DIRECTORY (callback_data);

	g_assert (directory->details->directory_load_in_progress != NULL);
	g_assert (directory->details->directory_load_in_progress == handle);

	nautilus_directory_ref (directory);

	for (element = list; element != NULL; element = element->next) {
		directory_load_one (directory, element->data);
	}

	if (nautilus_directory_file_list_length_reached (directory)
	    || result != GNOME_VFS_OK) {
		directory_load_done (directory, result);
	}

	nautilus_directory_unref (directory);
}

/* This checks if there's a request for the metafile contents. */
static gboolean
is_anyone_waiting_for_metafile (NautilusDirectory *directory)
{
	GList *node;
	ReadyCallback *callback;
	Monitor *monitor;	

	for (node = directory->details->call_when_ready_list; node != NULL; node = node->next) {
		callback = node->data;
		if (callback->request.metafile) {
			return TRUE;
		}
	}

	for (node = directory->details->monitor_list; node != NULL; node = node->next) {
		monitor = node->data;
		if (monitor->request.metafile) {
			return TRUE;
		}
	}	

	return FALSE;
}

static void
update_metadata_monitors (NautilusDirectory *directory)
{
	gboolean is_metadata_monitored;
	
	is_metadata_monitored = is_anyone_waiting_for_metafile (directory);
	
	if (directory->details->metafile_monitor == NULL) {
		if (is_metadata_monitored) {
			nautilus_directory_register_metadata_monitor (directory);	
		}
	} else {
		if (!is_metadata_monitored) {
			nautilus_directory_unregister_metadata_monitor (directory);	
		}
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

	if (file == NULL) {
		if (directory->details->monitor != NULL
		    && !find_any_monitor (directory, NULL)) {
			nautilus_monitor_cancel (directory->details->monitor);
			directory->details->monitor = NULL;
		}
	} else {
		if (file->details->monitor != NULL
		    && !find_any_monitor (directory, file)) {
			nautilus_monitor_cancel (file->details->monitor);
			file->details->monitor = NULL;
		}
	}

	update_metadata_monitors (directory);

	/* XXX - do we need to remove anything from the work queue? */

	nautilus_directory_async_state_changed (directory);
}

FileMonitors *
nautilus_directory_remove_file_monitors (NautilusDirectory *directory,
					 NautilusFile *file)
{
	GList *result, **list, *node, *next;
	Monitor *monitor;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (file->details->directory == directory);

	result = NULL;

	list = &directory->details->monitor_list;
	for (node = directory->details->monitor_list; node != NULL; node = next) {
		next = node->next;
		monitor = node->data;

		if (monitor->file == file) {
			*list = g_list_remove_link (*list, node);
			result = g_list_concat (node, result);
		}
	}

	update_metadata_monitors (directory);

	/* XXX - do we need to remove anything from the work queue? */

	nautilus_directory_async_state_changed (directory);

	return (FileMonitors *) result;
}

void
nautilus_directory_add_file_monitors (NautilusDirectory *directory,
				      NautilusFile *file,
				      FileMonitors *monitors)
{
	GList **list;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (file->details->directory == directory);

	if (monitors == NULL) {
		return;
	}

	list = &directory->details->monitor_list;
	*list = g_list_concat (*list, (GList *) monitors);

	nautilus_directory_add_file_to_work_queue (directory, file);

	update_metadata_monitors (directory);
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
			file_list = nautilus_directory_get_file_list (directory);
		}

		/* Pass back the file list if the user was waiting for it. */
		(* callback->callback.directory) (directory,
						  file_list,
						  callback->callback_data);

		nautilus_file_list_free (file_list);
	}
}

void
nautilus_directory_call_when_ready_internal (NautilusDirectory *directory,
					     NautilusFile *file,
					     GList *file_attributes,
					     gboolean wait_for_file_list,
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
	nautilus_directory_set_up_request (&callback.request, file_attributes);
	callback.request.file_list = wait_for_file_list;
	
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

	/* When we change the ready list we need to sync up metadata monitors.
	 * We could just call update_metadata_monitors here, but we can be smarter
	 * since we know what was just added.
	 */
	if (callback.request.metafile && directory->details->metafile_monitor == NULL) {
		nautilus_directory_register_metadata_monitor (directory);	
	}

	/* Put the callback file or all the files on the work queue. */
	if (file != NULL) {
		nautilus_directory_add_file_to_work_queue (directory, file);
	} else {
		add_all_files_to_work_queue (directory);
	}

	nautilus_directory_async_state_changed (directory);
}

gboolean      
nautilus_directory_check_if_ready_internal (NautilusDirectory *directory,
					    NautilusFile *file,
					    GList *file_attributes)
{
	Request request;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));

	nautilus_directory_set_up_request (&request, file_attributes);
	return request_is_satisfied (directory, file, &request);
}

static void
remove_callback_link_keep_data (NautilusDirectory *directory,
				GList *link)
{
	directory->details->call_when_ready_list = g_list_remove_link
		(directory->details->call_when_ready_list, link);
	g_list_free_1 (link);
}

static void
remove_callback_link (NautilusDirectory *directory,
		      GList *link)
{
	g_free (link->data);
	remove_callback_link_keep_data (directory, link);
}

void
nautilus_directory_cancel_callback_internal (NautilusDirectory *directory,
					     NautilusFile *file,
					     NautilusDirectoryCallback directory_callback,
					     NautilusFileCallback file_callback,
					     gpointer callback_data)
{
	ReadyCallback callback;
	GList *node;

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
	node = g_list_find_custom (directory->details->call_when_ready_list,
				&callback,
				ready_callback_key_compare);
	if (node != NULL) {
		remove_callback_link (directory, node);
		/* When we change the ready list we need to sync up metadata monitors. */
		update_metadata_monitors (directory);
		
		nautilus_directory_async_state_changed (directory);
	}
}

static void
directory_count_callback (GnomeVFSAsyncHandle *handle,
			  GnomeVFSResult result,
			  GList *list,
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

	nautilus_directory_ref (directory);

	count_file->details->directory_count_is_up_to_date = TRUE;

	/* Record either a failure or success. */
	if (result != GNOME_VFS_ERROR_EOF) {
		count_file->details->directory_count_failed = TRUE;
		count_file->details->got_directory_count = FALSE;
		count_file->details->directory_count = 0;
	} else {
		count_file->details->directory_count_failed = FALSE;
		count_file->details->got_directory_count = TRUE;
		count_file->details->directory_count = entries_read;
	}
	directory->details->count_file = NULL;
	directory->details->count_in_progress = NULL;

	/* Send file-changed even if count failed, so interested parties can
	 * distinguish between unknowable and not-yet-known cases.
	 */
	nautilus_file_changed (count_file);

	/* Start up the next one. */
	async_job_end (directory, "directory count");
	nautilus_directory_async_state_changed (directory);

	nautilus_directory_unref (directory);
}

static void
new_files_callback (GnomeVFSAsyncHandle *handle,
		    GList *results,
		    gpointer callback_data)
{
	GList **handles, *node;
	NautilusDirectory *directory;
	GnomeVFSGetFileInfoResult *result;

	directory = NAUTILUS_DIRECTORY (callback_data);
	handles = &directory->details->get_file_infos_in_progress;
	g_assert (handle == NULL || g_list_find (*handles, handle) != NULL);

	nautilus_directory_ref (directory);
	
	/* Note that this call is done. */
	*handles = g_list_remove (*handles, handle);

	/* Queue up the new files. */
	for (node = results; node != NULL; node = node->next) {
		result = node->data;

		if (result->result == GNOME_VFS_OK) {
			directory_load_one (directory, result->file_info);
		}
	}

	nautilus_directory_unref (directory);
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
	GList *node, *next;
	ReadyCallback *callback;
	Monitor *monitor;

	directory = file->details->directory;
	changed = FALSE;

	/* Check for callbacks. */
	for (node = directory->details->call_when_ready_list; node != NULL; node = next) {
		next = node->next;
		callback = node->data;

		if (callback->file == file) {
			/* Client should have cancelled callback. */
			g_warning ("destroyed file has call_when_ready pending");
			remove_callback_link (directory, node);
			changed = TRUE;
		}
	}

	/* Check for monitors. */
	for (node = directory->details->monitor_list; node != NULL; node = next) {
		next = node->next;
		monitor = node->data;

		if (monitor->file == file) {
			/* Client should have removed monitor earlier. */
			g_warning ("destroyed file still being monitored");
			remove_monitor_link (directory, node);
			changed = TRUE;
		}
	}

	/* When we change the monitor or ready list we need to sync up metadata monitors */
	if (changed) {
		update_metadata_monitors (directory);
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
	if (directory->details->mime_list_file == file) {
		directory->details->mime_list_file = NULL;
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
	if (directory->details->link_info_read_state != NULL
	    && directory->details->link_info_read_state->file == file) {
		directory->details->link_info_read_state->file = NULL;
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
	return !file->details->directory_count_is_up_to_date
		&& nautilus_file_should_show_directory_item_count (file);
}

static gboolean
should_get_directory_count_now (NautilusFile *file)
{
	return lacks_directory_count (file)
		&& !file->details->loading_directory;
}

static gboolean
wants_directory_count (const Request *request)
{
	return request->directory_count;
}

static gboolean
lacks_top_left (NautilusFile *file)
{
	return file->details->file_info_is_up_to_date &&
		!file->details->top_left_text_is_up_to_date 
		&& nautilus_file_should_get_top_left_text (file);
}

static gboolean
wants_top_left (const Request *request)
{
	return request->top_left_text;
}

static gboolean
lacks_info (NautilusFile *file)
{
	return !file->details->file_info_is_up_to_date
		&& !file->details->is_gone;
}

static gboolean
wants_info (const Request *request)
{
	return request->file_info;
}

static gboolean
lacks_deep_count (NautilusFile *file)
{
	return file->details->deep_counts_status != NAUTILUS_REQUEST_DONE;
}

static gboolean
wants_deep_count (const Request *request)
{
	return request->deep_count;
}

static gboolean
lacks_mime_list (NautilusFile *file)
{
	return !file->details->mime_list_is_up_to_date;
}

static gboolean
should_get_mime_list (NautilusFile *file)
{
	return lacks_mime_list (file)
		&& !file->details->loading_directory;
}

static gboolean
wants_mime_list (const Request *request)
{
	return request->mime_list;
}

static gboolean
lacks_link_info (NautilusFile *file)
{
	if (file->details->file_info_is_up_to_date && 
	    !file->details->link_info_is_up_to_date) {
		if ((nautilus_file_is_mime_type (file, "application/x-gmc-link") &&
		     nautilus_file_is_in_desktop (file)) ||
		    nautilus_file_is_nautilus_link (file) ||
		    nautilus_file_is_directory (file)) {
			return TRUE;
		} else {
			link_info_done (file->details->directory, file, NULL, NULL, NULL);
			return FALSE;
		}
	} else {
		return FALSE;
	}
}

static gboolean
wants_link_info (const Request *request)
{
	return request->link_info;
}


static gboolean
has_problem (NautilusDirectory *directory, NautilusFile *file, FileCheck problem)
{
	GList *node;

	if (file != NULL) {
		return (* problem) (file);
	}

	for (node = directory->details->file_list; node != NULL; node = node->next) {
		if ((* problem) (node->data)) {
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
	if (request->metafile && !nautilus_directory_is_metadata_read (directory)) {
		return FALSE;
	}

	if (request->file_list && !(directory->details->directory_loaded &&
				    directory->details->directory_loaded_sent_notification)) {
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

	if (request->top_left_text) {
		if (has_problem (directory, file, lacks_top_left)) {
			return FALSE;
		}
	}

	if (request->deep_count) {
		if (has_problem (directory, file, lacks_deep_count)) {
			return FALSE;
		}
	}

	if (request->mime_list) {
		if (has_problem (directory, file, lacks_mime_list)) {
			return FALSE;
		}
	}

	if (request->link_info) {
		if (has_problem (directory, file, lacks_link_info)) {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
call_ready_callbacks (NautilusDirectory *directory)
{
	gboolean called_any;
	GList *node, *next;
	ReadyCallback *callback;

	callback = NULL;
	called_any = FALSE;
	while (1) {
		/* Check if any callbacks are satisifed and call them if they are. */
		for (node = directory->details->call_when_ready_list;
		     node != NULL; node = next) {
			next = node->next;
			callback = node->data;
			if (request_is_satisfied (directory, callback->file, &callback->request)) {
				break;
			}
		}
		if (node == NULL) {
			if (called_any) {
				/* When we change the ready list we need to sync up metadata monitors. */
				update_metadata_monitors (directory);
			}
			return called_any;
		}
		
		/* Callbacks are one-shots, so remove it now. */
		remove_callback_link_keep_data (directory, node);
		
		/* Call the callback. */
		ready_callback_call (directory, callback);
		g_free (callback);
		called_any = TRUE;
	}
}

/* This checks if there's a request for monitoring the file list. */
gboolean
nautilus_directory_is_anyone_monitoring_file_list (NautilusDirectory *directory)
{
	GList *node;
	ReadyCallback *callback;
	Monitor *monitor;

	for (node = directory->details->call_when_ready_list;
	     node != NULL; node = node->next) {
		callback = node->data;
		if (callback->request.file_list) {
			return TRUE;
		}
	}

	for (node = directory->details->monitor_list;
	     node != NULL; node = node->next) {
		monitor = node->data;
		if (monitor->request.file_list) {
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
	GList *node;
	NautilusFile *file;

	for (node = directory->details->file_list; node != NULL; node = node->next) {
		file = node->data;
		set_file_unconfirmed (file, TRUE);
	}
}

static gboolean
should_display_file_name (const char *name,
			  GnomeVFSDirectoryFilterOptions options)
{
	/* Note that the name is URI-encoded, but this should not
	 * affect the . or the ~.
	 */

	if ((options & GNOME_VFS_DIRECTORY_FILTER_NODOTFILES) != 0
	    && nautilus_file_name_matches_hidden_pattern (name)) {
		return FALSE;
	}

	if ((options & GNOME_VFS_DIRECTORY_FILTER_NOBACKUPFILES) != 0
	    && nautilus_file_name_matches_backup_pattern (name)) {
		return FALSE;
	}
	
	/* Note that we don't bother to check for "." or ".." here, because
	 * this function is used only for search results, which never include
	 * those special files. If we later use this function more generally,
	 * we might have to change this.
	 */
	return TRUE;
}

/* Filter search results based on user preferences. This must be done
 * differently than filtering other files because the search results
 * are encoded: the entire file path is encoded and stored as the file
 * name.
 */
static gboolean
filter_search_uri (const GnomeVFSFileInfo *info, gpointer data)
{
	GnomeVFSDirectoryFilterOptions options;
	char *real_file_uri;
	gboolean result;

	options = GPOINTER_TO_INT (data);
	
	real_file_uri = nautilus_get_target_uri_from_search_result_name (info->name);
	result = should_display_file_name (g_basename (real_file_uri), options);	
	g_free (real_file_uri);

	return result;
}

static GnomeVFSDirectoryFilter *
get_file_count_filter (NautilusDirectory *directory)
{
	if (nautilus_is_search_uri (directory->details->uri)) {
		return gnome_vfs_directory_filter_new_custom
			(filter_search_uri,
			 GNOME_VFS_DIRECTORY_FILTER_NEEDS_NAME,
			 GINT_TO_POINTER (get_filter_options_for_directory_count ()));
	}

	return gnome_vfs_directory_filter_new
		(GNOME_VFS_DIRECTORY_FILTER_NONE,
		 get_filter_options_for_directory_count (),
		 NULL);
}

/* Start monitoring the file list if it isn't already. */
static void
start_monitoring_file_list (NautilusDirectory *directory)
{
	if (!directory->details->file_list_monitored) {
		g_assert (directory->details->directory_load_in_progress == NULL);
		directory->details->file_list_monitored = TRUE;
		nautilus_file_list_ref (directory->details->file_list);
	}

	if (directory->details->directory_loaded
	    || directory->details->directory_load_in_progress != NULL) {
		return;
	}

	if (!async_job_start (directory, "file list")) {
		return;
	}

	mark_all_files_unconfirmed (directory);

	g_assert (directory->details->uri != NULL);
        directory->details->load_directory_file =
		nautilus_directory_get_corresponding_file (directory);

	directory->details->load_directory_file->details->loading_directory = TRUE;
	directory->details->load_file_count = 0;
	directory->details->load_file_count_filter = get_file_count_filter (directory);
	directory->details->load_mime_list_hash = istr_set_new ();
#ifdef DEBUG_LOAD_DIRECTORY
	g_message ("load_directory called to monitor file list of %s", directory->details->uri);
#endif	
	gnome_vfs_async_load_directory
		(&directory->details->directory_load_in_progress, /* handle */
		 directory->details->uri,                         /* uri */
		 (GNOME_VFS_FILE_INFO_GET_MIME_TYPE	          /* options */
		  | GNOME_VFS_FILE_INFO_FOLLOW_LINKS),
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
	file_list_cancel (directory);
	nautilus_file_list_unref (directory->details->file_list);
	directory->details->directory_loaded = FALSE;
}

static void
file_list_start_or_stop (NautilusDirectory *directory)
{
	if (nautilus_directory_is_anyone_monitoring_file_list (directory)) {
		start_monitoring_file_list (directory);
	} else {
		nautilus_directory_stop_monitoring_file_list (directory);
	}
}

void
nautilus_file_invalidate_count_and_mime_list (NautilusFile *file)
{
	GList *attributes = NULL;

	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT);
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES);
	
	nautilus_file_invalidate_attributes (file, attributes);
}


/* Reset count and mime list. Invalidating deep counts is handled by
 * itself elsewhere because it's a relatively heavyweight and
 * special-purpose operation (see bug 5863). Also, the shallow count
 * needs to be refreshed when filtering changes, but the deep count
 * deliberately does not take filtering into account.
 */
void
nautilus_directory_invalidate_count_and_mime_list (NautilusDirectory *directory)
{
	NautilusFile *file;

	file = nautilus_directory_get_existing_corresponding_file (directory);
	if (file != NULL) {
		nautilus_file_invalidate_count_and_mime_list (file);
	}
	
	nautilus_file_unref (file);
}

static void
nautilus_directory_invalidate_file_attributes (NautilusDirectory *directory,
					       GList             *file_attributes)
{
	GList *node;

	cancel_loading_attributes (directory, file_attributes);

	for (node = directory->details->file_list; node != NULL; node = node->next) {
		nautilus_file_invalidate_attributes_internal (NAUTILUS_FILE (node->data),
							      file_attributes);
	}

	if (directory->details->as_file != NULL) {
		nautilus_file_invalidate_attributes_internal (directory->details->as_file,
							      file_attributes);
	}
}

void
nautilus_directory_force_reload_internal (NautilusDirectory *directory,
					  GList *file_attributes)
{
	/* invalidate attributes that are getting reloaded for all files */
	nautilus_directory_invalidate_file_attributes (directory, file_attributes);

	/* Start a new directory load. */
	file_list_cancel (directory);
	directory->details->directory_loaded = FALSE;

	/* Start a new directory count. */
	nautilus_directory_invalidate_count_and_mime_list (directory);

	add_all_files_to_work_queue (directory);
	nautilus_directory_async_state_changed (directory);
}

static gboolean
monitor_includes_file (const Monitor *monitor,
		       NautilusFile *file)
{
	if (monitor->file == file) {
		return TRUE;
	}
	if (monitor->file != NULL) {
		return FALSE;
	}
	if (file == file->details->directory->details->as_file) {
		return FALSE;
	}
	return nautilus_file_should_show (file,
					  monitor->monitor_hidden_files,
					  monitor->monitor_backup_files);
}

static gboolean
is_needy (NautilusFile *file,
	  FileCheck check_missing,
	  RequestCheck check_wanted)
{
	NautilusDirectory *directory;
	GList *node;
	ReadyCallback *callback;
	Monitor *monitor;

	g_assert (NAUTILUS_IS_FILE (file));

	if (!(* check_missing) (file)) {
		return FALSE;
	}

	directory = file->details->directory;
	for (node = directory->details->call_when_ready_list;
	     node != NULL; node = node->next) {
		callback = node->data;
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
	for (node = directory->details->monitor_list;
	     node != NULL; node = node->next) {
		monitor = node->data;
		if ((* check_wanted) (&monitor->request)) {
			if (monitor_includes_file (monitor, file)) {
				return TRUE;
			}
		}
	}
	return FALSE;
}

static void
directory_count_stop (NautilusDirectory *directory)
{
	NautilusFile *file;

	if (directory->details->count_in_progress != NULL) {
		file = directory->details->count_file;
		if (file != NULL) {
			g_assert (NAUTILUS_IS_FILE (file));
			g_assert (file->details->directory == directory);
			if (is_needy (file,
				      should_get_directory_count_now,
				      wants_directory_count)) {
				return;
			}
		}

		/* The count is not wanted, so stop it. */
		directory_count_cancel (directory);
	}
}

static void
directory_count_start (NautilusDirectory *directory,
		       NautilusFile *file)
{
	char *uri;

	if (directory->details->count_in_progress != NULL) {
		return;
	}

	if (!is_needy (file, 
		       should_get_directory_count_now,
		       wants_directory_count)) {
		return;
	}

	if (!nautilus_file_is_directory (file)) {
		file->details->directory_count_is_up_to_date = TRUE;
		file->details->directory_count_failed = FALSE;
		file->details->got_directory_count = FALSE;
		
		nautilus_directory_async_state_changed (directory);
		return;
	}

	if (!async_job_start (directory, "directory count")) {
		return;
	}

	/* Start counting. */
	directory->details->count_file = file;
	uri = nautilus_file_get_uri (file);
#ifdef DEBUG_LOAD_DIRECTORY		
	g_message ("load_directory called to get shallow file count for %s", uri);
#endif	
	gnome_vfs_async_load_directory
		(&directory->details->count_in_progress,
		 uri,
		 GNOME_VFS_FILE_INFO_DEFAULT,
		 GNOME_VFS_DIRECTORY_FILTER_NONE,
		 get_filter_options_for_directory_count (),
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
		     GList *list,
		     guint entries_read,
		     gpointer callback_data)
{
	NautilusDirectory *directory;
	NautilusFile *file;
	GList *element;
	char *uri;
	gboolean done;

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->deep_count_in_progress == handle);
	file = directory->details->deep_count_file;
	g_assert (NAUTILUS_IS_FILE (file));

	nautilus_directory_ref (directory);

	for (element = list; element != NULL; element = element->next) {
		deep_count_one (directory, element->data);
	}

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

	nautilus_file_updated_deep_count_in_progress (file);

	if (done) {
		nautilus_file_changed (file);
		async_job_end (directory, "deep count");
		nautilus_directory_async_state_changed (directory);
	}

	nautilus_directory_unref (directory);
}

static void
deep_count_load (NautilusDirectory *directory, const char *uri)
{
	g_assert (directory->details->deep_count_uri == NULL);
	directory->details->deep_count_uri = g_strdup (uri);
#ifdef DEBUG_LOAD_DIRECTORY		
	g_message ("load_directory called to get deep file count for %s", uri);
#endif	
	gnome_vfs_async_load_directory
		(&directory->details->deep_count_in_progress,
		 uri,
		 GNOME_VFS_FILE_INFO_DEFAULT,
		 GNOME_VFS_DIRECTORY_FILTER_NONE,
		 (GNOME_VFS_DIRECTORY_FILTER_NOSELFDIR
		  | GNOME_VFS_DIRECTORY_FILTER_NOPARENTDIR),
		 NULL,
		 G_MAXINT,
		 deep_count_callback,
		 directory);
}

static void
deep_count_stop (NautilusDirectory *directory)
{
	NautilusFile *file;

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
		deep_count_cancel (directory);
	}
}

static void
deep_count_start (NautilusDirectory *directory,
		  NautilusFile *file)
{
	char *uri;

	if (directory->details->deep_count_in_progress != NULL) {
		return;
	}

	if (!is_needy (file,
		       lacks_deep_count,
		       wants_deep_count)) {
		return;
	}

	if (!nautilus_file_is_directory (file)) {
		file->details->deep_counts_status = NAUTILUS_REQUEST_DONE;

		nautilus_directory_async_state_changed (directory);
		return;
	}

	if (!async_job_start (directory, "deep count")) {
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

static void
mime_list_one (NautilusDirectory *directory,
	       GnomeVFSFileInfo *info)
{
	if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) != 0) {
		istr_set_insert (directory->details->mime_list_hash, info->mime_type);
	}
}

static void
mime_list_callback (GnomeVFSAsyncHandle *handle,
		    GnomeVFSResult result,
		    GList *list,
		    guint entries_read,
		    gpointer callback_data)
{
	NautilusDirectory *directory;
	NautilusFile *file;
	GList *element;

	directory = NAUTILUS_DIRECTORY (callback_data);
	g_assert (directory->details->mime_list_in_progress == handle);
	file = directory->details->mime_list_file;
	g_assert (NAUTILUS_IS_FILE (file));

	for (element = list; element != NULL; element = element->next) {
		mime_list_one (directory, element->data);
	}

	if (result == GNOME_VFS_OK) {
		return;
	}

	nautilus_directory_ref (directory);

	file->details->mime_list_is_up_to_date = TRUE;

	/* Record either a failure or success. */
	eel_g_list_free_deep (file->details->mime_list);
	if (result != GNOME_VFS_ERROR_EOF) {
		file->details->mime_list_failed = TRUE;
		file->details->mime_list = NULL;
	} else {
		file->details->got_mime_list = TRUE;
		file->details->mime_list = istr_set_get_as_list
			(directory->details->mime_list_hash);
	}
	istr_set_destroy (directory->details->mime_list_hash);

	directory->details->mime_list_in_progress = NULL;
	directory->details->mime_list_file = NULL;
	directory->details->mime_list_hash = NULL;

	/* Send file-changed even if getting the item type list
	 * failed, so interested parties can distinguish between
	 * unknowable and not-yet-known cases.
	 */
	nautilus_file_changed (file);

	/* Start up the next one. */
	async_job_end (directory, "MIME list");
	nautilus_directory_async_state_changed (directory);

	nautilus_directory_unref (directory);
}

static void
mime_list_load (NautilusDirectory *directory, const char *uri)
{
	directory->details->mime_list_hash = istr_set_new ();
#ifdef DEBUG_LOAD_DIRECTORY		
	g_message ("load_directory called to get MIME list of %s", uri);
#endif	
	gnome_vfs_async_load_directory
		(&directory->details->mime_list_in_progress,
		 uri,
		 GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
		 GNOME_VFS_DIRECTORY_FILTER_NONE,
		 (GNOME_VFS_DIRECTORY_FILTER_NOSELFDIR
		  | GNOME_VFS_DIRECTORY_FILTER_NOPARENTDIR),
		 NULL,
		 DIRECTORY_LOAD_ITEMS_PER_CALLBACK,
		 mime_list_callback,
		 directory);
}

static void
mime_list_stop (NautilusDirectory *directory)
{
	NautilusFile *file;

	if (directory->details->mime_list_in_progress != NULL) {
		file = directory->details->mime_list_file;
		if (file != NULL) {
			g_assert (NAUTILUS_IS_FILE (file));
			g_assert (file->details->directory == directory);
			if (is_needy (file,
				      should_get_mime_list,
				      wants_mime_list)) {
				return;
			}
		}

		/* The count is not wanted, so stop it. */
		mime_list_cancel (directory);
	}
}

static void
mime_list_start (NautilusDirectory *directory,
		 NautilusFile *file)
{
	char *uri;

	mime_list_stop (directory);

	if (directory->details->mime_list_in_progress != NULL) {
		return;
	}

	/* Figure out which file to get a mime list for. */
	if (!is_needy (file,
		       should_get_mime_list,
		       wants_mime_list)) {
		return;
	}

	if (!nautilus_file_is_directory (file)) {
		g_list_free (file->details->mime_list);
		file->details->mime_list_failed = FALSE;
		file->details->got_directory_count = FALSE;
		file->details->mime_list_is_up_to_date = TRUE;

		nautilus_directory_async_state_changed (directory);
		return;
	}

	if (!async_job_start (directory, "MIME list")) {
		return;
	}

	directory->details->mime_list_file = file;
	uri = nautilus_file_get_uri (file);
	mime_list_load (directory, uri);
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

	g_free (directory->details->top_left_read_state);
	directory->details->top_left_read_state = NULL;

	async_job_end (directory, "top left");
	nautilus_directory_async_state_changed (directory);
}

static void
top_left_read_callback (GnomeVFSResult result,
			GnomeVFSFileSize bytes_read,
			char *file_contents,
			gpointer callback_data)
{
	NautilusDirectory *directory;
	NautilusFile *changed_file;

	directory = NAUTILUS_DIRECTORY (callback_data);

	directory->details->top_left_read_state->handle = NULL;
	directory->details->top_left_read_state->file->details->top_left_text_is_up_to_date = TRUE;

	changed_file = NULL;
	if (result == GNOME_VFS_OK) {
		g_free (directory->details->top_left_read_state->file->details->top_left_text);
		directory->details->top_left_read_state->file->details->top_left_text =
			nautilus_extract_top_left_text (file_contents, bytes_read);
		
		directory->details->top_left_read_state->file->details->got_top_left_text = TRUE;
	} else {
		g_free (directory->details->top_left_read_state->file->details->top_left_text);
		directory->details->top_left_read_state->file->details->got_top_left_text = FALSE;
	}

	g_free (file_contents);

	nautilus_file_changed (directory->details->top_left_read_state->file);

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
top_left_stop (NautilusDirectory *directory)
{
	NautilusFile *file;

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
		top_left_cancel (directory);
	}
}

static void
top_left_start (NautilusDirectory *directory,
		NautilusFile *file)
{
	char *uri;

	if (directory->details->top_left_read_state != NULL) {
		return;
	}

	/* Figure out which file to read the top left for. */
	if (!is_needy (file,
		       lacks_top_left,
		       wants_top_left)) {
		return;
	}

	if (!nautilus_file_contains_text (file)) {
		g_free (file->details->top_left_text);
		file->details->got_top_left_text = FALSE;
		file->details->top_left_text_is_up_to_date = TRUE;

		nautilus_directory_async_state_changed (directory);
		return;
	}

	if (!async_job_start (directory, "top left")) {
		return;
	}

	/* Start reading. */
	directory->details->top_left_read_state = g_new0 (TopLeftTextReadState, 1);
	directory->details->top_left_read_state->file = file;
	uri = nautilus_file_get_uri (file);
	directory->details->top_left_read_state->handle = eel_read_file_async
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
	g_assert (eel_g_list_exactly_one_item (results));
	get_info_file = directory->details->get_info_file;
	g_assert (NAUTILUS_IS_FILE (get_info_file));

	nautilus_directory_ref (directory);
	
	directory->details->get_info_file = NULL;
	directory->details->get_info_in_progress = NULL;

	/* ref here because we might be removing the last ref when we
	 * mark the file gone below, but we need to keep a ref at
	 * least long enough to send the change notification. 
	 */
	nautilus_file_ref (get_info_file);

	result = results->data;

	if (result->result != GNOME_VFS_OK) {
		get_info_file->details->file_info_is_up_to_date = TRUE;
		if (get_info_file->details->info != NULL) {
			gnome_vfs_file_info_unref (get_info_file->details->info);
			get_info_file->details->info = NULL;
		}
		get_info_file->details->get_info_failed = TRUE;
		get_info_file->details->get_info_error = result->result;
		if (result->result == GNOME_VFS_ERROR_NOT_FOUND) {
			/* mark file as gone */

			get_info_file->details->is_gone = TRUE;
			if (get_info_file != directory->details->as_file) {
				nautilus_directory_remove_file (directory, get_info_file);
			}
		}
	} else {
		nautilus_file_update_info (get_info_file, result->file_info);
	}

	nautilus_file_changed (get_info_file);
	nautilus_file_unref (get_info_file);

	async_job_end (directory, "file info");
	nautilus_directory_async_state_changed (directory);

	nautilus_directory_unref (directory);
}

static void
file_info_stop (NautilusDirectory *directory)
{
	NautilusFile *file;

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
		file_info_cancel (directory);
	}
}

static void
file_info_start (NautilusDirectory *directory,
		 NautilusFile *file)
{
	char *uri;
	GnomeVFSURI *vfs_uri;
	GList fake_list;

	file_info_stop (directory);

	if (directory->details->get_info_in_progress != NULL) {
		return;
	}

	if (!is_needy (file, lacks_info, wants_info)) {
		return;
	}
	
	uri = nautilus_file_get_uri (file);
	vfs_uri = gnome_vfs_uri_new (uri);
	g_free (uri);
	
	/* If we can't even get info, fill in the info and go on.
	 */

	if (vfs_uri == NULL) {
		file->details->file_info_is_up_to_date = TRUE;
		file->details->get_info_failed = TRUE;
		file->details->get_info_error = GNOME_VFS_ERROR_INVALID_URI;

		nautilus_directory_async_state_changed (directory);
		return;
	}

	if (!async_job_start (directory, "file info")) {
		return;
	}
	directory->details->get_info_file = file;
	fake_list.data = vfs_uri;
	fake_list.prev = NULL;
	fake_list.next = NULL;
	gnome_vfs_async_get_file_info
		(&directory->details->get_info_in_progress,
		 &fake_list,
		 GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		 | GNOME_VFS_FILE_INFO_FOLLOW_LINKS,
		 get_info_callback,
		 directory);
	gnome_vfs_uri_unref (vfs_uri);
}


static void
link_info_done (NautilusDirectory *directory,
		NautilusFile *file,
		const char *uri,
		const char *name, 
		const char *icon)
{
	file->details->link_info_is_up_to_date = TRUE;

	file->details->got_link_info = TRUE;
	g_free (file->details->activation_uri);
	g_free (file->details->display_name);
	g_free (file->details->custom_icon_uri);
	file->details->activation_uri = g_strdup (uri);
	file->details->display_name = g_strdup (name);
	file->details->custom_icon_uri = g_strdup (icon);

	nautilus_directory_async_state_changed (directory);
}


static void
link_info_read_done (NautilusDirectory *directory,
		     const char *uri,
		     const char *name,
		     const char *icon)
{
	NautilusFile *file;

	file = directory->details->link_info_read_state->file;
	g_free (directory->details->link_info_read_state);
	directory->details->link_info_read_state = NULL;

	nautilus_file_ref (file);
	link_info_done (directory, file, uri, name, icon);
	nautilus_file_changed (file);
	nautilus_file_unref (file);
	async_job_end (directory, "link info");
}


static void
link_info_nautilus_link_read_callback (GnomeVFSResult result,
				       GnomeVFSFileSize bytes_read,
				       char *file_contents,
				       gpointer callback_data)
{
	NautilusDirectory *directory;
	char *buffer, *uri, *name, *icon;

	directory = NAUTILUS_DIRECTORY (callback_data);

	nautilus_directory_ref (directory);

	/* Handle the case where we read the Nautilus link. */
	if (result != GNOME_VFS_OK) {
		/* FIXME bugzilla.gnome.org 42433: We should report this error to the user. */
		g_free (file_contents);
		uri = NULL;
		name = NULL;
		icon = NULL;
	} else {
		/* The gnome-xml parser requires a zero-terminated array. */
		buffer = g_realloc (file_contents, bytes_read + 1);
		buffer[bytes_read] = '\0';
		uri = nautilus_link_get_link_uri_given_file_contents (buffer, bytes_read);
                name = nautilus_link_get_link_name_given_file_contents (buffer, bytes_read);
                icon = nautilus_link_get_link_icon_given_file_contents (buffer, bytes_read);
		g_free (buffer);
	}

	link_info_read_done (directory, uri, name, icon);
	g_free (uri);
	g_free (name);
	g_free (icon);

	nautilus_directory_unref (directory);
}




static void
link_info_gmc_link_read_callback (GnomeVFSResult result,
				  GnomeVFSFileSize bytes_read,
				  char *file_contents,
				  gpointer callback_data)
{
	NautilusDirectory *directory;
	char *end_of_line, *uri, *name, *path, *icon, *icon_path;
	int size, res;
	
	directory = NAUTILUS_DIRECTORY (callback_data);

	nautilus_directory_ref (directory);

	/* Handle the case where we read the GMC link. */
	if (result != GNOME_VFS_OK || !eel_str_has_prefix (file_contents, "URL: ")) {
		/* FIXME bugzilla.gnome.org 42433: We should report this error to the user. */
		uri = NULL;
		name = NULL;
		icon = NULL;
	} else {
		/* Make sure we don't run off the end of the buffer. */
		end_of_line = memchr (file_contents, '\n', bytes_read);
		if (end_of_line == NULL) {
			end_of_line = file_contents + bytes_read;
		}
		uri = file_contents + strlen("URL: ");
		uri = g_strndup (uri, end_of_line - uri);

		path = gnome_vfs_get_local_path_from_uri (uri);
		
		if (path != NULL) {
			/* FIXME: this gnome_metata_get call is synchronous, but better to
			 * have it here where the results will at least be cached than in
			 * nautilus_file_get_display_name. 
			 */
			res = gnome_metadata_get (path, "icon-name", &size, &name);
		} else {
		        res = -1;
		}
		
		if (res != 0) {
		        name = NULL;
		}

		if (path != NULL) {
			/* FIXME: this gnome_metata_get call is synchronous, but better to
			 * have it here where the results will at least be cached than in
			 * nautilus_file_get_display_name. 
			 */
			res = gnome_metadata_get (path, "icon-filename", &size, &icon_path);
		} else {
			res = -1;
		}

		if (res == 0 && icon_path != NULL) {
			icon = gnome_vfs_get_uri_from_local_path (icon_path);
			g_free (icon_path);
		} else {
			icon = NULL;
		}

		g_free (path);
	}

	g_free (file_contents);
	link_info_read_done (directory, uri, name, icon);
	g_free (uri);
	g_free (name);
	g_free (icon);

	nautilus_directory_unref (directory);
}

static gboolean
link_info_gmc_link_read_more_callback (GnomeVFSFileSize bytes_read,
				       const char *file_contents,
				       gpointer callback_data)
{
	g_assert (NAUTILUS_IS_DIRECTORY (callback_data));

	/* We need the first 512 bytes to see if something is a gmc link. */
	return bytes_read < 512;
}

static char *
make_dot_directory_uri (const char *uri)
{
	char *dot_directory_uri;
	GnomeVFSURI *vfs_uri;
	GnomeVFSURI *dot_dir_vfs_uri;
	
	/* FIXME: what we really need is a uri_append_file_name call
	 * that works on strings, so we can avoid the VFS parsing step.
	 */

	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL) {
		return NULL;
	}
	
	dot_dir_vfs_uri = gnome_vfs_uri_append_file_name (vfs_uri, ".directory");
	dot_directory_uri = gnome_vfs_uri_to_string (dot_dir_vfs_uri, GNOME_VFS_URI_HIDE_NONE);
	
	gnome_vfs_uri_unref (vfs_uri);
	gnome_vfs_uri_unref (dot_dir_vfs_uri);

	return dot_directory_uri;
}


static void
link_info_stop (NautilusDirectory *directory)
{
	NautilusFile *file;

	if (directory->details->link_info_read_state != NULL) {
		file = directory->details->link_info_read_state->file;

		if (file != NULL) {
			g_assert (NAUTILUS_IS_FILE (file));
			g_assert (file->details->directory == directory);
			if (is_needy (file,
				      lacks_link_info,
				      wants_link_info)) {
				return;
			}
		}

		/* The link info is not wanted, so stop it. */
		link_info_cancel (directory);
	}
}


static void
link_info_start (NautilusDirectory *directory,
		 NautilusFile *file)
{
	char *uri, *dot_directory_uri = NULL;
	gboolean gmc_style_link, nautilus_style_link, is_directory;

	if (directory->details->link_info_read_state != NULL) {
		return;
	}

	if (!is_needy (file,
		       lacks_link_info,
		       wants_link_info)) {
		return;
	}

	/* Figure out if it is a link. */
	gmc_style_link = nautilus_file_is_mime_type (file, "application/x-gmc-link")
		&& nautilus_file_is_in_desktop (file);
	nautilus_style_link = nautilus_file_is_nautilus_link (file);
        is_directory = nautilus_file_is_directory (file);

	uri = nautilus_file_get_uri (file);
	
	if (is_directory) {
		dot_directory_uri = make_dot_directory_uri (uri);
	}
	
	/* If it's not a link we are done. If it is, we need to read it. */
	if (!(gmc_style_link || nautilus_style_link || (is_directory && dot_directory_uri != NULL) )) {
		link_info_done (directory, file, NULL, NULL, NULL);
	} else {
		if (!async_job_start (directory, "link info")) {
			g_free (dot_directory_uri);
			g_free (uri);
			return;
		}

		directory->details->link_info_read_state = g_new0 (LinkInfoReadState, 1);
		directory->details->link_info_read_state->file = file;
		if (gmc_style_link) {
			directory->details->link_info_read_state->handle = eel_read_file_async
				(uri,
				 link_info_gmc_link_read_callback,
				 link_info_gmc_link_read_more_callback,
				 directory);
		}  else if (is_directory) {
			directory->details->link_info_read_state->handle = eel_read_entire_file_async
				(dot_directory_uri,
				 link_info_nautilus_link_read_callback,
				 directory);
			g_free (dot_directory_uri);
		} else {
			directory->details->link_info_read_state->handle = eel_read_entire_file_async
				(uri,
				 link_info_nautilus_link_read_callback,
				 directory);
		}
	}
	g_free (uri);
}

static void
start_or_stop_io (NautilusDirectory *directory)
{
	NautilusFile *file;

	/* Start or stop reading files. */
	file_list_start_or_stop (directory);

	/* Stop any no longer wanted attribute fetches. */
	file_info_stop (directory);
	directory_count_stop (directory);
	deep_count_stop (directory);
	mime_list_stop (directory);
	top_left_stop (directory);
	link_info_stop (directory);

	/* Take files that are all done off the queue. */
	while (!nautilus_file_queue_is_empty (directory->details->high_priority_queue)) {
		file = nautilus_file_queue_head (directory->details->high_priority_queue);

		if (file_needs_high_priority_work_done (directory, file)) {
			/* Start getting attributes if possible */
			file_info_start (directory, file);
			link_info_start (directory, file);
			return;
		} else {
			move_file_to_low_priority_queue (directory, file);
		}
	}

	/* High priority queue must be empty */
	while (!nautilus_file_queue_is_empty (directory->details->low_priority_queue)) {
		file = nautilus_file_queue_head (directory->details->low_priority_queue);

		if (file_needs_low_priority_work_done (directory, file)) {
			/* Start getting attributes if possible */
			directory_count_start (directory, file);
			deep_count_start (directory, file);
			mime_list_start (directory, file);
			top_left_start (directory, file);
			return;
		} else {
			nautilus_directory_remove_file_from_work_queue (directory, file);

		}
	}
}

/* Call this when the monitor or call when ready list changes,
 * or when some I/O is completed.
 */
void
nautilus_directory_async_state_changed (NautilusDirectory *directory)
{
	/* Check if any callbacks are satisfied and call them if they
	 * are. Do this last so that any changes done in start or stop
	 * I/O functions immediately (not in callbacks) are taken into
	 * consideration. If any callbacks are called, consider the
	 * I/O state again so that we can release or cancel I/O that
	 * is not longer needed once the callbacks are satisfied.
	 */

	if (GTK_OBJECT_DESTROYED (directory)) {
		return;
	}
	if (directory->details->in_async_service_loop) {
		directory->details->state_changed = TRUE;
		return;
	}
	directory->details->in_async_service_loop = TRUE;
	nautilus_directory_ref (directory);
	do {
		directory->details->state_changed = FALSE;
		start_or_stop_io (directory);
		if (call_ready_callbacks (directory)) {
			directory->details->state_changed = TRUE;
		}
	} while (directory->details->state_changed);
	directory->details->in_async_service_loop = FALSE;
	nautilus_directory_unref (directory);

	/* Check if any directories should wake up. */
	async_job_wake_up ();
}

void
nautilus_directory_cancel (NautilusDirectory *directory)
{
	/* Arbitrary order (kept alphabetical). */
	deep_count_cancel (directory);
	directory_count_cancel (directory);
	file_info_cancel (directory);
	file_list_cancel (directory);
	link_info_cancel (directory);
	mime_list_cancel (directory);
	top_left_cancel (directory);

	/* We aren't waiting for anything any more. */
	if (waiting_directories != NULL) {
		g_hash_table_remove (waiting_directories, directory);
	}

	/* Check if any directories should wake up. */
	async_job_wake_up ();
}

static void
cancel_directory_count_for_file (NautilusDirectory *directory,
				 NautilusFile      *file)
{
	if (directory->details->count_file == file) {
		directory_count_cancel (directory);
	}
}

static void
cancel_deep_counts_for_file (NautilusDirectory *directory,
			     NautilusFile      *file)
{
	if (directory->details->deep_count_file == file) {
		deep_count_cancel (directory);
	}
}

static void
cancel_mime_list_for_file (NautilusDirectory *directory,
			   NautilusFile      *file)
{
	if (directory->details->mime_list_file == file) {
		mime_list_cancel (directory);
	}
}

static void
cancel_top_left_text_for_file (NautilusDirectory *directory,
			       NautilusFile      *file)
{
	if (directory->details->top_left_read_state != NULL &&
	    directory->details->top_left_read_state->file == file) {
		top_left_cancel (directory);
	}
}

static void
cancel_file_info_for_file (NautilusDirectory *directory,
			   NautilusFile      *file)
{
	if (directory->details->get_info_file == file) {
		file_info_cancel (directory);
	}
}

static void
cancel_link_info_for_file (NautilusDirectory *directory,
				NautilusFile      *file)
{
	if (directory->details->link_info_read_state != NULL &&
	    directory->details->link_info_read_state->file == file) {
		link_info_cancel (directory);
	}
}


static void
cancel_loading_attributes (NautilusDirectory *directory,
			   GList *file_attributes)
{
	Request request;
	
	nautilus_directory_set_up_request (&request,
					   file_attributes);

	if (request.directory_count) {
		directory_count_cancel (directory);
	}
	if (request.deep_count) {
		deep_count_cancel (directory);
	}
	if (request.mime_list) {
		mime_list_cancel (directory);
	}
	if (request.top_left_text) {
		top_left_cancel (directory);
	}
	if (request.file_info) {
		file_info_cancel (directory);
	}
	if (request.link_info) {
		link_info_cancel (directory);
	}
	
	/* FIXME bugzilla.gnome.org 45064: implement cancelling metadata when we
	   implement invalidating metadata */

	nautilus_directory_async_state_changed (directory);
}

void
nautilus_directory_cancel_loading_file_attributes (NautilusDirectory *directory,
						   NautilusFile      *file,
						   GList             *file_attributes)
{
	Request request;
	
	nautilus_directory_set_up_request (&request,
					   file_attributes);

	if (request.directory_count) {
		cancel_directory_count_for_file (directory, file);
	}
	if (request.deep_count) {
		cancel_deep_counts_for_file (directory, file);
	}
	if (request.mime_list) {
		cancel_mime_list_for_file (directory, file);
	}
	if (request.top_left_text) {
		cancel_top_left_text_for_file (directory, file);
	}
	if (request.file_info) {
		cancel_file_info_for_file (directory, file);
	}
	if (request.link_info) {
		cancel_link_info_for_file (directory, file);
	}

	/* FIXME bugzilla.gnome.org 45064: implement cancelling metadata when we
	   implement invalidating metadata */

	nautilus_directory_async_state_changed (directory);
}



static gboolean
file_needs_high_priority_work_done (NautilusDirectory *directory,
				    NautilusFile *file)
{
	if (is_needy (file, lacks_info, wants_info)) {
		return TRUE;
	}

	if (is_needy (file, lacks_link_info, wants_link_info)) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
file_needs_low_priority_work_done (NautilusDirectory *directory,
				   NautilusFile *file)
{
	if (is_needy (file, lacks_directory_count, wants_directory_count)) {
		return TRUE;
	}

	if (is_needy (file, lacks_deep_count, wants_deep_count)) {
		return TRUE;
	}

	if (is_needy (file, lacks_mime_list, wants_mime_list)) {
		return TRUE;
	}

	if (is_needy (file, lacks_top_left, wants_top_left)) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
file_needs_work_done (NautilusDirectory *directory,
		      NautilusFile *file)
{
	return (file_needs_high_priority_work_done (directory, file) ||
		file_needs_low_priority_work_done (directory, file));
}


void
nautilus_directory_add_file_to_work_queue (NautilusDirectory *directory,
					   NautilusFile *file)
{
	if (!file_needs_work_done (directory, file)) {
		return;
	}

	nautilus_file_queue_enqueue (directory->details->high_priority_queue,
				    file);
}


static void
add_all_files_to_work_queue (NautilusDirectory *directory)
{
	GList *node;
	NautilusFile *file;
	
	for (node = directory->details->file_list; node != NULL; node = node->next) {
		file = NAUTILUS_FILE (node->data);

		nautilus_directory_add_file_to_work_queue (directory, file);
	}
}

void
nautilus_directory_remove_file_from_work_queue (NautilusDirectory *directory,
						NautilusFile *file)
{
	nautilus_file_queue_remove (directory->details->high_priority_queue,
				    file);
	nautilus_file_queue_remove (directory->details->low_priority_queue,
				    file);
}


static void
move_file_to_low_priority_queue (NautilusDirectory *directory,
				 NautilusFile *file)
{
	if (!file_needs_low_priority_work_done (directory, file)) {
		nautilus_file_queue_remove (directory->details->high_priority_queue,
					    file);
		return;
	}

	/* Must add before removing to avoid ref underflow */
	nautilus_file_queue_enqueue (directory->details->low_priority_queue,
				     file);
	nautilus_file_queue_remove (directory->details->high_priority_queue,
				    file);
}
