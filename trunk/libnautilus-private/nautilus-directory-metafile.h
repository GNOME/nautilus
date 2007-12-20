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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <libnautilus-private/nautilus-directory.h>

/* Interface for file metadata. */
gboolean nautilus_directory_is_metadata_read	      (NautilusDirectory *directory);

char *   nautilus_directory_get_file_metadata         (NautilusDirectory *directory,
						       const char        *file_name,
						       const char        *key,
						       const char        *default_metadata);
GList *  nautilus_directory_get_file_metadata_list    (NautilusDirectory *directory,
						       const char        *file_name,
						       const char        *list_key,
						       const char        *list_subkey);
gboolean nautilus_directory_get_boolean_file_metadata (NautilusDirectory *directory,
						       const char        *file_name,
						       const char        *key,
						       gboolean           default_metadata);
int      nautilus_directory_get_integer_file_metadata (NautilusDirectory *directory,
						       const char        *file_name,
						       const char        *key,
						       int                default_metadata);

void nautilus_directory_set_file_metadata         (NautilusDirectory *directory,
						   const char        *file_name,
						   const char        *key,
						   const char        *default_metadata,
						   const char        *metadata);
void nautilus_directory_set_file_metadata_list    (NautilusDirectory *directory,
						   const char        *file_name,
						   const char        *list_key,
						   const char        *list_subkey,
						   GList             *list);
void nautilus_directory_set_boolean_file_metadata (NautilusDirectory *directory,
						   const char        *file_name,
						   const char        *key,
						   gboolean           default_metadata,
						   gboolean           metadata);
void nautilus_directory_set_integer_file_metadata (NautilusDirectory *directory,
						   const char        *file_name,
						   const char        *key,
						   int                default_metadata,
						   int                metadata);

void     nautilus_directory_copy_file_metadata             (NautilusDirectory *source_directory,
							    const char        *source_file_name,
							    NautilusDirectory *destination_directory,
							    const char        *destination_file_name);
void     nautilus_directory_remove_file_metadata           (NautilusDirectory *directory,
							    const char        *file_name);
void     nautilus_directory_rename_file_metadata           (NautilusDirectory *directory,
							    const char        *old_file_name,
							    const char        *new_file_name);
void	 nautilus_directory_rename_directory_metadata      (NautilusDirectory *directory,
							    const char        *new_directory_uri);

void nautilus_directory_register_metadata_monitor   (NautilusDirectory *directory);
void nautilus_directory_unregister_metadata_monitor (NautilusDirectory *directory);
