/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-file.h: Nautilus file model.
 
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

#ifndef NAUTILUS_FILE_H
#define NAUTILUS_FILE_H

#include <gtk/gtkobject.h>
#include <libgnomevfs/gnome-vfs-types.h>

/* NautilusFile is an object used to represent a single element of a
 * NautilusDirectory. It's lightweight and relies on NautilusDirectory
 * to do most of the work.
 */

/* NautilusFile is defined both here and in nautilus-directory.h. */
#ifndef NAUTILUS_FILE_DEFINED
#define NAUTILUS_FILE_DEFINED
typedef struct NautilusFile NautilusFile;
#endif

#define NAUTILUS_TYPE_FILE \
	(nautilus_file_get_type ())
#define NAUTILUS_FILE(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_FILE, NautilusFile))
#define NAUTILUS_FILE_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_FILE, NautilusFileClass))
#define NAUTILUS_IS_FILE(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_FILE))
#define NAUTILUS_IS_FILE_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_FILE))

typedef enum {
	NAUTILUS_FILE_SORT_NONE,
	NAUTILUS_FILE_SORT_BY_NAME,
	NAUTILUS_FILE_SORT_BY_DIRECTORY,
	NAUTILUS_FILE_SORT_BY_SIZE,
	NAUTILUS_FILE_SORT_BY_TYPE,
	NAUTILUS_FILE_SORT_BY_MTIME,
	NAUTILUS_FILE_SORT_BY_EMBLEMS
} NautilusFileSortType;	

typedef enum {
	NAUTILUS_REQUEST_NOT_STARTED,
	NAUTILUS_REQUEST_IN_PROGRESS,
	NAUTILUS_REQUEST_DONE
} NautilusRequestStatus;

typedef void (*NautilusFileCallback)          (NautilusFile  *file,
				               gpointer       callback_data);
typedef void (*NautilusFileOperationCallback) (NautilusFile  *file,
					       GnomeVFSResult result,
					       gpointer       callback_data);

/* GtkObject requirements. */
GtkType                 nautilus_file_get_type                  (void);

/* Getting at a single file. */
NautilusFile *          nautilus_file_get                       (const char                    *uri);

/* Basic operations on file objects. */
NautilusFile *          nautilus_file_ref                       (NautilusFile                  *file);
void                    nautilus_file_unref                     (NautilusFile                  *file);
void                    nautilus_file_delete                    (NautilusFile                  *file);

/* Monitor the file. */
void                    nautilus_file_monitor_add               (NautilusFile                  *file,
								 gconstpointer                  client,
								 GList                         *attributes,
								 gboolean                       monitor_metadata);
void                    nautilus_file_monitor_remove            (NautilusFile                  *file,
								 gconstpointer                  client);

/* Waiting for data that's read asynchronously.
 * This interface currently works only for metadata, but could be expanded
 * to other attributes as well.
 */
void                    nautilus_file_call_when_ready           (NautilusFile                  *file,
								 GList                         *attributes,
								 gboolean                       wait_for_metadata,
								 NautilusFileCallback           callback,
								 gpointer                       callback_data);
void                    nautilus_file_cancel_call_when_ready    (NautilusFile                  *file,
								 NautilusFileCallback           callback,
								 gpointer                       callback_data);
gboolean		nautilus_file_check_if_ready		(NautilusFile		       *file,
								 GList			       *attributes);

/* Basic attributes for file objects. */
char *                  nautilus_file_get_name                  (NautilusFile                  *file);
char *                  nautilus_file_get_uri                   (NautilusFile                  *file);
GnomeVFSFileSize        nautilus_file_get_size                  (NautilusFile                  *file);
GnomeVFSFileType        nautilus_file_get_file_type             (NautilusFile                  *file);
char *                  nautilus_file_get_mime_type             (NautilusFile                  *file);
gboolean                nautilus_file_is_mime_type              (NautilusFile                  *file,
								 const char                    *mime_type);
gboolean                nautilus_file_is_symbolic_link          (NautilusFile                  *file);
gboolean                nautilus_file_is_executable             (NautilusFile                  *file);
gboolean                nautilus_file_is_directory              (NautilusFile                  *file);
gboolean                nautilus_file_is_in_trash               (NautilusFile                  *file);
gboolean                nautilus_file_get_directory_item_count  (NautilusFile                  *file,
								 guint                         *count,
								 gboolean                      *count_unreadable);
void                    nautilus_file_recompute_deep_counts     (NautilusFile                  *file);
NautilusRequestStatus   nautilus_file_get_deep_counts           (NautilusFile                  *file,
								 guint                         *directory_count,
								 guint                         *file_count,
								 guint                         *unreadable_directory_count,
								 GnomeVFSFileSize              *total_size);
GList *                 nautilus_file_get_keywords              (NautilusFile                  *file);
void                    nautilus_file_set_keywords              (NautilusFile                  *file,
								 GList                         *keywords);
GList *                 nautilus_file_get_emblem_names          (NautilusFile                  *file);
char *                  nautilus_file_get_top_left_text         (NautilusFile                  *file);


/* Attributes that behave specially for search results */
gboolean                nautilus_file_is_search_result          (NautilusFile                  *file);
char *                  nautilus_file_get_real_name             (NautilusFile                  *file);
char *                  nautilus_file_get_real_directory        (NautilusFile                  *file);


