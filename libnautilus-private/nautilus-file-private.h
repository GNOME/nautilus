/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

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
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef NAUTILUS_FILE_PRIVATE_H
#define NAUTILUS_FILE_PRIVATE_H

#include "nautilus-directory.h"
#include "nautilus-file.h"
#include "nautilus-monitor.h"
#include <eel/eel-glib-extensions.h>

#define NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_CHARACTERS_PER_LINE 80
#define NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_LINES               24
#define NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_BYTES            10000

/* These are in the typical sort order. Known things come first, then
 * things where we can't know, finally things where we don't yet know.
 */
typedef enum {
	KNOWN,
	UNKNOWABLE,
	UNKNOWN
} Knowledge;

typedef struct {
	int automatic_emblems_as_integer;
	char emblem_keywords[1];
} NautilusFileSortByEmblemCache;

struct NautilusFileDetails
{
	NautilusDirectory *directory;
	char *relative_uri;

	GnomeVFSFileInfo *info;
	GnomeVFSResult get_info_error;

	NautilusMonitor *monitor;
	
	guint directory_count;

	guint deep_directory_count;
	guint deep_file_count;
	guint deep_unreadable_count;
	GnomeVFSFileSize deep_size;

	GList *mime_list; /* If this is a directory, the list of MIME types in it. */
	char *top_left_text;

	/* Info you might get from a link (.desktop, .directory or nautilus link) */
	char *display_name;
	char *custom_icon_uri;
	char *activation_uri;

	/* The following is for file operations in progress. Since
	 * there are normally only a few of these, we can move them to
	 * a separate hash table or something if required to keep the
	 * file objects small.
	 */
	GList *operations_in_progress;

	/* We use this to cache automatic emblems and emblem keywords
	   to speed up compare_by_emblems. */
	NautilusFileSortByEmblemCache *compare_by_emblem_cache;
	
	/* boolean fields: bitfield to save space, since there can be
           many NautilusFile objects. */

	eel_boolean_bit unconfirmed                   : 1;
	eel_boolean_bit is_gone                       : 1;
	/* Set by the NautilusDirectory while it's loading the file
	 * list so the file knows not to do redundant I/O.
	 */
	eel_boolean_bit loading_directory             : 1;
	/* got_info known from info field being non-NULL */
	eel_boolean_bit get_info_failed               : 1;
	eel_boolean_bit file_info_is_up_to_date       : 1;

	eel_boolean_bit got_directory_count           : 1;
	eel_boolean_bit directory_count_failed        : 1;
	eel_boolean_bit directory_count_is_up_to_date : 1;

	NautilusRequestStatus deep_counts_status      : 2;
	/* no deep_counts_are_up_to_date field; since we expose
           intermediate values for this attribute, we do actually
           forget it rather than invalidating. */

	eel_boolean_bit got_mime_list                 : 1;
	eel_boolean_bit mime_list_failed              : 1;
	eel_boolean_bit mime_list_is_up_to_date       : 1;

	eel_boolean_bit got_top_left_text             : 1;
	eel_boolean_bit top_left_text_is_up_to_date   : 1;

	eel_boolean_bit got_link_info                 : 1;
	eel_boolean_bit link_info_is_up_to_date       : 1;
};

NautilusFile *nautilus_file_new_from_info                  (NautilusDirectory      *directory,
							    GnomeVFSFileInfo       *info);
NautilusFile *nautilus_file_get_existing                   (const char             *uri);
void          nautilus_file_emit_changed                   (NautilusFile           *file);
void          nautilus_file_mark_gone                      (NautilusFile           *file);
gboolean      nautilus_file_info_missing                   (NautilusFile           *file,
							    GnomeVFSFileInfoFields  needed_mask);
char *        nautilus_extract_top_left_text               (const char             *text,
							    int                     length);
void          nautilus_file_set_directory                  (NautilusFile           *file,
							    NautilusDirectory      *directory);
gboolean      nautilus_file_get_date                       (NautilusFile           *file,
							    NautilusDateType        date_type,
							    time_t                 *date);
void          nautilus_file_updated_deep_count_in_progress (NautilusFile           *file);

/* Compare file's state with a fresh file info struct, return FALSE if
 * no change, update file and return TRUE if the file info contains
 * new state.  */
gboolean      nautilus_file_update_info                    (NautilusFile           *file,
							    GnomeVFSFileInfo       *info);
gboolean      nautilus_file_update_name                    (NautilusFile           *file,
							    const char             *name);

/* Return true if the top lefts of files in this directory should be
 * fetched, according to the preference settings.
 */
gboolean      nautilus_file_should_get_top_left_text       (NautilusFile           *file);

/* Mark specified attributes for this file out of date without canceling current
 * I/O or kicking off new I/O.
 */
void          nautilus_file_invalidate_attributes_internal (NautilusFile           *file,
							    GList                  *file_attributes);
GList *       nautilus_file_get_all_attributes             (void);

gboolean      nautilus_file_is_self_owned                  (NautilusFile           *file);

void          nautilus_file_invalidate_count_and_mime_list (NautilusFile              *file);

#endif
