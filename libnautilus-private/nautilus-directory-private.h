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

#include "nautilus-directory.h"

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

#include <tree.h>

#include "nautilus-file.h"

#define METADATA_NODE_NAME_FOR_FILE_NAME "NAME"

typedef struct MetafileReadState MetafileReadState;
typedef struct MetafileWriteState MetafileWriteState;

struct NautilusDirectoryDetails
{
	char *uri_text;
	GnomeVFSURI *uri;
	GnomeVFSURI *metafile_uri;
	GnomeVFSURI *alternate_metafile_uri;

	GList *files;

	xmlDoc *metafile;

	gboolean metafile_read;
	gboolean use_alternate_metafile;
	MetafileReadState *read_state;

	guint write_metafile_idle_id;
	MetafileWriteState *write_state;

	/* These lists are going to be pretty short.  If we think they
	 * are going to get big, we can use hash tables instead.
	 */
	GList *metafile_callbacks;
	GList *monitors;
	GList *file_monitors;

	gboolean directory_loaded;
	GnomeVFSAsyncHandle *directory_load_in_progress;
	GnomeVFSDirectoryListPosition directory_load_list_last_handled;
	GList *pending_file_info;
        guint dequeue_pending_idle_id;

	GnomeVFSAsyncHandle *count_in_progress;
	NautilusFile *count_file;
};

typedef struct {
	NautilusFile *file; /* Which file to monitor, NULL to monitor all. */
	union {
		NautilusDirectoryCallback directory;
		NautilusFileCallback file;
	} callback;
	gpointer callback_data;
} QueuedCallback;

typedef struct {
	NautilusFile *file; /* Which file to monitor, NULL to monitor all. */
	gconstpointer client;
	gboolean monitor_directory_counts;
} FileMonitor;

typedef struct {
	char *from_uri;
	char *to_uri;
} URIPair;

/* Almost-public change notification calls */
void          nautilus_directory_notify_files_added           (GList                    *uris);
void          nautilus_directory_notify_files_moved           (GList                    *uri_pairs);
void          nautilus_directory_notify_files_removed         (GList                    *uris);

/* async. interface */
void          nautilus_directory_cancel_callback_internal     (NautilusDirectory        *directory,
							       const QueuedCallback     *callback);
void          nautilus_directory_call_when_ready_internal     (NautilusDirectory        *directory,
							       const QueuedCallback     *callback);
void          nautilus_directory_file_monitor_add_internal    (NautilusDirectory        *directory,
							       NautilusFile             *file,
							       gconstpointer             client,
							       GList                    *attributes,
							       GList                    *metadata_keys,
							       NautilusFileListCallback  callback,
							       gpointer                  callback_data);
void          nautilus_directory_file_monitor_remove_internal (NautilusDirectory        *directory,
							       NautilusFile             *file,
							       gconstpointer             client);
gboolean      nautilus_directory_is_file_list_monitored       (NautilusDirectory        *directory);
void          nautilus_directory_remove_file_monitor_link     (NautilusDirectory        *directory,
							       GList                    *link);
void          nautilus_directory_request_read_metafile        (NautilusDirectory        *directory);
void          nautilus_directory_request_write_metafile       (NautilusDirectory        *directory);
void          nautilus_directory_schedule_dequeue_pending     (NautilusDirectory        *directory);
void          nautilus_directory_stop_monitoring_file_list    (NautilusDirectory        *directory);
void          nautilus_metafile_read_cancel                   (NautilusDirectory        *directory);
void          nautilus_metafile_write_start                   (NautilusDirectory        *directory);

/* Calls shared between directory, file, and async. code. */
NautilusFile *nautilus_directory_find_file                    (NautilusDirectory        *directory,
							       const char               *file_name);
char *        nautilus_directory_get_file_metadata            (NautilusDirectory        *directory,
							       const char               *file_name,
							       const char               *key,
							       const char               *default_metadata);
gboolean      nautilus_directory_set_file_metadata            (NautilusDirectory        *directory,
							       const char               *file_name,
							       const char               *key,
							       const char               *default_metadata,
							       const char               *metadata);
xmlNode *     nautilus_directory_get_file_metadata_node       (NautilusDirectory        *directory,
							       const char               *file_name,
							       gboolean                  create);
void          nautilus_directory_emit_metadata_changed        (NautilusDirectory        *directory);
void          nautilus_directory_emit_files_added             (NautilusDirectory        *directory,
							       GList                    *added_files);
void          nautilus_directory_emit_files_changed           (NautilusDirectory        *directory,
							       GList                    *changed_files);

/* debugging functions */
int           nautilus_directory_number_outstanding           (void);

/* Shared functions not directly related to NautilusDirectory/File. */
int           nautilus_compare_file_with_name                 (gconstpointer             a,
							       gconstpointer             b);
void          nautilus_gnome_vfs_file_info_list_free          (GList                    *list);
void          nautilus_gnome_vfs_file_info_list_unref         (GList                    *list);
