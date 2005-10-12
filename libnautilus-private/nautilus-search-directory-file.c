/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-search-directory-file.c: Subclass of NautilusFile to help implement the
   searches
 
   Copyright (C) 2005 Novell, Inc.
  
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
  
   Author: Anders Carlsson <andersca@imendio.com>
*/

#include <config.h>
#include "nautilus-search-directory-file.h"

#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include "nautilus-search-directory.h"
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

static void nautilus_search_directory_file_init       (gpointer   object,
						       gpointer   klass);
static void nautilus_search_directory_file_class_init (gpointer   klass);

EEL_CLASS_BOILERPLATE (NautilusSearchDirectoryFile,
		       nautilus_search_directory_file,
		       NAUTILUS_TYPE_FILE);

struct NautilusSearchDirectoryFileDetails {
	NautilusSearchDirectory *search_directory;
};

static void
search_directory_file_monitor_add (NautilusFile *file,
				   gconstpointer client,
				   NautilusFileAttributes attributes)
{
	nautilus_directory_monitor_add_internal (file->details->directory,
						 file, client, TRUE, TRUE,
						 attributes, NULL, NULL);
}

static void
search_directory_file_monitor_remove (NautilusFile *file,
				      gconstpointer client)
{
	nautilus_directory_monitor_remove_internal (file->details->directory,
						    file, client);
}

static void
search_directory_file_call_when_ready (NautilusFile *file,
				       NautilusFileAttributes file_attributes,
				       NautilusFileCallback callback,
				       gpointer callback_data)

{
	nautilus_directory_call_when_ready_internal (file->details->directory,
						     file, file_attributes,
						     FALSE, NULL,
						     callback, callback_data);
}
 
static void
search_directory_file_cancel_call_when_ready (NautilusFile *file,
					       NautilusFileCallback callback,
					       gpointer callback_data)
{
	/* Do nothing here, we don't have any pending calls */
}


static gboolean
search_directory_file_check_if_ready (NautilusFile *file,
				      NautilusFileAttributes attributes)
{
	return nautilus_directory_check_if_ready_internal
		(file->details->directory,
		 file, attributes);
}

static GnomeVFSFileType
search_directory_file_get_file_type (NautilusFile *file)
{
	return GNOME_VFS_FILE_TYPE_DIRECTORY;
}			      

static gboolean
search_directory_file_get_item_count (NautilusFile *file, 
				      guint *count,
				      gboolean *count_unreadable)
{
	NautilusSearchDirectory *search_dir;
	GList *file_list;

	if (count) {
		search_dir = NAUTILUS_SEARCH_DIRECTORY (file->details->directory);

		file_list = nautilus_directory_get_file_list (file->details->directory);

		*count = g_list_length (file_list);

		nautilus_file_list_free (file_list);
	}

	return TRUE;
}
    
static void
nautilus_search_directory_file_init (gpointer object, gpointer klass)
{
	NautilusSearchDirectoryFile *search_file;

	search_file = NAUTILUS_SEARCH_DIRECTORY_FILE (object);
}

static void
nautilus_search_directory_file_class_init (gpointer klass)
{
	GObjectClass *object_class;
	NautilusFileClass *file_class;

	object_class = G_OBJECT_CLASS (klass);
	file_class = NAUTILUS_FILE_CLASS (klass);

	file_class->monitor_add = search_directory_file_monitor_add;
	file_class->monitor_remove = search_directory_file_monitor_remove;
	file_class->call_when_ready = search_directory_file_call_when_ready;
	file_class->cancel_call_when_ready = search_directory_file_cancel_call_when_ready;
	file_class->get_file_type = search_directory_file_get_file_type;
	file_class->check_if_ready = search_directory_file_check_if_ready;
	file_class->get_item_count = search_directory_file_get_item_count;
}
