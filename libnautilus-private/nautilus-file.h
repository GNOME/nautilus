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
typedef struct NautilusFileClass NautilusFileClass;

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
	NAUTILUS_FILE_SORT_BY_SIZE,
	NAUTILUS_FILE_SORT_BY_TYPE,
	NAUTILUS_FILE_SORT_BY_MTIME,
	NAUTILUS_FILE_SORT_BY_EMBLEMS
} NautilusFileSortType;	

typedef void (*NautilusFileCallback) (NautilusFile *file,
				      gpointer      callback_data);

/* GtkObject requirements. */
GtkType          nautilus_file_get_type                  (void);

/* Getting at a single file. */
NautilusFile *   nautilus_file_get                       (const char           *uri);

/* Basic operations on file objects. */
NautilusFile *   nautilus_file_ref                       (NautilusFile         *file);
void             nautilus_file_unref                     (NautilusFile         *file);
void             nautilus_file_delete                    (NautilusFile         *file);

/* Monitor the file. */
void             nautilus_file_monitor_add               (NautilusFile         *file,
							  gconstpointer         client,
							  GList                *attributes,
							  GList                *metadata_keys);
void             nautilus_file_monitor_remove            (NautilusFile         *file,
							  gconstpointer         client);

/* Waiting for data that's read asynchronously.
 * This interface currently works only for metadata, but could be expanded
 * to other attributes as well.
 */
void             nautilus_file_call_when_ready           (NautilusFile         *file,
							  GList                *attributes,
							  GList                *metadata_keys,
							  NautilusFileCallback  callback,
							  gpointer              callback_data);
void             nautilus_file_cancel_callback           (NautilusFile         *file,
							  NautilusFileCallback  callback,
							  gpointer              callback_data);

/* Basic attributes for file objects. */
char *           nautilus_file_get_name                  (NautilusFile         *file);
char *           nautilus_file_get_uri                   (NautilusFile         *file);
char *           nautilus_file_get_mapped_uri            (NautilusFile         *file);
GnomeVFSFileSize nautilus_file_get_size                  (NautilusFile         *file);
GnomeVFSFileType nautilus_file_get_file_type             (NautilusFile         *file);
const char *     nautilus_file_get_mime_type             (NautilusFile         *file);
gboolean         nautilus_file_is_symbolic_link          (NautilusFile         *file);
gboolean         nautilus_file_is_executable             (NautilusFile         *file);
gboolean         nautilus_file_is_directory              (NautilusFile         *file);
gboolean         nautilus_file_get_directory_item_count  (NautilusFile         *file,
							  guint                *count,
							  gboolean             *count_unreadable);
GList *          nautilus_file_get_keywords              (NautilusFile         *file);
void             nautilus_file_set_keywords              (NautilusFile         *file,
							  GList                *keywords);
GList *          nautilus_file_get_emblem_names          (NautilusFile         *file);

/* Basic operations for file objects. */
gboolean         nautilus_file_can_read                  (NautilusFile         *file);
gboolean         nautilus_file_can_write                 (NautilusFile         *file);
gboolean         nautilus_file_can_execute               (NautilusFile         *file);
gboolean         nautilus_file_can_rename                (NautilusFile         *file);
GnomeVFSResult   nautilus_file_rename                    (NautilusFile         *file,
							  const char           *new_name);

/* Return true if this file has already been deleted.
   This object will be unref'd after sending the files_removed signal,
   but it could hang around longer if someone ref'd it.
*/
gboolean         nautilus_file_is_gone                   (NautilusFile         *file);

/* Simple getting and setting top-level metadata. */
char *           nautilus_file_get_metadata              (NautilusFile         *file,
							  const char           *key,
							  const char           *default_metadata);
void             nautilus_file_set_metadata              (NautilusFile         *file,
							  const char           *key,
							  const char           *default_metadata,
							  const char           *metadata);

/* Attributes for file objects as user-displayable strings. */
char *           nautilus_file_get_string_attribute      (NautilusFile         *file,
							  const char           *attribute_name);

/* Matching with another URI*/
gboolean         nautilus_file_matches_uri               (NautilusFile         *file,
							  const char           *uri);

/* Compare file's state with a fresh file info struct, return FALSE if no change,
   update file and return TRUE if the file info contains new state.
 */
gboolean         nautilus_file_update                    (NautilusFile         *file,
							  GnomeVFSFileInfo     *info);
/* Comparing two file objects for sorting */
int              nautilus_file_compare_for_sort          (NautilusFile         *file_1,
							  NautilusFile         *file_2,
							  NautilusFileSortType  sort_type);
int              nautilus_file_compare_for_sort_reversed (NautilusFile         *file_1,
							  NautilusFile         *file_2,
							  NautilusFileSortType  sort_type);

/* Change notification hack.
 * This is called when code modifies the file and it needs to trigger
 * a notification. Eventually this should become private, but for now
 * it needs to be used for code like the thumbnail generation.
 */
void             nautilus_file_changed                   (NautilusFile         *file);

/* Convenience functions for dealing with a list of NautilusFile objects that each have a ref. */
GList *          nautilus_file_list_ref                  (GList                *file_list);
void             nautilus_file_list_unref                (GList                *file_list);
void             nautilus_file_list_free                 (GList                *file_list);
GList *          nautilus_file_list_copy                 (GList                *file_list);

typedef struct NautilusFileDetails NautilusFileDetails;

struct NautilusFile
{
	GtkObject object;
	NautilusFileDetails *details;
};

struct NautilusFileClass
{
	GtkObjectClass parent_class;
	
	void (* changed) (NautilusFile *file);
};

#endif /* NAUTILUS_FILE_H */
