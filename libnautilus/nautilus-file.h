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

#include <glib.h> /* gnome-vfs-types.h requires glib.h but doesn't include it */
#include <libgnomevfs/gnome-vfs-types.h>

/* NautilusFile is an object used to represent a single element of a
 * NautilusDirectory. It's lightweight and relies on NautilusDirectory
 * to do most of the work.
 */

typedef enum {
	NAUTILUS_FILE_SORT_NONE,
	NAUTILUS_FILE_SORT_BY_NAME,
	NAUTILUS_FILE_SORT_BY_SIZE,
	NAUTILUS_FILE_SORT_BY_TYPE,
	NAUTILUS_FILE_SORT_BY_MTIME
} NautilusFileSortType;	

typedef struct NautilusFile NautilusFile;

#define NAUTILUS_IS_FILE(object) \
	((object) != NULL)
#define NAUTILUS_FILE(file) \
	((NautilusFile *)(file))

/* Getting at a single file. */
NautilusFile *     nautilus_file_get                       (const char               *uri);

/* Basic operations on file objects. */
void               nautilus_file_ref                       (NautilusFile             *file);
void               nautilus_file_unref                     (NautilusFile             *file);
void               nautilus_file_delete                    (NautilusFile             *file);

/* Basic attributes for file objects. */
char *             nautilus_file_get_name                  (NautilusFile             *file);
char *             nautilus_file_get_uri                   (NautilusFile             *file);
GnomeVFSFileSize   nautilus_file_get_size                  (NautilusFile             *file);
GnomeVFSFileType   nautilus_file_get_type                  (NautilusFile             *file);
const char *       nautilus_file_get_mime_type             (NautilusFile             *file);
gboolean           nautilus_file_is_symbolic_link          (NautilusFile             *file);
gboolean           nautilus_file_is_executable             (NautilusFile             *file);
gboolean           nautilus_file_is_directory              (NautilusFile             *file);
guint              nautilus_file_get_directory_item_count  (NautilusFile             *file,
							    gboolean                  ignore_invisible_items);
GList *            nautilus_file_get_keywords              (NautilusFile             *file);
void               nautilus_file_set_keywords              (NautilusFile             *file,
							    GList                    *keywords);

/* Simple getting and setting top-level metadata. */
char *             nautilus_file_get_metadata              (NautilusFile             *file,
							    const char               *tag,
							    const char               *default_metadata);
void               nautilus_file_set_metadata              (NautilusFile             *file,
							    const char               *tag,
							    const char               *default_metadata,
							    const char               *metadata);

/* Attributes for file objects as user-displayable strings. */
char *             nautilus_file_get_string_attribute      (NautilusFile             *file,
							    const char               *attribute_name);

/* Comparing two file objects for sorting */
int                nautilus_file_compare_for_sort          (NautilusFile             *file_1,
							    NautilusFile             *file_2,
							    NautilusFileSortType      sort_type);
int                nautilus_file_compare_for_sort_reversed (NautilusFile             *file_1,
							    NautilusFile             *file_2,
							    NautilusFileSortType      sort_type);

/* Convenience functions for dealing with a list of NautilusFile objects that each have a ref. */
void               nautilus_file_list_ref                  (GList                    *file_list);
void               nautilus_file_list_unref                (GList                    *file_list);
void               nautilus_file_list_free                 (GList                    *file_list);

/* Return true if this file has already been deleted.
   This object will be unref'd after sending the files_removed signal,
   but it could hang around longer if someone ref'd it.
*/
gboolean           nautilus_file_is_gone                   (NautilusFile             *file);

#endif /* NAUTILUS_FILE_H */
