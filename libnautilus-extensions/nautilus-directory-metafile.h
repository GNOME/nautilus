/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory-metafile.h: Nautilus directory model.
 
   Copyright (C) 2000 Eazel, Inc.
  
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
#include <tree.h>

/* Interface for file metadata. */
char *   nautilus_directory_get_file_metadata              (NautilusDirectory *directory,
							    const char        *file_name,
							    const char        *key,
							    const char        *default_metadata);
GList *  nautilus_directory_get_file_metadata_list         (NautilusDirectory *directory,
							    const char        *file_name,
							    const char        *list_key,
							    const char        *list_subkey);
gboolean nautilus_directory_set_file_metadata              (NautilusDirectory *directory,
							    const char        *file_name,
							    const char        *key,
							    const char        *default_metadata,
							    const char        *metadata);
gboolean nautilus_directory_set_file_metadata_list         (NautilusDirectory *directory,
							    const char        *file_name,
							    const char        *list_key,
							    const char        *list_subkey,
							    GList             *list);
void     nautilus_directory_update_file_metadata           (NautilusDirectory *directory,
							    const char        *old_file_name,
							    const char        *new_file_name);

/* Interface for housekeeping. */
void     nautilus_directory_metafile_apply_pending_changes (NautilusDirectory *directory);
void     nautilus_directory_metafile_destroy               (NautilusDirectory *directory);
