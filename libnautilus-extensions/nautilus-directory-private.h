/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory-private.h: Nautilus directory model.
 
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
#include "nautilus-file-utilities.h"

typedef struct ActivationURIReadState ActivationURIReadState;
typedef struct MetafileReadState MetafileReadState;
typedef struct MetafileWriteState MetafileWriteState;
typedef struct TopLeftTextReadState TopLeftTextReadState;

struct NautilusDirectoryDetails
{
	/* The location. */
	char *uri_text;
	GnomeVFSURI *uri;
	GnomeVFSURI *private_metafile_uri;
	GnomeVFSURI *public_metafile_uri;

	/* The file objects. */
	NautilusFile *as_file;
	GList *files;

	/* The metadata. */
	gboolean metafile_read;
	xmlDoc *metafile;
	GHashTable *metadata_changes;

	/* State for reading and writing metadata. */
	MetafileReadState *metafile_read_state;
	guint write_metafile_idle_id;
	MetafileWriteState *metafile_write_state;

	/* These lists are going to be pretty short.  If we think they
	 * are going to get big, we can use hash tables instead.
	 */
	GList *call_when_ready_list;
	GList *monitor_list;

	gboolean file_list_monitored;
	gboolean directory_loaded;
	gboolean directory_loaded_sent_notification;
	GnomeVFSAsyncHandle *directory_load_in_progress;
	GnomeVFSDirectoryListPosition directory_load_list_last_handled;

	GList *pending_file_info; /* list of GnomeVFSFileInfo's that are pending */
        guint dequeue_pending_idle_id;

	GList *get_file_infos_in_progress; /* list of GnomeVFSAsyncHandle * */

	GnomeVFSAsyncHandle *count_in_progress;
	NautilusFile *count_file;

	NautilusFile *deep_count_file;
	GnomeVFSAsyncHandle *deep_count_in_progress;
	char *deep_count_uri;
	GnomeVFSDirectoryListPosition deep_count_last_handled;
	GList *deep_count_subdirectories;

	GnomeVFSAsyncHandle *get_info_in_progress;
	NautilusFile *get_info_file;

	TopLeftTextReadState *top_left_read_state;
	ActivationURIReadState *activation_uri_read_state;

	GList *file_operations_in_progress; /* list of FileOperation * */
};

typedef struct {
	char *from_uri;
	char *to_uri;
} URIPair;

/* Almost-public change notification calls */
void          nautilus_directory_notify_files_added        (GList                     *uris);
void          nautilus_directory_notify_files_moved        (GList                     *uri_pairs);
void          nautilus_directory_notify_files_removed      (GList                     *uris);

/* async. interface */
void          nautilus_directory_async_state_changed       (NautilusDirectory         *directory);
void          nautilus_directory_call_when_ready_internal  (NautilusDirectory         *directory,
							    NautilusFile              *file,
							    GList                     *file_attributes,
							    gboolean                   monitor_metadata,
							    NautilusDirectoryCallback  directory_callback,
							    NautilusFileCallback       file_callback,
							    gpointer                   callback_data);
gboolean      nautilus_directory_check_if_ready_internal   (NautilusDirectory         *directory,
							    NautilusFile              *file,
							    GList                     *file_attributes);
void          nautilus_directory_cancel_callback_internal  (NautilusDirectory         *directory,
							    NautilusFile              *file,
							    NautilusDirectoryCallback  directory_callback,
							    NautilusFileCallback       file_callback,
							    gpointer                   callback_data);
void          nautilus_directory_monitor_add_internal      (NautilusDirectory         *directory,
							    NautilusFile              *file,
							    gconstpointer              client,
							    GList                     *attributes,
							    gboolean                   monitor_metadata,
							    NautilusDirectoryCallback  callback,
							    gpointer                   callback_data);
void          nautilus_directory_monitor_remove_internal   (NautilusDirectory         *directory,
							    NautilusFile              *file,
							    gconstpointer              client);
void          nautilus_directory_get_info_for_new_files    (NautilusDirectory         *directory,
							    GList                     *vfs_uris);
gboolean      nautilus_directory_is_file_list_monitored    (NautilusDirectory         *directory);
void          nautilus_directory_remove_file_monitor_link  (NautilusDirectory         *directory,
							    GList                     *link);
void          nautilus_directory_request_read_metafile     (NautilusDirectory         *directory);
void          nautilus_directory_request_write_metafile    (NautilusDirectory         *directory);
void          nautilus_directory_schedule_dequeue_pending  (NautilusDirectory         *directory);
void          nautilus_directory_stop_monitoring_file_list (NautilusDirectory         *directory);
void          nautilus_directory_cancel                    (NautilusDirectory         *directory);
void          nautilus_metafile_write_start                (NautilusDirectory         *directory);
void          nautilus_async_destroying_file               (NautilusFile              *file);
void          nautilus_directory_force_reload              (NautilusDirectory         *directory);

/* Calls shared between directory, file, and async. code. */
NautilusFile *nautilus_directory_find_file                 (NautilusDirectory         *directory,
							    const char                *file_name);
void          nautilus_directory_emit_metadata_changed     (NautilusDirectory         *directory);
void          nautilus_directory_emit_files_added          (NautilusDirectory         *directory,
							    GList                     *added_files);
void          nautilus_directory_emit_files_changed        (NautilusDirectory         *directory,
							    GList                     *changed_files);
void          nautilus_directory_emit_done_loading         (NautilusDirectory         *directory);


/* debugging functions */
int           nautilus_directory_number_outstanding        (void);

/* Shared functions not directly related to NautilusDirectory/File. */
int           nautilus_compare_file_with_name              (gconstpointer              a,
							    gconstpointer              b);
