/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory-private.h: Nautilus directory model.
 
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

#include "nautilus-directory-metafile-monitor.h"
#include "nautilus-directory.h"
#include "nautilus-file.h"
#include "nautilus-metafile-server.h"
#include "nautilus-monitor.h"
#include "nautilus-file-queue.h"
#include <eel/eel-vfs-extensions.h>
#include <libxml/tree.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus/nautilus-idle-queue.h>

typedef struct LinkInfoReadState LinkInfoReadState;
typedef struct TopLeftTextReadState TopLeftTextReadState;
typedef struct FileMonitors FileMonitors;

struct NautilusDirectoryDetails
{
	/* The location. */
	char *uri;
	GnomeVFSURI *vfs_uri;

	/* The file objects. */
	NautilusFile *as_file;
	GList *file_list;
	GHashTable *file_hash;

	/* Queues of files needing some I/O done. */
	NautilusFileQueue *high_priority_queue;
	NautilusFileQueue *low_priority_queue;

	/* These lists are going to be pretty short.  If we think they
	 * are going to get big, we can use hash tables instead.
	 */
	GList *call_when_ready_list;
	GList *monitor_list;

	NautilusMonitor *monitor;

	NautilusIdleQueue *idle_queue;
	NautilusMetafileMonitor *metafile_monitor;
	Nautilus_Metafile metafile_corba_object;

	gboolean in_async_service_loop;
	gboolean state_changed;

	gboolean file_list_monitored;
	gboolean directory_loaded;
	gboolean directory_loaded_sent_notification;
	GnomeVFSAsyncHandle *directory_load_in_progress;

	GList *pending_file_info; /* list of GnomeVFSFileInfo's that are pending */
	int confirmed_file_count;
        guint dequeue_pending_idle_id;

	NautilusFile *load_directory_file;
	int load_file_count;
	GnomeVFSDirectoryFilter *load_file_count_filter;
	GHashTable *load_mime_list_hash;

	GList *get_file_infos_in_progress; /* list of GnomeVFSAsyncHandle * */

	NautilusFile *count_file;
	GnomeVFSAsyncHandle *count_in_progress;

	NautilusFile *deep_count_file;
	GnomeVFSAsyncHandle *deep_count_in_progress;
	char *deep_count_uri;
	GList *deep_count_subdirectories;

	NautilusFile *mime_list_file;
	GnomeVFSAsyncHandle *mime_list_in_progress;
	GHashTable *mime_list_hash;

	NautilusFile *get_info_file;
	GnomeVFSAsyncHandle *get_info_in_progress;

	TopLeftTextReadState *top_left_read_state;

	LinkInfoReadState *link_info_read_state;

	GList *file_operations_in_progress; /* list of FileOperation * */
};

/* A request for information about one or more files. */
typedef struct {
	gboolean link_info;
	gboolean deep_count;
	gboolean directory_count;
	gboolean file_info;
	gboolean file_list; /* always FALSE if file != NULL */
	gboolean metafile;
	gboolean mime_list;
	gboolean top_left_text;
} Request;

NautilusDirectory *nautilus_directory_get_existing                    (const char                *uri);

/* async. interface */
void               nautilus_directory_async_state_changed             (NautilusDirectory         *directory);
void               nautilus_directory_call_when_ready_internal        (NautilusDirectory         *directory,
								       NautilusFile              *file,
								       GList                     *file_attributes,
								       gboolean                   wait_for_file_list,
								       NautilusDirectoryCallback  directory_callback,
								       NautilusFileCallback       file_callback,
								       gpointer                   callback_data);
gboolean           nautilus_directory_check_if_ready_internal         (NautilusDirectory         *directory,
								       NautilusFile              *file,
								       GList                     *file_attributes);
void               nautilus_directory_cancel_callback_internal        (NautilusDirectory         *directory,
								       NautilusFile              *file,
								       NautilusDirectoryCallback  directory_callback,
								       NautilusFileCallback       file_callback,
								       gpointer                   callback_data);
void               nautilus_directory_monitor_add_internal            (NautilusDirectory         *directory,
								       NautilusFile              *file,
								       gconstpointer              client,
								       gboolean                   monitor_hidden_files,
								       gboolean                   monitor_backup_files,
								       GList                     *attributes,
								       NautilusDirectoryCallback  callback,
								       gpointer                   callback_data);
