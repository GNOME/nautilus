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

#include "nautilus-file.h"
#include "nautilus-directory.h"

struct NautilusFileDetails
{
	NautilusDirectory *directory;
	gboolean self_owned;
	gboolean unconfirmed;

	gboolean is_gone;
	char *name;

	GnomeVFSFileInfo *info;
	gboolean get_info_failed;

	gboolean got_directory_count;
	gboolean directory_count_failed;
	guint directory_count;

	NautilusRequestStatus deep_counts_status;
	guint deep_directory_count;
	guint deep_file_count;
	guint deep_unreadable_count;
	GnomeVFSFileSize deep_size;

	gboolean got_top_left_text;
	char *top_left_text;

	gboolean got_activation_uri;
	char *activation_uri;

	/* The following is for file operations in progress. Since
	 * there are normally only a few of these, we can move them to
	 * a separate hash table or something if required to keep the
	 * file objects small.
	 */
	GList *operations_in_progress;
};

#define NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_CHARACTERS_PER_LINE 80
#define NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_LINES               24
#define NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_BYTES            10000

NautilusFile *nautilus_file_new_from_info            (NautilusDirectory *directory,
						      GnomeVFSFileInfo  *info);
NautilusFile *nautilus_file_get_existing             (const char        *uri);
void          nautilus_file_emit_changed             (NautilusFile      *file);
void          nautilus_file_mark_gone                (NautilusFile      *file);
char *        nautilus_extract_top_left_text         (const char        *text,
						      int                length);
gboolean      nautilus_file_contains_text            (NautilusFile      *file);

/* Compare file's state with a fresh file info struct, return FALSE if
 * no change, update file and return TRUE if the file info contains
 * new state.
 */
gboolean      nautilus_file_update_info              (NautilusFile      *file,
						      GnomeVFSFileInfo  *info);
gboolean      nautilus_file_update_name              (NautilusFile      *file,
						      const char        *name);

/* Return true if the top lefts of files in this directory should be
 * fetched, according to the preference settings.
 */
gboolean      nautilus_file_should_get_top_left_text (NautilusFile      *file);
