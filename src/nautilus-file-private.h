/*
   nautilus-file-private.h:
 
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
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#pragma once

#include "nautilus-directory.h"
#include "nautilus-file.h"
#include "nautilus-monitor.h"
#include "nautilus-file-undo-operations.h"

#define NAUTILUS_FILE_DEFAULT_ATTRIBUTES				\
	"standard::*,access::*,mountable::*,time::*,unix::*,owner::*,selinux::*,thumbnail::*,id::filesystem,trash::orig-path,trash::deletion-date,metadata::*,recent::*,preview::icon"

/* These are in the typical sort order. Known things come first, then
 * things where we can't know, finally things where we don't yet know.
 */
typedef enum {
	KNOWN,
	UNKNOWABLE,
	UNKNOWN
} Knowledge;

struct NautilusFilePrivate
{
	NautilusDirectory *directory;
	
	GRefString *name;

	/* File info: */
	GFileType type;

	GRefString *display_name;
	char *display_name_collation_key;
	char *directory_name_collation_key;
	GRefString *edit_name;

	goffset size; /* -1 is unknown */
	
	int sort_order;
	
	guint32 permissions;
	int uid; /* -1 is none */
	int gid; /* -1 is none */

	GRefString *owner;
	GRefString *owner_real;
	GRefString *group;
	
	time_t atime; /* 0 is unknown */
	time_t mtime; /* 0 is unknown */
	time_t btime; /* 0 is unknown */
	
	char *symlink_name;
	
	GRefString *mime_type;
	
	char *selinux_context;
	
	GError *get_info_error;
	
	guint directory_count;

	guint deep_directory_count;
	guint deep_file_count;
	guint deep_unreadable_count;
	goffset deep_size;

	GIcon *icon;
	
	char *thumbnail_path;
	GdkPixbuf *thumbnail;
	time_t thumbnail_mtime;

	GList *mime_list; /* If this is a directory, the list of MIME types in it. */

	/* Info you might get from a link (.desktop, .directory or nautilus link) */
	GIcon *custom_icon;
	char *activation_uri;

	/* used during DND, for checking whether source and destination are on
	 * the same file system.
	 */
	GRefString *filesystem_id;

	char *trash_orig_path;

	/* The following is for file operations in progress. Since
	 * there are normally only a few of these, we can move them to
	 * a separate hash table or something if required to keep the
	 * file objects small.
	 */
	GList *operations_in_progress;

	/* NautilusInfoProviders that need to be run for this file */
	GList *pending_info_providers;

	/* Emblems provided by extensions */
	GList *extension_emblems;
	GList *pending_extension_emblems;

	/* Attributes provided by extensions */
	GHashTable *extension_attributes;
	GHashTable *pending_extension_attributes;

	GHashTable *metadata;

	/* Mount for mountpoint or the references GMount for a "mountable" */
	GMount *mount;
	
	/* boolean fields: bitfield to save space, since there can be
           many NautilusFile objects. */

	guint unconfirmed                   : 1;
	guint is_gone                       : 1;
	/* Set when emitting files_added on the directory to make sure we
	   add a file, and only once */
	guint is_added                      : 1;
	/* Set by the NautilusDirectory while it's loading the file
	 * list so the file knows not to do redundant I/O.
	 */
	guint loading_directory             : 1;
	guint got_file_info                 : 1;
	guint get_info_failed               : 1;
	guint file_info_is_up_to_date       : 1;
	
	guint got_directory_count           : 1;
	guint directory_count_failed        : 1;
	guint directory_count_is_up_to_date : 1;

	guint deep_counts_status      : 2; /* NautilusRequestStatus */
	/* no deep_counts_are_up_to_date field; since we expose
           intermediate values for this attribute, we do actually
           forget it rather than invalidating. */

	guint got_mime_list                 : 1;
	guint mime_list_failed              : 1;
	guint mime_list_is_up_to_date       : 1;

	guint mount_is_up_to_date           : 1;
	
	guint got_custom_display_name       : 1;

	guint thumbnail_is_up_to_date       : 1;
	guint thumbnailing_failed           : 1;
	
	guint is_thumbnailing               : 1;

	guint is_symlink                    : 1;
	guint is_mountpoint                 : 1;
	guint is_hidden                     : 1;

	guint has_permissions               : 1;
	
