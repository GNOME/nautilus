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

#include "nautilus-directory.h"

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

#include <gnome-xml/tree.h>

#include "nautilus-file.h"

struct NautilusDirectoryDetails
{
	char *uri_text;
	GnomeVFSURI *uri;

	GnomeVFSURI *metafile_uri;
	GnomeVFSURI *alternate_metafile_uri;
	gboolean use_alternate_metafile;

	xmlDoc *metafile_tree;
	guint write_metafile_idle_id;

	GnomeVFSAsyncHandle *directory_load_in_progress;
	GnomeVFSDirectoryListPosition directory_load_list_last_handled;

	GList *pending_file_info;
        guint dequeue_pending_idle_id;

	gboolean directory_loaded;

	GList *files;
};

NautilusFile *nautilus_directory_find_file              (NautilusDirectory *directory,
							 const char        *file_name);
NautilusFile *nautilus_directory_new_file               (NautilusDirectory *directory,
							 GnomeVFSFileInfo  *info);
char *        nautilus_directory_get_file_metadata      (NautilusDirectory *directory,
							 const char        *file_name,
							 const char        *tag,
							 const char        *default_metadata);
void          nautilus_directory_set_file_metadata      (NautilusDirectory *directory,
							 const char        *file_name,
							 const char        *tag,
							 const char        *default_metadata,
							 const char        *metadata);
xmlNode *     nautilus_directory_get_file_metadata_node (NautilusDirectory *directory,
							 const char        *file_name,
							 gboolean           create);
void          nautilus_directory_files_changed          (NautilusDirectory *directory,
							 GList             *changed_files);
void          nautilus_directory_files_removed          (NautilusDirectory *directory,
							 GList             *removed_files);
void          nautilus_directory_request_write_metafile (NautilusDirectory *directory);

/* debugging functions */
int           nautilus_directory_number_outstanding     (void);