void               nautilus_directory_monitor_remove_internal         (NautilusDirectory         *directory,
								       NautilusFile              *file,
								       gconstpointer              client);
void               nautilus_directory_get_info_for_new_files          (NautilusDirectory         *directory,
								       GList                     *vfs_uris);
NautilusFile *     nautilus_directory_get_existing_corresponding_file (NautilusDirectory         *directory);
void               nautilus_directory_invalidate_count_and_mime_list  (NautilusDirectory         *directory);
gboolean           nautilus_directory_is_file_list_monitored          (NautilusDirectory         *directory);
gboolean           nautilus_directory_is_anyone_monitoring_file_list  (NautilusDirectory         *directory);
void               nautilus_directory_remove_file_monitor_link        (NautilusDirectory         *directory,
								       GList                     *link);
void               nautilus_directory_schedule_dequeue_pending        (NautilusDirectory         *directory);
void               nautilus_directory_stop_monitoring_file_list       (NautilusDirectory         *directory);
void               nautilus_directory_cancel                          (NautilusDirectory         *directory);
void               nautilus_async_destroying_file                     (NautilusFile              *file);
void               nautilus_directory_force_reload_internal           (NautilusDirectory         *directory,
								       GList                     *file_attributes);
void               nautilus_directory_cancel_loading_file_attributes  (NautilusDirectory         *directory,
								       NautilusFile              *file,
								       GList                     *file_attributes);

/* Calls shared between directory, file, and async. code. */
void               nautilus_directory_emit_files_added                (NautilusDirectory         *directory,
								       GList                     *added_files);
void               nautilus_directory_emit_files_changed              (NautilusDirectory         *directory,
								       GList                     *changed_files);
void               nautilus_directory_emit_change_signals             (NautilusDirectory         *directory,
								       GList                     *changed_files);
void               emit_change_signals_for_all_files		      (NautilusDirectory	 *directory);
void               nautilus_directory_emit_done_loading               (NautilusDirectory         *directory);
void               nautilus_directory_emit_load_error                 (NautilusDirectory         *directory,
								       GnomeVFSResult             error_result);
NautilusDirectory *nautilus_directory_get_internal                    (const char                *uri,
								       gboolean                   create);
char *             nautilus_directory_get_name_for_self_as_new_file   (NautilusDirectory         *directory);
void               nautilus_directory_set_up_request                  (Request                   *request,
								       GList                     *file_attributes);

/* Interface to the file list. */
NautilusFile *     nautilus_directory_find_file_by_name               (NautilusDirectory         *directory,
								       const char                *relative_uri);
NautilusFile *     nautilus_directory_find_file_by_relative_uri       (NautilusDirectory         *directory,
								       const char                *relative_uri);
NautilusFile *     nautilus_directory_find_file_by_internal_uri       (NautilusDirectory         *directory,
								       const char                *relative_uri);

void               nautilus_directory_add_file                        (NautilusDirectory         *directory,
								       NautilusFile              *file);
void               nautilus_directory_remove_file                     (NautilusDirectory         *directory,
								       NautilusFile              *file);
FileMonitors *     nautilus_directory_remove_file_monitors            (NautilusDirectory         *directory,
								       NautilusFile              *file);
void               nautilus_directory_add_file_monitors               (NautilusDirectory         *directory,
								       NautilusFile              *file,
								       FileMonitors              *monitors);
void               nautilus_directory_add_file                        (NautilusDirectory         *directory,
								       NautilusFile              *file);
GList *            nautilus_directory_begin_file_name_change          (NautilusDirectory         *directory,
								       NautilusFile              *file);
void               nautilus_directory_end_file_name_change            (NautilusDirectory         *directory,
								       NautilusFile              *file,
								       GList                     *node);
void               nautilus_directory_moved                           (const char                *from_uri,
								       const char                *to_uri);
/* Interface to the work queue. */

void               nautilus_directory_add_file_to_work_queue          (NautilusDirectory *directory,
								       NautilusFile *file);
void               nautilus_directory_remove_file_from_work_queue     (NautilusDirectory *directory,
								       NautilusFile *file);

/* debugging functions */
int                nautilus_directory_number_outstanding              (void);