	guint can_read                      : 1;
	guint can_write                     : 1;
	guint can_execute                   : 1;
	guint can_delete                    : 1;
	guint can_trash                     : 1;
	guint can_rename                    : 1;
	guint can_mount                     : 1;
	guint can_unmount                   : 1;
	guint can_eject                     : 1;
	guint can_start                     : 1;
	guint can_start_degraded            : 1;
	guint can_stop                      : 1;
	guint start_stop_type               : 3; /* GDriveStartStopType */
	guint can_poll_for_media            : 1;
	guint is_media_check_automatic      : 1;
	guint has_preview_icon              : 1;

	guint filesystem_readonly           : 1;
	guint filesystem_use_preview        : 2; /* GFilesystemPreviewType */
	guint filesystem_info_is_up_to_date : 1;
	guint filesystem_remote             : 1;

	time_t trash_time; /* 0 is unknown */
	time_t recency; /* 0 is unknown */

	gdouble search_relevance;
	gchar *fts_snippet;

	guint64 free_space; /* (guint)-1 for unknown */
	time_t free_space_read; /* The time free_space was updated, or 0 for never */
};

typedef struct {
	NautilusFile *file;
	GList *files;
	gint renamed_files;
	gint skipped_files;
	GCancellable *cancellable;
	NautilusFileOperationCallback callback;
	gpointer callback_data;
	gboolean is_rename;
	
	gpointer data;
	GDestroyNotify free_data;
	NautilusFileUndoInfo *undo_info;
} NautilusFileOperation;

NautilusFile *nautilus_file_new_from_info                  (NautilusDirectory      *directory,
							    GFileInfo              *info);
NautilusFile *nautilus_file_new_from_filename              (NautilusDirectory *directory,
                                                            const char        *filename,
                                                            gboolean           self_owned);
void          nautilus_file_emit_changed                   (NautilusFile           *file);
void          nautilus_file_mark_gone                      (NautilusFile           *file);

gboolean      nautilus_file_get_date                       (NautilusFile           *file,
							    NautilusDateType        date_type,
							    time_t                 *date);
void          nautilus_file_updated_deep_count_in_progress (NautilusFile           *file);


void          nautilus_file_clear_info                     (NautilusFile           *file);
/* Compare file's state with a fresh file info struct, return FALSE if
 * no change, update file and return TRUE if the file info contains
 * new state.  */
gboolean      nautilus_file_update_info                    (NautilusFile           *file,
							    GFileInfo              *info);
gboolean      nautilus_file_update_name                    (NautilusFile           *file,
							    const char             *name);
gboolean      nautilus_file_update_metadata_from_info      (NautilusFile           *file,
							    GFileInfo              *info);

gboolean      nautilus_file_update_name_and_directory      (NautilusFile           *file,
							    const char             *name,
							    NautilusDirectory      *directory);

gboolean      nautilus_file_set_display_name               (NautilusFile           *file,
							    const char             *display_name,
							    const char             *edit_name,
							    gboolean                custom);
NautilusDirectory *
              nautilus_file_get_directory                  (NautilusFile           *file);
void          nautilus_file_set_directory                  (NautilusFile           *file,
							    NautilusDirectory      *directory);
void          nautilus_file_set_mount                      (NautilusFile           *file,
							    GMount                 *mount);

/* Mark specified attributes for this file out of date without canceling current
 * I/O or kicking off new I/O.
 */
void                   nautilus_file_invalidate_attributes_internal     (NautilusFile           *file,
									 NautilusFileAttributes  file_attributes);
NautilusFileAttributes nautilus_file_get_all_attributes                 (void);
gboolean               nautilus_file_is_self_owned                      (NautilusFile           *file);
void                   nautilus_file_invalidate_count_and_mime_list     (NautilusFile           *file);
gboolean               nautilus_file_rename_in_progress                 (NautilusFile           *file);
void                   nautilus_file_invalidate_extension_info_internal (NautilusFile           *file);
void                   nautilus_file_info_providers_done                (NautilusFile           *file);


/* Thumbnailing: */
void          nautilus_file_set_is_thumbnailing            (NautilusFile           *file,
							    gboolean                is_thumbnailing);
gboolean          nautilus_file_set_thumbnail              (NautilusFile           *file,
                                                            GdkPixbuf              *pixbuf);

NautilusFileOperation *nautilus_file_operation_new      (NautilusFile                  *file,
							 NautilusFileOperationCallback  callback,
							 gpointer                       callback_data);
void                   nautilus_file_operation_free     (NautilusFileOperation         *op);
void                   nautilus_file_operation_complete (NautilusFileOperation         *op,
							 GFile                         *result_location,
							 GError                        *error);
void                   nautilus_file_operation_cancel   (NautilusFileOperation         *op);