/* Permissions. */
gboolean                nautilus_file_can_get_permissions       (NautilusFile                  *file);
gboolean                nautilus_file_can_set_permissions       (NautilusFile                  *file);
GnomeVFSFilePermissions nautilus_file_get_permissions           (NautilusFile                  *file);

gboolean		nautilus_file_can_get_owner		(NautilusFile		       *file);
gboolean		nautilus_file_can_set_owner		(NautilusFile		       *file);
gboolean		nautilus_file_can_get_group		(NautilusFile		       *file);
gboolean		nautilus_file_can_set_group		(NautilusFile		       *file);
char *			nautilus_file_get_owner_name		(NautilusFile		       *file);
char *			nautilus_file_get_group_name		(NautilusFile		       *file);
GList *			nautilus_get_user_names			(void);
GList *			nautilus_get_group_names		(void);
GList *			nautilus_file_get_settable_group_names	(NautilusFile		       *file);

/* "Capabilities". */
gboolean                nautilus_file_can_read                  (NautilusFile                  *file);
gboolean                nautilus_file_can_write                 (NautilusFile                  *file);
gboolean                nautilus_file_can_execute               (NautilusFile                  *file);
gboolean                nautilus_file_can_rename                (NautilusFile                  *file);

/* Basic operations for file objects. */
void			nautilus_file_set_owner			(NautilusFile		       *file,
								 const char		       *user_name_or_id,
								 NautilusFileOperationCallback  callback,
								 gpointer                       callback_data);
void			nautilus_file_set_group			(NautilusFile		       *file,
								 const char		       *group_name_or_id,
								 NautilusFileOperationCallback  callback,
								 gpointer                       callback_data);
void                    nautilus_file_set_permissions           (NautilusFile                  *file,
								 GnomeVFSFilePermissions        permissions,
								 NautilusFileOperationCallback  callback,
								 gpointer                       callback_data);
void                    nautilus_file_rename                    (NautilusFile                  *file,
								 const char                    *new_name,
								 NautilusFileOperationCallback  callback,
								 gpointer                       callback_data);
gboolean                nautilus_file_is_rename_in_progress     (NautilusFile                  *file,
								 char                         **old_name,
								 char                         **new_name);
void                    nautilus_file_cancel                    (NautilusFile                  *file,
								 NautilusFileOperationCallback  callback,
								 gpointer                       callback_data);

/* Return true if this file has already been deleted.
 * This object will be unref'd after sending the files_removed signal,
 * but it could hang around longer if someone ref'd it.
 */
gboolean                nautilus_file_is_gone                   (NautilusFile                  *file);

/* Simple getting and setting top-level metadata. */
char *                  nautilus_file_get_metadata              (NautilusFile                  *file,
								 const char                    *key,
								 const char                    *default_metadata);
GList *                 nautilus_file_get_metadata_list         (NautilusFile                  *file,
								 const char                    *list_key,
								 const char                    *list_subkey);
void                    nautilus_file_set_metadata              (NautilusFile                  *file,
								 const char                    *key,
								 const char                    *default_metadata,
								 const char                    *metadata);
void                    nautilus_file_set_metadata_list         (NautilusFile                  *file,
								 const char                    *list_key,
								 const char                    *list_subkey,
								 GList                         *list);

/* Attributes for file objects as user-displayable strings. */
char *                  nautilus_file_get_string_attribute      (NautilusFile                  *file,
								 const char                    *attribute_name);
char *                  nautilus_file_get_string_attribute_with_default      
								(NautilusFile                  *file,
								 const char                    *attribute_name);

/* Matching with another URI. */
gboolean                nautilus_file_matches_uri               (NautilusFile                  *file,
								 const char                    *uri);

/* Is the file local? */
gboolean                nautilus_file_is_local                  (NautilusFile                  *file);

/* Comparing two file objects for sorting */
int                     nautilus_file_compare_for_sort          (NautilusFile                  *file_1,
								 NautilusFile                  *file_2,
								 NautilusFileSortType           sort_type);
int                     nautilus_file_compare_for_sort_reversed (NautilusFile                  *file_1,
								 NautilusFile                  *file_2,
								 NautilusFileSortType           sort_type);
int                     nautilus_file_compare_name		(NautilusFile                  *file_1,
								 const char		       *pattern);

/* Get the URI that's used when activating the file.
 * Getting this can require reading the contents of the file.
 */
char *                  nautilus_file_get_activation_uri        (NautilusFile                  *file);

/* Change notification hack.
 * This is called when code modifies the file and it needs to trigger
 * a notification. Eventually this should become private, but for now
 * it needs to be used for code like the thumbnail generation.
 */
void                    nautilus_file_changed                   (NautilusFile                  *file);

/* Convenience functions for dealing with a list of NautilusFile objects that each have a ref. */
GList *                 nautilus_file_list_ref                  (GList                         *file_list);
void                    nautilus_file_list_unref                (GList                         *file_list);
void                    nautilus_file_list_free                 (GList                         *file_list);
GList *                 nautilus_file_list_copy                 (GList                         *file_list);

/* Debugging */
void			nautilus_file_dump			(NautilusFile		       *file);

typedef struct NautilusFileDetails NautilusFileDetails;

struct NautilusFile {
	GtkObject parent_slot;
	NautilusFileDetails *details;
};

typedef struct {
	GtkObjectClass parent_slot;
	
	/* Called then the file notices any change. */
	void (* changed) (NautilusFile *file);
} NautilusFileClass;

#endif /* NAUTILUS_FILE_H */
